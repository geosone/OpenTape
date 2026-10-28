/* Chunked fixture deliberately exceeds 64 KiB without one large allocation.
 * Failure injection proves checked-close atomicity and no serialized grants. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/catalog_file.h"
static void put32(oa_u8 *data, oa_u32 value)
{
    unsigned i;
    for (i = 0u; i < 4u; ++i) { data[i] = (oa_u8)value; value >>= 8; }
}
#define CHUNKS 160u
struct fixture {
    oa_u8 *data[CHUNKS];
    oa_u32 size, offset;
    unsigned closes, publishes, rechecks, reads, writes;
    unsigned read_fail, short_write, close_fail, check_fail, publish_fail, read_limit;
};
static struct fixture f;
static oa_u8 byte_at(oa_u32 at) { return f.data[(unsigned)(at / 512ul)][(unsigned)(at % 512ul)]; }
static void set_byte(oa_u32 at, oa_u8 value) { f.data[(unsigned)(at / 512ul)][(unsigned)(at % 512ul)] = value; }
static unsigned write_data(void *p, const oa_u8 *data, unsigned size, unsigned *accepted)
{
    struct fixture *v = p;
    unsigned i;
    ++v->writes;
    *accepted = v->short_write && size ? size - 1u : size;
    for (i = 0u; i < *accepted; ++i) { assert(v->size < CHUNKS * 512ul); set_byte(v->size++, data[i]); }
    return 0u;
}
static unsigned read_data(void *p, void *data, unsigned size, unsigned *got)
{
    struct fixture *v = p;
    unsigned i;
    ++v->reads; *got = 0u;
    if (v->read_fail && v->reads >= v->read_fail) return 5u;
    if (v->read_limit && size > v->read_limit) size = v->read_limit;
    for (i = 0u; i < size && v->offset < v->size; ++i) ((oa_u8 *)data)[i] = byte_at(v->offset++);
    *got = i; return 0u;
}
static unsigned close_data(void *p) { struct fixture *v = p; ++v->closes; return v->close_fail; }
static unsigned check_data(void *p) { struct fixture *v = p; ++v->rechecks; return v->check_fail; }
static unsigned publish(void *p) { struct fixture *v = p; ++v->publishes; return v->publish_fail; }
static struct ot_source_ops source = { &f, read_data, check_data, close_data };
static struct ot_selection_output output = { &f, write_data, close_data, publish };
static void reset(void)
{
    f.offset = 0ul; f.closes = f.publishes = f.rechecks = f.reads = f.writes = 0u;
    f.read_fail = f.short_write = f.close_fail = f.check_fail = f.publish_fail = 0u;
    f.read_limit = 13u;
}
static void label_init(struct ot_volume_label *label, const char *name)
{
    memset(label, 0, sizeof(*label)); label->version = 1u; label->volume_sequence = 1ul;
    strcpy(label->volume_name, name);
}
static void populate(struct ot_catalog *c, unsigned count, unsigned long_path)
{
    struct ot_file_metadata m;
    unsigned i, j;
    memset(c, 0, sizeof(*c));
    for (i = 0u; i < count; ++i) {
        memset(&m, 0, sizeof(m)); m.kind = OT_FILE_REGULAR; m.file_size = 35000ul + i;
        if (long_path) for (j = 0u; j < 26u; ++j) strcat(m.path, "ABCDEFGH/");
        sprintf(m.path + strlen(m.path), "F%03u.BIN", i);
        assert(ot_catalog_capture(c, 1ul + i / 128u, 1ul + i % 128u, &m) == 0);
    }
    if (count) { assert(ot_catalog_mark(c, 0u, 1u) == 0); assert(ot_catalog_replace(c, 0u, 1u) == 0); }
}
static void unchanged_failure(struct ot_catalog *c, struct ot_volume_label *label)
{
    struct ot_catalog before = *c;
    struct ot_catalog_entry node = *c->first;
    struct ot_volume_label volume = *label;
    assert(ot_catalog_file_load(c, label, &source) != 0);
    assert(memcmp(&before, c, sizeof(before)) == 0);
    assert(memcmp(&node, c->first, sizeof(node)) == 0);
    assert(memcmp(&volume, label, sizeof(volume)) == 0);
    assert(f.closes == 1u);
}
static void repair_header(void)
{
    oa_u8 header[32];
    unsigned i;
    for (i = 0u; i < 32u; ++i) header[i] = byte_at(i);
    put32(header + 28u, ot_crc32(header, 28u));
    for (i = 0u; i < 32u; ++i) set_byte(i, header[i]);
}
static void repair_crc(void)
{
    oa_u8 header[32], value;
    oa_u32 at, crc = 0ul;
    unsigned i;
    for (at = 32ul; at < f.size; ++at) { value = byte_at(at); crc = ot_crc32_extend(crc, &value, 1u); }
    for (i = 0u; i < 32u; ++i) header[i] = byte_at(i);
    put32(header + 20u, crc); put32(header + 28u, ot_crc32(header, 28u));
    for (i = 0u; i < 32u; ++i) set_byte(i, header[i]);
}
int main(void)
{
    struct ot_catalog original, loaded, old;
    struct ot_volume_label label, destination;
    struct ot_catalog_file_info info;
    const struct ot_catalog_entry *a, *b;
    oa_u32 saved_size, at;
    oa_u8 saved;
    unsigned i;
    for (i = 0u; i < CHUNKS; ++i) { f.data[i] = malloc(512u); assert(f.data[i] != 0); }
    label_init(&label, "CATTEST"); populate(&original, 256u, 1u);
    assert(ot_catalog_file_measure(&original, &label, &info) == 0 && info.size > 65535ul);
    reset(); assert(ot_catalog_file_save(&original, &label, &output) == 0);
    assert(f.size == info.size && f.closes == 1u && f.publishes == 1u);
    saved_size = f.size;
    populate(&loaded, 1u, 0u); label_init(&destination, "OLD");
    reset(); assert(ot_catalog_file_load(&loaded, &destination, &source) == 0);
    assert(f.closes == 1u && f.rechecks == 1u && loaded.count == 256u && loaded.marked == 0u && loaded.replacements == 0u);
    assert(!strcmp(destination.volume_name, "CATTEST") && original.marked == 1u && original.replacements == 1u);
    for (a = original.first, b = loaded.first; a; a = a->next, b = b->next) {
        assert(b && a->session == b->session && a->object == b->object);
        assert(!strcmp(a->metadata.path, b->metadata.path) && a->metadata.file_size == b->metadata.file_size);
        assert(!b->marked && !b->replace);
    }
    ot_catalog_free(&loaded); populate(&old, 1u, 0u); label_init(&destination, "KEEP");
    reset(); f.check_fail = 1u; unchanged_failure(&old, &destination);
    reset(); f.close_fail = 1u; unchanged_failure(&old, &destination);
    reset(); f.read_fail = 12u; unchanged_failure(&old, &destination);
    reset(); --f.size; unchanged_failure(&old, &destination); f.size = saved_size;
    reset(); set_byte(f.size++, 0u); unchanged_failure(&old, &destination); f.size = saved_size;
    /* Damage header and body independently. CRC never grants authenticity. */
    for (at = 0ul; at < 32ul; ++at) {
        reset(); saved = byte_at(at); set_byte(at, (oa_u8)(saved ^ 1u));
        unchanged_failure(&old, &destination); set_byte(at, saved);
    }
    /* Recomputed header CRC must still reject invalid structure. */
    for (i = 0u; i < 6u; ++i) {
        at = i == 0u ? 8ul : i == 1u ? 10ul : i == 2u ? 13ul :
            i == 3u ? 14ul : i == 4u ? 16ul : 24ul;
        saved = byte_at(at); set_byte(at, (oa_u8)(saved ^ 2u)); repair_header(); reset();
        unchanged_failure(&old, &destination); set_byte(at, saved); repair_header();
    }
    reset(); saved = byte_at(140ul); set_byte(140ul, (oa_u8)(saved ^ 1u));
    unchanged_failure(&old, &destination); set_byte(140ul, saved);
    reset(); set_byte(46ul, 1u); repair_crc();
    unchanged_failure(&old, &destination); set_byte(46ul, 0u); repair_crc();
    /* Valid checksums still cannot bypass entry validation. */
    for (i = 0u; i < 5u; ++i) {
        at = i == 0u ? 120ul : i == 1u ? 122ul : i == 2u ? 124ul : i == 3u ? 128ul : 132ul;
        saved = byte_at(at); set_byte(at, i == 1u ? 1u : 0u); repair_crc(); reset();
        unchanged_failure(&old, &destination); set_byte(at, saved); repair_crc();
    }
    /* Nonmonotonic identity despite valid checksums. */
    at = 120ul + 12ul + OT_FILE_METADATA_PREFIX + (26ul * 9ul + 8ul) + 8ul;
    saved = byte_at(at); set_byte(at, 1u); repair_crc(); reset();
    unchanged_failure(&old, &destination); set_byte(at, saved); repair_crc();
    ot_catalog_free(&old); ot_catalog_free(&original);
    populate(&original, 1u, 0u);
    reset(); f.size = 0ul; f.short_write = 1u;
    assert(ot_catalog_file_save(&original, &label, &output) == OT_CATALOG_FILE_IO && f.closes == 1u && !f.publishes);
    reset(); f.size = 0ul; f.close_fail = 1u;
    assert(ot_catalog_file_save(&original, &label, &output) == OT_CATALOG_FILE_IO && f.closes == 1u && !f.publishes);
    reset(); f.size = 0ul; f.publish_fail = 1u;
    assert(ot_catalog_file_save(&original, &label, &output) == OT_CATALOG_FILE_IO && f.closes == 1u && f.publishes == 1u);
    original.first->object = 0ul; reset();
    assert(ot_catalog_file_save(&original, &label, &output) == OT_CATALOG_ARGUMENT && f.closes == 1u && !f.writes && !f.publishes);
    ot_catalog_free(&original); memset(&loaded, 0, sizeof(loaded));
    reset(); f.size = 0ul; assert(ot_catalog_file_save(&original, &label, &output) == 0);
    reset(); assert(ot_catalog_file_load(&loaded, &destination, &source) == 0 && !loaded.count);
    ot_catalog_free(&loaded);
    for (i = 0u; i < CHUNKS; ++i) free(f.data[i]);
    puts("catalogue file: >64K roundtrip, metadata-only, CRC/structure/EOF and atomic failures OK");
    return 0;
}

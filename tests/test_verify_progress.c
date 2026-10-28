/* Per-block snapshots must retain totals across sessions, include partial data
 * and packed records, and remain atomic on malformed observations/overflow. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/verify_progress.h"
#include "opentape/archive_writer.h"
static oa_u8 *blocks[20];
static unsigned sizes[20], count;
static int capture(void *p, const oa_u8 *block, unsigned size, unsigned *accepted)
{
    (void)p; assert(count < 20u); blocks[count] = malloc(size); assert(blocks[count]);
    memcpy(blocks[count], block, size); sizes[count++] = size; *accepted = size; return 0;
}
static int finish(void *p) { (void)p; return 0; }
static void fixture(void)
{
    struct ot_writer_sink sink;
    struct ot_archive_writer w;
    struct ot_volume_label label;
    struct ot_session_start start;
    struct ot_file_metadata m;
    oa_u8 buffer[512];
    memset(&w, 0, sizeof(w)); memset(&label, 0, sizeof(label)); memset(&start, 0, sizeof(start));
    memset(&m, 0, sizeof(m)); label.version = 1u;
    sink.context = 0; sink.write_block = capture; sink.finish_session = finish;
    assert(ot_archive_writer_init(&w, &sink, buffer, sizeof(buffer)) == 0);
    assert(ot_archive_writer_volume(&w, &label) == 0 && ot_archive_writer_session(&w, 1ul, &start) == 0);
    m.kind = OT_FILE_DIRECTORY; m.dos_attributes = 0x10u; strcpy(m.path, "DIR");
    assert(ot_archive_writer_metadata(&w, 1ul, &m) == 0);
    m.kind = OT_FILE_REGULAR; m.dos_attributes = 0u; m.file_size = 3ul; strcpy(m.path, "DIR/ONE.BIN");
    assert(ot_archive_writer_metadata(&w, 2ul, &m) == 0);
    assert(ot_archive_writer_data(&w, (const oa_u8 *)"A", 1u) == 0);
    assert(ot_archive_writer_data(&w, (const oa_u8 *)"BC", 2u) == 0 && ot_archive_writer_file_end(&w) == 0);
    m.file_size = 0ul; strcpy(m.path, "EMPTY.BIN");
    assert(ot_archive_writer_metadata(&w, 3ul, &m) == 0 && ot_archive_writer_file_end(&w) == 0);
    assert(ot_archive_writer_session_end(&w) == 0 && ot_archive_writer_session(&w, 2ul, &start) == 0);
    m.file_size = 1ul; strcpy(m.path, "LAST.BIN");
    assert(ot_archive_writer_metadata(&w, 1ul, &m) == 0 && ot_archive_writer_data(&w, (const oa_u8 *)"Z", 1u) == 0);
    assert(ot_archive_writer_file_end(&w) == 0 && ot_archive_writer_session_end(&w) == 0 && ot_archive_writer_complete(&w) == 0);
    assert(count == 15u);
}
static void rejected(struct ot_verify_progress *p, const struct ot_archive_verify *v, const oa_u8 *block, unsigned size)
{
    struct ot_verify_progress old = *p;
    assert(ot_verify_progress_block(p, v, block, size) != 0 && memcmp(&old, p, sizeof(old)) == 0);
}
int main(void)
{
    struct ot_archive_verify v;
    struct ot_verify_progress p, old;
    struct ot_block_header header;
    oa_u8 packed[512], payload[400];
    unsigned i, used = 0u;
    fixture(); memset(&v, 0, sizeof(v)); memset(&p, 0, sizeof(p));
    for (i = 0u; i < count; ++i) {
        assert(ot_archive_verify_block(&v, blocks[i], (oa_u16)sizes[i]) == 0);
        assert(ot_verify_progress_block(&p, &v, blocks[i], sizes[i]) == 0);
        assert(p.blocks == i + 1ul && p.records == i + 1ul);
        if (i == 4u) assert(p.file_bytes == 1ul && p.bytes_low == 1ul && !p.files);
        if (i == 5u) assert(p.file_bytes == 3ul && p.bytes_low == 3ul && !p.files);
        if (i == 9u) assert(p.boundary && p.sessions == 1ul && p.files == 2ul);
        if (i == 10u) assert(!p.boundary && p.files == 2ul && !strcmp(p.current.path, "EMPTY.BIN"));
    }
    assert(p.files == 3ul && p.directories == 1ul && p.bytes_low == 4ul && !p.bytes_high && p.sessions == 2ul);
    rejected(&p, &v, blocks[14], sizes[14]);
    memset(&v, 0, sizeof(v)); memset(&p, 0, sizeof(p));
    for (i = 0u; i < 5u; ++i) {
        assert(ot_archive_verify_block(&v, blocks[i], (oa_u16)sizes[i]) == 0);
        if (i == 4u) { p.bytes_low = 0xfffffffful; p.bytes_high = 9ul; }
        assert(ot_verify_progress_block(&p, &v, blocks[i], sizes[i]) == 0);
    }
    assert(p.bytes_low == 0ul && p.bytes_high == 10ul);
    assert(ot_archive_verify_block(&v, blocks[5], (oa_u16)sizes[5]) == 0);
    p.bytes_low = 0xfffffffful; p.bytes_high = 0xfffffffful;
    rejected(&p, &v, blocks[5], sizes[5]);
    p.bytes_low = 0ul; p.bytes_high = 0ul;
    assert(ot_verify_progress_block(&p, &v, blocks[5], sizes[5]) == 0);
    assert(ot_archive_verify_block(&v, blocks[6], (oa_u16)sizes[6]) == 0);
    p.files = 0xfffffffful; rejected(&p, &v, blocks[6], sizes[6]);
    p.files = 0ul; old = p;
    rejected(&p, &v, blocks[6], 10u);
    memcpy(packed, blocks[6], sizes[6]); packed[OT_BLOCK_HEADER_SIZE + 4u] = 255u;
    rejected(&p, &v, packed, sizes[6]); assert(memcmp(&p, &old, sizeof(p)) == 0);
    memset(&v, 0, sizeof(v)); memset(&p, 0, sizeof(p));
    for (i = 0u; i < 3u; ++i) {
        assert(ot_archive_verify_block(&v, blocks[i], (oa_u16)sizes[i]) == 0);
        assert(ot_verify_progress_block(&p, &v, blocks[i], sizes[i]) == 0);
    }
    for (i = 3u; i <= 6u; ++i) {
        memcpy(payload + used, blocks[i] + OT_BLOCK_HEADER_SIZE, sizes[i] - OT_BLOCK_HEADER_SIZE);
        used += sizes[i] - OT_BLOCK_HEADER_SIZE;
    }
    memset(&header, 0, sizeof(header)); header.block_number = 3ul; header.session_id = 1ul; header.record_count = 4u;
    assert(ot_block_encode(packed, sizeof(packed), &header, payload, (oa_u16)used) == 0);
    assert(ot_archive_verify_block(&v, packed, (oa_u16)(used + OT_BLOCK_HEADER_SIZE)) == 0);
    assert(ot_verify_progress_block(&p, &v, packed, used + OT_BLOCK_HEADER_SIZE) == 0);
    assert(p.files == 1ul && p.directories == 1ul && p.bytes_low == 3ul && p.file_bytes == 3ul && p.records == 7ul);
    for (i = 0u; i < count; ++i) free(blocks[i]);
    puts("verification progress: partial files, sessions, packed blocks, wide carry and atomic errors OK");
    return 0;
}

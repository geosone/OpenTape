#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/source_file.h"

struct fixture {
    unsigned length, position, reads, closes, changed, read_error, bad_count, close_error;
    unsigned writes, finishes, sink_error;
    unsigned rechecks, corrupt, late_error, late_count;
    struct ot_archive_verify verified;
};
static unsigned read_source(void *p, void *out, unsigned size, unsigned *got)
{
    struct fixture *f = p;
    unsigned n, i, error = f->read_error, bad = f->bad_count;
    if (f->late_error && f->position < f->length) error = 0u;
    if (f->late_count && f->position < f->length) bad = 0u;
    ++f->reads;
    n = f->length - f->position;
    if (n > size) n = size;
    if (n > 3u) n = 3u; /* Legal short reads must not imply EOF. */
    for (i = 0u; i < n; ++i) {
        ((oa_u8 *)out)[i] = (oa_u8)(f->position + i);
        if (f->corrupt && f->position + i == 5u) ((oa_u8 *)out)[i] ^= 1u;
    }
    f->position += n;
    *got = bad ? size + 1u : n;
    return error;
}
static unsigned recheck(void *p)
{ struct fixture *f = p; ++f->rechecks; return f->changed; }
static unsigned close_source(void *p)
{
    struct fixture *f = p;
    ++f->closes;
    return f->close_error;
}
static int write_sink(void *p, const oa_u8 *b, unsigned n, unsigned *got)
{
    struct fixture *f = p;
    int rc;
    ++f->writes;
    if (f->sink_error && f->writes >= 4u) return -1;
    rc = ot_archive_verify_block(&f->verified, b, (oa_u16)n);
    *got = n;
    return rc;
}
static int finish_sink(void *p)
{
    struct fixture *f = p;
    ++f->finishes;
    return ot_archive_verify_finish(&f->verified);
}
static void test_readback(void)
{
    static const int expected[20] = {
        0, OT_SOURCE_CONTENT, OT_SOURCE_CONTENT, OT_SOURCE_CONTENT, OT_SOURCE_IO,
        OT_SOURCE_IO, OT_SOURCE_CHANGED, OT_SOURCE_IO, 0, OT_SOURCE_CONTENT,
        OT_SOURCE_CONTENT, OT_SOURCE_CONTENT, OT_SOURCE_IO, OT_SOURCE_CHANGED,
        OT_SOURCE_IO, OT_SOURCE_IO, OT_SOURCE_IO, OT_SOURCE_CHANGED, OT_SOURCE_CONTENT, 0
    };
    struct fixture f;
    struct ot_source_ops ops;
    oa_u8 buffer[16], original[20];
    oa_u32 size, crc;
    unsigned test, i;
    ops.context = &f; ops.read = read_source; ops.recheck = recheck; ops.close = close_source;
    for (i = 0u; i < sizeof(original); ++i) original[i] = (oa_u8)i;
    for (test = 0u; test < 20u; ++test) {
        memset(&f, 0, sizeof(f)); f.length = 20u; size = 20ul; crc = ot_crc32(original, 20u);
        if (test == 1u) f.length = 19u;
        if (test == 2u) f.length = 21u;
        if (test == 3u) crc ^= 1ul;
        if (test == 4u || test == 12u) f.read_error = 5u;
        if (test == 5u) f.bad_count = 1u;
        if (test == 6u || test == 13u) f.changed = 1u;
        if (test == 7u || test == 11u || test == 12u || test == 13u) f.close_error = 6u;
        if (test >= 8u && test <= 10u) { f.length = 0u; size = 0ul; crc = 0ul; }
        if (test == 9u) f.length = 1u;
        if (test == 10u) crc = 1ul;
        if (test == 11u) f.length = 19u;
        if (test == 14u) { f.read_error = 5u; f.late_error = 1u; }
        if (test == 15u) { f.bad_count = 1u; f.late_count = 1u; }
        if (test == 16u || test == 17u) { f.length = 0u; size = 0ul; crc = 0ul; }
        if (test == 16u) f.read_error = 5u;
        if (test == 17u) f.changed = 1u;
        if (test == 18u) f.corrupt = 1u;
        assert(ot_source_verify(&ops, size, crc, buffer, test == 19u ? 1u : sizeof(buffer)) == expected[test]);
        assert(f.closes == 1u);
        assert(f.rechecks == (test == 0u || test == 6u || test == 7u || test == 8u ||
                             test == 13u || test == 17u || test == 19u ? 1u : 0u));
        assert(f.writes == 0u && f.finishes == 0u);
    }
    memset(&f, 0, sizeof(f));
    assert(ot_source_verify(0, 0ul, 0ul, buffer, sizeof(buffer)) == OT_SOURCE_ARGUMENT);
    assert(ot_source_verify(&ops, 0ul, 0ul, 0, sizeof(buffer)) == OT_SOURCE_ARGUMENT);
    assert(ot_source_verify(&ops, 0ul, 0ul, buffer, 0u) == OT_SOURCE_ARGUMENT);
    ops.read = 0;
    assert(ot_source_verify(&ops, 0ul, 0ul, buffer, sizeof(buffer)) == OT_SOURCE_ARGUMENT);
    assert(f.reads == 0u && f.closes == 0u);
    puts("OpenTape source readback verification tests passed");
}
int main(void)
{
    struct fixture f;
    struct ot_archive_writer w;
    struct ot_writer_sink sink;
    struct ot_source_ops source;
    struct ot_file_metadata m;
    struct ot_volume_label label;
    struct ot_session_start start;
    oa_u8 buffer[512], data[16];
    unsigned test;
    int rc;
    for (test = 0u; test < 10u; ++test) {
        memset(&f, 0, sizeof(f));
        memset(&w, 0, sizeof(w));
        memset(&m, 0, sizeof(m));
        memset(&label, 0, sizeof(label));
        memset(&start, 0, sizeof(start));
        f.length = 20u;
        m.kind = OT_FILE_REGULAR;
        m.file_size = 20ul;
        strcpy(m.path, "DATA.BIN");
        label.version = 1u;
        sink.context = &f; sink.write_block = write_sink; sink.finish_session = finish_sink;
        source.context = &f; source.read = read_source; source.recheck = recheck; source.close = close_source;
        assert(ot_archive_writer_init(&w, &sink, buffer, sizeof(buffer)) == 0);
        assert(ot_archive_writer_volume(&w, &label) == 0);
        assert(ot_archive_writer_session(&w, 1ul, &start) == 0);
        if (test == 1u) f.length = 19u;
        if (test == 2u) f.length = 21u;
        if (test == 3u) f.changed = 1u;
        if (test == 4u) f.read_error = 5u;
        if (test == 5u) f.bad_count = 1u;
        if (test == 6u) f.close_error = 6u;
        if (test == 7u) f.sink_error = 1u;
        if (test == 8u) { m.file_size = 0ul; f.length = 0u; }
        if (test == 9u) { m.file_size = 0ul; f.length = 1u; }
        rc = ot_backup_source(&w, 1ul, &m, &source, data, sizeof(data));
        assert(f.closes == 1u);
        if (test == 0u || test == 8u) {
            assert(rc == 0);
            assert(ot_archive_writer_session_end(&w) == 0);
            assert(ot_archive_writer_complete(&w) == 0);
            assert(f.verified.bytes_low == m.file_size && f.verified.files == 1ul);
        } else {
            assert(rc != 0 && w.failed);
            assert(ot_archive_writer_session_end(&w) == OT_WRITER_FAILED);
            assert(f.finishes == 0u);
        }
    }
    test_readback();
    puts("OpenTape source-file tests passed");
    return 0;
}

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/restore.h"
#include "opentape/archive_writer.h"
struct fixture {
    oa_u8 block[16][512]; unsigned sizes[16], count;
    unsigned dirs, begins, writes, commits, aborts, mode;
    char published[32];
};
static struct fixture f;
static int capture(void *p, const oa_u8 *b, unsigned n, unsigned *accepted)
{
    struct fixture *s = p;
    assert(s->count < 16u && n <= 512u);
    memcpy(s->block[s->count], b, n); s->sizes[s->count++] = n; *accepted = n; return 0;
}
static int finish(void *p) { (void)p; return 0; }
static unsigned directory(void *p, const struct ot_file_metadata *m)
{ struct fixture *s = p; assert(m->kind == OT_FILE_DIRECTORY); ++s->dirs; return s->mode == 6u ? 5u : 0u; }
static unsigned begin(void *p, const struct ot_file_metadata *m)
{
    struct fixture *s = p; ++s->begins;
    if (s->mode == 15u || s->mode == 16u ||
        (s->mode == 14u && strcmp(m->path, "ROOT/A.BIN") == 0)) return OT_RESTORE_SKIP;
    return s->mode == 3u ? 5u : 0u;
}
static unsigned write_data(void *p, const oa_u8 *b, unsigned n, unsigned *got)
{
    struct fixture *s = p; ++s->writes;
    assert(n == 5u && memcmp(b, "hello", n) == 0);
    *got = s->mode == 4u ? n - 1u : s->mode == 12u ? n + 1u : n;
    return s->mode == 13u ? 5u : 0u;
}
static unsigned commit(void *p, const struct ot_file_metadata *m)
{
    struct fixture *s = p;
    if (s->mode == 5u) return 5u;
    ++s->commits; strcpy(s->published, m->path); return 0u;
}
static unsigned abort_file(void *p) { ++((struct fixture *)p)->aborts; return 0u; }
static void fixture(unsigned mode)
{
    struct ot_archive_writer w;
    struct ot_writer_sink sink;
    struct ot_volume_label label;
    struct ot_session_start start;
    struct ot_file_metadata m;
    oa_u8 buffer[512];
    memset(&f, 0, sizeof(f)); memset(&w, 0, sizeof(w));
    memset(&label, 0, sizeof(label)); memset(&start, 0, sizeof(start)); memset(&m, 0, sizeof(m));
    f.mode = mode; label.version = 1u;
    sink.context = &f; sink.write_block = capture; sink.finish_session = finish;
    assert(ot_archive_writer_init(&w, &sink, buffer, sizeof(buffer)) == 0);
    assert(ot_archive_writer_volume(&w, &label) == 0);
    assert(ot_archive_writer_session(&w, 1ul, &start) == 0);
    m.kind = OT_FILE_DIRECTORY; m.dos_attributes = 0x10u; strcpy(m.path, "ROOT");
    assert(ot_archive_writer_metadata(&w, 1ul, &m) == 0);
    m.kind = OT_FILE_REGULAR; m.dos_attributes = 0x20u; m.file_size = 5ul;
    strcpy(m.path, mode == 7u ? OT_RESTORE_TEMP : "ROOT/A.BIN");
    assert(ot_archive_writer_metadata(&w, 2ul, &m) == 0);
    assert(ot_archive_writer_data(&w, (const oa_u8 *)"hello", 5u) == 0);
    assert(ot_archive_writer_file_end(&w) == 0);
    m.file_size = 0ul;
    strcpy(m.path, mode == 8u ? "root/a.bin" : mode == 9u ? "ROOT/A.BIN/SUB" : "ROOT/ZERO.BIN");
    assert(ot_archive_writer_metadata(&w, 3ul, &m) == 0);
    assert(ot_archive_writer_file_end(&w) == 0);
    assert(ot_archive_writer_session_end(&w) == 0);
    assert(f.count == 9u);
    if (mode == 11u) {
        assert(ot_archive_writer_session(&w, 2ul, &start) == 0);
        strcpy(m.path, "SECOND.BIN");
        assert(ot_archive_writer_metadata(&w, 1ul, &m) == 0);
        assert(ot_archive_writer_file_end(&w) == 0);
        assert(ot_archive_writer_session_end(&w) == 0);
    }
}
struct exchange_file { unsigned exists, attributes; char data; };
struct exchange_fixture {
    struct exchange_file file[3];
    unsigned calls, fail, fail_second, removes, race;
};
static unsigned exchange_slot(const char *name)
{
    if (strcmp(name, "stage") == 0) return 0u;
    if (strcmp(name, "dest") == 0) return 1u;
    assert(strcmp(name, "old") == 0); return 2u;
}
static unsigned exchange_attr(void *p, const char *name, unsigned attributes)
{
    struct exchange_fixture *x = p;
    unsigned slot = exchange_slot(name);
    ++x->calls;
    if (x->calls == x->fail || x->calls == x->fail_second) return 5u;
    assert(x->file[slot].exists); x->file[slot].attributes = attributes; return 0u;
}
static unsigned exchange_rename(void *p, const char *from, const char *to)
{
    struct exchange_fixture *x = p;
    unsigned a = exchange_slot(from), b = exchange_slot(to);
    ++x->calls;
    if (x->calls == x->fail || x->calls == x->fail_second) return 5u;
    if (x->race && x->calls == 3u) { x->file[1].exists = 1u; x->file[1].data = 'x'; }
    if (!x->file[a].exists) return 2u;
    if (x->file[b].exists) return 80u;
    x->file[b] = x->file[a]; x->file[a].exists = 0u; return 0u;
}
static unsigned exchange_remove(void *p, const char *name)
{
    struct exchange_fixture *x = p;
    unsigned slot = exchange_slot(name);
    ++x->calls; ++x->removes;
    assert(slot == 2u && x->file[1].exists && x->file[1].data == 'n' &&
           x->file[1].attributes == 0x20u && x->file[2].data == 'o');
    if (x->calls == x->fail) return 5u;
    x->file[slot].exists = 0u; return 0u;
}
static void test_exchange(void)
{
    struct exchange_fixture x;
    struct ot_restore_exchange_ops ops;
    unsigned test, error, recovery;
    for (test = 0u; test < 9u; ++test) {
        memset(&x, 0, sizeof(x));
        x.file[0].exists = 1u; x.file[0].data = 'n';
        x.file[1].exists = 1u; x.file[1].data = 'o'; x.file[1].attributes = 1u;
        x.fail = test <= 6u ? test : test == 7u ? 3u : 0u;
        x.fail_second = test == 7u ? 4u : 0u; x.race = test == 8u;
        ops.context = &x; ops.attributes = exchange_attr; ops.rename = exchange_rename; ops.remove = exchange_remove;
        error = ot_restore_exchange(&ops, "stage", "dest", "old", 1u, 1u, 0x20u, &recovery);
        if (test == 0u) {
            assert(error == 0u && !x.file[0].exists && !x.file[2].exists && x.file[1].data == 'n' && x.removes == 1u);
        } else {
            assert(error != 0u);
            assert((x.file[1].exists && x.file[1].data == 'o') || (x.file[2].exists && x.file[2].data == 'o'));
            assert(x.removes == (test == 6u ? 1u : 0u));
            if (test == 3u) assert(x.file[1].data == 'o' && x.file[1].attributes == 1u && recovery == 0u);
            if (test == 7u || test == 8u) assert(recovery != 0u);
            if (test == 8u) assert(x.file[1].data == 'x');
        }
    }
}
static void corrupt_file_crc(void)
{
    struct ot_block_header header;
    struct ot_record_header record;
    assert(ot_block_decode(&header, f.block[5], (oa_u16)f.sizes[5]) == 0);
    ot_record_header_decode(&record, f.block[5] + OT_BLOCK_HEADER_SIZE);
    f.block[5][OT_BLOCK_HEADER_SIZE + OT_RECORD_HEADER_SIZE + 8u] ^= 1u;
    record.data_crc32 = ot_crc32(f.block[5] + OT_BLOCK_HEADER_SIZE + OT_RECORD_HEADER_SIZE, (oa_u16)record.data_size);
    ot_record_header_encode(f.block[5] + OT_BLOCK_HEADER_SIZE, &record);
    assert(ot_block_encode(f.block[5], 512u, &header, f.block[5] + OT_BLOCK_HEADER_SIZE,
                            (oa_u16)(f.sizes[5] - OT_BLOCK_HEADER_SIZE)) == 0);
}
static void test_filter(void)
{
    struct ot_restore s;
    struct ot_restore_ops ops;
    struct ot_restore_selection list;
    const char *paths[2];
    char path[32];
    unsigned test, i, count;
    int rc;
    ops.context = &f; ops.directory = directory; ops.begin = begin;
    ops.write = write_data; ops.commit = commit; ops.abort = abort_file;
    for (test = 0u; test < 12u; ++test) {
        fixture(test == 6u ? 8u : test == 7u ? 7u : test == 8u ? 15u : test == 10u ? 11u : 0u);
        memset(&s, 0, sizeof(s));
        assert(ot_restore_init(&s, &ops) == 0);
        count = 1u; paths[0] = "ROOT/ZERO.BIN"; paths[1] = "ROOT/A.BIN";
        if (test == 1u) paths[0] = "ROOT/A.BIN";
        if (test == 2u) count = 2u;
        if (test == 3u) paths[0] = "ABSENT.BIN";
        if (test == 4u) paths[0] = "ROOT";
        if (test == 5u) corrupt_file_crc(); /* Excluded nonempty file is still verified. */
        if (test == 10u) paths[0] = "SECOND.BIN";
        if (test == 11u) paths[0] = "root/zero.bin";
        assert(ot_restore_set_filter(&s, paths, count) == 0);
        assert(ot_restore_set_filter(&s, paths, count) == OT_RESTORE_PATH);
        rc = 0;
        for (i = 0u; i < f.count - (test == 9u ? 1u : 0u); ++i) {
            rc = ot_restore_block(&s, f.block[i], (oa_u16)f.sizes[i]);
            if (rc != 0) break;
        }
        assert(f.dirs == 0u && s.restored_directories == 0ul);
        if (test >= 3u && test <= 7u) {
            if (test == 3u || test == 4u) assert(rc == 0 && ot_restore_finish(&s) == OT_RESTORE_PATH);
            else assert(rc == (test == 5u ? OT_VERIFY_FILE_CRC : OT_RESTORE_PATH));
            assert(f.begins == 0u && f.writes == 0u && f.commits == 0u);
        } else if (test == 9u) assert(rc == 0 && ot_restore_finish(&s) == OT_VERIFY_INCOMPLETE);
        else {
            assert(rc == 0 && ot_restore_finish(&s) == 0);
            assert(f.begins == (test == 2u ? 2u : 1u));
            assert(f.commits == (test == 2u ? 2u : test == 8u ? 0u : 1u));
            assert(f.writes == (test == 1u || test == 2u ? 1u : 0u));
            assert(s.excluded_files == (test == 2u ? 0ul : test == 10u ? 2ul : 1ul));
            assert(s.skipped_files == (test == 8u ? 1ul : 0ul));
        }
        ot_restore_free(&s); assert(f.aborts == 0u);
    }
    fixture(0u); memset(&s, 0, sizeof(s)); memset(&list, 0, sizeof(list));
    assert(ot_restore_init(&s, &ops) == 0);
    assert(ot_restore_set_filter(&s, 0, 0u) == OT_RESTORE_PATH);
    paths[0] = "../BAD";
    assert(ot_restore_set_filter(&s, paths, 1u) == OT_RESTORE_PATH);
    for (i = 0u; i < OT_RESTORE_SELECTION_MAX - 1u; ++i) {
        sprintf(path, "F%03u.BIN", i); assert(ot_restore_selection_add(&list, path) == 0);
    }
    assert(ot_restore_selection_add(&list, "ROOT/ZERO.BIN") == 0);
    assert(ot_restore_set_filter(&s, list.paths, list.count) == 0);
    for (i = 0u; i < f.count; ++i) assert(ot_restore_block(&s, f.block[i], (oa_u16)f.sizes[i]) == 0);
    assert(s.filter_seen[255] == 1u && f.commits == 1u);
    assert(ot_restore_finish(&s) == OT_RESTORE_PATH);
    ot_restore_free(&s); ot_restore_selection_free(&list);
    fixture(0u); memset(&s, 0, sizeof(s));
    assert(ot_restore_init(&s, &ops) == 0);
    assert(ot_restore_block(&s, f.block[0], (oa_u16)f.sizes[0]) == 0);
    paths[0] = "ROOT/ZERO.BIN";
    assert(ot_restore_set_filter(&s, paths, 1u) == OT_RESTORE_PATH);
    ot_restore_free(&s);
    puts("OpenTape selective restore tests passed");
}
struct observations { unsigned calls; oa_u32 bytes; };
static void observe_restore(void *context, const struct ot_restore *state)
{
    struct observations *seen = context;
    struct ot_restore *mutable_state = (struct ot_restore *)state;
    assert(state->busy && !state->finished && !state->failed);
    assert(state->restored_files == (oa_u32)f.commits);
    assert(state->active_bytes <= state->active.file_size);
    assert(state->checked_high == 0ul && state->checked_low >= seen->bytes);
    seen->bytes = state->checked_low; ++seen->calls;
    assert(ot_restore_finish(mutable_state) == OT_RESTORE_FAILED);
    assert(ot_restore_block(mutable_state, f.block[0], (oa_u16)f.sizes[0]) == OT_RESTORE_FAILED);
    assert(ot_restore_observe(mutable_state, 0, 0) == OT_RESTORE_FAILED);
    ot_restore_free(mutable_state); /* Must not free/abort while observed. */
}
static void test_combined_observation(void)
{
    struct ot_restore state;
    struct ot_restore_ops ops;
    struct observations seen;
    struct ot_block_header header;
    oa_u8 combined[512], payload[512];
    unsigned i, used = 0u;
    fixture(5u); memset(&state, 0, sizeof(state)); memset(&seen, 0, sizeof(seen));
    ops.context = &f; ops.directory = directory; ops.begin = begin; ops.write = write_data; ops.commit = commit; ops.abort = abort_file;
    assert(ot_restore_init(&state, &ops) == 0);
    assert(ot_restore_observe(&state, observe_restore, &seen) == 0);
    for (i = 0u; i < 3u; ++i) assert(ot_restore_block(&state, f.block[i], (oa_u16)f.sizes[i]) == 0);
    for (i = 3u; i < 6u; ++i) {
        memcpy(payload + used, f.block[i] + OT_BLOCK_HEADER_SIZE, f.sizes[i] - OT_BLOCK_HEADER_SIZE);
        used += f.sizes[i] - OT_BLOCK_HEADER_SIZE;
    }
    memset(&header, 0, sizeof(header)); header.block_number = 3ul; header.session_id = 1ul; header.record_count = 3u;
    assert(ot_block_encode(combined, sizeof(combined), &header, payload, (oa_u16)used) == 0);
    assert(ot_restore_block(&state, combined, (oa_u16)(OT_BLOCK_HEADER_SIZE + used)) == OT_RESTORE_IO);
    assert(seen.calls == 3u && state.checked_low == 0ul && f.writes == 1u && !f.commits);
    ot_restore_free(&state); assert(f.aborts == 1u);
}
int main(void)
{
    struct ot_restore s;
    struct ot_restore_ops ops;
    unsigned test, i;
    struct observations seen;
    int rc;
    assert(ot_restore_observe(0, 0, 0) == OT_FORMAT_BAD_ARGUMENT);
    test_combined_observation();
    for (test = 0u; test < 17u; ++test) {
        fixture(test); memset(&s, 0, sizeof(s)); memset(&seen, 0, sizeof(seen));
        ops.context = &f; ops.directory = directory; ops.begin = begin; ops.write = write_data;
        ops.commit = commit; ops.abort = abort_file;
        assert(ot_restore_init(&s, &ops) == 0);
        assert(ot_restore_observe(&s, observe_restore, &seen) == 0);
        if (test == 1u) f.block[4][60] ^= 1u;
        if (test == 2u || test == 16u) {
            /* Valid record/block checksums, deliberately wrong whole-file CRC. */
            corrupt_file_crc();
        }
        rc = 0;
        for (i = 0u; i < f.count - (test == 10u ? 1u : 0u); ++i) {
            rc = ot_restore_block(&s, f.block[i], (oa_u16)f.sizes[i]);
            if (rc != 0) break;
        }
        assert(seen.calls == i);
        assert(ot_restore_observe(&s, 0, 0) != 0);
        if (test == 0u || test == 11u || test == 14u || test == 15u) {
            assert(s.checked_low == 5ul && s.checked_high == 0ul);
            assert(rc == 0 && f.commits == (test == 11u ? 3u : test == 14u ? 1u : test == 15u ? 0u : 2u) && f.dirs == 1u);
            assert(ot_restore_finish(&s) == 0 && s.finished);
            assert(s.restored_files == f.commits && s.restored_directories == 1ul);
            if (test == 14u || test == 15u) assert(f.writes == 0u && s.skipped_files == (test == 14u ? 1ul : 2ul));
            assert(ot_restore_block(&s, f.block[0], (oa_u16)f.sizes[0]) == OT_RESTORE_FAILED);
        } else {
            if (test == 10u) assert(rc == 0 && ot_restore_finish(&s) == OT_VERIFY_INCOMPLETE);
            else assert(rc != 0);
            assert(ot_restore_finish(&s) == OT_RESTORE_FAILED);
            if (test < 8u || test >= 12u) assert(f.commits == 0u);
            if (test == 1u) assert(f.writes == 0u);
            if (test == 2u || test == 16u) assert(rc == OT_VERIFY_FILE_CRC);
            if (test == 7u || test == 8u || test == 9u) assert(rc == OT_RESTORE_PATH);
        }
        ot_restore_free(&s);
        assert(f.aborts == ((test >= 1u && test <= 5u) || test == 12u || test == 13u ? 1u : 0u));
        assert(ot_restore_block(&s, f.block[0], (oa_u16)f.sizes[0]) == OT_RESTORE_FAILED);
        ot_restore_free(&s); /* Cleanup must not double-close a staged handle. */
    }
    {
        const char *paths[2];
        paths[0] = "ROOT/A.BIN"; paths[1] = "ROOT/ZERO.BIN";
        assert(ot_restore_selection_valid(paths, 2u));
        assert(ot_restore_selected(paths, 2u, "root/a.bin") == 1u);
        assert(ot_restore_selected(paths, 2u, "ROOT/OTHER") == 0u);
        paths[1] = "root/a.bin"; assert(!ot_restore_selection_valid(paths, 2u));
        paths[0] = "ROOT/*.BIN"; assert(!ot_restore_selection_valid(paths, 1u));
        paths[0] = "../BAD"; assert(!ot_restore_selection_valid(paths, 1u));
        paths[0] = OT_RESTORE_OLD; assert(!ot_restore_selection_valid(paths, 1u));
        assert(!ot_restore_selection_valid(paths, OT_RESTORE_SELECTION_MAX + 1u));
    }
    test_exchange();
    test_filter();
    puts("OpenTape restore tests passed");
    return 0;
}

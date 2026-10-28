#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/verify_stream.h"
#include "opentape/archive_writer.h"
#include "opentape/archive_catalog.h"
#include "opentape/restore.h"
#include "opentape/verify_progress.h"
struct fixture {
    oa_u8 blocks[80][160];
    unsigned sizes[80], count, position, reads, io_error, status_error, visited, visitor_error;
    unsigned opens, closes, open_error, close_error, ordinary, raw_error, initial_status_error, fixed;
};
static struct fixture f;
static int capture(void *p, const oa_u8 *b, unsigned n, unsigned *accepted)
{
    struct fixture *t = p;
    assert(t->count < 80u && n <= 160u);
    memcpy(t->blocks[t->count], b, n); t->sizes[t->count++] = n;
    *accepted = n; return 0;
}
static int finish(void *p) { (void)p; return 0; }
static unsigned read_block(void *p, unsigned h, void *b, unsigned n, unsigned *got)
{
    struct fixture *t = p;
    (void)h;
    ++t->reads;
    if (t->io_error && t->reads == 20u) return 5u;
    if (t->position == t->count) { *got = 0u; return 0u; }
    *got = t->sizes[t->position];
    if (*got <= n) memcpy(b, t->blocks[t->position], *got);
    ++t->position;
    return 0u;
}
static unsigned status(void *p, unsigned h, unsigned fn, void *b, unsigned n)
{
    struct fixture *t = p;
    struct oa_aspitape_mtget16 s;
    (void)h;
    assert(fn == OA_ASPITAPE_IOCTL_MTGET16 && n == sizeof(s));
    if (t->status_error && t->reads == 20u) return 5u;
    if (t->initial_status_error && t->reads == 0u) return 5u;
    memset(&s, 0, sizeof(s)); s.current_state = OA_MT_STATE_UNBUFFERED;
    if (t->fixed) { s.device_parameters = OA_MT_PARAM_FIXED_BLOCK; s.fixed_block_size = 160u; }
    memcpy(b, &s, sizeof(s)); return 0u;
}
static int visit(void *p, oa_u32 session, oa_u32 object, const struct ot_file_metadata *m)
{
    struct fixture *t = p;
    char expected[16];
    sprintf(expected, "F%02u.BIN", t->visited);
    assert(session == 1ul && object == t->visited + 1ul &&
           m->kind == OT_FILE_REGULAR && m->file_size == 0ul && strcmp(m->path, expected) == 0);
    ++t->visited;
    return t->visitor_error && t->visited == 10u ? 5 : 0;
}
static void fixture(void)
{
    struct ot_archive_writer w;
    struct ot_writer_sink sink;
    struct ot_volume_label label;
    struct ot_session_start start;
    struct ot_file_metadata m;
    oa_u8 buffer[160];
    unsigned i;
    memset(&f, 0, sizeof(f)); memset(&w, 0, sizeof(w));
    memset(&label, 0, sizeof(label)); memset(&start, 0, sizeof(start)); memset(&m, 0, sizeof(m));
    label.version = 1u; m.kind = OT_FILE_REGULAR;
    sink.context = &f; sink.write_block = capture; sink.finish_session = finish;
    assert(ot_archive_writer_init(&w, &sink, buffer, sizeof(buffer)) == 0);
    assert(ot_archive_writer_volume(&w, &label) == 0);
    assert(ot_archive_writer_session(&w, 1ul, &start) == 0);
    for (i = 0u; i < 30u; ++i) {
        sprintf(m.path, "F%02u.BIN", i);
        assert(ot_archive_writer_metadata(&w, i + 1ul, &m) == 0);
        assert(ot_archive_writer_file_end(&w) == 0);
    }
    assert(ot_archive_writer_session_end(&w) == 0);
    assert(f.count == 63u);
}

static unsigned open_readonly(void *p, const char *name, unsigned *handle)
{
    struct fixture *t = p;
    assert(strcmp(name, "NRMT0") == 0);
    ++t->opens; *handle = 3u; return t->open_error;
}
static unsigned device_info(void *p, unsigned h, unsigned *flags)
{
    struct fixture *t = p;
    assert(h == 3u); *flags = t->ordinary ? 0u : 0x80u; return 0u;
}
static unsigned set_raw(void *p, unsigned h, unsigned flags)
{
    struct fixture *t = p;
    assert(h == 3u && flags == 0xa0u); return t->raw_error;
}
static unsigned close_handle(void *p, unsigned h)
{
    struct fixture *t = p;
    assert(h == 3u); ++t->closes; return t->close_error;
}
struct observation {
    struct ot_verify_session_result *result;
    struct ot_verify_progress progress;
    unsigned calls, visiting;
};
static void observed(void *context, const struct ot_archive_verify *state, const oa_u8 *block, unsigned size)
{
    struct observation *watch = context;
    struct ot_tape_reader *reader = &watch->result->reader;
    struct ot_archive_verify before = *state, local;
    oa_u8 scratch[160];
    unsigned got = 99u, reads = f.reads, closes = f.closes;
    assert(reader->opened && reader->observing && !reader->failed);
    assert(state->framing.blocks == ++watch->calls);
    if (watch->visiting) assert(f.visited == state->last_object);
    assert(ot_verify_progress_block(&watch->progress, state, block, size) == 0);
    assert(ot_reader_read(reader, scratch, sizeof(scratch), &got) == OT_READ_BAD_ARGUMENT && got == 0u);
    assert(ot_reader_close(reader) == OT_READ_BAD_ARGUMENT && reader->opened);
    memset(&local, 0, sizeof(local));
    assert(ot_verify_stream(reader, &local, scratch, sizeof(scratch), 1ul) == OT_READ_BAD_ARGUMENT);
    assert(ot_verify_session_opened(scratch, sizeof(scratch), 0, 0, watch->result) == OT_READ_BAD_ARGUMENT);
    assert(f.reads == reads && f.closes == closes && memcmp(state, &before, sizeof(before)) == 0);
}
static void session_tests(void)
{
    struct ot_reader_ops ops;
    struct ot_verify_session_result result;
    oa_u8 buffer[160];
    struct observation watch;
    unsigned test;
    int rc;
    for (test = 0u; test < 22u; ++test) {
        fixture(); memset(&ops, 0, sizeof(ops));
        memset(&watch, 0, sizeof(watch)); watch.result = &result; watch.visiting = test != 1u;
        ops.probe.context = &f; ops.probe.open_readonly = open_readonly;
        ops.probe.device_info = device_info; ops.probe.status = status;
        ops.probe.close = close_handle; ops.set_raw = set_raw; ops.read = read_block;
        if (test == 2u) f.open_error = 5u;
        if (test == 3u) f.ordinary = 1u;
        if (test == 4u) f.raw_error = 5u;
        if (test == 5u) f.initial_status_error = 1u;
        if (test == 6u) f.fixed = 1u;
        if (test == 7u || test == 14u) f.io_error = 1u;
        if (test == 8u) f.status_error = 1u;
        if (test == 9u || test == 13u) f.blocks[20][159u % f.sizes[20]] ^= 1u;
        if (test == 10u) f.count = 62u;
        if (test == 11u) f.visitor_error = 1u;
        if (test == 12u) f.close_error = 5u;
        if (test == 13u || test == 14u) f.close_error = 6u;
        if (test == 21u) f.count = 0u;
        rc = ot_verify_session_observed(test == 18u ? 0 : &ops,
            test == 15u ? "NRT0$" : test == 20u ? 0 : "NRMT0",
            test == 16u ? 0 : buffer, test == 17u ? 1u : sizeof(buffer),
            test == 1u ? 0 : visit, &f, observed, &watch, test == 19u ? 0 : &result);
        if (test >= 15u && test <= 20u) {
            assert(rc == OT_READ_BAD_ARGUMENT && f.opens == 0u && f.closes == 0u && f.reads == 0u);
            continue;
        }
        assert(!result.reader.observing);
        assert(watch.calls == (test >= 2u && test <= 6u ? 0u :
            test == 7u || test == 8u || test == 14u ? 19u :
            test == 9u || test == 11u || test == 13u ? 20u :
            test == 10u ? 62u : test == 21u ? 0u : 63u));
        assert(f.opens == 1u && !result.reader.opened);
        assert(f.closes == (test == 2u ? 0u : 1u));
        if (test == 0u || test == 1u) {
            assert(rc == 0 && !result.transport_error && f.reads == 64u);
            assert(result.verify.sessions == 1ul && result.verify.files == 30ul);
            assert(f.visited == (test == 1u ? 0u : 30u));
        } else {
            assert(rc != 0);
            if (test <= 6u) assert(f.reads == 0u);
            if (test == 2u) assert(rc == OT_PROBE_OPEN_FAILED);
            if (test == 3u) assert(rc == OT_PROBE_NOT_DEVICE);
            if (test == 4u || test == 5u) assert(rc == OT_PROBE_IOCTL_FAILED);
            if (test == 6u) assert(rc == OT_READ_BAD_MODE);
            if (test == 7u || test == 8u || test == 14u)
                assert(rc == OT_READ_IO_ERROR && result.transport_error && result.reader.dos_error == 5u);
            if (test == 9u || test == 13u)
                assert(!result.transport_error && result.reader.dos_error == 0u && f.reads == 21u);
            if (test == 10u || test == 21u) assert(rc == OT_VERIFY_INCOMPLETE);
            if (test == 11u) assert(rc == OT_VERIFY_VISITOR_FAILED && f.visited == 10u && f.reads == 21u);
            if (test == 12u) assert(rc == OT_PROBE_CLOSE_FAILED && result.transport_error && result.reader.dos_error == 5u);
        }
    }
    puts("OpenTape verification session lifecycle tests passed: 22 cases");
}

static void catalog_export_tests(void)
{
    struct ot_catalog catalog;
    struct ot_restore_selection list;
    struct ot_file_metadata metadata;
    unsigned i;
    memset(&catalog, 0, sizeof(catalog)); memset(&list, 0, sizeof(list));
    memset(&metadata, 0, sizeof(metadata)); metadata.kind = OT_FILE_REGULAR;
    strcpy(metadata.path, "FILE.BIN");
    assert(ot_catalog_selection(0, &list) == OT_CATALOG_ARGUMENT);
    assert(ot_catalog_selection(&catalog, 0) == OT_CATALOG_ARGUMENT);
    assert(ot_catalog_selection(&catalog, &list) == OT_CATALOG_ARGUMENT && list.count == 0u);
    for (i = 0u; i < OT_CATALOG_MAX_ENTRIES; ++i) {
        sprintf(metadata.path, "F%03u.BIN", i);
        assert(ot_catalog_capture(&catalog, 1ul, i + 1ul, &metadata) == 0);
    }
    assert(ot_catalog_mark_all(&catalog, 1u) == 0);
    assert(ot_catalog_selection(&catalog, &list) == 0 && list.count == 256u);
    assert(strcmp(list.paths[255], "F255.BIN") == 0 && catalog.marked == 256u);
    assert(ot_catalog_selection(&catalog, &list) == OT_CATALOG_ARGUMENT && list.count == 256u);
    ot_catalog_free(&catalog);
    assert(strcmp(list.paths[0], "F000.BIN") == 0); /* Independent ownership. */
    ot_restore_selection_free(&list);
    for (i = 0u; i < 4u; ++i) {
        strcpy(metadata.path, "FIRST.BIN"); metadata.kind = OT_FILE_REGULAR;
        assert(ot_catalog_capture(&catalog, 1ul, 1ul, &metadata) == 0);
        strcpy(metadata.path, "SECOND.BIN");
        assert(ot_catalog_capture(&catalog, 1ul, 2ul, &metadata) == 0);
        assert(ot_catalog_mark_all(&catalog, 1u) == 0);
        if (i < 2u) {
            strcpy(metadata.path, i == 0u ? "SECOND.BIN" : "second.bin");
            assert(ot_catalog_capture(&catalog, 2ul, 1ul, &metadata) == 0);
            assert(ot_catalog_selection(&catalog, &list) == OT_CATALOG_AMBIGUOUS && list.count == 0u);
        } else if (i == 2u) {
            strcpy(metadata.path, "second.bin"); metadata.kind = OT_FILE_DIRECTORY;
            assert(ot_catalog_capture(&catalog, 2ul, 1ul, &metadata) == 0);
            assert(ot_catalog_selection(&catalog, &list) == OT_CATALOG_AMBIGUOUS && list.count == 0u);
        } else {
            catalog.error = OT_CATALOG_LIMIT;
            assert(ot_catalog_selection(&catalog, &list) == OT_CATALOG_ARGUMENT && list.count == 0u);
        }
        assert(catalog.marked == 2u); /* Failed export never loses marks. */
        ot_catalog_free(&catalog);
    }
    puts("OpenTape catalogue selection conversion tests passed");
}

/* Permissions are a separate, owned, DOS-case-insensitive subset snapshot.
 * Navigation/mark-all must never manufacture replacement rights. */
static void catalog_replacement_tests(void)
{
    struct ot_catalog catalog;
    struct ot_restore_selection selected, replacement;
    struct ot_file_metadata metadata;
    unsigned i;
    memset(&catalog, 0, sizeof(catalog)); memset(&metadata, 0, sizeof(metadata));
    memset(&selected, 0, sizeof(selected)); memset(&replacement, 0, sizeof(replacement));
    ot_catalog_clear_replacements(0);
    assert(ot_catalog_replace(0, 0u, 1u) == OT_CATALOG_ARGUMENT);
    assert(ot_catalog_replacements(0, &replacement) == OT_CATALOG_ARGUMENT);
    assert(ot_catalog_replacements(&catalog, 0) == OT_CATALOG_ARGUMENT);
    assert(ot_catalog_replacements(&catalog, &replacement) == 0 && replacement.count == 0u);
    metadata.kind = OT_FILE_DIRECTORY; strcpy(metadata.path, "ROOT");
    assert(ot_catalog_capture(&catalog, 1ul, 1ul, &metadata) == 0);
    assert(ot_catalog_replace(&catalog, 0u, 1u) == OT_CATALOG_ARGUMENT);
    metadata.kind = OT_FILE_REGULAR; strcpy(metadata.path, "ROOT/A.BIN");
    assert(ot_catalog_capture(&catalog, 1ul, 2ul, &metadata) == 0);
    strcpy(metadata.path, "ROOT/B.BIN");
    assert(ot_catalog_capture(&catalog, 1ul, 3ul, &metadata) == 0);
    assert(ot_catalog_replace(&catalog, 1u, 1u) == OT_CATALOG_ARGUMENT);
    assert(ot_catalog_mark_all(&catalog, 1u) == 0 && catalog.marked == 2u && catalog.replacements == 0u);
    assert(ot_catalog_replace(&catalog, 1u, 1u) == 0 && catalog.replacements == 1u);
    assert(ot_catalog_replace(&catalog, 1u, 1u) == 0 && catalog.replacements == 1u);
    assert(ot_catalog_replace(&catalog, 3u, 1u) == OT_CATALOG_ARGUMENT);
    assert(ot_catalog_replace(&catalog, 1u, 2u) == OT_CATALOG_ARGUMENT);
    assert(ot_catalog_mark_all(&catalog, 1u) == 0 && catalog.replacements == 1u);
    assert(ot_catalog_selection(&catalog, &selected) == 0 && selected.count == 2u);
    assert(ot_catalog_replacements(&catalog, &replacement) == 0 && replacement.count == 1u);
    assert(strcmp(replacement.paths[0], "ROOT/A.BIN") == 0);
    assert(ot_catalog_replacements(&catalog, &replacement) == OT_CATALOG_ARGUMENT && replacement.count == 1u);
    assert(ot_catalog_mark(&catalog, 1u, 0u) == 0 && catalog.replacements == 0u);
    assert(ot_catalog_mark(&catalog, 1u, 1u) == 0 && catalog.replacements == 0u);
    assert(ot_catalog_replace(&catalog, 2u, 1u) == 0);
    ot_catalog_clear_replacements(&catalog);
    assert(catalog.marked == 2u && catalog.replacements == 0u);
    assert(ot_catalog_replace(&catalog, 2u, 1u) == 0);
    assert(ot_catalog_mark_all(&catalog, 0u) == 0 && catalog.replacements == 0u && catalog.marked == 0u);
    ot_catalog_free(&catalog);
    assert(strcmp(replacement.paths[0], selected.paths[0]) == 0); /* Independent snapshots. */
    ot_restore_selection_free(&selected); ot_restore_selection_free(&replacement);
    for (i = 0u; i < OT_CATALOG_MAX_ENTRIES; ++i) {
        sprintf(metadata.path, "F%03u.BIN", i);
        assert(ot_catalog_capture(&catalog, 1ul, i + 1ul, &metadata) == 0);
    }
    assert(ot_catalog_mark_all(&catalog, 1u) == 0);
    for (i = 0u; i < OT_CATALOG_MAX_ENTRIES; ++i) assert(ot_catalog_replace(&catalog, i, 1u) == 0);
    assert(ot_catalog_replacements(&catalog, &replacement) == 0 && replacement.count == 256u);
    ot_catalog_free(&catalog);
    assert(strcmp(replacement.paths[255], "F255.BIN") == 0);
    ot_restore_selection_free(&replacement);
    /* Roll back an earlier copied path when a later grant is version-ambiguous. */
    strcpy(metadata.path, "FIRST.BIN"); assert(ot_catalog_capture(&catalog, 1ul, 1ul, &metadata) == 0);
    strcpy(metadata.path, "LAST.BIN"); assert(ot_catalog_capture(&catalog, 1ul, 2ul, &metadata) == 0);
    assert(ot_catalog_mark_all(&catalog, 1u) == 0);
    assert(ot_catalog_replace(&catalog, 0u, 1u) == 0 && ot_catalog_replace(&catalog, 1u, 1u) == 0);
    strcpy(metadata.path, "last.bin"); assert(ot_catalog_capture(&catalog, 2ul, 1ul, &metadata) == 0);
    assert(ot_catalog_replacements(&catalog, &replacement) == OT_CATALOG_AMBIGUOUS && replacement.count == 0u);
    assert(catalog.marked == 2u && catalog.replacements == 2u);
    catalog.error = OT_CATALOG_LIMIT;
    assert(ot_catalog_replace(&catalog, 0u, 0u) == OT_CATALOG_ARGUMENT && catalog.replacements == 2u);
    assert(ot_catalog_replacements(&catalog, &replacement) == OT_CATALOG_ARGUMENT);
    ot_catalog_clear_replacements(&catalog);
    assert(catalog.marked == 2u && catalog.replacements == 0u && catalog.error == OT_CATALOG_LIMIT);
    ot_catalog_free(&catalog);
    puts("OpenTape catalogue replacement-intent tests passed");
}

/* Import is an all-or-nothing replacement of annotations, not a replacement
 * grant or a directory/path expansion. Every failure preserves prior intent. */
static void catalog_import_tests(void)
{
    struct ot_catalog catalog;
    struct ot_restore_selection list;
    struct ot_file_metadata metadata;
    static char selected[OT_CATALOG_MAX_ENTRIES][13];
    unsigned i;
    memset(&catalog, 0, sizeof(catalog)); memset(&list, 0, sizeof(list));
    memset(&metadata, 0, sizeof(metadata));
    assert(ot_catalog_apply_selection(0, &list) == OT_CATALOG_ARGUMENT);
    assert(ot_catalog_apply_selection(&catalog, 0) == OT_CATALOG_ARGUMENT);
    assert(ot_catalog_apply_selection(&catalog, &list) == OT_CATALOG_ARGUMENT);
    metadata.kind = OT_FILE_DIRECTORY; strcpy(metadata.path, "ROOT");
    assert(ot_catalog_capture(&catalog, 1ul, 1ul, &metadata) == 0);
    metadata.kind = OT_FILE_REGULAR; strcpy(metadata.path, "ROOT/A.BIN");
    assert(ot_catalog_capture(&catalog, 1ul, 2ul, &metadata) == 0);
    strcpy(metadata.path, "ROOT/B.BIN");
    assert(ot_catalog_capture(&catalog, 1ul, 3ul, &metadata) == 0);
    assert(ot_catalog_mark(&catalog, 1u, 1u) == 0 && ot_catalog_replace(&catalog, 1u, 1u) == 0);
    list.paths[0] = "ROOT/B.BIN"; list.paths[1] = "ROOT/ABSENT.BIN"; list.count = 2u;
    assert(ot_catalog_apply_selection(&catalog, &list) == OT_CATALOG_MISSING);
    assert(ot_catalog_at(&catalog, 1u)->marked && ot_catalog_at(&catalog, 1u)->replace);
    assert(!ot_catalog_at(&catalog, 2u)->marked && catalog.marked == 1u && catalog.replacements == 1u);
    list.count = 1u; list.paths[0] = "ROOT";
    assert(ot_catalog_apply_selection(&catalog, &list) == OT_CATALOG_ARGUMENT);
    list.count = 2u; list.paths[0] = "ROOT/A.BIN"; list.paths[1] = "root/a.bin";
    assert(ot_catalog_apply_selection(&catalog, &list) == OT_CATALOG_ARGUMENT);
    list.count = 0u;
    assert(ot_catalog_apply_selection(&catalog, &list) == OT_CATALOG_ARGUMENT);
    list.count = 1u; list.paths[0] = "root/b.bin";
    catalog.error = OT_CATALOG_LIMIT;
    assert(ot_catalog_apply_selection(&catalog, &list) == OT_CATALOG_ARGUMENT && catalog.replacements == 1u);
    catalog.error = 0;
    assert(ot_catalog_apply_selection(&catalog, &list) == 0 && catalog.marked == 1u && catalog.replacements == 0u);
    assert(!ot_catalog_at(&catalog, 1u)->marked && ot_catalog_at(&catalog, 2u)->marked);
    assert(ot_catalog_replace(&catalog, 2u, 1u) == 0);
    assert(ot_catalog_apply_selection(&catalog, &list) == 0 && catalog.replacements == 0u);
    assert(ot_catalog_replace(&catalog, 2u, 1u) == 0);
    strcpy(metadata.path, "root/b.bin");
    assert(ot_catalog_capture(&catalog, 2ul, 1ul, &metadata) == 0);
    list.count = 2u; list.paths[0] = "ROOT/A.BIN"; list.paths[1] = "ROOT/B.BIN";
    assert(ot_catalog_apply_selection(&catalog, &list) == OT_CATALOG_AMBIGUOUS);
    assert(catalog.marked == 1u && catalog.replacements == 1u && !ot_catalog_at(&catalog, 1u)->marked);
    ot_catalog_free(&catalog);
    /* A directory/file collision is ambiguous in either catalogue order. */
    for (i = 0u; i < 2u; ++i) {
        metadata.kind = i == 0u ? OT_FILE_DIRECTORY : OT_FILE_REGULAR;
        strcpy(metadata.path, "SAME");
        assert(ot_catalog_capture(&catalog, 1ul, 1ul, &metadata) == 0);
        metadata.kind = i == 0u ? OT_FILE_REGULAR : OT_FILE_DIRECTORY;
        strcpy(metadata.path, "same");
        assert(ot_catalog_capture(&catalog, 2ul, 1ul, &metadata) == 0);
        assert(ot_catalog_mark(&catalog, i == 0u ? 1u : 0u, 1u) == 0);
        assert(ot_catalog_replace(&catalog, i == 0u ? 1u : 0u, 1u) == 0);
        list.count = 1u; list.paths[0] = "SAME";
        assert(ot_catalog_apply_selection(&catalog, &list) == OT_CATALOG_AMBIGUOUS);
        assert(catalog.marked == 1u && catalog.replacements == 1u);
        ot_catalog_free(&catalog);
    }
    metadata.kind = OT_FILE_REGULAR;
    /* All 256 entries may be imported; input storage can change afterwards. */
    memset(&list, 0, sizeof(list));
    for (i = 0u; i < OT_CATALOG_MAX_ENTRIES; ++i) {
        sprintf(selected[i], "F%03u.BIN", i);
        strcpy(metadata.path, selected[i]);
        assert(ot_catalog_capture(&catalog, 1ul, i + 1ul, &metadata) == 0);
        list.paths[i] = selected[i];
    }
    list.count = OT_CATALOG_MAX_ENTRIES;
    assert(ot_catalog_apply_selection(&catalog, &list) == 0 && catalog.marked == 256u);
    memset(selected, 'X', sizeof(selected));
    assert(strcmp(ot_catalog_at(&catalog, 255u)->metadata.path, "F255.BIN") == 0);
    ot_catalog_free(&catalog);
    puts("OpenTape atomic catalogue selection import tests passed");
}

static void catalog_tests(void)
{
    struct ot_catalog catalog;
    const struct ot_catalog_entry *entry;
    struct ot_file_metadata metadata;
    struct ot_reader_ops ops;
    struct ot_verify_session_result result;
    oa_u8 buffer[160];
    unsigned i, test;
    int rc;
    memset(&catalog, 0, sizeof(catalog)); memset(&metadata, 0, sizeof(metadata));
    assert(ot_catalog_at(0, 0u) == 0 && ot_catalog_at(&catalog, 0u) == 0);
    assert(ot_catalog_mark(0, 0u, 1u) == OT_CATALOG_ARGUMENT);
    assert(ot_catalog_mark_all(0, 1u) == OT_CATALOG_ARGUMENT);
    assert(ot_catalog_mark_all(&catalog, 1u) == 0 && catalog.marked == 0u);
    ot_catalog_free(0);
    metadata.kind = OT_FILE_DIRECTORY; metadata.dos_attributes = 0x10u;
    strcpy(metadata.path, "ROOT");
    assert(ot_catalog_capture(&catalog, 1ul, 1ul, &metadata) == 0);
    metadata.path[0] = 'X';
    entry = ot_catalog_at(&catalog, 0u);
    assert(entry != 0 && strcmp(entry->metadata.path, "ROOT") == 0 && entry->metadata.kind == OT_FILE_DIRECTORY);
    metadata.kind = OT_FILE_REGULAR; metadata.dos_attributes = 0x20u;
    metadata.file_size = 0xfffffffful;
    for (i = 0u; i < 28u; ++i) memcpy(metadata.path + i * 9u, "ABCDEFGH/", 9u);
    memcpy(metadata.path + 252u, "END", 4u);
    assert(ot_catalog_capture(&catalog, 2ul, 10ul, &metadata) == 0);
    entry = ot_catalog_at(&catalog, 1u);
    assert(entry != 0 && entry->session == 2ul && entry->object == 10ul &&
        entry->metadata.file_size == 0xfffffffful && strlen(entry->metadata.path) == 255u &&
        strcmp(entry->metadata.path, metadata.path) == 0 && ot_catalog_at(&catalog, 2u) == 0);
    assert(!entry->marked && catalog.marked == 0u);
    assert(ot_catalog_mark(&catalog, 0u, 1u) == OT_CATALOG_ARGUMENT && catalog.error == 0);
    assert(ot_catalog_mark(&catalog, 1u, 1u) == 0 && catalog.marked == 1u && entry->marked);
    assert(ot_catalog_mark(&catalog, 1u, 1u) == 0 && catalog.marked == 1u);
    assert(ot_catalog_mark(&catalog, 1u, 2u) == OT_CATALOG_ARGUMENT && catalog.marked == 1u);
    assert(ot_catalog_mark(&catalog, 2u, 0u) == OT_CATALOG_ARGUMENT && catalog.marked == 1u);
    assert(ot_catalog_mark_all(&catalog, 1u) == 0 && catalog.marked == 1u && !ot_catalog_at(&catalog, 0u)->marked);
    assert(ot_catalog_mark_all(&catalog, 2u) == OT_CATALOG_ARGUMENT && catalog.marked == 1u);
    assert(ot_catalog_mark(&catalog, 1u, 0u) == 0 && catalog.marked == 0u && !entry->marked);
    assert(ot_catalog_mark(&catalog, 1u, 0u) == 0 && catalog.marked == 0u);
    assert(ot_catalog_mark_all(&catalog, 0u) == 0 && catalog.marked == 0u);
    ot_catalog_free(&catalog);
    assert(catalog.count == 0u && catalog.marked == 0u && catalog.first == 0 && catalog.last == 0 && catalog.error == 0);
    /* Inspection marks are per session/object, not shared by equal path text. */
    assert(ot_catalog_capture(&catalog, 1ul, 1ul, &metadata) == 0);
    assert(ot_catalog_capture(&catalog, 2ul, 1ul, &metadata) == 0);
    assert(ot_catalog_mark(&catalog, 0u, 1u) == 0 && catalog.marked == 1u);
    assert(ot_catalog_at(&catalog, 0u)->marked && !ot_catalog_at(&catalog, 1u)->marked);
    assert(ot_catalog_mark_all(&catalog, 1u) == 0 && catalog.marked == 2u);
    ot_catalog_free(&catalog);
    for (i = 0u; i < OT_CATALOG_MAX_ENTRIES; ++i) {
        sprintf(metadata.path, "F%03u.BIN", i);
        assert(ot_catalog_capture(&catalog, 1ul, i + 1ul, &metadata) == 0);
    }
    assert(ot_catalog_at(&catalog, 255u)->object == 256ul);
    assert(ot_catalog_mark_all(&catalog, 1u) == 0 && catalog.marked == 256u);
    assert(ot_catalog_mark(&catalog, 255u, 0u) == 0 && catalog.marked == 255u);
    assert(ot_catalog_mark_all(&catalog, 0u) == 0 && catalog.marked == 0u);
    assert(ot_catalog_mark(&catalog, 0u, 1u) == 0 && catalog.marked == 1u);
    assert(ot_catalog_capture(&catalog, 1ul, 257ul, &metadata) == OT_CATALOG_LIMIT);
    assert(ot_catalog_capture(&catalog, 1ul, 258ul, &metadata) == OT_CATALOG_LIMIT && catalog.count == 256u);
    assert(ot_catalog_mark_all(&catalog, 0u) == OT_CATALOG_ARGUMENT && catalog.marked == 1u);
    assert(ot_catalog_mark(&catalog, 0u, 0u) == OT_CATALOG_ARGUMENT && catalog.marked == 1u && catalog.error == OT_CATALOG_LIMIT);
    ot_catalog_free(&catalog);
    for (test = 0u; test < 6u; ++test) {
        strcpy(metadata.path, "FILE.BIN"); metadata.kind = OT_FILE_REGULAR;
        if (test == 3u) strcpy(metadata.path, "../BAD");
        if (test == 4u) memset(metadata.path, 'A', sizeof(metadata.path));
        if (test == 5u) metadata.kind = 99u;
        assert(ot_catalog_capture(&catalog, test == 1u ? 0ul : 1ul, test == 2u ? 0ul : 1ul,
            test == 0u ? 0 : &metadata) == OT_CATALOG_ARGUMENT);
        assert(catalog.count == 0u && ot_catalog_capture(&catalog, 1ul, 1ul, &metadata) == OT_CATALOG_ARGUMENT);
        ot_catalog_free(&catalog);
    }
    assert(ot_catalog_capture(0, 1ul, 1ul, &metadata) == OT_CATALOG_ARGUMENT);
    for (test = 0u; test < 6u; ++test) {
        fixture(); memset(&ops, 0, sizeof(ops));
        ops.probe.context = &f; ops.probe.open_readonly = open_readonly;
        ops.probe.device_info = device_info; ops.probe.status = status;
        ops.probe.close = close_handle; ops.set_raw = set_raw; ops.read = read_block;
        if (test == 1u) f.close_error = 5u;
        if (test == 2u) f.blocks[20][159u % f.sizes[20]] ^= 1u;
        if (test == 3u) f.count = 62u;
        if (test == 4u) {
            strcpy(metadata.path, "FILE.BIN"); metadata.kind = OT_FILE_REGULAR;
            for (i = 0u; i < OT_CATALOG_MAX_ENTRIES; ++i)
                assert(ot_catalog_capture(&catalog, 1ul, i + 1ul, &metadata) == 0);
        }
        if (test == 5u) f.fixed = 1u;
        rc = ot_verify_session(&ops, "NRMT0", buffer, sizeof(buffer), ot_catalog_capture, &catalog, &result);
        assert(f.closes == 1u && !result.reader.opened);
        if (test == 0u) assert(rc == 0 && catalog.count == 30u && strcmp(ot_catalog_at(&catalog, 29u)->metadata.path, "F29.BIN") == 0);
        else {
            assert(rc != 0);
            if (test == 1u) assert(rc == OT_PROBE_CLOSE_FAILED && catalog.count == 30u);
            if (test == 2u) assert(catalog.count == 9u);
            if (test == 3u) assert(rc == OT_VERIFY_INCOMPLETE && catalog.count == 30u);
            if (test == 4u) assert(rc == OT_VERIFY_VISITOR_FAILED && catalog.error == OT_CATALOG_LIMIT && f.reads < f.count);
            if (test == 5u) assert(rc == OT_READ_BAD_MODE && catalog.count == 0u);
        }
        /* Even a complete-looking list is discarded after failed close/finish. */
        ot_catalog_free(&catalog);
        assert(catalog.count == 0u && catalog.first == 0 && catalog.last == 0);
    }
    puts("OpenTape catalogue marking tests passed");
    puts("OpenTape bounded archive catalogue tests passed");
}
int main(void)
{
    struct ot_tape_reader reader;
    struct ot_reader_ops ops;
    struct ot_archive_verify v;
    oa_u8 buffer[160];
    unsigned test;
    int rc;
    for (test = 0u; test < 13u; ++test) {
        fixture(); memset(&reader, 0, sizeof(reader)); memset(&ops, 0, sizeof(ops)); memset(&v, 0, sizeof(v));
        ops.probe.context = &f; ops.probe.status = status; ops.read = read_block;
        reader.ops = &ops; reader.opened = 1u;
        if (test == 2u) f.count = 62u;
        if (test == 3u) f.count = 0u;
        if (test == 4u) f.blocks[20][159u % f.sizes[20]] ^= 1u;
        if (test == 5u) f.io_error = 1u;
        if (test == 6u) f.status_error = 1u;
        if (test == 7u) reader.status.tape.device_parameters = OA_MT_PARAM_FIXED_BLOCK;
        if (test == 10u) f.visitor_error = 1u;
        if (test == 11u) f.blocks[20][159u % f.sizes[20]] ^= 1u;
        if (test == 12u) f.count = 62u;
        if (test >= 9u) rc = ot_verify_stream_visit(&reader, &v, buffer, sizeof(buffer), 0ul, visit, &f);
        else rc = ot_verify_stream(&reader, &v, buffer, sizeof(buffer),
                               test == 1u ? 16ul : test == 8u ? 63ul : 0ul);
        if (test == 0u || test == 9u) {
            assert(rc == 0 && f.reads == 64u && v.files == 30ul && v.sessions == 1ul);
            if (test == 9u) assert(f.visited == 30u);
        } else if (test == 1u || test == 8u) {
            assert(rc == OT_VERIFY_STREAM_LIMIT && f.reads == (test == 1u ? 16u : 63u));
        } else if (test == 2u || test == 3u) assert(rc == OT_VERIFY_INCOMPLETE);
        else if (test == 5u || test == 6u) assert(rc == OT_READ_IO_ERROR && reader.failed && f.reads == 20u);
        else if (test == 7u) assert(rc == OT_READ_BAD_MODE && f.reads == 0u);
        else if (test == 10u) assert(rc == OT_VERIFY_VISITOR_FAILED && f.visited == 10u && f.reads == 21u);
        else if (test == 11u) assert(rc != 0 && f.visited == 9u && f.reads == 21u);
        else if (test == 12u) assert(rc == OT_VERIFY_INCOMPLETE && f.visited == 30u);
        else assert(rc != 0 && f.reads == 21u);
    }
    session_tests();
    catalog_tests();
    catalog_export_tests();
    catalog_replacement_tests();
    catalog_import_tests();
    puts("OpenTape streaming verification tests passed");
    return 0;
}

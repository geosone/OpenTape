#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/restore.h"
#include "opentape/source_file.h"
struct fixture {
    const oa_u8 *text;
    unsigned size, chunk, closes, error, changed, close_error, bad_count;
    oa_u32 position, repeat;
};
static unsigned read_text(void *p, void *out, unsigned capacity, unsigned *got)
{
    struct fixture *f = p;
    unsigned n, i;
    oa_u32 remaining = (f->repeat ? f->repeat : f->size) - f->position;
    if (f->error && f->position >= 16ul) return 5u;
    n = remaining > capacity ? capacity : (unsigned)remaining;
    if (f->chunk && n > f->chunk) n = f->chunk;
    for (i = 0u; i < n; ++i)
        ((oa_u8 *)out)[i] = f->repeat ? ((f->position + i) % 256ul == 255ul ? '\n' : '#') :
                                         f->text[(unsigned)f->position + i];
    f->position += n;
    *got = f->bad_count ? capacity + 1u : n;
    return 0u;
}
static unsigned recheck(void *p) { return ((struct fixture *)p)->changed; }
static unsigned close_source(void *p)
{ struct fixture *f = p; ++f->closes; return f->close_error; }
struct output_fixture {
    oa_u8 bytes[4096];
    unsigned size, writes, closes, publishes, fault;
};
static unsigned write_list(void *p, const oa_u8 *data, unsigned size, unsigned *accepted)
{
    struct output_fixture *f = p;
    ++f->writes;
    assert(f->size + size <= sizeof(f->bytes));
    memcpy(f->bytes + f->size, data, size); f->size += size;
    *accepted = f->fault == 2u ? size - 1u : f->fault == 3u ? size + 1u : size;
    return f->fault == 1u ? 5u : 0u;
}
static unsigned close_list(void *p)
{ struct output_fixture *f = p; ++f->closes; return f->fault == 4u ? 6u : 0u; }
static unsigned publish_list(void *p)
{
    struct output_fixture *f = p;
    assert(f->closes == 1u); ++f->publishes;
    return f->fault == 5u ? 80u : 0u;
}
static void test_save(void)
{
    struct ot_restore_selection list, loaded;
    struct ot_selection_output ops;
    struct ot_source_ops source;
    static struct output_fixture f;
    struct fixture input;
    char path[OT_FILE_PATH_MAX + 1u];
    unsigned i, fault;
    memset(&list, 0, sizeof(list));
    for (i = 0u; i < OT_RESTORE_SELECTION_MAX; ++i) {
        sprintf(path, "F%03u.BIN", i);
        assert(ot_restore_selection_add(&list, path) == 0);
    }
    ops.context = &f; ops.write = write_list; ops.close = close_list; ops.publish = publish_list;
    for (fault = 0u; fault < 6u; ++fault) {
        memset(&f, 0, sizeof(f)); f.fault = fault;
        assert((ot_restore_selection_save(&list, &ops) == 0) == (fault == 0u));
        assert(f.closes == 1u && f.publishes == (fault == 0u || fault == 5u ? 1u : 0u));
        assert(f.writes == (fault >= 1u && fault <= 3u ? 1u : OT_RESTORE_SELECTION_MAX));
        if (fault == 0u) {
            memset(&loaded, 0, sizeof(loaded)); memset(&input, 0, sizeof(input));
            input.text = f.bytes; input.size = f.size; input.chunk = 1u;
            source.context = &input; source.read = read_text; source.recheck = recheck; source.close = close_source;
            assert(ot_restore_selection_load(&loaded, &source) == 0);
            assert(loaded.count == list.count);
            for (i = 0u; i < list.count; ++i) assert(strcmp(loaded.paths[i], list.paths[i]) == 0);
            ot_restore_selection_free(&loaded);
        }
    }
    ot_restore_selection_free(&list);
    for (i = 0u; i < 28u; ++i) memcpy(path + i * 9u, "ABCDEFGH/", 9u);
    memcpy(path + 252u, "ABC", 4u);
    assert(strlen(path) == OT_FILE_PATH_MAX);
    assert(ot_restore_selection_add(&list, path) == 0);
    memset(&f, 0, sizeof(f));
    assert(ot_restore_selection_save(&list, &ops) == 0 && f.size == OT_FILE_PATH_MAX + 2u);
    memset(&loaded, 0, sizeof(loaded)); memset(&input, 0, sizeof(input));
    input.text = f.bytes; input.size = f.size; input.chunk = 1u;
    source.context = &input; source.read = read_text; source.recheck = recheck; source.close = close_source;
    assert(ot_restore_selection_load(&loaded, &source) == 0 && loaded.count == 1u);
    assert(strcmp(loaded.paths[0], path) == 0);
    ot_restore_selection_free(&loaded); ot_restore_selection_free(&list);
    memset(&f, 0, sizeof(f));
    assert(ot_restore_selection_save(&list, &ops) == 0);
    assert(f.writes == 0u && f.closes == 1u && f.publishes == 1u);
    list.paths[0] = "../BAD"; list.count = 1u;
    memset(&f, 0, sizeof(f));
    assert(ot_restore_selection_save(&list, &ops) == OT_RESTORE_PATH);
    assert(f.writes == 0u && f.closes == 1u && f.publishes == 0u);
}
int main(void)
{
    static const char *bad[] = {
        "ROOT/A.BIN\nSAVED.BIN\n", "ROOT/*.BIN\n", "../BAD\n", "OT$OLD.TMP/X\n",
        "ROOT/A.BIN\r", "ROOT/A.BIN\rX", "ROOT/A.BIN\r\r\n", "ROOT/A.BIN \n",
        "ROOT/\tA.BIN\n", "ROOT/A.BIN\032", "\357\273\277ROOT/A.BIN\n"
    };
    struct ot_restore_selection list;
    struct ot_source_ops ops;
    struct fixture f;
    char long_line[257], path[32];
    unsigned test, i;
    int rc;
    for (test = 0u; test < 19u; ++test) {
        memset(&list, 0, sizeof(list)); memset(&f, 0, sizeof(f));
        ops.context = &f; ops.read = read_text; ops.recheck = recheck; ops.close = close_source;
        assert(ot_restore_selection_add(&list, "SAVED.BIN") == 0);
        f.text = (const oa_u8 *)"# comment\r\nROOT/A.BIN\r\n\nROOT/ZERO.BIN";
        f.size = (unsigned)strlen((const char *)f.text); f.chunk = 1u;
        if (test >= 1u && test <= 11u) {
            f.text = (const oa_u8 *)bad[test - 1u]; f.size = (unsigned)strlen((const char *)f.text);
        }
        if (test == 12u) f.error = 1u;
        if (test == 13u) f.changed = 1u;
        if (test == 14u) f.close_error = 6u;
        if (test == 15u) f.bad_count = 1u;
        if (test == 16u) {
            memset(long_line, 'A', 256u); long_line[256] = 0;
            f.text = (const oa_u8 *)long_line; f.size = 256u;
        }
        if (test == 17u) { f.text = (const oa_u8 *)"ROOT/A.BIN\0EVIL"; f.size = 15u; }
        if (test == 18u) { f.repeat = OT_RESTORE_SELECTION_BYTES + 1ul; f.chunk = 0u; }
        rc = ot_restore_selection_load(&list, &ops);
        assert(f.closes == 1u);
        if (test == 0u) {
            assert(rc == 0 && list.count == 3u);
            assert(ot_restore_selected(list.paths, list.count, "root/zero.bin") == 3u);
        } else {
            assert(rc != 0 && list.count == 1u && strcmp(list.paths[0], "SAVED.BIN") == 0);
            if (test == 18u) assert(rc == OT_RESTORE_LIMIT);
        }
        ot_restore_selection_free(&list); ot_restore_selection_free(&list);
    }
    memset(&list, 0, sizeof(list));
    for (i = 0u; i < OT_RESTORE_SELECTION_MAX; ++i) {
        sprintf(path, "F%03u.BIN", i);
        assert(ot_restore_selection_add(&list, path) == 0);
    }
    assert(ot_restore_selected(list.paths, list.count, "f255.bin") == 256u);
    memset(&f, 0, sizeof(f)); f.text = (const oa_u8 *)"OVER.BIN\n"; f.size = 9u;
    ops.context = &f;
    assert(ot_restore_selection_load(&list, &ops) == OT_RESTORE_LIMIT);
    assert(list.count == OT_RESTORE_SELECTION_MAX && f.closes == 1u);
    ot_restore_selection_free(&list);
    test_save();
    puts("OpenTape replacement-list tests passed");
    return 0;
}

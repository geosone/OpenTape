#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/backup_plan.h"
struct fixture {
    struct ot_archive_verify verified;
    unsigned closes, opens, changed, open_error, dir_error, sink_error;
};
static unsigned read_empty(void *p, void *b, unsigned n, unsigned *got)
{ (void)p; (void)b; (void)n; *got = 0u; return 0u; }
static unsigned recheck(void *p) { (void)p; return 0u; }
static unsigned close_file(void *p) { ++((struct fixture *)p)->closes; return 0u; }
static int open_file(void *p, const struct ot_plan_entry *e, struct ot_file_metadata *m,
                       struct ot_source_ops *ops)
{
    struct fixture *f = p;
    ++f->opens;
    if (f->open_error) return OT_SOURCE_IO;
    *m = e->metadata;
    if (f->changed) ++m->file_size;
    ops->context = f; ops->read = read_empty; ops->recheck = recheck; ops->close = close_file;
    return 0;
}
static int directory(void *p, const struct ot_plan_entry *e)
{ (void)e; return ((struct fixture *)p)->dir_error ? OT_SOURCE_CHANGED : 0; }
static int write_block(void *p, const oa_u8 *b, unsigned n, unsigned *got)
{
    struct fixture *f = p;
    if (f->sink_error) return -1;
    *got = n;
    return ot_archive_verify_block(&f->verified, b, (oa_u16)n);
}
static int finish(void *p) { return ot_archive_verify_finish(&((struct fixture *)p)->verified); }
int main(void)
{
    struct ot_backup_plan plan;
    struct ot_plan_ops ops;
    struct ot_writer_sink sink;
    struct ot_archive_writer w;
    struct ot_file_metadata m;
    struct ot_volume_label label;
    struct ot_session_start start;
    struct fixture f;
    oa_u8 buffer[512], data[16];
    unsigned test, i;
    int rc;
    for (test = 0u; test < 5u; ++test) {
        memset(&plan, 0, sizeof(plan)); memset(&f, 0, sizeof(f));
        memset(&m, 0, sizeof(m)); memset(&w, 0, sizeof(w));
        memset(&label, 0, sizeof(label)); memset(&start, 0, sizeof(start));
        m.kind = OT_FILE_DIRECTORY; m.dos_attributes = 0x10u; strcpy(m.path, "ROOT");
        assert(ot_plan_add(&plan, "C:\\SRC", &m) == 0);
        strcpy(m.path, "root"); assert(ot_plan_add(&plan, "C:\\SRC", &m) == OT_PLAN_DUPLICATE);
        strcpy(m.path, "ROOT/EMPTY"); assert(ot_plan_add(&plan, "C:\\SRC\\EMPTY", &m) == 0);
        m.kind = OT_FILE_REGULAR; m.dos_attributes = 0u;
        strcpy(m.path, "ROOT/A.TXT"); assert(ot_plan_add(&plan, "C:\\SRC\\A.TXT", &m) == 0);
        strcpy(m.path, "ROOT/B.TXT"); assert(ot_plan_add(&plan, "C:\\SRC\\B.TXT", &m) == 0);
        sink.context = &f; sink.write_block = write_block; sink.finish_session = finish;
        ops.context = &f; ops.open_file = open_file; ops.directory = directory;
        label.version = 1u;
        assert(ot_archive_writer_init(&w, &sink, buffer, sizeof(buffer)) == 0);
        assert(ot_archive_writer_volume(&w, &label) == 0);
        assert(ot_archive_writer_session(&w, 1ul, &start) == 0);
        f.changed = test == 1u; f.open_error = test == 2u;
        f.dir_error = test == 3u; f.sink_error = test == 4u;
        rc = ot_plan_emit(&plan, &ops, &w, data, sizeof(data));
        if (test == 0u) {
            assert(rc == 0 && f.opens == 2u && f.closes == 2u);
            assert(ot_archive_writer_session_end(&w) == 0);
            assert(f.verified.files == 2ul && f.verified.directories == 2ul);
        } else {
            assert(rc != 0 && w.failed);
            assert(ot_archive_writer_session_end(&w) == OT_WRITER_FAILED);
            if (test == 1u) assert(f.closes == 1u);
        }
        ot_plan_free(&plan); assert(plan.count == 0u && plan.first == 0);
    }
    memset(&plan, 0, sizeof(plan)); memset(&m, 0, sizeof(m)); m.kind = OT_FILE_REGULAR;
    for (i = 0u; i < OT_PLAN_MAX_ENTRIES; ++i) {
        sprintf(m.path, "F%03u.BIN", i);
        assert(ot_plan_add(&plan, "SOURCE", &m) == 0);
    }
    strcpy(m.path, "LIMIT.BIN"); assert(ot_plan_add(&plan, "SOURCE", &m) == OT_PLAN_LIMIT);
    ot_plan_free(&plan);
    strcpy(m.path, "DATA/FILE"); assert(ot_plan_add(&plan, "SOURCE", &m) == 0);
    strcpy(m.path, "data/file/CHILD"); assert(ot_plan_add(&plan, "SOURCE", &m) == OT_PLAN_DUPLICATE);
    strcpy(m.path, "DATA/FILE2"); assert(ot_plan_add(&plan, "SOURCE", &m) == 0);
    ot_plan_free(&plan);
    strcpy(m.path, "DATA/DIR/CHILD"); assert(ot_plan_add(&plan, "SOURCE", &m) == 0);
    strcpy(m.path, "data/dir"); assert(ot_plan_add(&plan, "SOURCE", &m) == OT_PLAN_DUPLICATE);
    m.kind = OT_FILE_DIRECTORY; m.dos_attributes = 0x10u;
    assert(ot_plan_add(&plan, "SOURCE", &m) == 0);
    ot_plan_free(&plan);
    puts("OpenTape backup-plan tests passed");
    return 0;
}

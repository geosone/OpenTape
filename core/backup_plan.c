#include <stdlib.h>
#include <string.h>
#include "opentape/backup_plan.h"

static int same_path(const char *a, const char *b)
{
    unsigned i;
    for (i = 0u; a[i] != 0 || b[i] != 0; ++i) {
        char x = a[i], y = b[i];
        if (x >= 'a' && x <= 'z') x = (char)(x - 'a' + 'A');
        if (y >= 'a' && y <= 'z') y = (char)(y - 'a' + 'A');
        if (x != y) return 0;
    }
    return 1;
}

int ot_plan_add(struct ot_backup_plan *p, const char *name, const struct ot_file_metadata *m)
{
    struct ot_plan_entry *e;
    oa_u8 encoded[OT_FILE_METADATA_PREFIX + OT_FILE_PATH_MAX];
    oa_u16 n;
    if (p == 0 || name == 0 || m == 0 || strlen(name) == 0u || strlen(name) > 127u ||
        ot_file_metadata_encode(encoded, sizeof(encoded), m, &n) != 0) return OT_SOURCE_ARGUMENT;
    if (p->count >= OT_PLAN_MAX_ENTRIES) return OT_PLAN_LIMIT;
    for (e = p->first; e != 0; e = e->next)
        if (same_path(e->metadata.path, m->path)) return OT_PLAN_DUPLICATE;
    e = malloc(sizeof(*e));
    if (e == 0) return OT_PLAN_LIMIT;
    e->next = 0;
    strcpy(e->source, name);
    e->metadata = *m;
    if (p->last != 0) p->last->next = e;
    else p->first = e;
    p->last = e;
    ++p->count;
    return 0;
}

void ot_plan_free(struct ot_backup_plan *p)
{
    struct ot_plan_entry *e, *next;
    if (p == 0) return;
    for (e = p->first; e != 0; e = next) { next = e->next; free(e); }
    memset(p, 0, sizeof(*p));
}

int ot_plan_emit(const struct ot_backup_plan *p, const struct ot_plan_ops *ops,
                  struct ot_archive_writer *w, oa_u8 *buffer, unsigned capacity)
{
    const struct ot_plan_entry *e;
    struct ot_file_metadata now;
    struct ot_source_ops source;
    oa_u32 object = 0ul;
    int rc = 0;
    if (p == 0 || p->first == 0 || ops == 0 || ops->open_file == 0 ||
        ops->directory == 0 || w == 0 || !w->initialized || buffer == 0 ||
        capacity == 0u || capacity > w->capacity - OT_WRITER_RECORD_OVERHEAD)
        return OT_SOURCE_ARGUMENT;
    for (e = p->first; e != 0 && rc == 0; e = e->next) {
        ++object;
        if (e->metadata.kind == OT_FILE_DIRECTORY) {
            rc = ops->directory(ops->context, e);
            if (rc == 0) rc = ot_archive_writer_metadata(w, object, &e->metadata);
        } else {
            memset(&source, 0, sizeof(source));
            rc = ops->open_file(ops->context, e, &now, &source);
            if (rc != 0) break; /* On failure, the callback owns cleanup. */
            if (source.read == 0 || source.recheck == 0 || source.close == 0) {
                if (source.close != 0) source.close(source.context);
                rc = OT_SOURCE_ARGUMENT;
            } else if (now.kind != e->metadata.kind || now.file_size != e->metadata.file_size ||
                now.dos_attributes != e->metadata.dos_attributes ||
                now.dos_date != e->metadata.dos_date || now.dos_time != e->metadata.dos_time ||
                strcmp(now.path, e->metadata.path) != 0) {
                source.close(source.context);
                rc = OT_SOURCE_CHANGED;
            } else rc = ot_backup_source(w, object, &now, &source, buffer, capacity);
        }
    }
    if (rc != 0) w->failed = 1u;
    return rc;
}

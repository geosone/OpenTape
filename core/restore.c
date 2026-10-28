#include <stdlib.h>
#include <string.h>
#include "opentape/restore.h"
struct ot_restore_entry {
    struct ot_restore_entry *next;
    unsigned kind;
    char path[OT_FILE_PATH_MAX + 1u];
};
static char upper(char c) { return c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c; }
static int prefix(const char *a, const char *b)
{
    unsigned i;
    for (i = 0u; a[i] != 0; ++i) if (upper(a[i]) != upper(b[i])) return 0;
    return b[i] == 0 ? 2 : b[i] == '/' ? 1 : 0;
}
static int remember(struct ot_restore *s, const struct ot_file_metadata *m)
{
    struct ot_restore_entry *e;
    int p;
    if (!ot_archive_path_valid(m->path) || prefix(OT_RESTORE_TEMP, m->path) ||
        prefix(OT_RESTORE_OLD, m->path)) return OT_RESTORE_PATH;
    for (e = s->entries; e != 0; e = e->next) {
        p = prefix(e->path, m->path);
        if (p == 2 || (p == 1 && e->kind == OT_FILE_REGULAR) ||
            (m->kind == OT_FILE_REGULAR && prefix(m->path, e->path))) return OT_RESTORE_PATH;
    }
    if (s->count >= OT_RESTORE_MAX_ENTRIES) return OT_RESTORE_LIMIT;
    e = malloc(sizeof(*e));
    if (e == 0) return OT_RESTORE_LIMIT;
    e->kind = m->kind; strcpy(e->path, m->path);
    e->next = s->entries; s->entries = e; ++s->count;
    return 0;
}
int ot_restore_init(struct ot_restore *s, const struct ot_restore_ops *ops)
{
    if (s == 0 || s->ops != 0 || ops == 0 || ops->directory == 0 || ops->begin == 0 ||
        ops->write == 0 || ops->commit == 0 || ops->abort == 0) return OT_FORMAT_BAD_ARGUMENT;
    memset(s, 0, sizeof(*s)); s->ops = ops;
    return 0;
}
int ot_restore_observe(struct ot_restore *s, ot_restore_observer observer, void *context)
{
    if (s == 0 || s->ops == 0) return OT_FORMAT_BAD_ARGUMENT;
    if (s->failed || s->finished || s->busy) return OT_RESTORE_FAILED;
    if (s->verified.framing.blocks != 0ul) return OT_FORMAT_BAD_ARGUMENT;
    s->observer = observer; s->observer_context = context;
    return 0;
}
int ot_restore_block(struct ot_restore *s, const oa_u8 *block, oa_u16 size)
{
    struct ot_archive_verify next;
    struct ot_record_header r;
    struct ot_file_metadata m;
    oa_u16 offset, i;
    unsigned error = 0u, accepted, selected;
    oa_u32 checked_low, checked_high, before, file_bytes;
    int rc;
    if (s == 0 || s->ops == 0 || block == 0) return OT_FORMAT_BAD_ARGUMENT;
    if (s->failed || s->finished || s->busy) return OT_RESTORE_FAILED;
    next = s->verified;
    rc = ot_archive_verify_block(&next, block, size);
    if (rc != 0) { s->failed = 1u; return rc; }
    checked_low = s->checked_low; checked_high = s->checked_high; file_bytes = s->active_bytes;
    /* Validate the complete OTB1 block before invoking any output callback. */
    s->busy = 1u;
    offset = OT_BLOCK_HEADER_SIZE;
    for (i = 0u; i < next.framing.last_block.record_count; ++i) {
        ot_record_header_decode(&r, block + offset);
        offset = (oa_u16)(offset + OT_RECORD_HEADER_SIZE);
        if (r.type == OT_RECORD_FILE_METADATA) {
            ot_file_metadata_decode(&m, block + offset, (oa_u16)r.data_size);
            rc = remember(s, &m);
            if (rc != 0) break;
            if (m.kind == OT_FILE_DIRECTORY) {
                if (!s->filter_enabled) {
                    error = s->ops->directory(s->ops->context, &m);
                    if (error == 0u) ++s->restored_directories;
                }
            }
            else {
                s->active = m; file_bytes = 0ul;
                selected = s->filter_enabled ? ot_restore_selected(s->filter, s->filter_count, m.path) : 0u;
                s->excluding = s->filter_enabled && selected == 0u;
                if (selected != 0u) s->filter_seen[selected - 1u] = 1u;
                if (s->excluding) { s->skipping = 1u; s->staging = 0u; }
                else {
                    s->staging = 1u; /* A failed begin may still need cleanup. */
                    error = s->ops->begin(s->ops->context, &m);
                    if (error == OT_RESTORE_SKIP) {
                        s->skipping = 1u; s->staging = 0u; error = 0u;
                    } else s->skipping = 0u;
                }
            }
        } else if (r.type == OT_RECORD_FILE_DATA) {
            if (!s->skipping) {
                accepted = 0u;
                error = s->ops->write(s->ops->context, block + offset, (unsigned)r.data_size, &accepted);
                if (error == 0u && accepted != r.data_size) error = 29u;
            }
        } else if (r.type == OT_RECORD_FILE_END) {
            if (s->skipping) {
                if (s->excluding) ++s->excluded_files; else ++s->skipped_files;
                s->skipping = s->excluding = 0u;
            }
            else {
                error = s->ops->commit(s->ops->context, &s->active);
                if (error == 0u) { s->staging = 0u; ++s->restored_files; }
            }
        }
        if (error != 0u) { rc = OT_RESTORE_IO; break; }
        if (r.type == OT_RECORD_FILE_DATA) {
            before = checked_low; checked_low += r.data_size; file_bytes += r.data_size;
            if (checked_low < before) ++checked_high;
        }
        offset = (oa_u16)(offset + (oa_u16)r.data_size);
    }
    s->busy = 0u;
    if (rc != 0) { s->failed = 1u; s->backend_error = error; return rc; }
    s->verified = next; s->checked_low = checked_low; s->checked_high = checked_high; s->active_bytes = file_bytes;
    if (s->observer != 0) {
        s->busy = 1u;
        s->observer(s->observer_context, s);
        s->busy = 0u;
    }
    return 0;
}
int ot_restore_finish(struct ot_restore *s)
{
    int rc;
    unsigned i;
    if (s == 0 || s->ops == 0) return OT_FORMAT_BAD_ARGUMENT;
    if (s->failed || s->finished || s->busy) return OT_RESTORE_FAILED;
    rc = ot_archive_verify_finish(&s->verified);
    if (rc != 0 || s->staging) { s->failed = 1u; return rc != 0 ? rc : OT_VERIFY_INCOMPLETE; }
    for (i = 0u; i < s->filter_count; ++i) if (!s->filter_seen[i]) {
        s->failed = 1u; return OT_RESTORE_PATH;
    }
    s->finished = 1u;
    return 0;
}
int ot_restore_set_filter(struct ot_restore *s, const char *const *paths, unsigned count)
{
    if (s == 0 || s->ops == 0 || s->failed || s->finished || s->busy ||
        s->filter_enabled || s->verified.framing.blocks != 0ul ||
        count == 0u || !ot_restore_selection_valid(paths, count)) return OT_RESTORE_PATH;
    s->filter = paths; s->filter_count = count; s->filter_enabled = 1u;
    return 0;
}
void ot_restore_free(struct ot_restore *s)
{
    struct ot_restore_entry *e, *next;
    if (s == 0 || s->busy) return;
    if (!s->finished) s->failed = 1u;
    if (s->staging && s->ops != 0) {
        unsigned error;
        s->staging = 0u;
        error = s->ops->abort(s->ops->context);
        if (s->backend_error == 0u) s->backend_error = error;
    }
    for (e = s->entries; e != 0; e = next) { next = e->next; free(e); }
    s->entries = 0;
    /* Deliberately retain completion/failure/diagnostic fields. */
}

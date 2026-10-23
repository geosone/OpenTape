#include <stdlib.h>
#include <string.h>
#include "opentape/archive_catalog.h"
#include "opentape/restore.h"

int ot_catalog_capture(void *context, oa_u32 session, oa_u32 object,
                       const struct ot_file_metadata *metadata)
{
    struct ot_catalog *catalog = context;
    struct ot_catalog_entry *entry;
    if (catalog == 0) return OT_CATALOG_ARGUMENT;
    if (catalog->error != 0) return catalog->error;
    if (metadata == 0 || session == 0ul || object == 0ul ||
        memchr(metadata->path, 0, sizeof(metadata->path)) == 0 ||
        !ot_archive_path_valid(metadata->path) ||
        (metadata->kind != OT_FILE_REGULAR && metadata->kind != OT_FILE_DIRECTORY))
        return catalog->error = OT_CATALOG_ARGUMENT;
    if (catalog->count >= OT_CATALOG_MAX_ENTRIES)
        return catalog->error = OT_CATALOG_LIMIT;
    entry = malloc(sizeof(*entry));
    if (entry == 0) return catalog->error = OT_CATALOG_MEMORY;
    entry->next = 0; entry->session = session; entry->object = object;
    entry->metadata = *metadata;
    entry->marked = 0u;
    if (catalog->last != 0) catalog->last->next = entry;
    else catalog->first = entry;
    catalog->last = entry;
    ++catalog->count;
    return 0;
}

const struct ot_catalog_entry *ot_catalog_at(const struct ot_catalog *catalog, unsigned index)
{
    const struct ot_catalog_entry *entry;
    if (catalog == 0 || index >= catalog->count) return 0;
    entry = catalog->first;
    while (index-- != 0u && entry != 0) entry = entry->next;
    return entry;
}

int ot_catalog_mark(struct ot_catalog *catalog, unsigned index, unsigned marked)
{
    struct ot_catalog_entry *entry;
    if (catalog == 0 || marked > 1u || catalog->error != 0 || index >= catalog->count)
        return OT_CATALOG_ARGUMENT;
    entry = catalog->first;
    while (index-- != 0u && entry != 0) entry = entry->next;
    if (entry == 0 || entry->metadata.kind != OT_FILE_REGULAR) return OT_CATALOG_ARGUMENT;
    if (entry->marked != marked) {
        if (marked) ++catalog->marked;
        else --catalog->marked;
        entry->marked = marked;
    }
    return 0;
}

int ot_catalog_mark_all(struct ot_catalog *catalog, unsigned marked)
{
    struct ot_catalog_entry *entry;
    if (catalog == 0 || marked > 1u || catalog->error != 0) return OT_CATALOG_ARGUMENT;
    catalog->marked = 0u;
    for (entry = catalog->first; entry != 0; entry = entry->next) {
        entry->marked = marked && entry->metadata.kind == OT_FILE_REGULAR;
        catalog->marked += entry->marked;
    }
    return 0;
}

void ot_catalog_free(struct ot_catalog *catalog)
{
    struct ot_catalog_entry *entry, *next;
    if (catalog == 0) return;
    for (entry = catalog->first; entry != 0; entry = next) {
        next = entry->next;
        free(entry);
    }
    memset(catalog, 0, sizeof(*catalog));
}

int ot_catalog_selection(const struct ot_catalog *catalog, struct ot_restore_selection *list)
{
    const struct ot_catalog_entry *entry, *other;
    const char *one[1];
    int rc = 0;
    if (catalog == 0 || list == 0 || list->count != 0u || catalog->error != 0 ||
        catalog->marked == 0u || catalog->count > OT_CATALOG_MAX_ENTRIES)
        return OT_CATALOG_ARGUMENT;
    for (entry = catalog->first; entry != 0 && rc == 0; entry = entry->next) {
        if (!entry->marked) continue;
        if (entry->metadata.kind != OT_FILE_REGULAR) { rc = OT_CATALOG_ARGUMENT; break; }
        one[0] = entry->metadata.path;
        for (other = catalog->first; other != 0; other = other->next)
            if (other != entry && ot_restore_selected(one, 1u, other->metadata.path) != 0u) {
                rc = OT_CATALOG_AMBIGUOUS; break;
            }
        if (rc == 0) rc = ot_restore_selection_add(list, entry->metadata.path);
    }
    if (rc == 0 && list->count != catalog->marked) rc = OT_CATALOG_ARGUMENT;
    if (rc != 0) ot_restore_selection_free(list);
    return rc;
}

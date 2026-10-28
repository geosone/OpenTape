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
    entry->marked = entry->replace = 0u;
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
        if (!marked && entry->replace) { entry->replace = 0u; --catalog->replacements; }
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
    if (!marked) ot_catalog_clear_replacements(catalog);
    return 0;
}

int ot_catalog_replace(struct ot_catalog *catalog, unsigned index, unsigned replace)
{
    struct ot_catalog_entry *entry;
    if (catalog == 0 || catalog->error != 0 || index >= catalog->count || replace > 1u)
        return OT_CATALOG_ARGUMENT;
    entry = catalog->first;
    while (index-- != 0u && entry != 0) entry = entry->next;
    if (entry == 0 || !entry->marked || entry->metadata.kind != OT_FILE_REGULAR)
        return OT_CATALOG_ARGUMENT;
    if (entry->replace != replace) {
        if (replace) ++catalog->replacements;
        else --catalog->replacements;
        entry->replace = replace;
    }
    return 0;
}

void ot_catalog_clear_replacements(struct ot_catalog *catalog)
{
    struct ot_catalog_entry *entry;
    if (catalog == 0) return;
    for (entry = catalog->first; entry != 0; entry = entry->next) entry->replace = 0u;
    catalog->replacements = 0u;
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

static int catalog_selection(const struct ot_catalog *catalog, struct ot_restore_selection *list, unsigned replacing)
{
    const struct ot_catalog_entry *entry, *other;
    const char *one[1];
    unsigned marked = 0u, replacements = 0u;
    int rc = 0;
    if (catalog == 0 || list == 0 || list->count != 0u || catalog->error != 0 ||
        (!replacing && catalog->marked == 0u) || catalog->count > OT_CATALOG_MAX_ENTRIES)
        return OT_CATALOG_ARGUMENT;
    for (entry = catalog->first; entry != 0 && rc == 0; entry = entry->next) {
        /* Reject inconsistent grants before any snapshot becomes usable. */
        if (entry->marked > 1u || entry->replace > 1u ||
            (entry->replace && !entry->marked) ||
            (entry->marked && entry->metadata.kind != OT_FILE_REGULAR)) { rc = OT_CATALOG_ARGUMENT; break; }
        marked += entry->marked; replacements += entry->replace;
        if (!(replacing ? entry->replace : entry->marked)) continue;
        if (entry->metadata.kind != OT_FILE_REGULAR) { rc = OT_CATALOG_ARGUMENT; break; }
        one[0] = entry->metadata.path;
        for (other = catalog->first; other != 0; other = other->next)
            if (other != entry && ot_restore_selected(one, 1u, other->metadata.path) != 0u) {
                rc = OT_CATALOG_AMBIGUOUS; break;
            }
        if (rc == 0) rc = ot_restore_selection_add(list, entry->metadata.path);
    }
    if (rc == 0 && (marked != catalog->marked || replacements != catalog->replacements ||
        list->count != (replacing ? catalog->replacements : catalog->marked))) rc = OT_CATALOG_ARGUMENT;
    if (rc != 0) ot_restore_selection_free(list);
    return rc;
}

int ot_catalog_selection(const struct ot_catalog *catalog, struct ot_restore_selection *list)
{ return catalog_selection(catalog, list, 0u); }

int ot_catalog_replacements(const struct ot_catalog *catalog, struct ot_restore_selection *list)
{ return catalog_selection(catalog, list, 1u); }

/* Resolve the entire list before committing any annotation. A bounded index
 * bitmap avoids retaining input pointers or allocating another catalogue. */
int ot_catalog_apply_selection(struct ot_catalog *catalog, const struct ot_restore_selection *list)
{
    struct ot_catalog_entry *entry;
    oa_u8 selected[OT_CATALOG_MAX_ENTRIES];
    unsigned i, index, match, found, regular;
    if (catalog == 0 || list == 0 || catalog->error != 0 ||
        catalog->count > OT_CATALOG_MAX_ENTRIES || list->count == 0u ||
        !ot_restore_selection_valid(list->paths, list->count)) return OT_CATALOG_ARGUMENT;
    memset(selected, 0, sizeof(selected));
    for (i = 0u; i < list->count; ++i) {
        found = 0u; match = 0u; index = 0u; regular = 0u;
        for (entry = catalog->first; entry != 0; entry = entry->next, ++index) {
            if (index >= catalog->count) return OT_CATALOG_ARGUMENT;
            if (ot_restore_selected(list->paths + i, 1u, entry->metadata.path)) {
                if (++found > 1u) return OT_CATALOG_AMBIGUOUS;
                regular = entry->metadata.kind == OT_FILE_REGULAR;
                match = index;
            }
        }
        if (index != catalog->count) return OT_CATALOG_ARGUMENT;
        if (!found) return OT_CATALOG_MISSING;
        if (!regular) return OT_CATALOG_ARGUMENT;
        selected[match] = 1u;
    }
    /* No failure-capable operations remain. Permissions are not list data. */
    index = 0u;
    for (entry = catalog->first; entry != 0; entry = entry->next, ++index) {
        entry->marked = selected[index];
        entry->replace = 0u;
    }
    catalog->marked = list->count;
    catalog->replacements = 0u;
    return 0;
}

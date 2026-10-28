/* Atomic directory selection over the offline catalogue. Plan a bounded entry
 * bitmap before changing marks so malformed/partial snapshots cannot produce
 * a partly applied group. Reuse individual mark rules for replacement intent. */
#include <string.h>
#include "opentape/archive_catalog.h"

static unsigned fold(unsigned char c)
{ return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c; }

static int descendant(const char *directory, const char *path)
{
    unsigned i, length = (unsigned)strlen(directory);
    if (strlen(path) <= length || path[length] != '/') return 0;
    for (i = 0u; i < length; ++i)
        if (fold((unsigned char)directory[i]) != fold((unsigned char)path[i])) return 0;
    return 1;
}

int ot_catalog_mark_tree(struct ot_catalog *catalog, unsigned directory, unsigned marked,
                         struct ot_catalog_tree_result *result)
{
    const struct ot_catalog_entry *root, *entry, *last = 0;
    unsigned char members[(OT_CATALOG_MAX_ENTRIES + 7u) / 8u];
    unsigned i, total_marked = 0u, total_replacements = 0u, files = 0u, selected = 0u;
    struct ot_catalog_tree_result value;
    if (catalog == 0 || result == 0 || catalog->error != 0 ||
        catalog->count > OT_CATALOG_MAX_ENTRIES || directory >= catalog->count ||
        marked > OT_CATALOG_TREE_TOGGLE) return OT_CATALOG_ARGUMENT;
    root = ot_catalog_at(catalog, directory);
    if (root == 0 || root->metadata.kind != OT_FILE_DIRECTORY ||
        memchr(root->metadata.path, 0, sizeof(root->metadata.path)) == 0 ||
        !ot_archive_path_valid(root->metadata.path)) return OT_CATALOG_ARGUMENT;
    memset(members, 0, sizeof(members));
    entry = catalog->first;
    for (i = 0u; i < catalog->count; ++i) {
        if (entry == 0 || entry->session == 0ul || entry->object == 0ul ||
            memchr(entry->metadata.path, 0, sizeof(entry->metadata.path)) == 0 ||
            !ot_archive_path_valid(entry->metadata.path) ||
            (entry->metadata.kind != OT_FILE_REGULAR && entry->metadata.kind != OT_FILE_DIRECTORY) ||
            entry->marked > 1u || entry->replace > entry->marked ||
            (entry->metadata.kind == OT_FILE_DIRECTORY && entry->marked)) return OT_CATALOG_ARGUMENT;
        total_marked += entry->marked; total_replacements += entry->replace;
        if (entry->metadata.kind == OT_FILE_REGULAR && entry->session == root->session &&
            descendant(root->metadata.path, entry->metadata.path)) {
            members[i / 8u] |= (unsigned char)(1u << (i % 8u));
            ++files; selected += entry->marked;
        }
        last = entry; entry = entry->next;
    }
    if (entry != 0 || last != catalog->last || total_marked != catalog->marked ||
        total_replacements != catalog->replacements) return OT_CATALOG_ARGUMENT;
    if (marked == OT_CATALOG_TREE_TOGGLE) marked = selected < files;
    /* Everything below is memory-only and cannot fail after the full snapshot
     * checks above. Existing per-file transitions own grant revocation. */
    for (i = 0u; i < catalog->count; ++i)
        if (members[i / 8u] & (1u << (i % 8u))) (void)ot_catalog_mark(catalog, i, marked);
    value.files = files; value.selected = marked ? files : 0u;
    *result = value;
    return 0;
}

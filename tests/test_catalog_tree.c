/* Directory groups must respect '/' boundaries/session identity and apply
 * atomically. No case may create replacement permission or mark a directory. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/archive_catalog.h"
/* DOS large model: each node stays below a 64 KiB segment, like production. */
static struct ot_catalog_entry *entries[OT_CATALOG_MAX_ENTRIES];
static struct ot_catalog_entry *before[OT_CATALOG_MAX_ENTRIES];
static void init(struct ot_catalog *catalog, unsigned count)
{
    unsigned i;
    memset(catalog, 0, sizeof(*catalog));
    for (i = 0u; i < OT_CATALOG_MAX_ENTRIES; ++i) memset(entries[i], 0, sizeof(*entries[i]));
    for (i = 0u; i < count; ++i) {
        entries[i]->session = 1ul; entries[i]->object = i + 1ul;
        entries[i]->metadata.kind = i == 0u ? OT_FILE_DIRECTORY : OT_FILE_REGULAR;
        sprintf(entries[i]->metadata.path, "DATA/F%03u.BIN", i);
        entries[i]->next = i + 1u < count ? entries[i + 1u] : 0;
    }
    strcpy(entries[0]->metadata.path, "DATA");
    catalog->first = entries[0]; catalog->last = entries[count - 1u]; catalog->count = count;
}
static void failure(struct ot_catalog *catalog, unsigned directory, unsigned marked)
{
    struct ot_catalog saved = *catalog;
    unsigned i;
    struct ot_catalog_tree_result result;
    for (i = 0u; i < OT_CATALOG_MAX_ENTRIES; ++i) *before[i] = *entries[i];
    result.files = 111u; result.selected = 222u;
    assert(ot_catalog_mark_tree(catalog, directory, marked, &result) == OT_CATALOG_ARGUMENT);
    assert(result.files == 111u && result.selected == 222u);
    assert(memcmp(&saved, catalog, sizeof(saved)) == 0);
    for (i = 0u; i < OT_CATALOG_MAX_ENTRIES; ++i)
        assert(memcmp(before[i], entries[i], sizeof(*entries[i])) == 0);
}
int main(void)
{
    struct ot_catalog catalog;
    struct ot_catalog_tree_result result;
    unsigned i;
    for (i = 0u; i < OT_CATALOG_MAX_ENTRIES; ++i) {
        entries[i] = malloc(sizeof(*entries[i])); before[i] = malloc(sizeof(*before[i]));
        assert(entries[i] != 0 && before[i] != 0);
    }
    init(&catalog, 8u);
    strcpy(entries[1]->metadata.path, "data/A.BIN");
    strcpy(entries[2]->metadata.path, "DATA/SUB"); entries[2]->metadata.kind = OT_FILE_DIRECTORY;
    strcpy(entries[3]->metadata.path, "Data/sub/B.BIN");
    strcpy(entries[4]->metadata.path, "DATA2/OTHER.BIN");
    strcpy(entries[5]->metadata.path, "DATA/OLD.BIN"); entries[5]->session = 2ul;
    strcpy(entries[6]->metadata.path, "DATA/SUB2/OUT.BIN");
    strcpy(entries[7]->metadata.path, "DATA"); /* Exact file/directory collision is not a child. */
    assert(ot_catalog_mark(&catalog, 1u, 1u) == 0 && ot_catalog_replace(&catalog, 1u, 1u) == 0);
    assert(ot_catalog_mark(&catalog, 4u, 1u) == 0 && ot_catalog_replace(&catalog, 4u, 1u) == 0);
    assert(ot_catalog_mark_tree(&catalog, 0u, OT_CATALOG_TREE_TOGGLE, &result) == 0);
    assert(result.files == 3u && result.selected == 3u && catalog.marked == 4u && catalog.replacements == 2u);
    assert(entries[1]->replace && !entries[3]->replace && !entries[6]->replace);
    assert(!entries[0]->marked && !entries[2]->marked && !entries[5]->marked && !entries[7]->marked);
    assert(ot_catalog_mark_tree(&catalog, 2u, OT_CATALOG_TREE_TOGGLE, &result) == 0);
    assert(result.files == 1u && !result.selected && !entries[3]->marked && entries[6]->marked);
    assert(catalog.replacements == 2u && entries[1]->replace && entries[4]->replace);
    assert(ot_catalog_mark_tree(&catalog, 2u, 1u, &result) == 0 && result.selected == 1u);
    assert(!entries[3]->replace); /* Re-adding never restores a grant. */
    assert(ot_catalog_mark_tree(&catalog, 0u, OT_CATALOG_TREE_TOGGLE, &result) == 0);
    assert(!result.selected && catalog.marked == 1u && catalog.replacements == 1u && entries[4]->replace);
    assert(!entries[1]->replace);
    assert(ot_catalog_mark_tree(&catalog, 0u, 0u, &result) == 0 && catalog.marked == 1u);
    assert(ot_catalog_mark_tree(&catalog, 0u, 1u, &result) == 0 && result.selected == 3u);
    assert(catalog.replacements == 1u); /* Explicit add is idempotent. */
    strcpy(entries[2]->metadata.path, "EMPTY");
    assert(ot_catalog_mark_tree(&catalog, 2u, OT_CATALOG_TREE_TOGGLE, &result) == 0);
    assert(!result.files && !result.selected && catalog.marked == 4u);
    failure(&catalog, 1u, 1u); failure(&catalog, 8u, 1u); failure(&catalog, 0u, 3u);
    catalog.marked++; failure(&catalog, 0u, 0u); catalog.marked--;
    catalog.replacements++; failure(&catalog, 0u, 0u); catalog.replacements--;
    entries[7]->replace = 1u; failure(&catalog, 0u, 1u); entries[7]->replace = 0u;
    entries[7]->metadata.kind = 99u; failure(&catalog, 0u, 1u); entries[7]->metadata.kind = OT_FILE_REGULAR;
    entries[7]->session = 0ul; failure(&catalog, 0u, 1u); entries[7]->session = 1ul;
    entries[7]->object = 0ul; failure(&catalog, 0u, 1u); entries[7]->object = 8ul;
    memset(entries[7]->metadata.path, 'A', sizeof(entries[7]->metadata.path)); failure(&catalog, 0u, 1u);
    strcpy(entries[7]->metadata.path, "../BAD"); failure(&catalog, 0u, 1u);
    strcpy(entries[7]->metadata.path, "DATA");
    entries[7]->next = entries[0]; failure(&catalog, 0u, 1u); entries[7]->next = 0;
    entries[6]->next = 0; failure(&catalog, 0u, 1u); entries[6]->next = entries[7];
    catalog.last = entries[0]; failure(&catalog, 0u, 1u); catalog.last = entries[7];
    catalog.count = 257u; failure(&catalog, 0u, 1u); catalog.count = 8u;
    catalog.error = -123; failure(&catalog, 0u, 1u); catalog.error = 0;
    assert(ot_catalog_mark_tree(0, 0u, 1u, &result) == OT_CATALOG_ARGUMENT);
    assert(ot_catalog_mark_tree(&catalog, 0u, 1u, 0) == OT_CATALOG_ARGUMENT);
    init(&catalog, 256u);
    assert(ot_catalog_mark(&catalog, 255u, 1u) == 0 && ot_catalog_replace(&catalog, 255u, 1u) == 0);
    assert(ot_catalog_mark_tree(&catalog, 0u, 1u, &result) == 0);
    assert(result.files == 255u && result.selected == 255u && catalog.replacements == 1u);
    assert(ot_catalog_mark_tree(&catalog, 0u, OT_CATALOG_TREE_TOGGLE, &result) == 0);
    assert(!result.selected && !catalog.marked && !catalog.replacements);
    for (i = 0u; i < 256u; ++i) assert(!entries[i]->marked && !entries[i]->replace);
    for (i = 0u; i < OT_CATALOG_MAX_ENTRIES; ++i) { free(before[i]); free(entries[i]); }
    puts("Catalogue directory selection tests passed");
    return 0;
}

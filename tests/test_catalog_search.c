/* Bounded offline search: independent entries, full-path matching and circular
 * order never change marks/grants, even on malformed/no-match queries. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/catalog_search.h"
/* Allocate per entry, matching production and DOS segmented pointer limits. */
static struct ot_catalog_entry *entries[OT_CATALOG_MAX_ENTRIES];
static void hit(struct ot_catalog *catalog, const char *query, unsigned start,
                int direction, unsigned include, unsigned index, unsigned wrapped)
{
    struct ot_catalog_match result;
    assert(ot_catalog_search(catalog, query, start, direction, include, &result) == 0);
    assert(result.index == index && result.wrapped == wrapped);
}
static void fail(struct ot_catalog *catalog, const char *query, unsigned start,
                 int direction, unsigned include, int code)
{
    struct ot_catalog_match result;
    result.index = 123u; result.wrapped = 456u;
    assert(ot_catalog_search(catalog, query, start, direction, include, &result) == code);
    assert(result.index == 123u && result.wrapped == 456u);
}
int main(void)
{
    struct ot_catalog catalog;
    unsigned i;
    char query[42];
    memset(&catalog, 0, sizeof(catalog));
    fail(&catalog, "", 0u, 1, 1u, OT_CATALOG_SEARCH_EMPTY);
    fail(&catalog, "x", 0u, 1, 1u, OT_CATALOG_SEARCH_MISS);
    for (i = 0u; i < OT_CATALOG_MAX_ENTRIES; ++i) {
        entries[i] = calloc(1u, sizeof(*entries[i])); assert(entries[i] != 0);
    }
    for (i = 0u; i < OT_CATALOG_MAX_ENTRIES; ++i) {
        sprintf(entries[i]->metadata.path, "DATA/FILE%03u.BIN", i);
        entries[i]->metadata.kind = OT_FILE_REGULAR;
        entries[i]->next = i + 1u < OT_CATALOG_MAX_ENTRIES ? entries[i + 1u] : 0;
        entries[i]->marked = entries[i]->replace = i % 2u;
    }
    catalog.first = entries[0]; catalog.last = entries[255]; catalog.count = 256u;
    catalog.marked = catalog.replacements = 128u;
    hit(&catalog, "file", 0u, 1, 1u, 0u, 0u);
    hit(&catalog, "FiLe", 0u, 1, 0u, 1u, 0u);
    hit(&catalog, "FILE", 255u, 1, 0u, 0u, 1u);
    hit(&catalog, "file", 0u, -1, 0u, 255u, 1u);
    hit(&catalog, "FILE", 255u, -1, 0u, 254u, 0u);
    hit(&catalog, "file255.bin", 0u, 1, 1u, 255u, 0u);
    hit(&catalog, "file255.bin", 255u, 1, 0u, 255u, 1u);
    hit(&catalog, "file255.bin", 255u, -1, 0u, 255u, 1u);
    hit(&catalog, "file000.bin", 100u, -1, 0u, 0u, 0u);
    hit(&catalog, "file255.bin", 100u, -1, 0u, 255u, 1u);
    strcpy(entries[5]->metadata.path, "DATA/SUBDIR"); entries[5]->metadata.kind = OT_FILE_DIRECTORY;
    hit(&catalog, "subdir", 4u, 1, 0u, 5u, 0u);
    strcpy(entries[10]->metadata.path, "DATA/FILE255.BIN");
    hit(&catalog, "FILE255", 0u, 1, 0u, 10u, 0u);
    hit(&catalog, "FILE255", 10u, 1, 0u, 255u, 0u);
    hit(&catalog, "FILE255", 255u, 1, 0u, 10u, 1u);
    strcpy(entries[200]->metadata.path,
        "ABCDEFGH/ABCDEFGH/ABCDEFGH/ABCDEFGH/ABCDEFGH/ABCDEFGH/ABCDEFGH/LAST.BIN");
    hit(&catalog, "abcdefgh/last.bin", 0u, 1, 0u, 200u, 0u);
    strcpy(entries[201]->metadata.path, "DATA/\200FILE.BIN");
    hit(&catalog, "\200file", 0u, 1, 0u, 201u, 0u);
    fail(&catalog, "\201file", 0u, 1, 0u, OT_CATALOG_SEARCH_MISS);
    fail(&catalog, "*.BIN", 0u, 1, 0u, OT_CATALOG_SEARCH_MISS);
    fail(&catalog, "absent", 0u, -1, 0u, OT_CATALOG_SEARCH_MISS);
    assert(catalog.marked == 128u && catalog.replacements == 128u && !catalog.error);
    for (i = 0u; i < 256u; ++i) assert(entries[i]->marked == i % 2u && entries[i]->replace == i % 2u);
    memset(query, 'A', 40u); query[40] = 0;
    assert(ot_catalog_query_valid(query)); query[40] = 'A'; query[41] = 0;
    fail(&catalog, query, 0u, 1, 0u, OT_CATALOG_ARGUMENT);
    fail(&catalog, "x\n", 0u, 1, 0u, OT_CATALOG_ARGUMENT);
    fail(&catalog, "\177", 0u, 1, 0u, OT_CATALOG_ARGUMENT);
    fail(&catalog, 0, 0u, 1, 0u, OT_CATALOG_ARGUMENT);
    fail(&catalog, "x", 256u, 1, 0u, OT_CATALOG_ARGUMENT);
    fail(&catalog, "x", 0u, 0, 0u, OT_CATALOG_ARGUMENT);
    fail(&catalog, "x", 0u, 1, 2u, OT_CATALOG_ARGUMENT);
    assert(ot_catalog_search(&catalog, "x", 0u, 1, 0u, 0) == OT_CATALOG_ARGUMENT);
    fail(0, "x", 0u, 1, 0u, OT_CATALOG_ARGUMENT);
    catalog.count = 257u; fail(&catalog, "file", 0u, 1, 0u, OT_CATALOG_ARGUMENT);
    catalog.count = 255u; fail(&catalog, "file", 0u, 1, 0u, OT_CATALOG_ARGUMENT);
    catalog.count = 256u; entries[5]->next = 0;
    fail(&catalog, "file", 0u, 1, 0u, OT_CATALOG_ARGUMENT);
    entries[5]->next = entries[6]; entries[255]->next = entries[0];
    fail(&catalog, "file", 0u, 1, 0u, OT_CATALOG_ARGUMENT);
    entries[255]->next = 0; memset(entries[255]->metadata.path, 'A', sizeof(entries[255]->metadata.path));
    fail(&catalog, "file", 0u, 1, 0u, OT_CATALOG_ARGUMENT);
    catalog.error = -123; fail(&catalog, "file", 0u, 1, 0u, OT_CATALOG_ARGUMENT);
    assert(catalog.error == -123);
    for (i = 0u; i < OT_CATALOG_MAX_ENTRIES; ++i) free(entries[i]);
    puts("Catalogue search tests passed");
    return 0;
}

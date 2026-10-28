/* Search the already verified offline catalogue. Rank matching entries by
 * circular distance from the cursor in one linked-list pass. Duplicate paths
 * and directories remain separate browseable entries; search grants nothing. */
#include <string.h>
#include "opentape/catalog_search.h"

int ot_catalog_query_valid(const char *query)
{
    unsigned i;
    if (query == 0) return 0;
    for (i = 0u; i <= OT_CATALOG_QUERY_MAX; ++i) {
        unsigned char c = (unsigned char)query[i];
        if (c == 0u) return 1;
        if (c < 32u || c == 127u) return 0;
    }
    return 0;
}
static unsigned fold(unsigned char c)
{ return c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c; }

static int contains(const char *path, const char *query)
{
    unsigned offset, i;
    for (offset = 0u; path[offset] != 0; ++offset) {
        for (i = 0u; query[i] != 0 && path[offset + i] != 0; ++i)
            if (fold((unsigned char)path[offset + i]) != fold((unsigned char)query[i])) break;
        if (query[i] == 0) return 1;
    }
    return 0;
}

int ot_catalog_search(const struct ot_catalog *catalog, const char *query, unsigned start,
                      int direction, unsigned include_current, struct ot_catalog_match *result)
{
    const struct ot_catalog_entry *entry;
    struct ot_catalog_match found;
    unsigned i, distance, best;
    if (catalog == 0 || result == 0 || catalog->error != 0 ||
        catalog->count > OT_CATALOG_MAX_ENTRIES || !ot_catalog_query_valid(query) ||
        (direction != 1 && direction != -1) || include_current > 1u ||
        (catalog->count != 0u ? start >= catalog->count : start != 0u)) return OT_CATALOG_ARGUMENT;
    if (*query == 0) return OT_CATALOG_SEARCH_EMPTY;
    best = catalog->count + 1u;
    memset(&found, 0, sizeof(found));
    entry = catalog->first;
    for (i = 0u; i < catalog->count; ++i) {
        if (entry == 0 || memchr(entry->metadata.path, 0, sizeof(entry->metadata.path)) == 0)
            return OT_CATALOG_ARGUMENT;
        if (contains(entry->metadata.path, query)) {
            distance = direction == 1 ? (i >= start ? i - start : catalog->count - start + i) :
                (i <= start ? start - i : start + catalog->count - i);
            if (distance == 0u && !include_current) distance = catalog->count;
            if (distance < best) {
                best = distance; found.index = i;
                found.wrapped = direction == 1 ? i < start : i > start;
                if (i == start && !include_current) found.wrapped = 1u;
            }
        }
        entry = entry->next;
    }
    /* A malformed count/list must not produce a plausible but wrong cursor. */
    if (entry != 0) return OT_CATALOG_ARGUMENT;
    if (best > catalog->count) return OT_CATALOG_SEARCH_MISS;
    *result = found;
    return 0;
}

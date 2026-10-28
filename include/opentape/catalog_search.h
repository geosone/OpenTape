#ifndef OPENTAPE_CATALOG_SEARCH_H
#define OPENTAPE_CATALOG_SEARCH_H
#include "opentape/archive_catalog.h"
#define OT_CATALOG_QUERY_MAX 40u
#define OT_CATALOG_SEARCH_MISS 1
#define OT_CATALOG_SEARCH_EMPTY 2
struct ot_catalog_match { unsigned index, wrapped; };
/* Bounded literal substring; ASCII case-insensitive, OEM bytes compare exactly.
 * No wildcard interpretation, no I/O, and no mutation of catalogue or grants. */
int ot_catalog_query_valid(const char *);
/* direction is +1/-1. include_current chooses initial find vs next/previous.
 * Scan at most one full cycle, including current last when it is the only hit.
 * Only a match (return 0) updates result; all failures leave it untouched. */
int ot_catalog_search(const struct ot_catalog *, const char *, unsigned start,
                      int direction, unsigned include_current, struct ot_catalog_match *);
#endif

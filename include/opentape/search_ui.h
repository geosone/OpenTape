#ifndef OPENTAPE_SEARCH_UI_H
#define OPENTAPE_SEARCH_UI_H
#include "opentape/catalog_search.h"
#include "opentape/dos_ui.h"
/* Edit a caller-owned terminated query[OT_CATALOG_QUERY_MAX+1]. F2 applies,
 * Esc discards; no catalogue, mark, file, device or screen lifetime ownership. */
int ot_dos_catalog_search_ui(char *, ot_dos_ui_line);
#endif

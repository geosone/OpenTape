/* Read-only selection-list import for the DOS catalogue UI. The shared list
 * loader owns parsing, resource bounds, source recheck and checked close; only
 * a completely loaded list can reach the atomic in-memory mark application. */
#include <stdlib.h>
#include <string.h>
#include "opentape/archive_catalog.h"
#include "opentape/restore.h"
#include "opentape/source_file.h"

int ot_dos_catalog_import(struct ot_catalog *catalog, const char *name, unsigned *dos_error)
{
    struct ot_restore_selection *list;
    struct ot_dos_source source;
    int rc;
    if (dos_error != 0) *dos_error = 0u;
    if (catalog == 0 || catalog->error != 0 || catalog->count > OT_CATALOG_MAX_ENTRIES ||
        name == 0 || *name == 0) return OT_CATALOG_ARGUMENT;
    list = calloc(1u, sizeof(*list));
    if (list == 0) return OT_CATALOG_MEMORY;
    memset(&source, 0, sizeof(source));
    rc = ot_dos_source_open(&source, name, "IMPORT.LST");
    if (rc == 0) rc = ot_restore_selection_load(list, &source.ops);
    if (rc == 0) rc = ot_catalog_apply_selection(catalog, list);
    if (dos_error != 0) *dos_error = source.dos_error;
    ot_restore_selection_free(list); free(list);
    return rc;
}

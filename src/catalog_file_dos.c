/* Explicit regular-file snapshots. Validate before creating staging, and never
 * open a tape alias even for metadata import. The codec owns checked close. */
#include <string.h>
#include "opentape/catalog_file.h"
#include "opentape/local_name.h"
int ot_dos_catalog_file_save(const struct ot_catalog *catalog, const struct ot_volume_label *label,
                              const char *name, unsigned *error)
{
    struct ot_catalog_file_info info;
    struct ot_dos_file_stream output;
    int rc;
    if (error) *error = 0u;
    if (!ot_local_file_name_valid(name, 40u)) return OT_CATALOG_ARGUMENT;
    rc = ot_catalog_file_measure(catalog, label, &info);
    if (rc) return rc;
    rc = ot_dos_file_stream_open(&output, name);
    if (!rc) rc = ot_catalog_file_save(catalog, label, &output.ops);
    if (error) *error = output.file.dos_error;
    return output.result ? output.result : rc;
}
int ot_dos_catalog_file_load(struct ot_catalog *catalog, struct ot_volume_label *label,
                              const char *name, unsigned *error)
{
    struct ot_dos_source source;
    int rc;
    if (error) *error = 0u;
    if (catalog == 0 || label == 0 || !ot_local_file_name_valid(name, 40u)) return OT_CATALOG_ARGUMENT;
    memset(&source, 0, sizeof(source));
    rc = ot_dos_source_open(&source, name, "CATALOG.CAT");
    if (!rc) rc = ot_catalog_file_load(catalog, label, &source.ops);
    if (error) *error = source.dos_error;
    return rc;
}

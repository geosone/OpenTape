/* Profile I/O is explicit and independent of tape state. Import commits only
 * after exact EOF, source snapshot recheck, checked close and full decoding.
 * Export encodes before acquiring output, then uses guarded staged readback. */
#include <string.h>
#include "opentape/settings.h"
#include "opentape/restore.h"
int ot_dos_settings_load(const char *name, struct ot_settings *settings, unsigned *dos_error)
{
    struct ot_dos_source source;
    oa_u8 data[OT_SETTINGS_WIRE_SIZE], extra;
    unsigned used = 0u, got, error;
    int rc;
    if (dos_error != 0) *dos_error = 0u;
    if (settings == 0 || !ot_settings_filename_valid(name)) return OT_SETTINGS_ARGUMENT;
    memset(&source, 0, sizeof(source));
    rc = ot_dos_source_open(&source, name, "PROFILE.CFG");
    if (rc == 0 && source.metadata.file_size != (oa_u32)sizeof(data)) rc = OT_SETTINGS_FORMAT;
    while (rc == 0 && used < sizeof(data)) {
        error = source.ops.read(&source, data + used, sizeof(data) - used, &got);
        if (error != 0u) rc = OT_SOURCE_IO;
        else if (got == 0u || got > sizeof(data) - used) rc = OT_SOURCE_CONTENT;
        else used += got;
    }
    if (rc == 0) {
        error = source.ops.read(&source, &extra, 1u, &got);
        if (error != 0u) rc = OT_SOURCE_IO;
        else if (got != 0u) rc = OT_SOURCE_CONTENT;
    }
    if (rc == 0 && source.ops.recheck(&source) != 0u) rc = OT_SOURCE_CHANGED;
    if (source.opened) {
        error = source.ops.close(&source);
        if (rc == 0 && error != 0u) rc = OT_SOURCE_IO;
    }
    if (dos_error != 0) *dos_error = source.dos_error;
    return rc != 0 ? rc : ot_settings_decode(settings, data, sizeof(data));
}
int ot_dos_settings_save(const char *name, const struct ot_settings *settings, unsigned *dos_error)
{
    struct ot_dos_selection_export output;
    oa_u8 data[OT_SETTINGS_WIRE_SIZE];
    int rc;
    if (dos_error != 0) *dos_error = 0u;
    if (!ot_settings_filename_valid(name)) return OT_SETTINGS_ARGUMENT;
    rc = ot_settings_encode(settings, data, sizeof(data));
    if (rc != 0) return rc;
    rc = ot_dos_selection_prepare(&output, name);
    if (rc == 0) rc = ot_dos_file_save_blob(&output, data, sizeof(data));
    if (dos_error != 0) *dos_error = output.dos_error;
    return rc;
}

/* Explicit batch interface to the same offline codec and DOS file guards as
 * F8/F9. A metadata file never grants restore or associates a mounted tape.
 * Live entries are withheld until verification and checked tape close succeed. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/catalog_cli.h"
#include "opentape/catalog_file.h"
#include "opentape/local_name.h"
#include "opentape/archive_dos.h"
#include "opentape/messages.h"

int ot_dos_list_entry(oa_u32 session, oa_u32 object, const struct ot_file_metadata *m)
{
    unsigned year = m->dos_date != 0u ? 1980u + (m->dos_date >> 9) : 0u;
    return printf("%c %10lu %02X %04u-%02u-%02u %02u:%02u:%02u %lu:%lu %s\n",
        m->kind == OT_FILE_DIRECTORY ? 'D' : 'F', (unsigned long)m->file_size,
        (unsigned)m->dos_attributes, year, (unsigned)((m->dos_date >> 5) & 15u),
        (unsigned)(m->dos_date & 31u), (unsigned)(m->dos_time >> 11),
        (unsigned)((m->dos_time >> 5) & 63u), (unsigned)((m->dos_time & 31u) * 2u),
        (unsigned long)session, (unsigned long)object, m->path) < 0 ? -1 : 0;
}
static int print_catalog(const struct ot_catalog *catalog, unsigned offline)
{
    const struct ot_catalog_entry *entry;
    if (puts(offline ? OT_MSG_CATCLI_OFFLINE : OT_MSG_CATCLI_VERIFIED) == EOF ||
        puts(OT_MSG_CATCLI_COLUMNS) == EOF) return OT_CATALOG_FILE_IO;
    for (entry = catalog->first; entry != 0; entry = entry->next)
        if (ot_dos_list_entry(entry->session, entry->object, &entry->metadata)) return OT_CATALOG_FILE_IO;
    if (printf(OT_MSG_CATCLI_COUNT, catalog->count) < 0 || fflush(stdout) || ferror(stdout)) return OT_CATALOG_FILE_IO;
    return 0;
}
int ot_dos_catalog_list(const char *input, const char *output, const char *device)
{
    struct ot_catalog catalog;
    struct ot_volume_label label;
    struct ot_verify_session_result verification;
    struct ot_dos_selection_export prepared;
    oa_u8 *buffer = 0;
    unsigned error = 0u;
    int rc;
    const char *name = input != 0 ? input : output;
    if ((input == 0) == (output == 0) || !ot_local_file_name_valid(name, 40u) ||
        !ot_probe_raw_name_valid(device)) return 1;
    memset(&catalog, 0, sizeof(catalog));
    memset(&label, 0, sizeof(label));
    memset(&verification, 0, sizeof(verification));
    if (input != 0) rc = ot_dos_catalog_file_load(&catalog, &label, input, &error);
    else {
        /* Read-only preflight refuses existing targets/staging BEFORE tape I/O.
         * Save rechecks these conditions after verification; no handle is held. */
        rc = ot_dos_selection_prepare(&prepared, output); error = prepared.dos_error;
        if (rc == 0) {
            buffer = malloc(OT_BLOCK_RECOMMENDED_SIZE);
            if (buffer == 0) { rc = OT_CATALOG_MEMORY; error = 8u; }
        }
        if (rc == 0 && (puts(OT_MSG_VERIFY_ALL_WARNING) == EOF || fflush(stdout) || ferror(stdout)))
            rc = OT_CATALOG_FILE_IO;
        if (rc == 0) {
            rc = ot_dos_archive_verify(device, buffer, OT_BLOCK_RECOMMENDED_SIZE,
                ot_catalog_capture, &catalog, &verification);
            error = verification.reader.dos_error;
            if (rc == OT_VERIFY_VISITOR_FAILED && catalog.error) rc = catalog.error;
            if (rc == 0) label = verification.verify.framing.label;
        }
    }
    free(buffer);
    if (rc == 0) rc = print_catalog(&catalog, input != 0);
    if (rc == 0 && output != 0) {
        rc = ot_dos_catalog_file_save(&catalog, &label, output, &error);
        if (rc == 0 && (printf(OT_MSG_CATCLI_SAVED, output) < 0 || fflush(stdout) || ferror(stdout)))
            rc = OT_CATALOG_FILE_IO;
    }
    ot_catalog_free(&catalog);
    if (rc != 0) fprintf(stderr, OT_MSG_CATCLI_FAILED, name, rc, error);
    return rc == 0 ? 0 : 2;
}

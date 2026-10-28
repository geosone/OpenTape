/* Memory-only archive fixture: the caller has just backed up DATA/INPUT.BIN.
 * Exercise the quiet UI services with separate extraction/replacement lists. The shell fixture checks exact ASPI counts and disk bytes. */
#include <stdio.h>
#include <string.h>
#include "opentape/archive_dos.h"
#include "opentape/archive_catalog.h"
#include "opentape/restore.h"

static unsigned notifications, observation_failed, partial;
static oa_u32 checked;
static void observed(void *context, const struct ot_restore *state)
{
    unsigned *calls = context; ++*calls;
    if (!state->busy || state->finished || state->failed || state->checked_high ||
        state->checked_low < checked || state->checked_low > 35000ul || state->active_bytes > state->active.file_size)
        observation_failed = 1u;
    checked = state->checked_low;
    if (checked > 0ul && checked < 35000ul && state->restored_files == 0ul) partial = 1u;
}
static int observed_arguments(void)
{
    struct ot_restore_selection selected, replacement;
    struct ot_dos_restore_result result;
    memset(&selected, 0, sizeof(selected)); memset(&replacement, 0, sizeof(replacement));
    selected.count = replacement.count = 1u;
    selected.paths[0] = replacement.paths[0] = "DATA/INPUT.BIN";
    if (ot_dos_selected_observed("NRMT0", "RECOVER", &selected, 0, 99u, observed, &notifications, &result) != OT_RESTORE_PATH) return 51;
    if (ot_dos_selected_observed("NRMT0", "RECOVER", &selected, &replacement, OT_DOS_FILE_COMPARE, observed, &notifications, &result) != OT_RESTORE_PATH) return 52;
    replacement.paths[0] = "DATA/OTHER.BIN";
    if (ot_dos_selected_observed("NRMT0", "RECOVER", &selected, &replacement, OT_DOS_FILE_RESTORE, observed, &notifications, &result) != OT_RESTORE_PATH) return 53;
    return notifications ? 54 : 0;
}

/* Writable cases use separate new roots. Even invalid/unmatched selections
 * must report their actual side effects; the shell checks resulting bytes. */
static int writable(void)
{
    struct ot_restore_selection selected;
    struct ot_dos_restore_result result;
    int rc;
    memset(&selected, 0, sizeof(selected));
    if (ot_dos_restore_selected("NRMT0", "RECOVER", &selected, &result) != OT_RESTORE_PATH) return 11;
    selected.count = 1u; selected.paths[0] = "DATA/INPUT.BIN";
    if (ot_dos_restore_selected("NRMT0", "RECOVER", &selected, 0) != OT_RESTORE_PATH) return 12;
    if (ot_dos_restore_selected("INPUT.BIN", "RECOVER", &selected, &result) != OT_RESTORE_PATH) return 13;
    if (ot_dos_restore_selected("NRMT0", "INPUT.BIN", &selected, &result) == 0 || result.destination_created) return 14;
    if (ot_dos_restore_selected("NRMT0", "MISSING/CHILD", &selected, &result) == 0 || result.destination_created) return 15;
    selected.count = 2u; selected.paths[1] = "data/input.bin";
    if (ot_dos_restore_selected("NRMT0", "RECOVER", &selected, &result) != OT_RESTORE_PATH) return 16;
    selected.count = 1u; selected.paths[0] = "DATA/ABSENT.BIN";
    rc = ot_dos_restore_selected("NRMT0", "BADPLAN", &selected, &result);
    if (rc != OT_RESTORE_PATH || result.result != rc || result.unmatched != 1u ||
        result.files != 0ul || result.excluded != 1ul || result.destination_created != 1u || result.readback_files != 0u)
        return 17;
    selected.paths[0] = "DATA/INPUT.BIN";
    rc = ot_dos_selected_observed("NRMT0", "RECOVER", &selected, 0, OT_DOS_FILE_RESTORE,
                                  observed, &notifications, &result);
    if (rc != 0 || result.result != 0 || result.files != 1ul || result.readback_files != 1u ||
        result.skipped != 0ul || result.excluded != 0ul || result.unmatched != 0u ||
        result.destination_created != 1u || result.tape_error != 0u || result.disk_error != 0u ||
        result.readback_error != 0 || result.recovery_error != 0u) return 18;
    if (ot_dos_restore_selected("NRMT0", "RECOVER", &selected, &result) == 0 || result.destination_created) return 19;
    if (notifications != 7u || observation_failed || !partial || checked != 35000ul) return 55;
    puts("Quiet selected writable restore passed");
    return 0;
}

/* Static lists keep the large-model DOS test stack available to the real
 * restore/readback call chain, rather than two 256-pointer test fixtures. */
static int replacing(void)
{
    static struct ot_restore_selection selected, replacement;
    struct ot_dos_restore_result result;
    int rc;
    memset(&selected, 0, sizeof(selected)); memset(&replacement, 0, sizeof(replacement));
    replacement.count = 1u; replacement.paths[0] = "DATA/INPUT.BIN";
    if (ot_dos_restore_replacing("NRMT0", "RECOVER", &selected, &replacement, &result) != OT_RESTORE_PATH) return 21;
    selected.count = 1u; selected.paths[0] = "DATA/INPUT.BIN";
    replacement.paths[0] = "DATA/OTHER.BIN";
    if (ot_dos_restore_replacing("NRMT0", "RECOVER", &selected, &replacement, &result) != OT_RESTORE_PATH ||
        result.output_prepared || result.destination_created) return 22;
    if (ot_dos_preview_replacing("NRMT0", "RECOVER", &selected, &replacement, &result) != OT_RESTORE_PATH) return 23;
    replacement.paths[0] = "data/input.bin"; replacement.paths[1] = "DATA/INPUT.BIN"; replacement.count = 2u;
    if (ot_dos_restore_replacing("NRMT0", "RECOVER", &selected, &replacement, &result) != OT_RESTORE_PATH) return 24;
    replacement.count = 0u;
    if (ot_dos_restore_replacing("NRMT0", "RECOVER", &selected, &replacement, &result) == 0) return 25;
    replacement.count = 1u;
    if (ot_dos_restore_replacing("NRMT0", "RECOVER", &selected, &replacement, 0) != OT_RESTORE_PATH) return 26;
    if (ot_dos_restore_replacing("INPUT.BIN", "RECOVER", &selected, &replacement, &result) != OT_RESTORE_PATH) return 27;
    rc = ot_dos_preview_replacing("NRMT0", "RECOVER", &selected, &replacement, &result);
    if (rc != 0 || result.result != rc || result.files != 1ul || result.replaced != 1u ||
        result.skipped != 0ul || result.excluded != 0ul || result.output_prepared || result.destination_created || result.readback_files)
        return 28;
    rc = ot_dos_restore_replacing("NRMT0", "RECOVER", &selected, &replacement, &result);
    if (rc != 0 || result.result != rc || result.files != 1ul || result.replaced != 1u ||
        result.readback_files != 1u || result.output_prepared != 1u || result.destination_created != 0u ||
        result.skipped != 0ul || result.excluded != 0ul || result.tape_error || result.disk_error || result.recovery_error)
        return 29;
    rc = ot_dos_restore_replacing("NRMT0", "FRESH", &selected, &replacement, &result);
    if (rc != 0 || result.result != rc || result.files != 1ul || result.replaced != 0u ||
        result.readback_files != 1u || result.output_prepared != 1u || result.destination_created != 1u)
        return 30;
    puts("Quiet per-file replacement preview/restore passed");
    return 0;
}

/* Both archive and destination are read-only. A missing selected path fails
 * after complete archive verification; invalid input/root fails before rewind. */
static int comparing(void)
{
    struct ot_restore_selection selected;
    struct ot_dos_restore_result result;
    int rc;
    memset(&selected, 0, sizeof(selected));
    if (ot_dos_compare_selected("NRMT0", "RECOVER", &selected, &result) != OT_RESTORE_PATH) return 31;
    selected.count = 1u; selected.paths[0] = "DATA/INPUT.BIN";
    if (ot_dos_compare_selected("NRMT0", "RECOVER", &selected, 0) != OT_RESTORE_PATH) return 32;
    if (ot_dos_compare_selected("INPUT.BIN", "RECOVER", &selected, &result) != OT_RESTORE_PATH) return 33;
    if (ot_dos_compare_selected("NRMT0", "INPUT.BIN", &selected, &result) == 0 || result.output_prepared) return 34;
    if (ot_dos_compare_selected("NRMT0", "MISSING", &selected, &result) == 0 || result.destination_created) return 35;
    selected.paths[1] = "data/input.bin"; selected.count = 2u;
    if (ot_dos_compare_selected("NRMT0", "RECOVER", &selected, &result) != OT_RESTORE_PATH) return 36;
    selected.count = 1u; selected.paths[0] = "DATA/ABSENT.BIN";
    rc = ot_dos_compare_selected("NRMT0", "RECOVER", &selected, &result);
    if (rc != OT_RESTORE_PATH || result.result != rc || result.unmatched != 1u ||
        result.files != 0ul || result.excluded != 1ul || result.readback_files != 0u ||
        result.output_prepared || result.destination_created || result.replaced) return 37;
    selected.paths[0] = "DATA/INPUT.BIN";
    rc = ot_dos_compare_selected("NRMT0", "RECOVER", &selected, &result);
    if (rc != 0 || result.result != 0 || result.files != 1ul || result.readback_files != 1u ||
        result.excluded != 0ul || result.unmatched != 0u || result.tape_error || result.disk_error ||
        result.readback_error || result.output_prepared || result.destination_created || result.replaced) return 38;
    puts("Quiet selected content comparison passed");
    return 0;
}

/* An offline catalogue exercises disk input and atomic resolution without
 * acquiring tape. The shell checks exact ASPI counts and full image identity. */
static int importing(void)
{
    struct ot_catalog catalog;
    struct ot_file_metadata metadata;
    unsigned error, i;
    const char *bad[] = { "UNKNOWN.LST", "BAD.LST", "DUP.LST", "EMPTY.LST", "DIR.LST", "NONE.LST", "MT0" };
    memset(&catalog, 0, sizeof(catalog)); memset(&metadata, 0, sizeof(metadata));
    if (ot_dos_catalog_import(0, "VALID.LST", &error) != OT_CATALOG_ARGUMENT || error) return 41;
    metadata.kind = OT_FILE_REGULAR; strcpy(metadata.path, "DATA/INPUT.BIN");
    if (ot_catalog_capture(&catalog, 1ul, 1ul, &metadata) != 0) return 42;
    strcpy(metadata.path, "DATA/NOTE.TXT");
    if (ot_catalog_capture(&catalog, 1ul, 2ul, &metadata) != 0) return 43;
    metadata.kind = OT_FILE_DIRECTORY; strcpy(metadata.path, "DATA");
    if (ot_catalog_capture(&catalog, 1ul, 3ul, &metadata) != 0) return 44;
    if (ot_catalog_mark(&catalog, 0u, 1u) != 0 || ot_catalog_replace(&catalog, 0u, 1u) != 0) return 45;
    if (ot_dos_catalog_import(&catalog, 0, &error) != OT_CATALOG_ARGUMENT || error) return 46;
    for (i = 0u; i < sizeof(bad) / sizeof(bad[0]); ++i) {
        if (ot_dos_catalog_import(&catalog, bad[i], &error) == 0 || catalog.marked != 1u ||
            catalog.replacements != 1u || !ot_catalog_at(&catalog, 0u)->marked ||
            !ot_catalog_at(&catalog, 0u)->replace || ot_catalog_at(&catalog, 1u)->marked) return 47;
    }
    if (ot_dos_catalog_import(&catalog, "VALID.LST", &error) != 0 || error ||
        catalog.marked != 1u || catalog.replacements != 0u || ot_catalog_at(&catalog, 0u)->marked ||
        !ot_catalog_at(&catalog, 1u)->marked) return 48;
    metadata.kind = OT_FILE_REGULAR; strcpy(metadata.path, "data/note.txt");
    if (ot_catalog_capture(&catalog, 2ul, 1ul, &metadata) != 0 || ot_catalog_replace(&catalog, 1u, 1u) != 0) return 49;
    if (ot_dos_catalog_import(&catalog, "VALID.LST", &error) != OT_CATALOG_AMBIGUOUS ||
        catalog.marked != 1u || catalog.replacements != 1u || !ot_catalog_at(&catalog, 1u)->replace) return 50;
    ot_catalog_free(&catalog);
    puts("Read-only atomic catalogue selection import passed");
    return 0;
}

int main(int argc, char **argv)
{
    struct ot_restore_selection selected;
    struct ot_dos_restore_result result;
    int rc;
#ifdef OT_NATIVE_TAPE_API
    ot_dos_archive_select_native();
#endif
    rc = observed_arguments(); if (rc != 0) return rc;
    if (argc == 2 && strcmp(argv[1], "--restore") == 0) return writable();
    if (argc == 2 && strcmp(argv[1], "--replace") == 0) return replacing();
    if (argc == 2 && strcmp(argv[1], "--compare") == 0) return comparing();
    if (argc == 2 && strcmp(argv[1], "--import") == 0) return importing();
    if (argc != 1) return 20;
    memset(&selected, 0, sizeof(selected));
    memset(&result, 0x55, sizeof(result));
    rc = ot_dos_preview_selected("NRMT0", "PREVIEW", &selected, &result);
    if (rc != OT_RESTORE_PATH || result.result != rc || result.files != 0ul ||
        result.skipped != 0ul || result.unmatched != 0u) return 1;
    selected.paths[0] = "DATA/INPUT.BIN"; selected.count = 1u;
    if (ot_dos_preview_selected("NRMT0", "PREVIEW", &selected, 0) != OT_RESTORE_PATH) return 2;
    if (ot_dos_preview_selected("INPUT.BIN", "PREVIEW", &selected, &result) != OT_RESTORE_PATH) return 3;
    if (ot_dos_preview_selected("NRMT0", "INPUT.BIN", &selected, &result) == 0) return 4;
    if (ot_dos_preview_selected("NRMT0", "MISSING/CHILD", &selected, &result) == 0) return 5;
    selected.paths[1] = "data/input.bin"; selected.count = 2u;
    if (ot_dos_preview_selected("NRMT0", "PREVIEW", &selected, &result) != OT_RESTORE_PATH) return 6;
    selected.count = 1u; selected.paths[0] = "DATA/ABSENT.BIN";
    rc = ot_dos_preview_selected("NRMT0", "PREVIEW", &selected, &result);
    if (rc != OT_RESTORE_PATH || result.result != rc || result.unmatched != 1u ||
        result.files != 0ul || result.excluded != 1ul) return 7;
    selected.paths[0] = "DATA/INPUT.BIN";
    rc = ot_dos_preview_selected("NRMT0", "PREVIEW", &selected, &result);
    if (rc != 0 || result.result != 0 || result.files != 1ul || result.skipped != 0ul ||
        result.excluded != 0ul || result.unmatched != 0u || result.tape_error != 0u || result.disk_error != 0u)
        return 8;
    puts("Quiet selected restore preview passed");
    return 0;
}

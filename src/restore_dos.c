#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/restore.h"
#include "opentape/tape_reader.h"
#include "opentape/messages.h"
#include "opentape/archive_dos.h"
#include "opentape/source_file.h"
static int ot_dos_restore(const char *device, const char *destination, const char *const *selection, unsigned count,
                          const struct ot_restore_selection *filter, int preview, int comparing)
{
    struct ot_dos_restore disk;
    struct ot_tape_reader reader;
    struct ot_restore restore;
    oa_u8 *buffer;
    unsigned bytes, i;
    int rc, close_rc;
    if (!ot_probe_raw_name_valid(device) ||
        !ot_restore_selection_valid(selection, count)) return 1;
    memset(&disk, 0, sizeof(disk)); memset(&reader, 0, sizeof(reader)); memset(&restore, 0, sizeof(restore));
    buffer = malloc(OT_BLOCK_RECOMMENDED_SIZE);
    if (buffer == 0) { puts(OT_MSG_READ_MEMORY); return 2; }
    rc = comparing ? ot_dos_restore_compare(&disk, destination) :
         preview ? ot_dos_restore_preview(&disk, destination, selection, count) :
                   ot_dos_restore_open(&disk, destination, selection, count);
    if (rc != 0) goto done;
    rc = ot_restore_init(&restore, &disk.ops);
    if (rc != 0) goto done;
    if (filter != 0) {
        rc = ot_restore_set_filter(&restore, filter->paths, filter->count);
        if (rc != 0) goto done;
    }
    rc = ot_dos_archive_reader_open(&reader, device);
    if (rc != 0) goto done;
    if ((reader.status.tape.device_parameters & OA_MT_PARAM_FIXED_BLOCK) != 0u) { rc = OT_READ_BAD_MODE; goto done; }
    if (puts(comparing ? OT_MSG_COMPARE_WARNING : preview ? OT_MSG_PREVIEW_WARNING : OT_MSG_RESTORE_WARNING) == EOF) {
        rc = OT_RESTORE_IO; goto done;
    }
    for (;;) {
        rc = ot_reader_read(&reader, buffer, OT_BLOCK_RECOMMENDED_SIZE, &bytes);
        if (rc == OT_READ_FILEMARK) {
            rc = ot_restore_finish(&restore);
            if (filter != 0) for (i = 0u; i < filter->count; ++i)
                if (!restore.filter_seen[i]) printf(OT_MSG_RESTORE_UNMATCHED, filter->paths[i]);
            if (rc == 0) for (i = 0u; i < count; ++i)
                if (disk.selection_seen[i] == 0u) {
                    printf(OT_MSG_RESTORE_UNMATCHED, selection[i]); rc = OT_RESTORE_PATH;
                }
            break;
        }
        if (rc != OT_READ_BLOCK) break;
        rc = ot_restore_block(&restore, buffer, (oa_u16)bytes);
        if (rc != 0) break;
    }
done:
    if (reader.opened) {
        close_rc = ot_reader_close(&reader);
        if (rc == 0) rc = close_rc;
    }
    ot_restore_free(&restore); free(buffer);
    if (rc != 0) {
        printf(comparing ? OT_MSG_COMPARE_ERROR : OT_MSG_RESTORE_ERROR, rc, reader.dos_error, disk.dos_error);
        if (comparing) puts(OT_MSG_COMPARE_INCOMPLETE);
        else if (preview) puts(OT_MSG_PREVIEW_INCOMPLETE);
        else if (disk.created) puts(OT_MSG_RESTORE_PARTIAL);
        if (disk.readback_error != 0) printf(comparing ? OT_MSG_COMPARE_FILE_ERROR : OT_MSG_RESTORE_READBACK_ERROR,
                                            disk.readback_error, disk.checked_path);
        if (disk.recovery_error != 0u) printf(OT_MSG_RESTORE_RECOVERY, disk.recovery_error);
        return 2;
    }
    if (!preview && !comparing) printf(OT_MSG_RESTORE_OK, (unsigned long)restore.restored_files,
                         (unsigned long)restore.restored_directories);
    if (!comparing) printf(preview ? OT_MSG_PREVIEW_SELECTION : OT_MSG_RESTORE_SELECTION,
                           disk.replaced, (unsigned long)restore.skipped_files);
    if (filter != 0) printf(OT_MSG_RESTORE_EXCLUDED, (unsigned long)restore.excluded_files);
    if (!preview && !comparing) printf(OT_MSG_RESTORE_READBACK_OK, disk.readback_files);
    if (fflush(stdout) != 0 || ferror(stdout)) {
        if (comparing) fputs(OT_MSG_COMPARE_INCOMPLETE "\n", stderr);
        else if (preview) fputs(OT_MSG_PREVIEW_INCOMPLETE "\n", stderr);
        return 2;
    }
    if (preview || comparing) {
        printf(comparing ? OT_MSG_COMPARE_OK : OT_MSG_PREVIEW_OK, (unsigned long)restore.restored_files,
               (unsigned long)restore.restored_directories);
        if (fflush(stdout) != 0 || ferror(stdout)) {
            fputs(comparing ? OT_MSG_COMPARE_INCOMPLETE "\n" : OT_MSG_PREVIEW_INCOMPLETE "\n", stderr); return 2;
        }
    }
    return 0;
}

int ot_dos_restore_command(int argc, char **argv)
{
    struct ot_restore_selection *selection, *filter, *list;
    struct ot_dos_source source;
    int i, rc = 0, filtered = 0, preview = 0, comparing;
    unsigned j;
    if (argc < 6 || strcmp(argv[2], "--device") != 0 || strcmp(argv[4], "--to") != 0 ||
        !ot_probe_raw_name_valid(argv[3]))
        return 1;
    comparing = strcmp(argv[1], "--compare") == 0;
    for (i = 6; i < argc;) {
        if (!comparing && strcmp(argv[i], "--dry-run") == 0 && !preview) { preview = 1; ++i; }
        else if (i + 1 < argc && ((!comparing && (strcmp(argv[i], "--replace") == 0 || strcmp(argv[i], "--replace-list") == 0)) ||
                 strcmp(argv[i], "--only") == 0 || strcmp(argv[i], "--only-list") == 0)) i += 2;
        else return 1;
    }
    selection = calloc(1u, sizeof(*selection));
    filter = calloc(1u, sizeof(*filter));
    if (selection == 0 || filter == 0) {
        free(selection); free(filter); puts(OT_MSG_READ_MEMORY); return 2;
    }
    for (i = 6; i < argc && rc == 0;) {
        if (strcmp(argv[i], "--dry-run") == 0) { ++i; continue; }
        list = selection;
        if (strcmp(argv[i], "--only") == 0 || strcmp(argv[i], "--only-list") == 0) {
            list = filter; filtered = 1;
        }
        if (strcmp(argv[i], "--replace") == 0 || strcmp(argv[i], "--only") == 0)
            rc = ot_restore_selection_add(list, argv[i + 1]);
        else {
            memset(&source, 0, sizeof(source));
            rc = ot_dos_source_open(&source, argv[i + 1], "REPLACE.LST");
            if (rc == 0) rc = ot_restore_selection_load(list, &source.ops);
            if (rc != 0) printf(OT_MSG_RESTORE_LIST_ERROR, argv[i + 1], rc, source.dos_error);
        }
        i += 2;
    }
    if (rc == 0 && filtered) {
        if (filter->count == 0u) rc = OT_RESTORE_PATH;
        for (j = 0u; j < selection->count && rc == 0; ++j)
            if (!ot_restore_selected(filter->paths, filter->count, selection->paths[j])) rc = OT_RESTORE_PATH;
        if (rc != 0) puts(OT_MSG_RESTORE_FILTER_ERROR);
    }
    if (rc == 0) rc = ot_dos_restore(argv[3], argv[5], selection->paths, selection->count,
                                    filtered ? filter : 0, preview, comparing);
    else rc = 1;
    ot_restore_selection_free(selection); free(selection);
    ot_restore_selection_free(filter); free(filter);
    return rc;
}

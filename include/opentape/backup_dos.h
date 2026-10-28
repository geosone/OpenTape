#ifndef OPENTAPE_BACKUP_DOS_H
#define OPENTAPE_BACKUP_DOS_H
#include "opentape/tape_probe.h"
#include "opentape/source_file.h"
#include "opentape/backup_progress.h"

/* Borrowed immutable source configuration for one synchronous call. UI startup
 * or the saved settings form supplies it; it contains no tape-write grant. */
#define OT_BACKUP_FILE 0u
#define OT_BACKUP_TREE 1u
#define OT_BACKUP_SET 2u
/* Historical tree member now holds FILE/TREE/SET mode; layout is unchanged. */
struct ot_dos_backup_request {
    const char *source, *path, *volume;
    unsigned tree;
};
struct ot_dos_backup_result {
    int result;
    unsigned source_error, tape_error, output_opened, finished;
    oa_u32 files, directories, bytes_low, bytes_high;
    struct ot_tape_status status;
    char source_name[128];
};
int ot_dos_backup_request_valid(const struct ot_dos_backup_request *);
/* Explicit authorization == 1 is required on EVERY call, before file/device
 * I/O. Plan/open the source before acquiring tape, then use the existing archive
 * writer and selected native/legacy/fixed backend at CURRENT position. No
 * rewind, retries or readback. Close once and preserve the first error.
 * output_opened means output was acquired, not that a complete session exists;
 * counts are provisional unless result==0 and finished. Success still requires
 * a separate readback qualification before relying on the backup. No console I/O. */
int ot_dos_backup_execute(const struct ot_dos_backup_request *, const char *device,
                          unsigned authorized, struct ot_dos_backup_result *);
/* Same one-shot API, with optional observation after complete source planning
 * and accepted records. Observer must not reenter or issue file/device I/O. No completion
 * event: only the returned result after cleanup establishes batch completion. */
int ot_dos_backup_execute_observed(const struct ot_dos_backup_request *, const char *, unsigned,
                                   ot_backup_observer, void *, struct ot_dos_backup_result *);
int ot_dos_backup(const char *, const char *, const char *, const char *, int);
#endif

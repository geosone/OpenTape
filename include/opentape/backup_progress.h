#ifndef OPENTAPE_BACKUP_PROGRESS_H
#define OPENTAPE_BACKUP_PROGRESS_H
#include "opentape/backup_plan.h"
enum ot_backup_phase { OT_BACKUP_PLANNED, OT_BACKUP_STREAMING, OT_BACKUP_FINALIZING };
struct ot_backup_progress {
    struct ot_plan_summary total;
    oa_u32 files, directories, accepted_low, accepted_high;
    oa_u32 file_bytes, file_size;
    unsigned phase;
    char path[OT_FILE_PATH_MAX + 1u];
};
/* Observation only: borrowed snapshot during callback, no reentry, file/device I/O or mutation.
 * Accepted bytes are provisional; FILE_END requires source recheck/close, but
 * no notification establishes successful sink finish, output close or readback. */
typedef void (*ot_backup_observer)(void *, const struct ot_backup_progress *);
struct ot_backup_tracking {
    struct ot_backup_progress progress;
    ot_backup_observer observer;
    void *context;
};
int ot_backup_tracking_init(struct ot_backup_tracking *, const struct ot_plan_summary *, ot_backup_observer, void *);
/* Writer observer adapter. Input records have already passed verification and
 * exact sink acceptance. No extra payload CRC pass or transport operation. */
void ot_backup_tracking_record(void *, const struct ot_archive_verify *, const struct ot_record_header *, const oa_u8 *);
#endif

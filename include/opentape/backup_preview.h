#ifndef OPENTAPE_BACKUP_PREVIEW_H
#define OPENTAPE_BACKUP_PREVIEW_H
#include "opentape/backup_dos.h"
#include "opentape/backup_plan.h"
#include "opentape/dos_ui.h"
struct ot_backup_preview_result {
    int result;
    unsigned source_error;
    struct ot_plan_summary summary;
};
/* Quiet read-only source metadata inspection. No tape calls, output handles,
 * payload reads, writes or grant. Counts are published only after complete
 * planning/checked source closes; the later backup must build its own fresh plan. */
int ot_dos_backup_preview(const struct ot_dos_backup_request *, struct ot_backup_preview_result *);
/* Caller owns initialized QUILIB. Result dismissal is consumed. */
void ot_dos_backup_preview_ui(const struct ot_dos_backup_request *, ot_dos_ui_line);
#endif

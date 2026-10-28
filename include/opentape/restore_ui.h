#ifndef OPENTAPE_RESTORE_UI_H
#define OPENTAPE_RESTORE_UI_H
#include "opentape/restore.h"
#include "opentape/dos_ui.h"
/* Caller-owned for one synchronous selected batch. Target/selection count are
 * copied from its frozen review, not inferred from a previously seen tape. */
struct ot_restore_progress_view {
    const char *device, *destination;
    unsigned action, selected, initialized;
    ot_dos_ui_line line;
};
void ot_dos_restore_progress(void *, const struct ot_restore *);
#endif

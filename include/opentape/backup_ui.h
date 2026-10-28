#ifndef OPENTAPE_BACKUP_UI_H
#define OPENTAPE_BACKUP_UI_H
#include "opentape/backup_form.h"
#include "opentape/dos_ui.h"
/* Caller owns initialized QUILIB input/screen. One confirmation is consumed,
 * one authorized batch can run, then any result key returns. No sticky grant.
 * Returns 1 after a confirmed attempt, 0 after cancellation/disabled action. */
int ot_dos_backup_ui(const char *, const struct ot_dos_backup_request *, ot_dos_ui_line);
/* Local draft, F2 save or Esc discard; caller owns QUILIB lifetime. */
int ot_dos_backup_form_ui(struct ot_backup_form *, ot_dos_ui_line);
#endif

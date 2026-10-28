#ifndef OPENTAPE_SETTINGS_UI_H
#define OPENTAPE_SETTINGS_UI_H
#include "opentape/settings.h"
#include "opentape/dos_ui.h"
/* Device-screen profile action. Filename editing, separate one-shot confirm,
 * operation and consumed result dismissal share QUILIB's normal lifecycle.
 * Only a successful import replaces both settings groups. No tape actions. */
void ot_dos_settings_ui(struct ot_backup_form *, struct ot_paths_form *, char *filename,
                         unsigned exporting, ot_dos_ui_line);
#endif

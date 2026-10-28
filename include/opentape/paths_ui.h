#ifndef OPENTAPE_PATHS_UI_H
#define OPENTAPE_PATHS_UI_H
#include "opentape/paths_form.h"
#include "opentape/dos_ui.h"
/* Caller owns QUILIB. Returns 1 after complete valid save, 0 after discard. */
int ot_dos_paths_form_ui(struct ot_paths_form *, ot_dos_ui_line);
#endif

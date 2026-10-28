#ifndef OPENTAPE_NATIVE_UI_H
#define OPENTAPE_NATIVE_UI_H
/* DOS controls reuse the application's QUILIB windows and colors. The callback
 * keeps window-layout details in main.c; panel 0 is data, panel 1 is help. */
#include "opentape/dos_ui.h"
typedef ot_dos_ui_line ot_native_ui_line;
/* The caller owns initialized QUILIB input/screen setup. ESC returns without
 * ending that lifecycle; snapshots/catalogue state are invalidated by main. */
void ot_dos_native_controls_ui(const char *,ot_native_ui_line);
#endif

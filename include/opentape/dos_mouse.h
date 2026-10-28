#ifndef OPENTAPE_DOS_MOUSE_H
#define OPENTAPE_DOS_MOUSE_H
#include <input.h>
#include "opentape/dos_ui.h"
#define OT_UI_MOUSE_ROW 0xE000u
#define OT_UI_MOUSE_MARK 0xE100u
void ot_dos_mouse_clear(void);
void ot_dos_mouse_bind(unsigned panel, int row, int offset, unsigned width, unsigned key);
void ot_dos_ui_action(ot_dos_ui_line, unsigned panel, int row, const char *, unsigned char, unsigned key);
void ot_dos_mouse_pair(unsigned panel, int row, const char *, unsigned first, unsigned second);
/* QUILIB handles INT 33h, cursor lifetime and coordinates; no private driver. */
int ot_dos_mouse_input(INPUTEVENT *);
#endif

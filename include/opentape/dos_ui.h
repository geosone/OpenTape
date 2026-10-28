#ifndef OPENTAPE_DOS_UI_H
#define OPENTAPE_DOS_UI_H
/* Reuse the main QUILIB renderer: panel 0 is data, panel 1 is quick help. */
typedef void (*ot_dos_ui_line)(unsigned panel, int row, const char *text, unsigned char color);
#endif

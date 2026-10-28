/* Translate existing QUILIB mouse events into the same commands as keyboard
 * input. The application registers only visible targets of the current view. */
#include <string.h>
#include <ui.h>
#include "opentape/dos_mouse.h"
#include "opentape/ui_mouse.h"
static struct ot_mouse_map targets;
void ot_dos_mouse_clear(void) { ot_mouse_clear(&targets); }
void ot_dos_mouse_bind(unsigned panel, int row, int offset, unsigned width, unsigned key)
{
    WINDESC *window = panel ? &QuickWin : &DataWin;
    int available = window->ncols - 4 - offset;
    if (offset < 0 || row < 0 || row >= window->nrows || available <= 0) return;
    if (width > (unsigned)available) width = (unsigned)available;
    (void)ot_mouse_bind(&targets, window->col + 2 + offset, window->row + row, width, key);
}
void ot_dos_ui_action(ot_dos_ui_line line, unsigned panel, int row, const char *text, unsigned char color, unsigned key)
{
    line(panel, row, text, color);
    ot_dos_mouse_bind(panel, row, 0, (unsigned)strlen(text), key);
}
void ot_dos_mouse_pair(unsigned panel, int row, const char *text, unsigned first, unsigned second)
{
    const char *start, *end;
    unsigned i, keys[2];
    keys[0] = first; keys[1] = second; start = text;
    for (i = 0u; i < 2u; ++i) {
        start = strchr(start, '['); if (start == 0) return;
        end = strchr(start, ']'); if (end == 0) return;
        ot_dos_mouse_bind(panel, row, (int)(start - text), (unsigned)(end - start) + 1u, keys[i]);
        start = end + 1;
    }
}
int ot_dos_mouse_input(INPUTEVENT *event)
{
    unsigned kind, key;
    for (;;) {
        if (!get_input(INPUT_WAIT, event)) continue;
        if (event->key) { (void)ot_mouse_event(&targets, OT_MOUSE_CANCEL, 0, 0); return 1; }
        if (event->click == MOU_LSINGLE || event->click == MOU_LDOUBLE) kind = OT_MOUSE_DOWN;
        else if (event->click == MOU_LSINGLEUP) kind = OT_MOUSE_UP;
        else if (event->click == MOU_LDRAG) kind = OT_MOUSE_MOVE;
        else { (void)ot_mouse_event(&targets, OT_MOUSE_CANCEL, 0, 0); continue; }
        key = ot_mouse_event(&targets, kind, event->x, event->y);
        if (key) { event->key = (KEYCODE)key; return 1; }
    }
}

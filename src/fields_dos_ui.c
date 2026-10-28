/* Reuse QUILIB's editor for every settings view. Bounds and mouse caret
 * handling live here so no view can omit the inherited-editor safeguards. */
#include <string.h>
#include <ui.h>
#include <video.h>
#include <libcolor.h>
#include "opentape/fields_ui.h"
#include "opentape/dos_mouse.h"
#define FORM_FIELD 0xE320u

static void focus_click(FLDITEM *field, const INPUTEVENT *input)
{
    INPUTEVENT click = *input;
    int last = field->win.col + (int)strlen(field->text) - field->start.col;
    /* FLDEDIT accepts any cell in its window. Clamp blank-tail clicks so an
     * ensuing delete/paste can never operate beyond the string terminator. */
    if (click.x < field->win.col) click.x = field->win.col;
    if (click.x > last) click.x = last;
    if (click.x >= field->win.col + field->win.ncols)
        click.x = field->win.col + field->win.ncols - 1;
    click.y = field->win.row;
    click.click = MOU_LSINGLE;
    (void)sendFldEditClick(field, &click);
    /* A plain click places a caret, not a zero-length selected block. The
     * inherited Backspace/Delete handlers otherwise consume the first edit. */
    field->anchor.row = field->anchor.col = -1;
    (void)sendFldEditKey(field, KEY_TAB, 0);
}

void ot_dos_fields_show(struct ot_dos_fields *fields,
                        const struct ot_dos_field_spec *spec, unsigned count)
{
    unsigned i;
    WINDESC window;
    memset(fields, 0, sizeof(*fields));
    if (count > OT_DOS_FIELDS_MAX) return;
    fields->count = count;
    for (i = 0u; i < count; ++i) {
        window = DataWin; window.row += spec[i].row;
        window.col += 2; window.nrows = 1; window.ncols = 44;
        (void)showFldEdit(&fields->items[i], &window, 0, spec[i].text,
            spec[i].maximum, DataColor, 0, 0);
        ot_dos_mouse_bind(0u, spec[i].row, 0, 44u, FORM_FIELD + i);
    }
    if (count != 0u) (void)sendFldEditKey(&fields->items[0], KEY_TAB, 0);
}

int ot_dos_fields_input(struct ot_dos_fields *fields, const INPUTEVENT *event)
{
    unsigned key = event->key, next = fields->focus;
    FLDITEM *field;
    if (fields->count == 0u) return 0;
    if (key >= FORM_FIELD && key < FORM_FIELD + fields->count) next = key - FORM_FIELD;
    else if (key == KEY_TAB || key == KEY_ENTER || key == KEY_DOWN)
        next = (fields->focus + 1u) % fields->count;
    else if (key == KEY_STAB || key == KEY_UP)
        next = (fields->focus + fields->count - 1u) % fields->count;
    else {
        field = &fields->items[fields->focus];
        /* QUILIB's overwrite-at-EOF branch needs this guard before dispatch;
         * replacing a nonempty selection is safe. Keep the dependency intact. */
        if (NORMALKEY(key) >= 32 && field->cursor.col >= field->maxLen &&
            (field->anchor.col < 0 || field->anchor.col == field->cursor.col)) return 1;
        if (NORMALKEY(key) != 0 && NORMALKEY(key) < 32 && key != KEY_BKSP) return 0;
        return sendFldEditKey(field, event->key, event->flag) == FLD_NOROOM;
    }
    (void)sendFldEditKey(&fields->items[fields->focus], KEY_STAB, 0);
    fields->focus = next;
    if (key >= FORM_FIELD && key < FORM_FIELD + fields->count)
        focus_click(&fields->items[next], event);
    else (void)sendFldEditKey(&fields->items[next], KEY_TAB, 0);
    return 0;
}

void ot_dos_fields_close(struct ot_dos_fields *fields)
{
    unsigned i;
    for (i = 0u; i < fields->count; ++i) (void)killFldEdit(&fields->items[i]);
    hide_cursor(); ot_dos_mouse_clear(); fields->count = 0u;
}

#ifndef OPENTAPE_FIELDS_UI_H
#define OPENTAPE_FIELDS_UI_H
#include <FLDEDIT.H>
#include "opentape/dos_ui.h"
#define OT_DOS_FIELDS_MAX 5u
/* Shared bounded QUILIB field adapter. The caller owns buffers/screens and
 * handles Save/Cancel/mode commands before dispatch. No filesystem/device I/O. */
struct ot_dos_fields {
    FLDITEM items[OT_DOS_FIELDS_MAX];
    unsigned count, focus;
};
struct ot_dos_field_spec { char *text; unsigned maximum; int row; };
void ot_dos_fields_show(struct ot_dos_fields *, const struct ot_dos_field_spec *, unsigned);
/* Returns 1 for field limit, otherwise 0. Preserves caret/selection on events. */
int ot_dos_fields_input(struct ot_dos_fields *, const INPUTEVENT *);
void ot_dos_fields_close(struct ot_dos_fields *);
#endif

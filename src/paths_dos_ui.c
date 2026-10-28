/* Configure catalogue destinations/list files with the same field controls as
 * backup settings. The local draft is committed only after validation; no
 * catalogue selection or operation grant is retained in this dialog. */
#include <string.h>
#include <commfunc.h>
#include <libcolor.h>
#include <ui.h>
#include "opentape/fields_ui.h"
#include "opentape/paths_ui.h"
#include "opentape/dos_mouse.h"
#include "opentape/messages.h"

int ot_dos_paths_form_ui(struct ot_paths_form *committed, ot_dos_ui_line line)
{
    struct ot_paths_form draft;
    struct ot_dos_fields fields;
    struct ot_dos_field_spec spec[OT_PATHS_FORM_COUNT];
    const char *labels[OT_PATHS_FORM_COUNT];
    INPUTEVENT event;
    char blank[48];
    unsigned i, saved = 0u;
    if (committed == 0 || line == 0) return 0;
    draft = *committed;
    labels[0] = OT_MSG_PFORM_PREVIEW; labels[1] = OT_MSG_PFORM_RESTORE;
    labels[2] = OT_MSG_PFORM_COMPARE; labels[3] = OT_MSG_PFORM_IMPORT;
    labels[4] = OT_MSG_PFORM_EXPORT;
    paintBackgroundScreen(); ot_dos_mouse_clear();
    line(0u, 1, OT_MSG_APP_TITLE, DataYellowColor);
    line(0u, 2, OT_MSG_PFORM_TITLE, DataColor);
    line(0u, 3, OT_MSG_PFORM_POLICY, DataColor);
    for (i = 0u; i < OT_PATHS_FORM_COUNT; ++i) {
        spec[i].text = draft.names[i]; spec[i].row = 5 + (int)(3u * i);
        spec[i].maximum = OT_PATHS_FORM_MAX;
        line(0u, spec[i].row - 1, labels[i], DataColor);
    }
    ot_dos_fields_show(&fields, spec, OT_PATHS_FORM_COUNT);
    line(0u, 19, OT_MSG_BFORM_SAVE, DataColor);
    ot_dos_mouse_pair(0u, 19, OT_MSG_BFORM_SAVE, KEY_F2, KEY_ESCAPE);
    line(1u, 1, OT_MSG_HELP_TITLE, QuickBackColor);
    line(1u, 3, OT_MSG_BFORM_KEYS, QuickBackColor);
    line(1u, 5, OT_MSG_BFORM_PREV, QuickBackColor);
    line(1u, 7, OT_MSG_PFORM_EMPTY, QuickBackColor);
    line(1u, 9, OT_MSG_PFORM_LIMIT, QuickBackColor);
    line(1u, 16, OT_MSG_UI_HELP_READY, QuickBackColor);
    memset(blank, ' ', 47u); blank[47] = 0;
    for (;;) {
        (void)ot_dos_mouse_input(&event);
        if (event.key == KEY_ESCAPE) break;
        if (event.key == KEY_F2) {
            if (ot_paths_form_commit(committed, &draft)) { saved = 1u; break; }
            line(0u, 18, OT_MSG_PFORM_INVALID, DataYellowColor); continue;
        }
        line(0u, 18, blank, DataColor);
        if (ot_dos_fields_input(&fields, &event)) line(0u, 18, OT_MSG_BFORM_LIMIT, DataYellowColor);
    }
    ot_dos_fields_close(&fields);
    return (int)saved;
}

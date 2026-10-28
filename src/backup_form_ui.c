/* A draft editor over the shared 386MAX QUILIB controls. It owns no files or
 * device handles: F2 commits settings, while B has its own one-shot grant.
 * Keep FLDITEM instances alive across events so selection/caret state survives.
 * Mouse targets use the common press/release mapping, including drag cancel. */
#include <string.h>
#include <commfunc.h>
#include "opentape/fields_ui.h"
#include <libcolor.h>
#include <ui.h>
#include <video.h>
#include "opentape/backup_ui.h"
#include "opentape/dos_mouse.h"
#include "opentape/messages.h"

#define FORM_FILE 0xE330u
#define FORM_TREE 0xE331u
#define FORM_SET 0xE332u

static void status_line(ot_dos_ui_line line, const char *message)
{
    char blank[48];
    memset(blank, ' ', 47u); blank[47] = 0;
    line(0u, 18, blank, DataColor);
    if (message != 0) line(0u, 18, message, DataYellowColor);
}

static void mode_line(ot_dos_ui_line line, unsigned tree)
{
    /* Pad all alternatives; changing from the longer caption must erase it. */
    char text[48];
    unsigned length;
    memset(text, ' ', 47u); text[47] = 0;
    length = (unsigned)strlen(tree == OT_BACKUP_SET ? OT_MSG_BUI_SET : tree ? OT_MSG_BUI_TREE : OT_MSG_BUI_FILE);
    if (length > 47u) length = 47u;
    memcpy(text, tree == OT_BACKUP_SET ? OT_MSG_BUI_SET : tree ? OT_MSG_BUI_TREE : OT_MSG_BUI_FILE, length);
    line(0u, 5, text, DataYellowColor);
}

int ot_dos_backup_form_ui(struct ot_backup_form *committed, ot_dos_ui_line line)
{
    struct ot_backup_form draft;
    struct ot_dos_fields fields;
    struct ot_dos_field_spec spec[3];
    INPUTEVENT event;
    char *buffers[3];
    unsigned i, key, saved = 0u;
    if (committed == 0 || line == 0) return 0;
    draft = *committed;
    buffers[0] = draft.source; buffers[1] = draft.path; buffers[2] = draft.volume;
    paintBackgroundScreen(); ot_dos_mouse_clear();
    line(0u, 1, OT_MSG_APP_TITLE, DataYellowColor);
    line(0u, 2, OT_MSG_BFORM_TITLE, DataColor);
    mode_line(line, draft.tree);
    line(0u, 6, OT_MSG_BFORM_MODE, DataColor);
    ot_dos_mouse_pair(0u, 6, OT_MSG_BFORM_MODE, FORM_FILE, FORM_TREE);
    {
        const char *last = strrchr(OT_MSG_BFORM_MODE, '[');
        ot_dos_mouse_bind(0u, 6, (int)(last - OT_MSG_BFORM_MODE), (unsigned)strlen(last), FORM_SET);
    }
    line(0u, 7, OT_MSG_BUI_SOURCE, DataColor);
    line(0u, 10, OT_MSG_BUI_PATH, DataColor);
    line(0u, 13, OT_MSG_BUI_VOLUME, DataColor);
    for (i = 0u; i < 3u; ++i) {
        spec[i].text = buffers[i]; spec[i].row = 8 + (int)(3u * i);
        spec[i].maximum = i == 2u ? OT_BACKUP_FORM_VOLUME_MAX : OT_BACKUP_FORM_PATH_MAX;
    }
    ot_dos_fields_show(&fields, spec, 3u);
    line(0u, 16, OT_MSG_BFORM_POLICY, DataColor);
    line(0u, 17, OT_MSG_BFORM_HINT, DataColor);
    line(0u, 19, OT_MSG_BFORM_SAVE, DataColor);
    ot_dos_mouse_pair(0u, 19, OT_MSG_BFORM_SAVE, KEY_F2, KEY_ESCAPE);
    line(1u, 1, OT_MSG_HELP_TITLE, QuickBackColor);
    line(1u, 3, OT_MSG_BFORM_KEYS, QuickBackColor);
    line(1u, 5, OT_MSG_BFORM_PREV, QuickBackColor);
    ot_dos_ui_action(line, 1u, 7, OT_MSG_BFORM_TOGGLE, QuickBackColor, KEY_F4);
    line(1u, 16, OT_MSG_UI_HELP_READY, QuickBackColor);
    for (;;) {
        (void)ot_dos_mouse_input(&event); key = event.key;
        if (key == KEY_ESCAPE) break;
        if (key == KEY_F2) {
            if (ot_backup_form_commit(committed, &draft)) { saved = 1u; break; }
            status_line(line, OT_MSG_BFORM_INVALID); continue;
        }
        status_line(line, 0);
        if (key == KEY_F4 || key == FORM_FILE || key == FORM_TREE || key == FORM_SET) {
            draft.tree = key == KEY_F4 ? (draft.tree + 1u) % 3u :
                key == FORM_SET ? OT_BACKUP_SET : key == FORM_TREE;
            mode_line(line, draft.tree); continue;
        }
        if (ot_dos_fields_input(&fields, &event)) status_line(line, OT_MSG_BFORM_LIMIT);
    }
    ot_dos_fields_close(&fields);
    return (int)saved;
}

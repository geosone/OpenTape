/* Explicit profile export/import over the shared field and confirmation UI.
 * F2 ends filename editing; only a fresh Y/J grants one filesystem operation.
 * Neither profiles nor the result dismissal can grant a tape write. */
#include <stdio.h>
#include <string.h>
#include <commfunc.h>
#include <libcolor.h>
#include <ui.h>
#include "opentape/fields_ui.h"
#include "opentape/settings_ui.h"
#include "opentape/dos_mouse.h"
#include "opentape/messages.h"
static void draw(ot_dos_ui_line line, const char *name, unsigned exporting, unsigned phase, int rc, unsigned error)
{
    char text[80];
    paintBackgroundScreen(); ot_dos_mouse_clear();
    line(0u, 1, OT_MSG_APP_TITLE, DataYellowColor);
    line(0u, 2, exporting ? OT_MSG_CFG_EXPORT_TITLE : OT_MSG_CFG_IMPORT_TITLE, DataColor);
    line(0u, 5, OT_MSG_CFG_SCOPE, DataColor);
    line(0u, 7, OT_MSG_CFG_NAME, DataColor);
    if (phase != 0u) line(0u, 8, name, DataYellowColor);
    line(0u, 11, exporting ? OT_MSG_CFG_NEW_ONLY : OT_MSG_CFG_REPLACE_RAM, DataColor);
    line(0u, 13, OT_MSG_CFG_NO_TAPE, DataColor);
    if (phase == 0u) {
        line(0u, 19, OT_MSG_CFG_EDIT_KEYS, DataColor);
        ot_dos_mouse_pair(0u, 19, OT_MSG_CFG_EDIT_KEYS, KEY_F2, KEY_ESCAPE);
    } else if (phase == 1u) {
        line(0u, 17, OT_MSG_CFG_CONFIRM, DataYellowColor);
        line(0u, 19, OT_MSG_UI_REWIND_KEYS, DataColor);
        ot_dos_mouse_pair(0u, 19, OT_MSG_UI_REWIND_KEYS, OT_UI_CONFIRM_KEY, KEY_ESCAPE);
    } else if (phase == 2u) line(0u, 19, OT_MSG_CFG_BUSY, DataColor);
    else {
        if (rc == 0) strcpy(text, exporting ? OT_MSG_CFG_EXPORTED : OT_MSG_CFG_IMPORTED);
        else sprintf(text, OT_MSG_CFG_FAILED, rc, error);
        line(0u, 17, text, DataYellowColor);
        if (rc != 0) line(0u, 19, exporting ? OT_MSG_CFG_EXPORT_KEPT : OT_MSG_CFG_IMPORT_KEPT, DataColor);
        ot_dos_ui_action(line, 1u, 3, OT_MSG_UI_PREVIEW_BACK, QuickBackColor, KEY_ESCAPE);
    }
    line(1u, 1, OT_MSG_HELP_TITLE, QuickBackColor);
    if (phase != 0u) line(1u, 16, OT_MSG_UI_HELP_READY, QuickBackColor);
}
void ot_dos_settings_ui(struct ot_backup_form *backup, struct ot_paths_form *paths, char *filename,
                         unsigned exporting, ot_dos_ui_line line)
{
    struct ot_settings settings;
    struct ot_dos_fields fields;
    struct ot_dos_field_spec spec;
    INPUTEVENT event;
    char name[OT_PATHS_FORM_MAX + 1u], blank[48];
    int rc;
    unsigned error = 0u;
    if (backup == 0 || paths == 0 || filename == 0 || line == 0 || exporting > 1u ||
        !ot_settings_filename_valid(filename)) return;
    settings.backup = *backup; settings.paths = *paths;
    strcpy(name, filename);
    draw(line, name, exporting, 0u, 0, 0u);
    spec.text = name; spec.maximum = OT_PATHS_FORM_MAX; spec.row = 8;
    ot_dos_fields_show(&fields, &spec, 1u);
    line(1u, 16, OT_MSG_UI_HELP_READY, QuickBackColor);
    memset(blank, ' ', 47u); blank[47] = 0;
    for (;;) {
        (void)ot_dos_mouse_input(&event);
        if (event.key == KEY_ESCAPE) { ot_dos_fields_close(&fields); return; }
        if (event.key == KEY_F2) {
            if (ot_settings_filename_valid(name)) break;
            line(0u, 18, OT_MSG_CFG_INVALID_NAME, DataYellowColor); continue;
        }
        line(0u, 18, blank, DataColor);
        if (ot_dos_fields_input(&fields, &event)) line(0u, 18, OT_MSG_BFORM_LIMIT, DataYellowColor);
    }
    ot_dos_fields_close(&fields);
    draw(line, name, exporting, 1u, 0, 0u);
    (void)ot_dos_mouse_input(&event);
    if (event.key != OT_UI_CONFIRM_KEY && event.key != OT_UI_CONFIRM_KEY - 'a' + 'A') return;
    strcpy(filename, name);
    draw(line, name, exporting, 2u, 0, 0u);
    rc = exporting ? ot_dos_settings_save(name, &settings, &error) : ot_dos_settings_load(name, &settings, &error);
    if (!exporting && rc == 0) { *backup = settings.backup; *paths = settings.paths; }
    draw(line, name, exporting, 3u, rc, error);
    (void)ot_dos_mouse_input(&event);
}

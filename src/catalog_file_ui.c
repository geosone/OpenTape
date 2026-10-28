/* Explicit offline metadata files over the shared field and confirmation UI.
 * F2 ends filename editing; only a fresh Y/J grants one filesystem operation.
 * Neither file operations nor result dismissal can grant a tape write. */
#include <stdio.h>
#include <string.h>
#include <commfunc.h>
#include <libcolor.h>
#include <ui.h>
#include "opentape/fields_ui.h"
#include "opentape/catalog_file.h"
#include "opentape/local_name.h"
#include "opentape/paths_form.h"
#include "opentape/dos_mouse.h"
#include "opentape/messages.h"
static void draw(ot_dos_ui_line line, const char *name, unsigned exporting, unsigned phase, int rc, unsigned error)
{
    char text[80];
    paintBackgroundScreen(); ot_dos_mouse_clear();
    line(0u, 1, OT_MSG_APP_TITLE, DataYellowColor);
    line(0u, 2, exporting ? OT_MSG_CATFILE_EXPORT : OT_MSG_CATFILE_IMPORT, DataColor);
    line(0u, 5, OT_MSG_CATFILE_SCOPE, DataColor);
    line(0u, 7, OT_MSG_CATFILE_NAME, DataColor);
    if (phase != 0u) line(0u, 8, name, DataYellowColor);
    line(0u, 11, exporting ? OT_MSG_CFG_NEW_ONLY : OT_MSG_CATFILE_REPLACE, DataColor);
    line(0u, 13, OT_MSG_CFG_NO_TAPE, DataColor);
    if (phase == 0u) {
        line(0u, 19, OT_MSG_CFG_EDIT_KEYS, DataColor);
        ot_dos_mouse_pair(0u, 19, OT_MSG_CFG_EDIT_KEYS, KEY_F2, KEY_ESCAPE);
    } else if (phase == 1u) {
        line(0u, 17, OT_MSG_CATFILE_CONFIRM, DataYellowColor);
        line(0u, 19, OT_MSG_UI_REWIND_KEYS, DataColor);
        ot_dos_mouse_pair(0u, 19, OT_MSG_UI_REWIND_KEYS, OT_UI_CONFIRM_KEY, KEY_ESCAPE);
    } else if (phase == 2u) line(0u, 19, OT_MSG_CATFILE_BUSY, DataColor);
    else {
        if (rc == 0) strcpy(text, exporting ? OT_MSG_CATFILE_SAVED : OT_MSG_CATFILE_LOADED);
        else sprintf(text, OT_MSG_CATFILE_FAILED, rc, error);
        line(0u, 17, text, DataYellowColor);
        if (rc != 0) line(0u, 19, exporting ? OT_MSG_CFG_EXPORT_KEPT : OT_MSG_CATFILE_KEPT, DataColor);
        ot_dos_ui_action(line, 1u, 3, OT_MSG_UI_PREVIEW_BACK, QuickBackColor, KEY_ESCAPE);
    }
    line(1u, 1, OT_MSG_HELP_TITLE, QuickBackColor);
    if (phase != 0u) line(1u, 16, OT_MSG_UI_HELP_READY, QuickBackColor);
}
int ot_dos_catalog_file_ui(struct ot_catalog *catalog, struct ot_volume_label *label, char *filename,
                            unsigned exporting, ot_dos_ui_line line)
{
    struct ot_dos_fields fields;
    struct ot_dos_field_spec spec;
    INPUTEVENT event;
    char name[OT_PATHS_FORM_MAX + 1u], blank[48];
    int rc;
    unsigned error = 0u;
    if (catalog == 0 || label == 0 || filename == 0 || line == 0 || exporting > 1u ||
        !ot_local_file_name_valid(filename, OT_PATHS_FORM_MAX)) return 0;
    strcpy(name, filename);
    draw(line, name, exporting, 0u, 0, 0u);
    spec.text = name; spec.maximum = OT_PATHS_FORM_MAX; spec.row = 8;
    ot_dos_fields_show(&fields, &spec, 1u);
    line(1u, 16, OT_MSG_UI_HELP_READY, QuickBackColor);
    memset(blank, ' ', 47u); blank[47] = 0;
    for (;;) {
        (void)ot_dos_mouse_input(&event);
        if (event.key == KEY_ESCAPE) { ot_dos_fields_close(&fields); return 0; }
        if (event.key == KEY_F2) {
            if (ot_local_file_name_valid(name, OT_PATHS_FORM_MAX)) break;
            line(0u, 18, OT_MSG_CATFILE_INVALID, DataYellowColor); continue;
        }
        line(0u, 18, blank, DataColor);
        if (ot_dos_fields_input(&fields, &event)) line(0u, 18, OT_MSG_BFORM_LIMIT, DataYellowColor);
    }
    ot_dos_fields_close(&fields);
    draw(line, name, exporting, 1u, 0, 0u);
    (void)ot_dos_mouse_input(&event);
    if (event.key != OT_UI_CONFIRM_KEY && event.key != OT_UI_CONFIRM_KEY - 'a' + 'A') return 0;
    strcpy(filename, name);
    draw(line, name, exporting, 2u, 0, 0u);
    rc = exporting ? ot_dos_catalog_file_save(catalog, label, name, &error) :
        ot_dos_catalog_file_load(catalog, label, name, &error);
    draw(line, name, exporting, 3u, rc, error);
    (void)ot_dos_mouse_input(&event);
    return !exporting && rc == 0;
}

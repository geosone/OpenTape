/* F7 inspects source metadata only, then returns. B retains its independent
 * configuration review, current-position warning and one-shot write grant. */
#include <stdio.h>
#include <string.h>
#include <commfunc.h>
#include <libcolor.h>
#include <ui.h>
#include "opentape/backup_preview.h"
#include "opentape/dos_mouse.h"
#include "opentape/messages.h"
static void draw(const struct ot_dos_backup_request *request, const struct ot_backup_preview_result *result,
                 unsigned busy, ot_dos_ui_line line)
{
    char text[100];
    paintBackgroundScreen(); ot_dos_mouse_clear();
    line(0u, 1, OT_MSG_APP_TITLE, DataYellowColor);
    line(0u, 2, OT_MSG_BPLAN_TITLE, DataColor);
    line(0u, 5, OT_MSG_BPLAN_POLICY, DataColor);
    if (request == 0) line(0u, 8, OT_MSG_BUI_DISABLED, DataColor);
    else {
        line(0u, 7, request->tree == OT_BACKUP_SET ? OT_MSG_BUI_SET : request->tree ? OT_MSG_BUI_TREE : OT_MSG_BUI_FILE, DataColor);
        line(0u, 8, request->source, DataYellowColor);
        line(0u, 9, request->path, DataColor);
        if (busy) line(0u, 12, OT_MSG_BPLAN_BUSY, DataColor);
        else if (result->result != 0) {
            sprintf(text, OT_MSG_BPLAN_FAILED, result->result, result->source_error);
            line(0u, 12, text, DataYellowColor);
            line(0u, 14, OT_MSG_BPLAN_NO_COUNTS, DataColor);
        } else {
            sprintf(text, OT_MSG_BUI_COUNTS, (unsigned long)result->summary.files, (unsigned long)result->summary.directories);
            line(0u, 12, text, DataYellowColor);
            if (result->summary.bytes_high == 0ul)
                sprintf(text, OT_MSG_BPLAN_BYTES, (unsigned long)result->summary.bytes_low);
            else sprintf(text, OT_MSG_BPLAN_WIDE,
                (unsigned long)((result->summary.bytes_high << 12) + (result->summary.bytes_low >> 20)),
                (unsigned long)(result->summary.bytes_low & 0xffffful));
            line(0u, 13, text, DataColor);
            line(0u, 15, OT_MSG_BPLAN_FRESH, DataColor);
        }
    }
    line(0u, 18, OT_MSG_BPLAN_LIMITS, DataColor);
    line(1u, 1, OT_MSG_HELP_TITLE, QuickBackColor);
    if (!busy) ot_dos_ui_action(line, 1u, 3, OT_MSG_UI_PREVIEW_BACK, QuickBackColor, KEY_ESCAPE);
    line(1u, 16, OT_MSG_UI_HELP_READY, QuickBackColor);
}
void ot_dos_backup_preview_ui(const struct ot_dos_backup_request *request, ot_dos_ui_line line)
{
    struct ot_backup_preview_result result;
    INPUTEVENT event;
    if (line == 0) return;
    if (!ot_dos_backup_request_valid(request)) request = 0;
    memset(&result, 0, sizeof(result));
    if (request != 0) {
        draw(request, &result, 1u, line);
        (void)ot_dos_backup_preview(request, &result);
    }
    draw(request, &result, 0u, line);
    (void)ot_dos_mouse_input(&event);
}

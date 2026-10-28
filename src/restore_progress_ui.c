/* Read-side observation has no transport/destination authority. Counters mean
 * planned/restored/matched files according to the chosen backend, with final
 * success still depending on filemark, selected-path coverage and tape close. */
#include <stdio.h>
#include <string.h>
#include <commfunc.h>
#include <libcolor.h>
#include <ui.h>
#include "opentape/restore_ui.h"
#include "opentape/archive_dos.h"
#include "opentape/dos_mouse.h"
#include "opentape/messages.h"
static void field(ot_dos_ui_line line, int row, const char *text, unsigned char color)
{
    char padded[48]; unsigned length = (unsigned)strlen(text);
    if (length > 47u) length = 47u;
    memset(padded, ' ', 47u); padded[47] = 0; memcpy(padded, text, length);
    line(0u, row, padded, color);
}
void ot_dos_restore_progress(void *context, const struct ot_restore *restore)
{
    struct ot_restore_progress_view *view = context;
    char text[128];
    oa_u32 low = restore->checked_low, high = restore->checked_high;
    unsigned offset, length;
    if (!view->initialized) {
        paintBackgroundScreen(); ot_dos_mouse_clear(); view->initialized = 1u;
        view->line(0u, 1, OT_MSG_APP_TITLE, DataYellowColor);
        view->line(0u, 2, view->action == OT_DOS_FILE_COMPARE ? OT_MSG_UI_COMPARE_TITLE :
            view->action == OT_DOS_FILE_RESTORE ? OT_MSG_UI_WRITE_TITLE : OT_MSG_UI_PREVIEW_TITLE, DataColor);
        if (ot_dos_archive_is_native() && ot_dos_archive_fixed_block() != 0u) {
            sprintf(text, OT_MSG_UI_API_FIXED, ot_dos_archive_fixed_block()); view->line(0u, 3, text, DataColor);
        } else view->line(0u, 3, ot_dos_archive_is_native() ? OT_MSG_UI_API_NATIVE : OT_MSG_UI_API_LEGACY, DataColor);
        sprintf(text, OT_MSG_UI_SELECTED, view->device); view->line(0u, 4, text, DataYellowColor);
        view->line(0u, 6, view->destination, DataColor);
        view->line(0u, 11, OT_MSG_RPROG_PATH, DataColor);
        view->line(0u, 19, OT_MSG_RPROG_PENDING, DataColor);
        view->line(1u, 1, OT_MSG_HELP_TITLE, QuickBackColor);
        view->line(1u, 3, OT_MSG_PROGRESS_WAIT, QuickBackColor);
    }
    field(view->line, 5, !restore->verified.session_active && restore->verified.sessions != 0ul ?
        OT_MSG_RPROG_BOUNDARY : OT_MSG_RPROG_READ, DataYellowColor);
    sprintf(text, view->action == OT_DOS_FILE_COMPARE ? OT_MSG_RPROG_MATCHED :
        view->action == OT_DOS_FILE_RESTORE ? OT_MSG_RPROG_RESTORED : OT_MSG_RPROG_PLANNED,
        (unsigned long)restore->restored_files, view->selected);
    field(view->line, 7, text, DataColor);
    sprintf(text, OT_MSG_RPROG_KEPT, (unsigned long)restore->skipped_files, (unsigned long)restore->excluded_files);
    field(view->line, 8, text, DataColor);
    if (high == 0ul) sprintf(text, OT_MSG_RPROG_BYTES, (unsigned long)low);
    else sprintf(text, OT_MSG_RPROG_MIB, (unsigned long)((high << 12) + (low >> 20)));
    field(view->line, 9, text, DataColor);
    sprintf(text, OT_MSG_PROGRESS_FILE, (unsigned long)restore->active_bytes, (unsigned long)restore->active.file_size);
    field(view->line, 10, text, DataColor);
    length = (unsigned)strlen(restore->active.path);
    for (offset = 0u; offset < 282u; offset += 47u) {
        if (offset < length) sprintf(text, "%.47s", restore->active.path + offset); else *text = 0;
        field(view->line, 12 + (int)(offset / 47u), text, DataYellowColor);
    }
    field(view->line, 18, length == 0u ? "" : restore->filter_enabled &&
        !ot_restore_selected(restore->filter, restore->filter_count, restore->active.path) ?
        OT_MSG_RPROG_EXCLUDED : OT_MSG_RPROG_SELECTED, DataColor);
    view->line(1u, 16, OT_MSG_UI_HELP_READY, QuickBackColor);
}

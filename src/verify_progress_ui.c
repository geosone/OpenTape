/* Verification/catalogue observation is display-only. No input targets, file
 * publication, tape query or completion decision belongs to this callback. */
#include <stdio.h>
#include <string.h>
#include <commfunc.h>
#include <libcolor.h>
#include <ui.h>
#include "opentape/verify_progress.h"
#include "opentape/archive_dos.h"
#include "opentape/dos_mouse.h"
#include "opentape/messages.h"
static void field(ot_dos_ui_line line, int row, const char *text, unsigned char color)
{
    char padded[48]; unsigned size = (unsigned)strlen(text);
    if (size > 47u) size = 47u;
    memset(padded, ' ', 47u); padded[47] = 0; memcpy(padded, text, size);
    line(0u, row, padded, color);
}
void ot_dos_verify_progress(void *context, const struct ot_archive_verify *state, const oa_u8 *block, unsigned size)
{
    struct ot_verify_progress_view *view = context;
    const struct ot_verify_progress *p = &view->progress;
    char text[128]; unsigned offset, length;
    if (!view->initialized) {
        paintBackgroundScreen(); ot_dos_mouse_clear(); view->initialized = 1u;
        view->line(0u, 1, OT_MSG_APP_TITLE, DataYellowColor);
        view->line(0u, 2, view->cataloguing ? OT_MSG_VPROG_CATALOG : OT_MSG_VPROG_VERIFY, DataColor);
        if (ot_dos_archive_is_native() && ot_dos_archive_fixed_block() != 0u) {
            sprintf(text, OT_MSG_UI_API_FIXED, ot_dos_archive_fixed_block()); view->line(0u, 3, text, DataColor);
        } else view->line(0u, 3, ot_dos_archive_is_native() ? OT_MSG_UI_API_NATIVE : OT_MSG_UI_API_LEGACY, DataColor);
        sprintf(text, OT_MSG_UI_SELECTED, view->device); view->line(0u, 4, text, DataYellowColor);
        view->line(0u, 11, OT_MSG_VPROG_PATH, DataColor);
        view->line(0u, 19, OT_MSG_RPROG_PENDING, DataColor);
        view->line(1u, 1, OT_MSG_HELP_TITLE, QuickBackColor);
        view->line(1u, 3, OT_MSG_PROGRESS_WAIT, QuickBackColor);
    }
    if (view->failed || ot_verify_progress_block(&view->progress, state, block, size) != 0) {
        view->failed = 1u; field(view->line, 5, OT_MSG_VPROG_UNAVAILABLE, DataYellowColor);
    } else {
        field(view->line, 5, p->boundary ? OT_MSG_RPROG_BOUNDARY : OT_MSG_VPROG_READING, DataYellowColor);
        sprintf(text, OT_MSG_VPROG_BLOCKS, (unsigned long)p->blocks, (unsigned long)p->records);
        field(view->line, 6, text, DataColor);
        sprintf(text, OT_MSG_VPROG_FILES, (unsigned long)p->files, (unsigned long)p->directories);
        field(view->line, 7, text, DataColor);
        sprintf(text, OT_MSG_VPROG_SESSIONS, (unsigned long)p->sessions);
        field(view->line, 8, text, DataColor);
        if (p->bytes_high == 0ul) sprintf(text, OT_MSG_RPROG_BYTES, (unsigned long)p->bytes_low);
        else sprintf(text, OT_MSG_RPROG_MIB, (unsigned long)((p->bytes_high << 12) + (p->bytes_low >> 20)));
        field(view->line, 9, text, DataColor);
        sprintf(text, OT_MSG_PROGRESS_FILE, (unsigned long)p->file_bytes, (unsigned long)p->current.file_size);
        field(view->line, 10, text, DataColor);
        length = (unsigned)strlen(p->current.path);
        for (offset = 0u; offset < 282u; offset += 47u) {
            if (offset < length) sprintf(text, "%.47s", p->current.path + offset); else *text = 0;
            field(view->line, 12 + (int)(offset / 47u), text, DataYellowColor);
        }
        field(view->line, 18, view->cataloguing ? OT_MSG_VPROG_PROVISIONAL : OT_MSG_VPROG_READONLY, DataColor);
    }
    view->line(1u, 16, OT_MSG_UI_HELP_READY, QuickBackColor);
}

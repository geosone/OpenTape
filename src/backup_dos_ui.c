/* One-shot backup dialog on the application's shared QUILIB renderer. Source
 * configuration is reviewed before any file/device I/O. A confirmation grants
 * exactly one call; the result key is consumed and cannot start another write. */
#include <stdio.h>
#include <string.h>
#include <commfunc.h>
#include <input.h>
#include <libcolor.h>
#include <ui.h>
#include "opentape/backup_ui.h"
#include "opentape/dos_mouse.h"
#include "opentape/archive_dos.h"
#include "opentape/messages.h"

static void backend_line(ot_dos_ui_line line)
{
    char text[80];
    if (ot_dos_archive_is_native() && ot_dos_archive_fixed_block() != 0u) {
        sprintf(text, OT_MSG_UI_API_FIXED, ot_dos_archive_fixed_block()); line(0u, 3, text, DataColor);
    } else line(0u, 3, ot_dos_archive_is_native() ? OT_MSG_UI_API_NATIVE : OT_MSG_UI_API_LEGACY, DataColor);
}

static void draw(const char *device, const struct ot_dos_backup_request *request,
                 const struct ot_dos_backup_result *result, unsigned phase, ot_dos_ui_line line)
{
    char text[128];
    unsigned offset, length;
    paintBackgroundScreen();
    ot_dos_mouse_clear();
    line(0u, 1, OT_MSG_APP_TITLE, DataYellowColor);
    line(0u, 2, OT_MSG_BUI_TITLE, DataColor);
    backend_line(line);
    sprintf(text, OT_MSG_UI_SELECTED, device); line(0u, 4, text, DataYellowColor);
    if (request == 0) {
        line(0u, 8, OT_MSG_BUI_DISABLED, DataColor);
        line(0u, 10, OT_MSG_BUI_DISABLED_TREE, DataColor);
        line(0u, 12, OT_MSG_BUI_DISABLED_ARGS, DataColor);
    } else {
        line(0u, 6, request->tree == OT_BACKUP_SET ? OT_MSG_BUI_SET : request->tree ? OT_MSG_BUI_TREE : OT_MSG_BUI_FILE, DataColor);
        line(0u, 7, OT_MSG_BUI_SOURCE, DataColor); line(0u, 8, request->source, DataYellowColor);
        line(0u, 9, OT_MSG_BUI_PATH, DataColor); line(0u, 10, request->path, DataYellowColor);
        line(0u, 11, OT_MSG_BUI_VOLUME, DataColor); line(0u, 12, request->volume, DataYellowColor);
        if (phase != 2u) {
            line(0u, 14, OT_MSG_BUI_WARNING, DataYellowColor);
            line(0u, 15, OT_MSG_BUI_OVERWRITE, DataYellowColor);
            line(0u, 19, phase == 0u ? OT_MSG_UI_REWIND_KEYS : OT_MSG_BUI_BUSY, DataColor);
        } else {
            sprintf(text, OT_MSG_BUI_COUNTS, (unsigned long)result->files, (unsigned long)result->directories);
            line(0u, 13, text, DataColor);
            if (result->result == 0 && result->finished) {
                line(0u, 14, OT_MSG_BUI_OK, DataYellowColor);
                line(0u, 15, OT_MSG_BUI_READBACK, DataColor);
                line(0u, 19, OT_MSG_BUI_POSITION, DataColor);
            } else {
                sprintf(text, OT_MSG_BUI_FAILED, result->result, result->source_error, result->tape_error);
                line(0u, 14, text, DataYellowColor);
                line(0u, 15, result->output_opened ? OT_MSG_BUI_PARTIAL : OT_MSG_BUI_STOPPED, DataColor);
                line(0u, 16, OT_MSG_BUI_PROVISIONAL, DataColor);
                length = (unsigned)strlen(result->source_name);
                if (result->source_error != 0u) for (offset = 0u; offset < length; offset += 47u) {
                    sprintf(text, "%.47s", result->source_name + offset);
                    line(0u, 16 + (int)(offset / 47u), text, DataYellowColor);
                }
                sprintf(text, "SCSI=%02X sense=%02X op=%04X rest=%u",
                    (unsigned)result->status.tape.last_scsi_status, (unsigned)result->status.tape.last_sense_key,
                    (unsigned)result->status.tape.last_scsi_opcode, (unsigned)result->status.tape.residual_bytes);
                line(0u, 19, text, DataColor);
            }
        }
    }
    line(1u, 1, OT_MSG_HELP_TITLE, QuickBackColor);
    line(1u, 3, phase == 0u && request != 0 ? OT_MSG_BUI_CONFIRM : OT_MSG_UI_PREVIEW_BACK, QuickBackColor);
    if (phase == 0u && request != 0)
        ot_dos_mouse_pair(0u, 19, OT_MSG_UI_REWIND_KEYS, OT_UI_CONFIRM_KEY, KEY_ESCAPE);
    else if (phase != 1u)
        ot_dos_mouse_bind(1u, 3, 0, (unsigned)strlen(OT_MSG_UI_PREVIEW_BACK), KEY_ESCAPE);
    line(1u, 16, OT_MSG_UI_HELP_READY, QuickBackColor);
}

static unsigned next_key(void)
{
    INPUTEVENT event;
    while (!ot_dos_mouse_input(&event)) { }
    return event.key;
}

struct progress_view {
    const char *device;
    ot_dos_ui_line line;
    unsigned initialized;
};
static void progress_line(ot_dos_ui_line line, int row, const char *text, unsigned char color)
{
    char padded[48];
    unsigned length = (unsigned)strlen(text);
    if (length > 47u) length = 47u;
    memset(padded, ' ', 47u); padded[47] = 0;
    memcpy(padded, text, length); line(0u, row, padded, color);
}
static void show_progress(void *context, const struct ot_backup_progress *progress)
{
    struct progress_view *view = context;
    char text[128];
    unsigned offset, length;
    if (!view->initialized) {
        paintBackgroundScreen(); ot_dos_mouse_clear(); view->initialized = 1u;
        view->line(0u, 1, OT_MSG_APP_TITLE, DataYellowColor);
        view->line(0u, 2, OT_MSG_BUI_TITLE, DataColor);
        backend_line(view->line);
        sprintf(text, OT_MSG_UI_SELECTED, view->device); view->line(0u, 4, text, DataYellowColor);
        view->line(0u, 11, OT_MSG_BUI_PATH, DataColor);
        view->line(0u, 19, OT_MSG_PROGRESS_PENDING, DataColor);
        view->line(1u, 1, OT_MSG_HELP_TITLE, QuickBackColor);
        view->line(1u, 3, OT_MSG_PROGRESS_WAIT, QuickBackColor);
    }
    progress_line(view->line, 5, progress->phase == OT_BACKUP_PLANNED ? OT_MSG_PROGRESS_PLAN :
        progress->phase == OT_BACKUP_FINALIZING ? OT_MSG_PROGRESS_FINISH : OT_MSG_PROGRESS_WRITE, DataYellowColor);
    sprintf(text, OT_MSG_PROGRESS_FILES, (unsigned long)progress->files, (unsigned long)progress->total.files);
    progress_line(view->line, 7, text, DataColor);
    sprintf(text, OT_MSG_PROGRESS_DIRS, (unsigned long)progress->directories, (unsigned long)progress->total.directories);
    progress_line(view->line, 8, text, DataColor);
    if (progress->accepted_high == 0ul && progress->total.bytes_high == 0ul)
        sprintf(text, OT_MSG_PROGRESS_PAYLOAD, (unsigned long)progress->accepted_low, (unsigned long)progress->total.bytes_low);
    else sprintf(text, OT_MSG_PROGRESS_WIDE,
        (unsigned long)((progress->accepted_high << 12) + (progress->accepted_low >> 20)),
        (unsigned long)((progress->total.bytes_high << 12) + (progress->total.bytes_low >> 20)));
    progress_line(view->line, 9, text, DataColor);
    sprintf(text, OT_MSG_PROGRESS_FILE, (unsigned long)progress->file_bytes, (unsigned long)progress->file_size);
    progress_line(view->line, 10, text, DataColor);
    length = (unsigned)strlen(progress->path);
    for (offset = 0u; offset < 282u; offset += 47u) {
        if (offset < length) sprintf(text, "%.47s", progress->path + offset); else *text = 0;
        progress_line(view->line, 12 + (int)(offset / 47u), text, DataYellowColor);
    }
    view->line(1u, 16, OT_MSG_UI_HELP_READY, QuickBackColor);
}

int ot_dos_backup_ui(const char *device, const struct ot_dos_backup_request *request, ot_dos_ui_line line)
{
    struct ot_dos_backup_result result;
    struct progress_view view;
    unsigned key;
    if (line == 0 || !ot_probe_raw_name_valid(device)) return 0;
    if (!ot_dos_backup_request_valid(request)) request = 0;
    memset(&result, 0, sizeof(result));
    draw(device, request, &result, 0u, line);
    key = next_key();
    if (request == 0 || !(key == OT_UI_CONFIRM_KEY || key == OT_UI_CONFIRM_KEY - 'a' + 'A')) return 0;
    draw(device, request, &result, 1u, line);
    memset(&view, 0, sizeof(view)); view.device = device; view.line = line;
    (void)ot_dos_backup_execute_observed(request, device, 1u, show_progress, &view, &result);
    draw(device, request, &result, 2u, line);
    (void)next_key();
    return 1;
}

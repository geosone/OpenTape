#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <commfunc.h>
#include <input.h>
#include <libcolor.h>
#include <screen.h>
#include <ui.h>
#include <video.h>
#include <windowfn.h>

#include "opentape/backend.h"
#include "opentape/messages.h"
#include "opentape/archive_dos.h"
#include "opentape/tape_probe.h"
#include "opentape/tape_reader.h"
#include "opentape/volume_format.h"
#include "opentape/native_scan.h"
#include "opentape/archive_verify.h"
#include "opentape/verify_stream.h"
#include "opentape/archive_catalog.h"
#include "opentape/restore.h"
#ifdef OT_NATIVE_TAPE_API
#include "opentape/native_tape.h"
#endif

int ot_dos_backup(const char *, const char *, const char *, const char *, int);
int ot_dos_restore_command(int, char **);

static void usage(void)
{
    puts(OT_MSG_VERSION);
    puts(OT_MSG_USAGE);
    puts(OT_MSG_READ_USAGE);
    puts(OT_MSG_SCAN_USAGE);
    puts(OT_MSG_VERIFY_USAGE);
    puts(OT_MSG_LIST_USAGE);
    puts(OT_MSG_REWIND_USAGE);
    puts(OT_MSG_DEVICES_USAGE);
#ifdef OT_NATIVE_TAPE_API
    puts(OT_MSG_NATIVE_USAGE);
    puts(OT_MSG_NATIVE_SPACE_USAGE);
    puts(OT_MSG_NATIVE_MEDIA_USAGE);
    puts(OT_MSG_NATIVE_ARCHIVE_USAGE);
#endif
    puts(OT_MSG_UI_USAGE);
    puts(OT_MSG_SELECTION_USAGE);
    puts(OT_MSG_BACKUP_USAGE);
    puts(OT_MSG_TREE_USAGE);
    puts(OT_MSG_RESTORE_USAGE);
    puts(OT_MSG_RESTORE_LIST_USAGE);
    puts(OT_MSG_RESTORE_FILTER_USAGE);
    puts(OT_MSG_PREVIEW_USAGE);
    puts(OT_MSG_COMPARE_USAGE);
    puts(OT_MSG_NOT_IMPLEMENTED);
}

static void put_line(WINDESC *panel, int row, char *text, char color)
{
    WINDESC line;
    unsigned length;

    if (row < 0 || row >= panel->nrows || panel->ncols <= 4) return;
    line = *panel;
    line.row += row;
    line.col += 2;
    line.nrows = 1;
    line.ncols -= 4;
    /* QUILIB copies exactly ncols bytes, not a NUL-terminated string. */
    length = (unsigned)strlen(text);
    if (length < (unsigned)line.ncols) line.ncols = (int)length;
    if (line.ncols > 0) wput_csa(&line, text, color);
}

static void clear_ui(void)
{
    WINDESC screen;

    SETWINDOW(&screen, 0, 0, ScreenSize.row, ScreenSize.col);
    wput_sca(&screen, VIDWORD(0x07, ' '));
    move_cursor(ScreenSize.row - 1, 0);
    show_cursor();
}

/* Required cleanup callback of QUILIB's shared INT 23h/24h support. */
void abortprep(int flag)
{
    (void)flag;
    end_input();
    clear_ui();
}

enum ui_operation { UI_REWIND, UI_VERIFY, UI_CATALOG };
enum ui_phase { UI_IDLE, UI_CONFIRM, UI_RESULT, UI_BUSY };
enum ui_export_phase { EXPORT_IDLE, EXPORT_CONFIRM, EXPORT_BUSY, EXPORT_OK, EXPORT_FAILED, EXPORT_EMPTY, EXPORT_DISABLED };
#define UI_EXPORT_PATH_MAX 40u

struct device_ui_state {
    char alias[7];
    unsigned native_backend;
#ifdef OT_NATIVE_TAPE_API
    struct ot_tape_info_v1 native_status;
    struct ot_tape_control_v1 native_control;
#endif
    struct ot_device_entry entries[4];
    struct ot_device_summary summary;
    struct ot_tape_status status;
    unsigned count, scanned, status_known, dos_error;
    int scan_result, status_result;
    enum ui_operation operation;
    enum ui_phase phase;
    unsigned operation_error, verify_memory;
    int operation_result;
    struct ot_verify_session_result verification;
    struct ot_catalog catalog;
    unsigned catalog_visible, catalog_cursor, catalog_left;
    const char *export_name;
    enum ui_export_phase export_phase;
    int export_result;
    unsigned export_error;
};

static void draw_backend(const struct device_ui_state *state)
{
    char text[80];
    if (state->native_backend && ot_dos_archive_fixed_block() != 0u) {
        sprintf(text, OT_MSG_UI_API_FIXED, ot_dos_archive_fixed_block());
        put_line(&DataWin, 3, text, DataColor);
    } else put_line(&DataWin, 3, state->native_backend ? OT_MSG_UI_API_NATIVE : OT_MSG_UI_API_LEGACY, DataColor);
}

static void export_catalog_marks(struct device_ui_state *state)
{
    struct ot_restore_selection list;
    struct ot_dos_selection_export output;
    memset(&list, 0, sizeof(list)); memset(&output, 0, sizeof(output));
    state->export_result = ot_catalog_selection(&state->catalog, &list);
    if (state->export_result == 0)
        state->export_result = ot_dos_selection_prepare(&output, state->export_name);
    if (state->export_result == 0) state->export_result = ot_dos_selection_save(&output, &list);
    state->export_error = output.dos_error;
    state->export_phase = state->export_result == 0 ? EXPORT_OK : EXPORT_FAILED;
    ot_restore_selection_free(&list);
}

static int ui_device_entry(void *context, const struct ot_device_entry *entry)
{
    struct device_ui_state *state = context;
    if (state->count >= 4u) return -1;
    state->entries[state->count++] = *entry;
    return 0;
}

#ifdef OT_NATIVE_TAPE_API
static int ui_native_scan(struct device_ui_state *state)
{
    struct ot_device_entry entry;
    struct ot_tape_info_v1 info;
    unsigned i;
    memset(&state->summary, 0, sizeof(state->summary));
    for (i = 0u; i < 4u; ++i) {
        memset(&entry, 0, sizeof(entry));
        strcpy(entry.name, state->alias); entry.name[4] = (char)('0' + i);
        entry.result = ot_dos_native_probe(entry.name, 0u, &info, &entry.dos_error);
        ++state->summary.examined;
        if (entry.result == OT_NATIVE_OK) {
            entry.kind = OT_DEVICE_FOUND; ++state->summary.found;
            entry.status.tape.host_id = info.host_id;
            entry.status.tape.target_id = info.target_id;
            entry.status.tape.lun = info.lun;
        } else if (entry.result == OT_NATIVE_OPEN && entry.dos_error == 2u) {
            entry.kind = OT_DEVICE_MISSING; ++state->summary.missing;
        } else if (entry.result == OT_NATIVE_DEVICE) {
            entry.kind = OT_DEVICE_REJECTED; ++state->summary.rejected;
        } else { entry.kind = OT_DEVICE_ERROR; ++state->summary.errors; }
        ui_device_entry(state, &entry);
    }
    if (state->summary.errors || state->summary.rejected) return OT_ENUM_PARTIAL;
    return state->summary.found ? 0 : OT_ENUM_EMPTY;
}
#endif

static void draw_catalog_ui(const struct device_ui_state *state)
{
    const struct ot_catalog_entry *entry;
    unsigned first = (state->catalog_cursor / 10u) * 10u, last, i;
    char text[128];
    last = first + 10u;
    if (last > state->catalog.count) last = state->catalog.count;
    paintBackgroundScreen();
    put_line(&DataWin, 1, OT_MSG_APP_TITLE, DataYellowColor);
    put_line(&DataWin, 2, OT_MSG_UI_CATALOG_TITLE, DataColor);
    draw_backend(state);
    sprintf(text, OT_MSG_UI_SELECTED, state->alias);
    put_line(&DataWin, 4, text, DataYellowColor);
    if (state->export_phase == EXPORT_CONFIRM) strcpy(text, OT_MSG_UI_EXPORT_WARNING);
    else sprintf(text, OT_MSG_UI_CATALOG_PAGE, state->catalog.count, last != 0u ? first + 1u : 0u, last);
    put_line(&DataWin, 5, text, DataColor);
    sprintf(text, OT_MSG_UI_CATALOG_COLUMNS, state->catalog.marked);
    put_line(&DataWin, 6, text, DataColor);
    entry = ot_catalog_at(&state->catalog, first);
    for (i = first; i < last && entry != 0; ++i, entry = entry->next) {
        sprintf(text, "[%c] %c %10lu %.27s", entry->metadata.kind == OT_FILE_DIRECTORY ? '-' : entry->marked ? 'x' : ' ',
            entry->metadata.kind == OT_FILE_DIRECTORY ? 'D' : 'F',
            (unsigned long)entry->metadata.file_size, entry->metadata.path);
        put_line(&DataWin, 7 + (int)(i - first), text,
            i == state->catalog_cursor ? DataYellowColor : DataColor);
    }
    entry = ot_catalog_at(&state->catalog, state->catalog_cursor);
    if (state->export_phase == EXPORT_CONFIRM) {
        sprintf(text, OT_MSG_UI_EXPORT_CONFIRM, state->catalog.marked);
        put_line(&DataWin, 17, text, DataYellowColor);
        put_line(&DataWin, 18, (char *)state->export_name, DataYellowColor);
    } else if (entry != 0) {
        sprintf(text, "session=%lu object=%lu attr=%02X", (unsigned long)entry->session,
            (unsigned long)entry->object, (unsigned)entry->metadata.dos_attributes);
        put_line(&DataWin, 17, text, DataColor);
        sprintf(text, "%03u: %.40s", state->catalog_left, entry->metadata.path + state->catalog_left);
        put_line(&DataWin, 18, text, DataYellowColor);
    }
    if (state->export_phase == EXPORT_FAILED) {
        sprintf(text, OT_MSG_UI_EXPORT_FAILED, state->export_result, state->export_error);
        put_line(&DataWin, 19, text, DataYellowColor);
    } else put_line(&DataWin, 19,
        state->export_phase == EXPORT_CONFIRM ? OT_MSG_UI_REWIND_KEYS :
        state->export_phase == EXPORT_BUSY ? OT_MSG_UI_EXPORT_BUSY :
        state->export_phase == EXPORT_OK ? OT_MSG_UI_EXPORT_OK :
        state->export_phase == EXPORT_EMPTY ? OT_MSG_UI_EXPORT_EMPTY :
        state->export_phase == EXPORT_DISABLED ? OT_MSG_UI_EXPORT_DISABLED : OT_MSG_UI_CATALOG_SCOPE, DataColor);
    put_line(&QuickWin, 1, OT_MSG_HELP_TITLE, QuickBackColor);
    put_line(&QuickWin, 3, OT_MSG_UI_CATALOG_ARROWS, QuickBackColor);
    put_line(&QuickWin, 5, OT_MSG_UI_CATALOG_PAGES, QuickBackColor);
    put_line(&QuickWin, 7, OT_MSG_UI_CATALOG_ENDS, QuickBackColor);
    put_line(&QuickWin, 9, OT_MSG_UI_CATALOG_PATH, QuickBackColor);
    put_line(&QuickWin, 11, OT_MSG_UI_CATALOG_BACK, QuickBackColor);
    put_line(&QuickWin, 12, OT_MSG_UI_CATALOG_MARK, QuickBackColor);
    put_line(&QuickWin, 13, OT_MSG_UI_CATALOG_ALL, QuickBackColor);
    put_line(&QuickWin, 14, OT_MSG_UI_HELP_READONLY, QuickBackColor);
    put_line(&QuickWin, 15, OT_MSG_UI_CATALOG_VIEW, QuickBackColor);
    put_line(&QuickWin, 17, OT_MSG_UI_EXPORT_HELP, QuickBackColor);
    put_line(&QuickWin, 16, OT_MSG_UI_HELP_READY, QuickBackColor);
}

static void draw_device_ui(const struct device_ui_state *state)
{
    char text[128], alias[7];
    unsigned i;
    const struct ot_device_entry *entry;
    if (state->catalog_visible) { draw_catalog_ui(state); return; }
    paintBackgroundScreen();
    put_line(&DataWin, 1, OT_MSG_APP_TITLE, DataYellowColor);
    put_line(&DataWin, 2, OT_MSG_UI_TITLE, DataColor);
    draw_backend(state);
    sprintf(text, OT_MSG_UI_SELECTED, state->alias);
    put_line(&DataWin, 4, text, DataYellowColor);
    put_line(&DataWin, 5, state->operation != UI_REWIND && state->phase == UI_CONFIRM ?
        OT_MSG_UI_VERIFY_SCOPE : OT_MSG_UI_SNAPSHOT, DataColor);
    put_line(&DataWin, 7, OT_MSG_UI_ALIASES, DataColor);
    for (i = 0u; i < 4u; ++i) {
        strcpy(alias, state->alias); alias[4] = (char)('0' + i);
        if (i >= state->count) sprintf(text, "%u %s %s", i, alias, OT_MSG_UI_UNQUERIED);
        else {
            entry = &state->entries[i];
            if (entry->kind == OT_DEVICE_FOUND)
                sprintf(text, "%u %s FOUND ASPI %02X:%02X:%02X", i, entry->name,
                    (unsigned)entry->status.tape.host_id, (unsigned)entry->status.tape.target_id,
                    (unsigned)entry->status.tape.lun);
            else sprintf(text, "%u %s %s rc=%d DOS=%u", i, entry->name,
                entry->kind == OT_DEVICE_MISSING ? "MISSING" : entry->kind == OT_DEVICE_REJECTED ? "REJECTED" : "ERROR",
                entry->result, entry->dos_error);
        }
        put_line(&DataWin, (int)i + 8, text, state->alias[4] == (char)('0' + i) ? DataYellowColor : DataColor);
    }
    if (state->scanned) {
        sprintf(text, OT_MSG_UI_SCAN, state->scanned, state->scan_result, state->summary.found, state->summary.missing);
        put_line(&DataWin, 12, text, DataColor);
        sprintf(text, OT_MSG_UI_SCAN_ERRORS, state->summary.rejected, state->summary.errors);
        put_line(&DataWin, 13, text, DataColor);
    }
    if (state->operation != UI_REWIND && state->phase == UI_RESULT) {
        if (state->verify_memory) strcpy(text, OT_MSG_READ_MEMORY);
        else if (state->operation_result == 0) strcpy(text, OT_MSG_UI_VERIFY_OK);
        else sprintf(text, OT_MSG_UI_VERIFY_ERROR, state->operation_result, state->verification.reader.dos_error);
        put_line(&DataWin, 15, text, DataYellowColor);
        sprintf(text, OT_MSG_UI_VERIFY_COUNTS, (unsigned long)state->verification.verify.framing.blocks,
            (unsigned long)state->verification.verify.framing.records);
        put_line(&DataWin, 16, text, DataColor);
        sprintf(text, OT_MSG_UI_VERIFY_SESSIONS, (unsigned long)state->verification.verify.sessions);
        put_line(&DataWin, 17, text, DataColor);
        put_line(&DataWin, 18, state->operation_result == 0 && !state->verify_memory ?
            OT_MSG_UI_VERIFY_SCOPE : OT_MSG_UI_VERIFY_INCOMPLETE, DataYellowColor);
    } else if (!state->status_known) put_line(&DataWin, 15, OT_MSG_UI_STATUS_UNKNOWN, DataColor);
    else if (state->status_result != 0) {
        sprintf(text, OT_MSG_UI_STATUS_ERROR, state->status_result, state->dos_error);
        put_line(&DataWin, 15, text, DataYellowColor);
    }
#ifdef OT_NATIVE_TAPE_API
    else if (state->native_backend) {
        sprintf(text, "ASPI %02X:%02X:%02X", (unsigned)state->native_status.host_id,
            (unsigned)state->native_status.target_id, (unsigned)state->native_status.lun);
        put_line(&DataWin, 15, text, DataColor);
        sprintf(text, OT_MSG_UI_NATIVE_STATE, state->native_status.session_state,
            (unsigned long)state->native_status.valid_fields);
        put_line(&DataWin, 16, text, DataColor);
        sprintf(text, OT_MSG_UI_NATIVE_VERSION, state->native_status.version);
        put_line(&DataWin, 17, text, DataColor);
        if ((state->native_status.session_state & OT_TAPE_SESSION_NATIVE_FAILED) != 0u)
            put_line(&DataWin, 18, OT_MSG_UI_NATIVE_FAILED, DataYellowColor);
        else if ((state->native_status.session_state & OT_TAPE_SESSION_NATIVE_WRITER) != 0u)
            put_line(&DataWin, 18, OT_MSG_UI_NATIVE_WRITER, DataYellowColor);
    }
#endif
    else {
        sprintf(text, "ASPI %02X:%02X:%02X state=%04X block=%u", (unsigned)state->status.tape.host_id,
            (unsigned)state->status.tape.target_id, (unsigned)state->status.tape.lun,
            (unsigned)state->status.tape.current_state, (unsigned)state->status.tape.fixed_block_size);
        put_line(&DataWin, 15, text, DataColor);
        if (state->status.extended) sprintf(text, "ABI=16 driver=%u", (unsigned)state->status.tape.driver_error);
        else strcpy(text, "ABI=14 driver=--");
        put_line(&DataWin, 16, text, DataColor);
        sprintf(text, "SCSI=%02X sense=%02X op=%04X rest=%u", (unsigned)state->status.tape.last_scsi_status,
            (unsigned)state->status.tape.last_sense_key, (unsigned)state->status.tape.last_scsi_opcode,
            (unsigned)state->status.tape.residual_bytes);
        put_line(&DataWin, 17, text, DataColor);
    }
    if (state->phase == UI_CONFIRM) {
        sprintf(text, state->operation == UI_REWIND ? OT_MSG_UI_REWIND_CONFIRM :
            state->operation == UI_CATALOG ? OT_MSG_UI_CATALOG_CONFIRM : OT_MSG_UI_VERIFY_CONFIRM, state->alias);
        put_line(&DataWin, 18, text, DataYellowColor);
        put_line(&DataWin, 19, OT_MSG_UI_REWIND_KEYS, DataYellowColor);
    } else if (state->phase == UI_RESULT && state->operation == UI_REWIND) {
        if (state->operation_result == 0) strcpy(text, OT_MSG_UI_REWIND_OK);
        else sprintf(text, OT_MSG_UI_REWIND_ERROR, state->operation_result, state->operation_error);
        put_line(&DataWin, 19, text, DataYellowColor);
#ifdef OT_NATIVE_TAPE_API
        if (state->native_backend && state->operation_result != 0 &&
            (state->native_control.completion_flags & OT_TAPE_POSITION_UNKNOWN) != 0u)
            put_line(&DataWin, 18, OT_MSG_UI_NATIVE_POSITION, DataYellowColor);
#endif
    } else if (state->phase == UI_BUSY)
        put_line(&DataWin, 19, state->operation != UI_REWIND ? OT_MSG_UI_VERIFY_BUSY : OT_MSG_UI_REWIND_BUSY, DataYellowColor);
    else if (state->phase == UI_RESULT && state->operation != UI_REWIND)
        put_line(&DataWin, 19, OT_MSG_UI_VERIFY_POSITION, DataColor);
    else put_line(&DataWin, 19, state->native_backend ? OT_MSG_UI_NATIVE_TRANSPORT : OT_MSG_UI_TRANSPORT, DataColor);
    put_line(&QuickWin, 1, OT_MSG_HELP_TITLE, QuickBackColor);
    put_line(&QuickWin, 3, OT_MSG_UI_HELP_DEVICES, QuickBackColor);
    put_line(&QuickWin, 5, OT_MSG_UI_HELP_SELECT, QuickBackColor);
    put_line(&QuickWin, 7, OT_MSG_UI_HELP_STATUS, QuickBackColor);
    put_line(&QuickWin, 9, OT_MSG_UI_HELP_ENTER, QuickBackColor);
    put_line(&QuickWin, 11, OT_MSG_UI_HELP_EXIT, QuickBackColor);
    put_line(&QuickWin, 13, OT_MSG_UI_HELP_REWIND, QuickBackColor);
    put_line(&QuickWin, 14, OT_MSG_UI_HELP_READONLY, QuickBackColor);
    put_line(&QuickWin, 15, OT_MSG_UI_HELP_VERIFY, QuickBackColor);
    put_line(&QuickWin, 17, OT_MSG_UI_HELP_CATALOG, QuickBackColor);
    put_line(&QuickWin, 16, OT_MSG_UI_HELP_READY, QuickBackColor);
}

static void show_ui(const char *alias, const char *export_name)
{
    struct device_ui_state state;
    INPUTEVENT event;
    unsigned key, i;
    const struct ot_catalog_entry *entry;
    oa_u8 *buffer;
    memset(&state, 0, sizeof(state)); strcpy(state.alias, alias);
    state.export_name = export_name;
    state.native_backend = ot_dos_archive_is_native();
    for (i = 0u; state.alias[i] != 0; ++i)
        if (state.alias[i] >= 'a' && state.alias[i] <= 'z') state.alias[i] = (char)(state.alias[i] - 'a' + 'A');
    initializeScreenInfo(FALSE);
    init_input(NULL);
    hide_cursor();
    draw_device_ui(&state);
    for (;;) {
        if (!get_input(INPUT_WAIT, &event) || event.key == 0) continue;
        key = event.key;
        if (state.catalog_visible) {
            if (state.export_phase == EXPORT_CONFIRM) {
                state.export_phase = EXPORT_IDLE;
                if (key == OT_UI_CONFIRM_KEY || key == OT_UI_CONFIRM_KEY - 'a' + 'A') {
                    state.export_phase = EXPORT_BUSY;
                    draw_device_ui(&state);
                    export_catalog_marks(&state);
                }
                draw_device_ui(&state);
                continue;
            }
            if (key == 'e' || key == 'E') {
                state.export_phase = state.export_name == 0 ? EXPORT_DISABLED :
                    state.catalog.marked == 0u ? EXPORT_EMPTY : EXPORT_CONFIRM;
                draw_device_ui(&state);
                continue;
            }
            i = state.catalog_cursor;
            if (key == KEY_ESCAPE || key == 'b' || key == 'B') {
                ot_catalog_free(&state.catalog);
                state.catalog_visible = 0u; state.phase = UI_IDLE;
            } else if (state.catalog.count == 0u) continue;
            else if (key == KEY_UP && i > 0u) --state.catalog_cursor;
            else if (key == KEY_DOWN && i + 1u < state.catalog.count) ++state.catalog_cursor;
            else if (key == KEY_PGUP) state.catalog_cursor = i >= 10u ? i - 10u : 0u;
            else if (key == KEY_PGDN) state.catalog_cursor = i + 10u < state.catalog.count ? i + 10u : state.catalog.count - 1u;
            else if (key == KEY_HOME) state.catalog_cursor = 0u;
            else if (key == KEY_END) state.catalog_cursor = state.catalog.count - 1u;
            else if (key == ' ') {
                entry = ot_catalog_at(&state.catalog, i);
                if (entry != 0 && entry->metadata.kind == OT_FILE_REGULAR)
                    ot_catalog_mark(&state.catalog, i, !entry->marked);
            } else if (key == 'a' || key == 'A') ot_catalog_mark_all(&state.catalog, 1u);
            else if (key == 'n' || key == 'N') ot_catalog_mark_all(&state.catalog, 0u);
            else if (key == KEY_LEFT) state.catalog_left = state.catalog_left >= 8u ? state.catalog_left - 8u : 0u;
            else if (key == KEY_RIGHT) {
                entry = ot_catalog_at(&state.catalog, i);
                if (entry != 0 && state.catalog_left + 1u < strlen(entry->metadata.path)) {
                    state.catalog_left += 8u;
                    if (state.catalog_left >= strlen(entry->metadata.path))
                        state.catalog_left = (unsigned)strlen(entry->metadata.path) - 1u;
                }
            } else continue;
            state.export_phase = EXPORT_IDLE;
            if (i != state.catalog_cursor) state.catalog_left = 0u;
            draw_device_ui(&state);
            continue;
        }
        if (state.phase == UI_CONFIRM) {
            state.phase = UI_IDLE;
            /* Confirmation consumes one key. Never reinterpret a cancel as
             * selection, another operation or application exit. */
            if (key == OT_UI_CONFIRM_KEY || key == OT_UI_CONFIRM_KEY - 'a' + 'A') {
                state.status_known = 0u;
                memset(&state.status, 0, sizeof(state.status));
                state.count = state.scanned = 0u;
                state.phase = UI_BUSY;
                draw_device_ui(&state);
                if (state.operation == UI_REWIND) {
#ifdef OT_NATIVE_TAPE_API
                    if (state.native_backend)
                        state.operation_result = ot_dos_native_execute(state.alias, OT_TAPE_CONTROL_REWIND,
                            &state.native_control, &state.operation_error);
                    else
#endif
                        state.operation_result = ot_dos_tape_rewind(state.alias, &state.status, &state.operation_error);
                } else {
                    memset(&state.verification, 0, sizeof(state.verification));
                    buffer = malloc(OT_BLOCK_RECOMMENDED_SIZE);
                    state.verify_memory = buffer == 0;
                    state.operation_result = OT_READ_BAD_ARGUMENT;
                    if (buffer != 0) {
                        state.operation_result = ot_dos_archive_verify(state.alias,
                            buffer, OT_BLOCK_RECOMMENDED_SIZE,
                            state.operation == UI_CATALOG ? ot_catalog_capture : 0,
                            &state.catalog, &state.verification);
                        free(buffer);
                    }
                    if (state.operation == UI_CATALOG) {
                        if (state.operation_result == 0 && !state.verify_memory) {
                            state.catalog_visible = 1u;
                            state.catalog_cursor = state.catalog_left = 0u;
                            state.export_phase = EXPORT_IDLE;
                        } else {
                            if (state.operation_result == OT_VERIFY_VISITOR_FAILED && state.catalog.error != 0)
                                state.operation_result = state.catalog.error;
                            ot_catalog_free(&state.catalog);
                        }
                    }
                }
                /* The operation's status is diagnostic, not a fresh UI query. */
                memset(&state.status, 0, sizeof(state.status));
                state.phase = UI_RESULT;
            }
            draw_device_ui(&state);
            continue;
        }
        if (key == KEY_ESCAPE) break;
        if (key == KEY_UP) key = state.alias[4] == '0' ? '3' : state.alias[4] - 1;
        else if (key == KEY_DOWN) key = state.alias[4] == '3' ? '0' : state.alias[4] + 1;
        if (!((key >= '0' && key <= '3') || key == 'd' || key == 'D' ||
              key == 'r' || key == 'R' || key == 'v' || key == 'V' ||
              key == 'l' || key == 'L' ||
              key == 's' || key == 'S' || key == KEY_ENTER)) continue;
        state.phase = UI_IDLE;
        if (key >= '0' && key <= '3') {
            state.alias[4] = (char)key; state.status_known = 0u;
            memset(&state.status, 0, sizeof(state.status));
        } else if (key == 'd' || key == 'D') {
            state.count = 0u;
            state.status_known = 0u;
            memset(&state.status, 0, sizeof(state.status));
#ifdef OT_NATIVE_TAPE_API
            if (state.native_backend) state.scan_result = ui_native_scan(&state);
            else
#endif
            state.scan_result = ot_tape_enumerate(&ot_dos_reader_ops()->probe, state.alias[2],
                state.alias[5] == '$', ui_device_entry, &state, &state.summary);
            if (state.scanned < 65535u) ++state.scanned;
        } else if (key == 'r' || key == 'R') {
            state.operation = UI_REWIND;
            state.phase = UI_CONFIRM;
        } else if (key == 'v' || key == 'V') {
            state.operation = UI_VERIFY;
            state.phase = UI_CONFIRM;
        } else if (key == 'l' || key == 'L') {
            state.operation = UI_CATALOG;
            state.phase = UI_CONFIRM;
        } else if (key == 's' || key == 'S' || key == KEY_ENTER) {
            memset(&state.status, 0, sizeof(state.status));
#ifdef OT_NATIVE_TAPE_API
            if (state.native_backend)
                state.status_result = ot_dos_native_probe(state.alias, 0u, &state.native_status, &state.dos_error);
            else
#endif
                state.status_result = ot_dos_tape_probe(state.alias, &state.status, &state.dos_error);
            state.status_known = 1u;
        } else continue;
        draw_device_ui(&state);
    }
    ot_catalog_free(&state.catalog);
    end_input();
    clear_ui();
}

static int print_device_entry(void *context, const struct ot_device_entry *entry)
{
    int written;
    char driver[8];
    (void)context;
    if (entry->kind == OT_DEVICE_FOUND) {
        if (entry->status.extended) sprintf(driver, "%u", (unsigned)entry->status.tape.driver_error);
        else strcpy(driver, "--");
        written = printf("FOUND %s ASPI %02X:%02X:%02X state=%04X block=%u ABI=%u driver=%s\n",
            entry->name, (unsigned)entry->status.tape.host_id, (unsigned)entry->status.tape.target_id,
            (unsigned)entry->status.tape.lun, (unsigned)entry->status.tape.current_state,
            (unsigned)entry->status.tape.fixed_block_size, entry->status.extended ? 16u : 14u,
            driver);
    }
    else written = printf("%s %s rc=%d DOS=%u\n", entry->kind == OT_DEVICE_MISSING ? "MISSING" :
        entry->kind == OT_DEVICE_REJECTED ? "REJECTED" : "ERROR", entry->name,
        entry->result, entry->dos_error);
    return written < 0 || fflush(stdout) != 0 || ferror(stdout) ? -1 : 0;
}

static int enumerate_devices(char letter, unsigned dollar)
{
    struct ot_device_summary summary;
    int rc;
    if (puts(OT_MSG_DEVICES_WARNING) == EOF || fflush(stdout) != 0 || ferror(stdout)) return 2;
    rc = ot_tape_enumerate(&ot_dos_reader_ops()->probe, letter, dollar, print_device_entry, 0, &summary);
    printf(OT_MSG_DEVICES_COUNTS, summary.examined, summary.found, summary.missing,
           summary.rejected, summary.errors);
    if (fflush(stdout) != 0 || ferror(stdout) || (rc != 0 && rc != OT_ENUM_EMPTY)) {
        fputs(OT_MSG_DEVICES_INCOMPLETE "\n", stderr); return 2;
    }
    puts(OT_MSG_DEVICES_COMPLETE);
    if (fflush(stdout) != 0 || ferror(stdout)) {
        fputs(OT_MSG_DEVICES_INCOMPLETE "\n", stderr); return 2;
    }
    return rc == OT_ENUM_EMPTY ? 3 : 0;
}

static int print_tape_status(const char *name)
{
    struct ot_tape_status status;
    unsigned dos_error;
    int rc;

    rc = ot_dos_tape_probe(name, &status, &dos_error);
    if (rc != OT_PROBE_OK) {
        fprintf(stderr, OT_MSG_PROBE_ERROR, name, rc, dos_error);
        return 2;
    }
    printf(OT_MSG_PROBE_STATUS, name, (unsigned)status.tape.host_id,
           (unsigned)status.tape.target_id, (unsigned)status.tape.lun,
           (unsigned)status.tape.current_state,
           (unsigned)status.tape.fixed_block_size);
    printf(OT_MSG_PROBE_SCSI, (unsigned)status.tape.last_scsi_status,
           (unsigned)status.tape.last_sense_key,
           (unsigned)status.tape.last_scsi_opcode,
           (unsigned)status.tape.residual_bytes);
    if (status.extended) {
        printf(OT_MSG_PROBE_ABI, 16u, (unsigned)status.tape.driver_error);
    } else {
        puts(OT_MSG_PROBE_LEGACY);
    }
    puts(OT_MSG_PROBE_READONLY);
    return 0;
}

static const char *scan_error_reason(int rc)
{
    switch (rc) {
    case OT_VERIFY_ORDER: return OT_MSG_VERIFY_ORDER;
    case OT_VERIFY_FILE_DATA: return OT_MSG_VERIFY_DATA;
    case OT_VERIFY_FILE_CRC: return OT_MSG_SCAN_CRC;
    case OT_VERIFY_SUMMARY: return OT_MSG_VERIFY_SUMMARY;
    case OT_VERIFY_INCOMPLETE: return OT_MSG_VERIFY_INCOMPLETE;
    case OT_VERIFY_UNSUPPORTED: return OT_MSG_VERIFY_UNKNOWN;
    case OT_VERIFY_OVERFLOW: return OT_MSG_SCAN_SEQUENCE;
    case OT_FORMAT_BAD_MAGIC: return OT_MSG_SCAN_MAGIC;
    case OT_FORMAT_BAD_VERSION: return OT_MSG_SCAN_VERSION;
    case OT_FORMAT_BAD_HEADER_CRC:
    case OT_FORMAT_BAD_PAYLOAD_CRC:
    case OT_SCAN_RECORD_CRC: return OT_MSG_SCAN_CRC;
    case OT_SCAN_BAD_SEQUENCE: return OT_MSG_SCAN_SEQUENCE;
    case OT_SCAN_MISSING_LABEL: return OT_MSG_SCAN_NO_LABEL;
    case OT_SCAN_BAD_LABEL: return OT_MSG_SCAN_BAD_LABEL;
    default: return OT_MSG_SCAN_SIZE;
    }
}

static int print_archive_entry(void *context, oa_u32 session, oa_u32 object,
                                const struct ot_file_metadata *m)
{
    unsigned year = m->dos_date != 0u ? 1980u + (m->dos_date >> 9) : 0u;
    if (context != 0 && m->kind != OT_FILE_DIRECTORY &&
        ot_restore_selection_add((struct ot_restore_selection *)context, m->path) != 0)
        return -1;
    return printf("%c %10lu %02X %04u-%02u-%02u %02u:%02u:%02u %lu:%lu %s\n",
        m->kind == OT_FILE_DIRECTORY ? 'D' : 'F', (unsigned long)m->file_size,
        (unsigned)m->dos_attributes, year, (unsigned)((m->dos_date >> 5) & 15u),
        (unsigned)(m->dos_date & 31u), (unsigned)(m->dos_time >> 11),
        (unsigned)((m->dos_time >> 5) & 63u), (unsigned)((m->dos_time & 31u) * 2u),
        (unsigned long)session, (unsigned long)object, m->path) < 0 ? -1 : 0;
}

static int verify_tape_session(const char *name, int listing, const char *selection_name)
{
    struct ot_verify_session_result result;
    struct ot_restore_selection selection;
    struct ot_dos_selection_export output;
    oa_u8 *buffer;
    int rc;
    memset(&result, 0, sizeof(result));
    memset(&selection, 0, sizeof(selection));
    if (selection_name != 0) {
        rc = ot_dos_selection_prepare(&output, selection_name);
        if (rc != 0) {
            fprintf(stderr, OT_MSG_SELECTION_ERROR, selection_name, rc, output.dos_error);
            return 2;
        }
    }
    buffer = malloc(OT_BLOCK_RECOMMENDED_SIZE);
    if (buffer == 0) { puts(OT_MSG_READ_MEMORY); return 2; }
    rc = 0;
    if (puts(OT_MSG_VERIFY_ALL_WARNING) == EOF ||
        (listing && puts(OT_MSG_LIST_HEADER) == EOF) ||
        fflush(stdout) != 0 || ferror(stdout)) rc = OT_VERIFY_VISITOR_FAILED;
    if (rc == 0) rc = ot_dos_archive_verify(name, buffer,
        OT_BLOCK_RECOMMENDED_SIZE, listing ? print_archive_entry : 0,
        selection_name != 0 ? &selection : 0, &result);
    if (listing && rc == 0 && (fflush(stdout) != 0 || ferror(stdout))) rc = OT_VERIFY_VISITOR_FAILED;
    free(buffer);
    if (rc == 0 && selection_name != 0) {
        rc = ot_dos_selection_save(&output, &selection);
        if (rc != 0) {
            fprintf(stderr, OT_MSG_SELECTION_ERROR, selection_name, rc, output.dos_error);
            fprintf(stderr, OT_MSG_SELECTION_STAGE, output.temporary);
            ot_restore_selection_free(&selection);
            return 2;
        }
        printf(OT_MSG_SELECTION_SAVED, selection_name, selection.count);
    }
    ot_restore_selection_free(&selection);
    if (rc != 0) {
        if (listing) fputs(OT_MSG_LIST_INCOMPLETE, stderr);
        if (rc == OT_VERIFY_VISITOR_FAILED) fputs(OT_MSG_LIST_OUTPUT_ERROR, stderr);
        else if (result.transport_error) fprintf(stderr, OT_MSG_PROBE_ERROR, name, rc, result.reader.dos_error);
        else printf(OT_MSG_SCAN_ERROR, rc, scan_error_reason(rc), (unsigned long)result.verify.framing.blocks);
        printf(OT_MSG_PROBE_SCSI, (unsigned)result.reader.status.tape.last_scsi_status,
               (unsigned)result.reader.status.tape.last_sense_key,
               (unsigned)result.reader.status.tape.last_scsi_opcode,
               (unsigned)result.reader.status.tape.residual_bytes);
        return 2;
    }
    printf(OT_MSG_SCAN_EOF, (unsigned long)result.verify.framing.blocks, (unsigned long)result.verify.framing.records);
    printf(OT_MSG_VERIFY_OK, (unsigned long)result.verify.sessions);
    puts(OT_MSG_VERIFY_ALL_SCOPE);
    if (listing) {
        puts(OT_MSG_LIST_COMPLETE);
        if (fflush(stdout) != 0 || ferror(stdout)) {
            fputs(OT_MSG_LIST_INCOMPLETE, stderr); return 2;
        }
    }
    return 0;
}

static int read_tape_test(const char *name, int native)
{
    struct ot_tape_reader reader;
    struct ot_native_scan scan;
    struct ot_archive_verify verify;
    char label_name[33];
    unsigned char *buffer;
    unsigned count, bytes, capacity, block_size, i;
    unsigned long total;
    int rc, exit_code;

    buffer = malloc(OT_BLOCK_RECOMMENDED_SIZE);
    if (buffer == 0) {
        fputs(OT_MSG_READ_MEMORY, stderr);
        return 2;
    }
    memset(&reader, 0, sizeof(reader));
    rc = ot_dos_archive_reader_open(&reader, name);
    if (rc != OT_PROBE_OK) {
        fprintf(stderr, OT_MSG_PROBE_ERROR, name, rc, reader.dos_error);
        free(buffer);
        return 2;
    }
    capacity = OT_BLOCK_RECOMMENDED_SIZE;
    block_size = reader.status.tape.fixed_block_size;
    memset(&scan, 0, sizeof(scan));
    memset(&verify, 0, sizeof(verify));
    if (native && (reader.status.tape.device_parameters & OA_MT_PARAM_FIXED_BLOCK) != 0u) {
        fprintf(stderr, OT_MSG_PROBE_ERROR, name, OT_READ_BAD_MODE, 0u);
        ot_reader_close(&reader);
        free(buffer);
        return 2;                       /* fixed transport reassembly is pending */
    }
    if ((reader.status.tape.device_parameters & OA_MT_PARAM_FIXED_BLOCK) != 0u)
        capacity = (capacity / block_size) * block_size;
    puts(OT_MSG_READ_WARNING);
    total = 0ul;
    exit_code = 3;
    for (count = 0u; count < 16u; ++count) {
        rc = ot_reader_read(&reader, buffer, capacity, &bytes);
        if (rc == OT_READ_FILEMARK) {
            if (native) {
                if (!scan.have_label) {
                    printf(OT_MSG_SCAN_ERROR, OT_SCAN_MISSING_LABEL,
                           scan_error_reason(OT_SCAN_MISSING_LABEL), 0ul);
                    exit_code = 2;
                } else if (native == 2 && (rc = ot_archive_verify_finish(&verify)) != OT_FORMAT_OK) {
                    printf(OT_MSG_SCAN_ERROR, rc, scan_error_reason(rc),
                           (unsigned long)scan.blocks);
                    exit_code = 2;
                } else {
                    printf(OT_MSG_SCAN_EOF, (unsigned long)scan.blocks,
                           (unsigned long)scan.records);
                    if (native == 2) printf(OT_MSG_VERIFY_OK, (unsigned long)verify.sessions);
                    puts(OT_MSG_SCAN_PARTIAL);
                    exit_code = 0;
                }
                break;
            }
            printf(OT_MSG_READ_EOF, count, total);
            exit_code = 0;
            break;
        }
        if (rc != OT_READ_BLOCK) {
            fprintf(stderr, OT_MSG_PROBE_ERROR, name, rc, reader.dos_error);
            printf(OT_MSG_PROBE_SCSI, (unsigned)reader.status.tape.last_scsi_status,
                   (unsigned)reader.status.tape.last_sense_key,
                   (unsigned)reader.status.tape.last_scsi_opcode,
                   (unsigned)reader.status.tape.residual_bytes);
            exit_code = 2;
            break;
        }
        if (native) {
            if (native == 2) {
                rc = ot_archive_verify_block(&verify, buffer, (oa_u16)bytes);
                scan = verify.framing;
            } else rc = ot_native_scan_block(&scan, buffer, (oa_u16)bytes);
            if (rc != OT_FORMAT_OK) {
                printf(OT_MSG_SCAN_ERROR, rc, scan_error_reason(rc),
                       (unsigned long)scan.blocks);
                exit_code = 2;
                break;
            }
            if (scan.blocks == 1ul) {
                memset(label_name, 0, sizeof(label_name));
                for (i = 0u; i < 32u && scan.label.volume_name[i] != 0; ++i) {
                    unsigned char c = (unsigned char)scan.label.volume_name[i];
                    label_name[i] = c >= 32u && c <= 126u ? (char)c : '.';
                }
                printf(OT_MSG_SCAN_LABEL, label_name,
                       (unsigned long)scan.label.volume_sequence);
            }
            printf(OT_MSG_SCAN_BLOCK, (unsigned long)scan.last_block.block_number,
                   (unsigned)scan.last_block.record_count);
        } else {
            printf(OT_MSG_READ_BLOCK, count, bytes,
                   (unsigned long)ot_crc32(buffer, (oa_u16)bytes));
        }
        total += bytes;
    }
    if (exit_code == 3) puts(OT_MSG_READ_LIMIT);
    if (ot_reader_close(&reader) != OT_PROBE_OK) {
        fprintf(stderr, OT_MSG_PROBE_ERROR, name, OT_PROBE_CLOSE_FAILED,
                reader.dos_error);
        exit_code = 2;
    }
    free(buffer);
    return exit_code;
}

int main(int argc, char **argv)
{
#ifdef OT_NATIVE_TAPE_API
    if (argc > 1 && strcmp(argv[1], "--native") == 0) {
        int i, skip = 1;
        unsigned block = 0u;
        if (argc > 2 && strcmp(argv[2], "--fixed-block") == 0) {
            const char *digit;
            if (argc < 5 || argv[3][0] == 0) { usage(); return 1; }
            for (digit = argv[3]; *digit; ++digit) {
                if (*digit < '0' || *digit > '9') { usage(); return 1; }
                block = block * 10u + (unsigned)(*digit - '0');
                if (block > OT_DOS_ARCHIVE_FIXED_MAX) { usage(); return 1; }
            }
            if (block == 0u) { usage(); return 1; }
            skip = 3;
        }
        if (argc <= skip + 1 || (strcmp(argv[skip + 1], "--ui") != 0 && strcmp(argv[skip + 1], "--backup-file") != 0 && strcmp(argv[skip + 1], "--backup-tree") != 0 &&
            strcmp(argv[skip + 1], "--read-test") != 0 && strcmp(argv[skip + 1], "--scan") != 0 &&
            strcmp(argv[skip + 1], "--verify") != 0 && strcmp(argv[skip + 1], "--list") != 0 &&
            strcmp(argv[skip + 1], "--restore") != 0 && strcmp(argv[skip + 1], "--compare") != 0)) { usage(); return 1; }
        for (i = 1; i + skip <= argc; ++i) argv[i] = argv[i + skip];
        argc -= skip;
        if (block != 0u) (void)ot_dos_archive_select_native_fixed(block);
        else ot_dos_archive_select_native();
    }
    if (argc > 1 && (strcmp(argv[1], "--native-status") == 0 ||
        strcmp(argv[1], "--native-ready") == 0 || strcmp(argv[1], "--native-rewind") == 0 ||
        strcmp(argv[1], "--native-space-records") == 0 || strcmp(argv[1], "--native-space-filemarks") == 0 ||
        strcmp(argv[1], "--native-eod") == 0 || strcmp(argv[1], "--native-load") == 0 ||
        strcmp(argv[1], "--native-unload") == 0))
        return ot_dos_native_command(argc, argv);
#endif
    if (argc > 1 && strcmp(argv[1], "--ui") == 0) {
        const char *alias = "NRMT0";
        const char *export_name = 0;
        int i, seen_device = 0;
        for (i = 2; i + 1 < argc; i += 2) {
            if (!seen_device && strcmp(argv[i], "--device") == 0) {
                alias = argv[i + 1]; seen_device = 1;
            } else if (export_name == 0 && strcmp(argv[i], "--selection-out") == 0)
                export_name = argv[i + 1];
            else break;
        }
        if (i != argc || !ot_probe_raw_name_valid(alias) ||
            (export_name != 0 && (*export_name == 0 || strlen(export_name) > UI_EXPORT_PATH_MAX))) {
            usage(); return 1;
        }
        show_ui(alias, export_name);
        return 0;
    }

    if (argc > 1 && strcmp(argv[1], "--devices") == 0) {
        char letter = 'M';
        unsigned dollar = 0u;
        int i, seen_letter = 0;
        for (i = 2; i < argc;) {
            if (strcmp(argv[i], "--dollar") == 0 && !dollar) { dollar = 1u; ++i; }
            else if (strcmp(argv[i], "--letter") == 0 && !seen_letter && i + 1 < argc &&
                strlen(argv[i + 1]) == 1u) {
                letter = argv[i + 1][0]; seen_letter = 1; i += 2;
            } else break;
        }
        if (i == argc && ((letter >= 'A' && letter <= 'Z') || (letter >= 'a' && letter <= 'z')))
            return enumerate_devices(letter, dollar);
        usage(); return 1;
    }

    if (argc > 1 && strcmp(argv[1], "--rewind") == 0) {
        const char *device = "NRMT0";
        struct ot_tape_status status;
        unsigned error;
        int rc;
        if (argc == 4 && strcmp(argv[2], "--device") == 0) device = argv[3];
        else if (argc != 2) { usage(); return 1; }
        if (!ot_probe_raw_name_valid(device)) {
            usage(); return 1;
        }
        memset(&status, 0, sizeof(status));
        printf(OT_MSG_REWIND_WARNING, device);
        if (fflush(stdout) != 0 || ferror(stdout)) return 2;
        rc = ot_dos_tape_rewind(device, &status, &error);
        if (rc != 0) {
            fprintf(stderr, OT_MSG_PROBE_ERROR, device, rc, error);
            printf(OT_MSG_PROBE_SCSI, (unsigned)status.tape.last_scsi_status,
                (unsigned)status.tape.last_sense_key, (unsigned)status.tape.last_scsi_opcode,
                (unsigned)status.tape.residual_bytes);
            return 2;
        }
        printf(OT_MSG_REWIND_OK, device);
        return fflush(stdout) != 0 || ferror(stdout) ? 2 : 0;
    }

    if (argc > 1 && strcmp(argv[1], "--list") == 0) {
        const char *device = "NRMT0", *out = 0;
        int i, seen_device = 0;
        for (i = 2; i < argc; i += 2) {
            if (i + 1 >= argc) break;
            if (strcmp(argv[i], "--device") == 0 && !seen_device) {
                device = argv[i + 1]; seen_device = 1;
            } else if (strcmp(argv[i], "--selection-out") == 0 && out == 0) out = argv[i + 1];
            else break;
        }
        if (i == argc && ot_probe_name_valid(device) && (device[1] == 'R' || device[1] == 'r'))
            return verify_tape_session(device, 1, out);
        usage(); return 1;
    }

    if (argc > 1 && (strcmp(argv[1], "--restore") == 0 || strcmp(argv[1], "--compare") == 0)) {
        int rc = ot_dos_restore_command(argc, argv);
        if (rc == 1) usage();
        return rc;
    }

    if (argc > 1 && (strcmp(argv[1], "--backup-file") == 0 || strcmp(argv[1], "--backup-tree") == 0)) {
        if (argc == 10 && strcmp(argv[3], "--as") == 0 &&
            strcmp(argv[5], "--device") == 0 && strcmp(argv[7], "--overwrite") == 0 &&
            strcmp(argv[8], "--volume") == 0)
            return ot_dos_backup(argv[2], argv[4], argv[6], argv[9], strcmp(argv[1], "--backup-tree") == 0);
        usage();
        return 1;
    }

    if (argc > 1 && (strcmp(argv[1], "--read-test") == 0 ||
                     strcmp(argv[1], "--scan") == 0 || strcmp(argv[1], "--verify") == 0)) {
        int native = strcmp(argv[1], "--verify") == 0 ? 2 : strcmp(argv[1], "--scan") == 0;
        if (native == 2 && argc >= 3 && strcmp(argv[2], "--all") == 0) {
            if (argc == 3) return verify_tape_session("NRMT0", 0, 0);
            if (argc == 5 && strcmp(argv[3], "--device") == 0 &&
                ot_probe_name_valid(argv[4]) && (argv[4][1] == 'R' || argv[4][1] == 'r'))
                return verify_tape_session(argv[4], 0, 0);
            usage();
            return 1;
        }
        if (argc == 2) return read_tape_test("NRMT0", native);
        if (argc == 4 && strcmp(argv[2], "--device") == 0 &&
            ot_probe_name_valid(argv[3]) &&
            (argv[3][1] == 'R' || argv[3][1] == 'r'))
            return read_tape_test(argv[3], native);
        usage();
        return 1;
    }
    if (argc > 1 && (strcmp(argv[1], "--status") == 0 ||
                     strcmp(argv[1], "/STATUS") == 0)) {
        if (argc == 2) return print_tape_status("NRMT0");
        if (argc == 4 && strcmp(argv[2], "--device") == 0 &&
            ot_probe_name_valid(argv[3])) return print_tape_status(argv[3]);
        usage();
        return 1;
    }
    if (argc > 2 || (argc > 1 && strcmp(argv[1], "--help") != 0 &&
        strcmp(argv[1], "/?") != 0)) {
        fputs(OT_MSG_UNKNOWN_OPTION, stderr);
        fputc('\n', stderr);
        usage();
        return 1;
    }

    if (argc > 1) {
        usage();
    } else {
        show_ui("NRMT0", 0);
    }
    return 0;
}

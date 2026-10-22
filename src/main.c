#include <stdio.h>
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
#include "opentape/tape_probe.h"

static void usage(void)
{
    puts(OT_MSG_VERSION);
    puts(OT_MSG_USAGE);
    puts(OT_MSG_NOT_IMPLEMENTED);
}

static void put_line(WINDESC *panel, int row, char *text, char color)
{
    WINDESC line;

    line = *panel;
    line.row += row;
    line.col += 2;
    line.nrows = 1;
    line.ncols -= 4;
    wput_csa(&line, text, color);
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

static void show_ui(enum ot_backend_kind selected)
{
    char backend[48];

    initializeScreenInfo(FALSE);
    init_input(NULL);
    hide_cursor();
    paintBackgroundScreen();

    put_line(&DataWin, 1, OT_MSG_APP_TITLE, DataYellowColor);
    put_line(&DataWin, 3, OT_MSG_TASK_SUMMARY, DataColor);
    put_line(&DataWin, 5, OT_MSG_NOT_IMPLEMENTED, DataColor);
    sprintf(backend, OT_MSG_BACKEND_FMT, ot_backend_name(selected));
    put_line(&DataWin, 7, backend, DataColor);

    put_line(&QuickWin, 1, OT_MSG_HELP_TITLE, QuickBackColor);
    put_line(&QuickWin, 3, OT_MSG_ESCAPE_DOS, QuickBackColor);

    pressAnyKey(INPUT_WAIT);
    end_input();
    clear_ui();
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

int main(int argc, char **argv)
{
    enum ot_backend_kind selected;

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

    selected = ot_backend_select(0u, OT_BACKEND_AUTO);
    if (argc > 1) {
        usage();
        printf(OT_MSG_BACKEND_FMT "\n", ot_backend_name(selected));
    } else {
        show_ui(selected);
    }
    return 0;
}

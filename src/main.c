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

int main(int argc, char **argv)
{
    enum ot_backend_kind selected;

    if (argc > 1 && strcmp(argv[1], "--help") != 0 &&
        strcmp(argv[1], "/?") != 0) {
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

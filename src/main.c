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

static void usage(void)
{
    puts("OpenTape 0.1 bootstrap");
    puts("Usage: OPENTAPE [--help]");
    puts("Tape access is not implemented in this bootstrap build.");
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

    put_line(&DataWin, 1, "OpenTape", DataYellowColor);
    put_line(&DataWin, 3, "Backup, restore, verify and catalogue", DataColor);
    put_line(&DataWin, 5, "Tape access is not implemented yet.", DataColor);
    sprintf(backend, "Detected backend: %s", ot_backend_name(selected));
    put_line(&DataWin, 7, backend, DataColor);

    put_line(&QuickWin, 1, "OpenTape help", QuickBackColor);
    put_line(&QuickWin, 3, "Esc returns to DOS.", QuickBackColor);

    pressAnyKey(INPUT_WAIT);
    end_input();
    clear_ui();
}

int main(int argc, char **argv)
{
    enum ot_backend_kind selected;

    if (argc > 1 && strcmp(argv[1], "--help") != 0 &&
        strcmp(argv[1], "/?") != 0) {
        fputs("OpenTape: unknown option\n", stderr);
        usage();
        return 1;
    }

    selected = ot_backend_select(0u, OT_BACKEND_AUTO);
    if (argc > 1) {
        usage();
        printf("Detected backend: %s\n", ot_backend_name(selected));
    } else {
        show_ui(selected);
    }
    return 0;
}

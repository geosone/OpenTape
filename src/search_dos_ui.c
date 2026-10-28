/* Search input uses the same bounded QUILIB field adapter as settings. Only
 * an applied valid draft reaches the caller; escaping retains the old query. */
#include <string.h>
#include <commfunc.h>
#include <libcolor.h>
#include <ui.h>
#include "opentape/fields_ui.h"
#include "opentape/search_ui.h"
#include "opentape/dos_mouse.h"
#include "opentape/messages.h"
int ot_dos_catalog_search_ui(char *query, ot_dos_ui_line line)
{
    char draft[OT_CATALOG_QUERY_MAX + 1u], blank[48];
    struct ot_dos_fields fields;
    struct ot_dos_field_spec spec;
    INPUTEVENT event;
    unsigned applied = 0u;
    if (line == 0 || !ot_catalog_query_valid(query)) return 0;
    strcpy(draft, query);
    paintBackgroundScreen(); ot_dos_mouse_clear();
    line(0u, 1, OT_MSG_APP_TITLE, DataYellowColor);
    line(0u, 2, OT_MSG_SEARCH_TITLE, DataColor);
    line(0u, 5, OT_MSG_SEARCH_POLICY, DataColor);
    line(0u, 7, OT_MSG_SEARCH_QUERY, DataColor);
    spec.text = draft; spec.maximum = OT_CATALOG_QUERY_MAX; spec.row = 8;
    ot_dos_fields_show(&fields, &spec, 1u);
    line(0u, 11, OT_MSG_SEARCH_LITERAL, DataColor);
    line(0u, 13, OT_MSG_SEARCH_CLEAR, DataColor);
    line(0u, 19, OT_MSG_SEARCH_APPLY, DataColor);
    ot_dos_mouse_pair(0u, 19, OT_MSG_SEARCH_APPLY, KEY_F2, KEY_ESCAPE);
    line(1u, 1, OT_MSG_HELP_TITLE, QuickBackColor);
    line(1u, 3, OT_MSG_SEARCH_LIMIT, QuickBackColor);
    line(1u, 5, OT_MSG_SEARCH_F3, QuickBackColor);
    line(1u, 7, OT_MSG_SEARCH_PREV, QuickBackColor);
    line(1u, 16, OT_MSG_UI_HELP_READY, QuickBackColor);
    memset(blank, ' ', 47u); blank[47] = 0;
    for (;;) {
        (void)ot_dos_mouse_input(&event);
        if (event.key == KEY_ESCAPE) break;
        if (event.key == KEY_F2) {
            if (ot_catalog_query_valid(draft)) { strcpy(query, draft); applied = 1u; break; }
            line(0u, 18, OT_MSG_SEARCH_INVALID, DataYellowColor); continue;
        }
        line(0u, 18, blank, DataColor);
        if (ot_dos_fields_input(&fields, &event)) line(0u, 18, OT_MSG_BFORM_LIMIT, DataYellowColor);
    }
    ot_dos_fields_close(&fields);
    return (int)applied;
}

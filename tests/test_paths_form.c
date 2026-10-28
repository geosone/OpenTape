/* No I/O: exercise the all-or-nothing path settings transaction independently
 * of any filesystem, installed device or catalogue/restore grant. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/paths_form.h"
int main(void)
{
    struct ot_paths_form saved, draft, before;
    unsigned i;
    char value[42];
    memset(&saved, 0, sizeof(saved));
    for (i = 0u; i < OT_PATHS_FORM_COUNT; ++i) assert(ot_paths_form_name(&saved, i) == 0);
    assert(ot_paths_form_commit(&saved, &saved));
    strcpy(value, "C:\\TARGET");
    assert(ot_paths_form_set(&saved, OT_PATH_RESTORE, value));
    value[3] = 'X'; assert(strcmp(ot_paths_form_name(&saved, OT_PATH_RESTORE), "C:\\TARGET") == 0);
    draft = saved; strcpy(draft.names[OT_PATH_RESTORE], "OTHER");
    assert(strcmp(saved.names[OT_PATH_RESTORE], "C:\\TARGET") == 0);
    before = saved;
    strcpy(draft.names[OT_PATH_EXPORT], "*.LST");
    assert(!ot_paths_form_commit(&saved, &draft) && memcmp(&before, &saved, sizeof(saved)) == 0);
    strcpy(draft.names[OT_PATH_EXPORT], "LIST.LST");
    assert(ot_paths_form_commit(&saved, &draft));
    draft.names[OT_PATH_RESTORE][0] = 0;
    assert(ot_paths_form_commit(&saved, &draft) && ot_paths_form_name(&saved, OT_PATH_RESTORE) == 0);
    assert(ot_paths_form_set(&saved, OT_PATH_EXPORT, saved.names[OT_PATH_EXPORT]));
    before = saved;
    assert(!ot_paths_form_set(&saved, 5u, "X") && !ot_paths_form_set(&saved, 0u, 0));
    assert(!ot_paths_form_set(&saved, 0u, "BAD?") && !ot_paths_form_set(&saved, 0u, "BAD\n"));
    assert(!ot_paths_form_set(&saved, 0u, "BAD\177"));
    assert(memcmp(&before, &saved, sizeof(saved)) == 0);
    memset(value, 'A', 40u); value[40] = 0;
    assert(ot_paths_form_set(&saved, 0u, value));
    assert(strlen(ot_paths_form_name(&saved, 0u)) == 40u);
    value[40] = 'A'; value[41] = 0; before = saved;
    assert(!ot_paths_form_set(&saved, 0u, value));
    assert(memcmp(&before, &saved, sizeof(saved)) == 0);
    for (i = 0u; i < OT_PATHS_FORM_COUNT; ++i) {
        draft = saved; memset(draft.names[i], 'A', sizeof(draft.names[i]));
        assert(!ot_paths_form_commit(&saved, &draft));
        assert(memcmp(&before, &saved, sizeof(saved)) == 0);
    }
    assert(!ot_paths_form_commit(0, &draft) && !ot_paths_form_commit(&saved, 0));
    assert(!ot_paths_form_set(0, 0u, "X"));
    assert(ot_paths_form_name(0, 0u) == 0 && ot_paths_form_name(&saved, 5u) == 0);
    assert(!ot_paths_form_value_valid(0) && ot_paths_form_value_valid(""));
    puts("Catalogue path form tests passed");
    return 0;
}

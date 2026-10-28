/* The settings transaction must be independent of source/media availability.
 * Invalid drafts and borrowed output failures leave previous state intact. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/backup_form.h"

static void invalid(struct ot_backup_form *saved, const struct ot_backup_form *draft)
{
    struct ot_backup_form before = *saved;
    struct ot_dos_backup_request request, old;
    memset(&request, 0, sizeof(request)); old = request;
    assert(!ot_backup_form_request(draft, &request));
    assert(memcmp(&request, &old, sizeof(request)) == 0);
    assert(!ot_backup_form_commit(saved, draft));
    assert(memcmp(saved, &before, sizeof(before)) == 0);
}

int main(void)
{
    struct ot_backup_form saved, draft;
    struct ot_dos_backup_request request;
    char source[42] = "MISSING.BIN";
    request.source = source; request.path = "DATA/INPUT.BIN";
    request.volume = "TEST_01"; request.tree = 0u;
    ot_backup_form_init(&saved, 0);
    assert(!saved.configured && !saved.tree && !saved.source[0]);
    assert(strcmp(saved.path, "DATA") == 0 && strcmp(saved.volume, "BACKUP") == 0);
    invalid(&saved, &saved);
    ot_backup_form_init(&saved, &request);
    source[0] = 'X';
    assert(saved.configured && strcmp(saved.source, "MISSING.BIN") == 0);
    assert(ot_backup_form_request(&saved, &request));
    assert(request.source == saved.source && request.path == saved.path && request.volume == saved.volume);
    draft = saved; strcpy(draft.source, "NO-SUCH-DIRECTORY"); draft.tree = 1u;
    assert(strcmp(saved.source, "MISSING.BIN") == 0 && !saved.tree);
    assert(ot_backup_form_commit(&saved, &draft));
    assert(saved.tree && strcmp(saved.source, draft.source) == 0);
    draft.source[0] = 'X'; assert(saved.source[0] == 'N');
    draft = saved; draft.source[0] = 0; invalid(&saved, &draft);
    draft = saved; strcpy(draft.source, "*.BIN"); invalid(&saved, &draft);
    draft = saved; strcpy(draft.source, "A?.BIN"); invalid(&saved, &draft);
    draft = saved; strcpy(draft.path, "../OUT"); invalid(&saved, &draft);
    draft = saved; strcpy(draft.path, "/OUT"); invalid(&saved, &draft);
    draft = saved; draft.path[0] = 0; invalid(&saved, &draft);
    draft = saved; draft.volume[0] = 0; invalid(&saved, &draft);
    draft = saved; strcpy(draft.volume, "BAD!"); invalid(&saved, &draft);
    draft = saved; draft.tree = OT_BACKUP_SET; strcpy(draft.source, "SOURCES.LST");
    assert(ot_backup_form_commit(&saved, &draft) && saved.tree == OT_BACKUP_SET);
    draft = saved; draft.tree = 3u; invalid(&saved, &draft);
    draft = saved; memset(draft.source, 'A', sizeof(draft.source)); invalid(&saved, &draft);
    draft = saved; memset(draft.path, 'A', sizeof(draft.path)); invalid(&saved, &draft);
    draft = saved; memset(draft.volume, 'A', sizeof(draft.volume)); invalid(&saved, &draft);
    draft = saved;
    memset(draft.source, 'S', 40u); draft.source[40] = 0;
    strcpy(draft.path, "ABCDEFGH/ABCDEFGH/ABCDEFGH/ABCDEFGH/ABCD");
    memset(draft.volume, 'V', 31u); draft.volume[31] = 0;
    assert(ot_backup_form_commit(&saved, &draft));
    assert(ot_backup_form_request(&saved, &request));
    assert(strlen(request.source) == 40u && strlen(request.path) == 40u && strlen(request.volume) == 31u);
    assert(ot_backup_form_commit(&saved, &saved));
    assert(!ot_backup_form_commit(0, &draft) && !ot_backup_form_commit(&saved, 0));
    assert(!ot_backup_form_request(0, &request) && !ot_backup_form_request(&saved, 0));
    ot_backup_form_init(0, &request);
    memset(source, 'S', 41u); source[41] = 0;
    request.source = source;
    ot_backup_form_init(&saved, &request); assert(!saved.configured && !saved.source[0]);
    puts("Backup form transaction tests passed");
    return 0;
}

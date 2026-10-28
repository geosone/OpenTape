/* Transaction boundary for the DOS editor. Draft changes become visible to B
 * only after a complete valid save. Filesystem/device checks belong to the
 * independently confirmed backup operation, never to this settings model. */
#include <string.h>
#include "opentape/backup_form.h"

int ot_backup_form_request(const struct ot_backup_form *form, struct ot_dos_backup_request *request)
{
    struct ot_dos_backup_request value;
    if (form == 0 || request == 0 ||
        memchr(form->source, 0, sizeof(form->source)) == 0 ||
        memchr(form->path, 0, sizeof(form->path)) == 0 ||
        memchr(form->volume, 0, sizeof(form->volume)) == 0) return 0;
    value.source = form->source; value.path = form->path;
    value.volume = form->volume; value.tree = form->tree;
    if (!ot_dos_backup_request_valid(&value)) return 0;
    *request = value;
    return 1;
}

int ot_backup_form_commit(struct ot_backup_form *form, const struct ot_backup_form *draft)
{
    struct ot_dos_backup_request request;
    if (form == 0 || !ot_backup_form_request(draft, &request)) return 0;
    *form = *draft;
    form->configured = 1u;
    return 1;
}

void ot_backup_form_init(struct ot_backup_form *form, const struct ot_dos_backup_request *request)
{
    if (form == 0) return;
    memset(form, 0, sizeof(*form));
    strcpy(form->path, "DATA"); strcpy(form->volume, "BACKUP");
    if (!ot_dos_backup_request_valid(request) ||
        strlen(request->source) > OT_BACKUP_FORM_PATH_MAX ||
        strlen(request->path) > OT_BACKUP_FORM_PATH_MAX) return;
    strcpy(form->source, request->source); strcpy(form->path, request->path);
    strcpy(form->volume, request->volume); form->tree = request->tree;
    form->configured = 1u;
}

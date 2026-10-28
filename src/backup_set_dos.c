/* Resolve an entire checked manifest before acquiring tape. Member plans are
 * provisional; no source-list failure may leave a usable partial plan. */
#include <string.h>
#include "opentape/backup_set.h"
#include "opentape/local_name.h"
int ot_dos_plan_set(struct ot_backup_plan *plan, const char *name, const char *base, unsigned *error)
{
    struct ot_backup_set set;
    const struct ot_backup_root *root;
    const struct ot_plan_entry *entry;
    struct ot_backup_plan temporary, member;
    struct ot_dos_source source;
    char path[OT_FILE_PATH_MAX + 1u];
    unsigned close_error;
    int rc;
    if (error) *error = 0u;
    if (plan == 0 || plan->first || plan->last || plan->count || error == 0 ||
        name == 0 || !*name || strlen(name) > 127u || ot_local_tape_name(name) || !ot_archive_path_valid(base))
        return OT_SOURCE_ARGUMENT;
    memset(&set, 0, sizeof(set)); memset(&temporary, 0, sizeof(temporary));
    memset(&member, 0, sizeof(member)); memset(&source, 0, sizeof(source));
    rc = ot_dos_source_open(&source, name, "BACKUP.LST");
    if (rc == 0) rc = ot_backup_set_load(&set, &source.ops);
    *error = source.dos_error;
    for (root = set.first; root && rc == 0; root = root->next) {
        if (strlen(base) + 1u + strlen(root->path) > OT_FILE_PATH_MAX) { rc = OT_PLAN_LIMIT; break; }
        strcpy(path, base); strcat(path, "/"); strcat(path, root->path);
        memset(&source, 0, sizeof(source));
        if (root->tree) rc = ot_dos_plan_tree(&member, root->source, path, error);
        else {
            rc = ot_dos_source_open(&source, root->source, path);
            if (rc == 0 && source.ops.recheck(&source)) rc = OT_SOURCE_CHANGED;
            if (rc == 0) rc = ot_plan_add(&member, source.name, &source.metadata);
            if (source.opened) {
                close_error = source.ops.close(&source);
                if (rc == 0 && close_error) rc = OT_SOURCE_IO;
            }
            if (source.dos_error) *error = source.dos_error;
        }
        for (entry = member.first; entry && rc == 0; entry = entry->next)
            rc = ot_plan_add(&temporary, entry->source, &entry->metadata);
        ot_plan_free(&member);
    }
    ot_backup_set_free(&set); ot_plan_free(&member);
    if (rc == 0) *plan = temporary; else ot_plan_free(&temporary);
    return rc;
}

/* Separate planning action, intentionally independent of a tape alias/backend.
 * Tree enumeration reuses the writer's exact limits and source-close checks.
 * No plan survives this call and no successful preview grants a backup batch. */
#include <string.h>
#include "opentape/backup_preview.h"
#include "opentape/local_name.h"
#include "opentape/backup_set.h"
int ot_dos_backup_preview(const struct ot_dos_backup_request *request, struct ot_backup_preview_result *result)
{
    struct ot_dos_source source;
    struct ot_backup_plan plan;
    struct ot_plan_summary summary;
    unsigned error;
    int rc;
    if (result == 0) return OT_SOURCE_ARGUMENT;
    memset(result, 0, sizeof(*result)); result->result = OT_SOURCE_ARGUMENT;
    if (!ot_dos_backup_request_valid(request) || ot_local_tape_name(request->source)) return result->result;
    memset(&source, 0, sizeof(source)); memset(&plan, 0, sizeof(plan)); memset(&summary, 0, sizeof(summary));
    if (request->tree) {
        rc = request->tree == OT_BACKUP_SET ?
            ot_dos_plan_set(&plan, request->source, request->path, &result->source_error) :
            ot_dos_plan_tree(&plan, request->source, request->path, &result->source_error);
        if (rc == 0) rc = ot_plan_summarize(&plan, &summary);
    } else {
        rc = ot_dos_source_open(&source, request->source, request->path);
        if (rc == 0 && source.ops.recheck(&source) != 0u) rc = OT_SOURCE_CHANGED;
        if (source.opened) {
            error = source.ops.close(&source);
            if (rc == 0 && error != 0u) rc = OT_SOURCE_IO;
        }
        result->source_error = source.dos_error;
        if (rc == 0) { summary.files = 1ul; summary.bytes_low = source.metadata.file_size; }
    }
    ot_plan_free(&plan);
    result->result = rc;
    if (rc == 0) result->summary = summary;
    return rc;
}

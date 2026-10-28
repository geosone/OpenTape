/* Convert committed writer state into UI progress without duplicating archive
 * verification or altering the source/writer/finalization path. */
#include <string.h>
#include "opentape/backup_progress.h"
int ot_backup_tracking_init(struct ot_backup_tracking *tracking, const struct ot_plan_summary *total,
                            ot_backup_observer observer, void *context)
{
    struct ot_plan_summary snapshot;
    if (tracking == 0 || total == 0 || observer == 0) return OT_SOURCE_ARGUMENT;
    snapshot = *total;
    memset(tracking, 0, sizeof(*tracking));
    tracking->progress.total = snapshot; tracking->observer = observer; tracking->context = context;
    tracking->progress.phase = OT_BACKUP_PLANNED;
    observer(context, &tracking->progress);
    return 0;
}
void ot_backup_tracking_record(void *context, const struct ot_archive_verify *state,
                                const struct ot_record_header *record, const oa_u8 *payload)
{
    struct ot_backup_tracking *tracking = context;
    struct ot_backup_progress *progress = &tracking->progress;
    struct ot_file_metadata metadata;
    oa_u32 before;
    if (record->type == OT_RECORD_FILE_METADATA &&
        ot_file_metadata_decode(&metadata, payload, record->data_size) == 0) {
        strcpy(progress->path, metadata.path);
        progress->file_size = metadata.file_size; progress->file_bytes = 0ul;
    }
    progress->files = state->files; progress->directories = state->directories;
    progress->accepted_low = state->bytes_low; progress->accepted_high = state->bytes_high;
    if (state->file_active) {
        before = progress->accepted_low;
        progress->accepted_low += state->file_bytes;
        if (progress->accepted_low < before) ++progress->accepted_high;
        progress->file_bytes = state->file_bytes;
    } else if (record->type == OT_RECORD_FILE_END) progress->file_bytes = state->file_bytes;
    progress->phase = record->type == OT_RECORD_SESSION_END ? OT_BACKUP_FINALIZING : OT_BACKUP_STREAMING;
    tracking->observer(tracking->context, progress);
}

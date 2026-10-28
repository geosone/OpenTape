#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "opentape/source_file.h"
#include "opentape/tape_output.h"
#include "opentape/messages.h"
#include "opentape/archive_dos.h"
#include "opentape/backup_plan.h"
#include "opentape/backup_dos.h"
#include "opentape/backup_set.h"

/* CLI and GUI share the entire stream/finalization path. The quiet result
 * retains diagnostics after cleanup rather than printing into QUILIB. */
static int backup_run(const char *name, const char *path, const char *device,
                      const char *volume, unsigned tree, struct ot_dos_backup_result *summary, ot_backup_observer observer, void *observer_context)
{
    struct ot_dos_source source;
    struct ot_tape_output output;
    struct ot_archive_writer writer;
    struct ot_volume_label label;
    struct ot_session_start start;
    struct ot_backup_plan plan;
    struct ot_plan_ops plan_ops;
    struct ot_backup_tracking tracking;
    struct ot_plan_summary total;
    oa_u8 *buffer;
    int rc;
    unsigned output_opened = 0u;
    time_t now;
    memset(&source, 0, sizeof(source));
    memset(&output, 0, sizeof(output));
    memset(&writer, 0, sizeof(writer));
    memset(&label, 0, sizeof(label));
    memset(&start, 0, sizeof(start));
    memset(&plan, 0, sizeof(plan));
    buffer = malloc(OT_BLOCK_RECOMMENDED_SIZE);
    if (buffer == 0) {
        if (summary == 0) { puts(OT_MSG_READ_MEMORY); return OT_SOURCE_IO; }
        source.dos_error = 8u; rc = OT_SOURCE_IO; goto done;
    }
    if (tree) strcpy(source.name, name);
    rc = tree == OT_BACKUP_SET ? ot_dos_plan_set(&plan, name, path, &source.dos_error) :
         tree == OT_BACKUP_TREE ? ot_dos_plan_tree(&plan, name, path, &source.dos_error) :
                ot_dos_source_open(&source, name, path);
    if (rc != 0) goto done;
    if (observer != 0) {
        memset(&total, 0, sizeof(total));
        if (tree) rc = ot_plan_summarize(&plan, &total);
        else { total.files = 1ul; total.bytes_low = source.metadata.file_size; }
        if (rc == 0) rc = ot_backup_tracking_init(&tracking, &total, observer, observer_context);
        if (rc != 0) goto done;
    }
    if (summary == 0 && (printf(OT_MSG_BACKUP_WARNING, device) < 0 || fflush(stdout) != 0 || ferror(stdout))) {
        rc = OT_OUTPUT_IO_ERROR; goto done;
    }
    rc = ot_dos_archive_output_open(&output, device, 1u);
    output_opened = output.opened;
    if (rc != 0) goto done;
    now = time(0);
    label.version = OT_VOLUME_FORMAT_VERSION;
    label.volume_sequence = 1ul;
    label.created_utc = now == (time_t)-1 ? 0ul : (oa_u32)now;
    strcpy(label.volume_name, volume);
    start.created_utc = label.created_utc;
    strcpy(start.name, tree == OT_BACKUP_SET ? "OpenTape source-set backup" :
        tree ? "OpenTape directory backup" : "OpenTape single-file backup");
    rc = ot_archive_writer_init(&writer, &output.sink, buffer, OT_BLOCK_RECOMMENDED_SIZE);
    if (rc == 0 && observer != 0) rc = ot_archive_writer_observe(&writer, ot_backup_tracking_record, &tracking);
    if (rc == 0) rc = ot_archive_writer_volume(&writer, &label);
    if (rc == 0) rc = ot_archive_writer_session(&writer, 1ul, &start);
    if (rc == 0 && tree) {
        ot_dos_plan_ops(&plan_ops, &source);
        rc = ot_plan_emit(&plan, &plan_ops, &writer, buffer + OT_WRITER_RECORD_OVERHEAD,
                           OT_BLOCK_RECOMMENDED_SIZE - OT_WRITER_RECORD_OVERHEAD);
    } else if (rc == 0) rc = ot_backup_source(&writer, 1ul, &source.metadata, &source.ops,
                         buffer + OT_WRITER_RECORD_OVERHEAD,
                         OT_BLOCK_RECOMMENDED_SIZE - OT_WRITER_RECORD_OVERHEAD);
    if (rc == 0) rc = ot_archive_writer_session_end(&writer);
    if (rc == 0) rc = ot_archive_writer_complete(&writer);
done:
    if (source.opened) {
        unsigned error = source.ops.close != 0 ? source.ops.close(&source) : 1u;
        if (rc == 0 && error != 0u) rc = OT_SOURCE_IO;
    }
    if (output.opened) {
        int close_rc = ot_output_close(&output);
        if (rc == 0) rc = close_rc;
    }
    free(buffer);
    ot_plan_free(&plan);
    if (summary != 0) {
        summary->result = rc;
        summary->source_error = source.dos_error; summary->tape_error = output.dos_error;
        summary->output_opened = output_opened; summary->finished = rc == 0;
        summary->files = writer.verified.files; summary->directories = writer.verified.directories;
        summary->bytes_low = writer.verified.bytes_low; summary->bytes_high = writer.verified.bytes_high;
        summary->status = output.status;
        strcpy(summary->source_name, source.name);
        return rc;
    }
    if (rc != 0) {
        printf(OT_MSG_BACKUP_ERROR, rc, source.dos_error, output.dos_error);
        printf(OT_MSG_PROBE_SCSI, (unsigned)output.status.tape.last_scsi_status,
               (unsigned)output.status.tape.last_sense_key,
               (unsigned)output.status.tape.last_scsi_opcode,
               (unsigned)output.status.tape.residual_bytes);
        return rc;
    }
    if (tree) printf(tree == OT_BACKUP_SET ? OT_MSG_SET_OK : OT_MSG_TREE_OK, (unsigned long)writer.verified.files,
                      (unsigned long)writer.verified.directories, (unsigned long)writer.verified.bytes_low);
    else printf(OT_MSG_BACKUP_OK, path, (unsigned long)source.metadata.file_size);
    return fflush(stdout) != 0 || ferror(stdout) ? OT_OUTPUT_IO_ERROR : 0;
}

int ot_dos_backup_execute(const struct ot_dos_backup_request *request, const char *device,
                          unsigned authorized, struct ot_dos_backup_result *result)
{
    return ot_dos_backup_execute_observed(request, device, authorized, 0, 0, result);
}

int ot_dos_backup_execute_observed(const struct ot_dos_backup_request *request, const char *device,
                                    unsigned authorized, ot_backup_observer observer, void *context,
                                    struct ot_dos_backup_result *result)
{
    if (result == 0) return OT_SOURCE_ARGUMENT;
    memset(result, 0, sizeof(*result));
    result->result = OT_SOURCE_ARGUMENT;
    if (!ot_dos_backup_request_valid(request) || !ot_probe_raw_name_valid(device)) return result->result;
    if (authorized != 1u) return result->result = OT_OUTPUT_NOT_AUTHORIZED;
    result->result = backup_run(request->source, request->path, device, request->volume, request->tree, result, observer, context);
    return result->result;
}

int ot_dos_backup(const char *name, const char *path, const char *device, const char *volume, int tree)
{
    struct ot_dos_backup_request request;
    int rc;
    request.source = name; request.path = path; request.volume = volume; request.tree = (unsigned)tree;
    if (!ot_dos_backup_request_valid(&request) || !ot_probe_raw_name_valid(device)) return 1;
    rc = backup_run(name, path, device, volume, request.tree, 0, 0, 0);
    return rc == 0 ? 0 : 2;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "opentape/source_file.h"
#include "opentape/tape_output.h"
#include "opentape/messages.h"
#include "opentape/archive_dos.h"
#include "opentape/backup_plan.h"

int ot_dos_backup(const char *name, const char *path, const char *device, const char *volume, int tree)
{
    struct ot_dos_source source;
    struct ot_tape_output output;
    struct ot_archive_writer writer;
    struct ot_volume_label label;
    struct ot_session_start start;
    struct ot_backup_plan plan;
    struct ot_plan_ops plan_ops;
    oa_u8 *buffer;
    int rc;
    unsigned i;
    time_t now;
    if (!ot_archive_path_valid(path) || !ot_probe_name_valid(device) ||
        (device[1] != 'r' && device[1] != 'R') || strlen(volume) == 0u || strlen(volume) > 31u)
        return 1;
    for (i = 0u; volume[i] != 0; ++i)
        if (!((volume[i] >= 'A' && volume[i] <= 'Z') ||
              (volume[i] >= 'a' && volume[i] <= 'z') ||
              (volume[i] >= '0' && volume[i] <= '9') || volume[i] == '-' || volume[i] == '_'))
            return 1;
    memset(&source, 0, sizeof(source));
    memset(&output, 0, sizeof(output));
    memset(&writer, 0, sizeof(writer));
    memset(&label, 0, sizeof(label));
    memset(&start, 0, sizeof(start));
    memset(&plan, 0, sizeof(plan));
    buffer = malloc(OT_BLOCK_RECOMMENDED_SIZE);
    if (buffer == 0) { puts(OT_MSG_READ_MEMORY); return 2; }
    rc = tree ? ot_dos_plan_tree(&plan, name, path, &source.dos_error) :
                ot_dos_source_open(&source, name, path);
    if (rc != 0) goto done;
    printf(OT_MSG_BACKUP_WARNING, device);
    rc = ot_dos_archive_output_open(&output, device, 1u);
    if (rc != 0) goto done;
    now = time(0);
    label.version = OT_VOLUME_FORMAT_VERSION;
    label.volume_sequence = 1ul;
    label.created_utc = now == (time_t)-1 ? 0ul : (oa_u32)now;
    strcpy(label.volume_name, volume);
    start.created_utc = label.created_utc;
    strcpy(start.name, tree ? "OpenTape directory backup" : "OpenTape single-file backup");
    rc = ot_archive_writer_init(&writer, &output.sink, buffer, OT_BLOCK_RECOMMENDED_SIZE);
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
    if (rc != 0) {
        printf(OT_MSG_BACKUP_ERROR, rc, source.dos_error, output.dos_error);
        printf(OT_MSG_PROBE_SCSI, (unsigned)output.status.tape.last_scsi_status,
               (unsigned)output.status.tape.last_sense_key,
               (unsigned)output.status.tape.last_scsi_opcode,
               (unsigned)output.status.tape.residual_bytes);
        return 2;
    }
    if (tree) printf(OT_MSG_TREE_OK, (unsigned long)writer.verified.files,
                      (unsigned long)writer.verified.directories, (unsigned long)writer.verified.bytes_low);
    else printf(OT_MSG_BACKUP_OK, path, (unsigned long)source.metadata.file_size);
    return 0;
}

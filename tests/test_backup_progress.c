/* Accepted-record notifications must not report failed writes, permit writer
 * reentry, or mistake a complete payload for completed session finalization. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/backup_progress.h"
static struct ot_archive_writer writer;
static struct ot_backup_tracking tracking;
static struct ot_backup_progress last;
static unsigned calls, writes, fail_write, finish_error, saw_partial, saw_full_open;
static oa_u8 buffer[512];
static void notify(void *context, const struct ot_backup_progress *p)
{
    assert(context == &calls); ++calls;
    assert(p->accepted_high == 0ul && p->accepted_low <= 100ul);
    if (p->phase != OT_BACKUP_PLANNED) {
        assert(ot_archive_writer_complete(&writer) == OT_WRITER_BAD_ARGUMENT);
        assert(ot_archive_writer_observe(&writer, 0, 0) == OT_WRITER_BAD_ARGUMENT);
    }
    if (p->accepted_low == 40ul && p->files == 0ul) saw_partial = 1u;
    if (p->accepted_low == 100ul && p->files == 0ul) saw_full_open = 1u;
    last = *p;
}
static int write_record(void *context, const oa_u8 *data, unsigned size, unsigned *accepted)
{
    (void)context; (void)data; ++writes;
    *accepted = fail_write ? size - 1u : size;
    return 0;
}
static int finish(void *context) { (void)context; return (int)finish_error; }
static void run(unsigned fault)
{
    struct ot_writer_sink sink;
    struct ot_plan_summary total;
    struct ot_volume_label volume;
    struct ot_session_start session;
    struct ot_file_metadata metadata;
    oa_u8 bytes[100];
    unsigned before;
    memset(&writer, 0, sizeof(writer)); memset(&total, 0, sizeof(total));
    memset(&volume, 0, sizeof(volume)); memset(&session, 0, sizeof(session));
    memset(&metadata, 0, sizeof(metadata)); memset(bytes, 0x41, sizeof(bytes));
    calls = writes = fail_write = finish_error = saw_partial = saw_full_open = 0u;
    sink.context = 0; sink.write_block = write_record; sink.finish_session = finish;
    total.files = 1ul; total.directories = 1ul; total.bytes_low = fault == 4u ? 0ul : 100ul;
    assert(ot_backup_tracking_init(&tracking, &total, notify, &calls) == 0);
    assert(calls == 1u && last.phase == OT_BACKUP_PLANNED && !last.accepted_low && !writes);
    assert(ot_archive_writer_init(&writer, &sink, buffer, sizeof(buffer)) == 0);
    assert(ot_archive_writer_observe(&writer, ot_backup_tracking_record, &tracking) == 0);
    volume.version = 1u; strcpy(volume.volume_name, "PROGRESS");
    if (fault == 1u) {
        fail_write = 1u;
        assert(ot_archive_writer_volume(&writer, &volume) == OT_WRITER_SHORT_WRITE);
        assert(calls == 1u && !last.accepted_low && writer.failed); return;
    }
    assert(ot_archive_writer_volume(&writer, &volume) == 0);
    assert(ot_archive_writer_observe(&writer, 0, 0) == OT_WRITER_BAD_ARGUMENT);
    strcpy(session.name, "TEST"); assert(ot_archive_writer_session(&writer, 1ul, &session) == 0);
    metadata.kind = OT_FILE_DIRECTORY; metadata.dos_attributes = 0x10u; strcpy(metadata.path, "DATA");
    assert(ot_archive_writer_metadata(&writer, 1ul, &metadata) == 0 && last.directories == 1ul);
    metadata.kind = OT_FILE_REGULAR; metadata.dos_attributes = 0x20u; metadata.file_size = fault == 4u ? 0ul : 100ul;
    strcpy(metadata.path, "DATA/INPUT.BIN");
    assert(ot_archive_writer_metadata(&writer, 2ul, &metadata) == 0);
    if (fault != 4u) {
    assert(last.file_size == 100ul && !last.file_bytes && strcmp(last.path, metadata.path) == 0);
    assert(ot_archive_writer_data(&writer, bytes, 40u) == 0);
    if (fault == 2u) {
        before = calls; fail_write = 1u;
        assert(ot_archive_writer_data(&writer, bytes + 40u, 60u) == OT_WRITER_SHORT_WRITE);
        assert(calls == before && last.accepted_low == 40ul && !last.files); return;
    }
    assert(ot_archive_writer_data(&writer, bytes + 40u, 60u) == 0);
    assert(saw_partial && saw_full_open && !last.files);
    }
    assert(ot_archive_writer_file_end(&writer) == 0 && last.files == 1ul && last.accepted_low == (fault == 4u ? 0ul : 100ul));
    if (fault == 3u) finish_error = 7u;
    assert(ot_archive_writer_session_end(&writer) == (fault == 3u ? OT_WRITER_SINK_ERROR : 0));
    assert(last.phase == OT_BACKUP_FINALIZING && last.files == 1ul && last.accepted_low == (fault == 4u ? 0ul : 100ul));
    assert(ot_archive_writer_complete(&writer) == (fault == 3u ? OT_WRITER_FAILED : 0));
}
static void wide_notify(void *context, const struct ot_backup_progress *p)
{
    unsigned *seen = context;
    if (p->phase == OT_BACKUP_PLANNED) return;
    assert(p->accepted_high == 6ul && p->accepted_low == 16ul && p->file_bytes == 32ul);
    ++*seen;
}
static void wide_accounting(void)
{
    struct ot_plan_summary total;
    struct ot_archive_verify state;
    struct ot_record_header record;
    unsigned seen = 0u;
    memset(&total, 0, sizeof(total)); memset(&state, 0, sizeof(state)); memset(&record, 0, sizeof(record));
    total.files = 7ul; total.bytes_high = 6ul; total.bytes_low = 16ul;
    assert(ot_backup_tracking_init(&tracking, &total, wide_notify, &seen) == 0);
    state.files = 6ul; state.bytes_high = 5ul; state.bytes_low = 0xfffffff0ul;
    state.file_active = 1u; state.file_size = state.file_bytes = 32ul;
    record.type = OT_RECORD_FILE_DATA; record.data_size = 32u;
    ot_backup_tracking_record(&tracking, &state, &record, 0);
    assert(seen == 1u);
    assert(ot_backup_tracking_init(&tracking, &tracking.progress.total, wide_notify, &seen) == 0);
    assert(tracking.progress.total.files == 7ul && tracking.progress.total.bytes_high == 6ul && tracking.progress.total.bytes_low == 16ul);
}
int main(void)
{
    unsigned i;
    struct ot_plan_summary total;
    memset(&total, 0, sizeof(total));
    assert(ot_backup_tracking_init(0, &total, notify, 0) == OT_SOURCE_ARGUMENT);
    assert(ot_backup_tracking_init(&tracking, 0, notify, 0) == OT_SOURCE_ARGUMENT);
    assert(ot_backup_tracking_init(&tracking, &total, 0, 0) == OT_SOURCE_ARGUMENT);
    assert(ot_archive_writer_observe(0, 0, 0) == OT_WRITER_BAD_ARGUMENT);
    for (i = 0u; i < 5u; ++i) run(i);
    wide_accounting();
    puts("Backup progress commit/observation tests passed");
    return 0;
}

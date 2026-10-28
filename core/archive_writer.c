#include <string.h>
#include "opentape/archive_writer.h"

static int ready(const struct ot_archive_writer *w)
{
    if (w == 0 || !w->initialized || w->busy) return OT_WRITER_BAD_ARGUMENT;
    return w->failed || w->pending_finish ? OT_WRITER_FAILED : OT_FORMAT_OK;
}

static int emit(struct ot_archive_writer *w, oa_u16 type, oa_u32 object,
                   oa_u16 flags, oa_u32 offset, const oa_u8 *data, oa_u16 size,
                   oa_u32 session, oa_u32 time)
{
    struct ot_archive_verify next;
    struct ot_block_header b;
    struct ot_record_header r;
    unsigned accepted;
    oa_u16 total;
    int rc = ready(w);
    if (rc != 0) return rc;
    if ((data == 0 && size != 0u) ||
        (oa_u32)size + OT_BLOCK_HEADER_SIZE + OT_RECORD_HEADER_SIZE > w->capacity)
        return OT_WRITER_BAD_ARGUMENT;
    total = (oa_u16)(OT_BLOCK_HEADER_SIZE + OT_RECORD_HEADER_SIZE + size);
    /* Data may alias our scratch buffer: move it before writing any header. */
    if (size != 0u) memmove(w->buffer + OT_WRITER_RECORD_OVERHEAD, data, size);
    memset(&r, 0, sizeof(r));
    r.type = type;
    r.flags = flags;
    r.object_id = object;
    r.fragment_offset = offset;
    r.data_size = size;
    r.data_crc32 = ot_crc32(w->buffer + OT_WRITER_RECORD_OVERHEAD, size);
    ot_record_header_encode(w->buffer + OT_BLOCK_HEADER_SIZE, &r);
    memset(&b, 0, sizeof(b));
    b.block_number = w->verified.framing.blocks;
    b.session_id = session;
    b.session_time = time;
    b.record_count = 1u;
    rc = ot_block_encode(w->buffer, w->capacity, &b, w->buffer + OT_BLOCK_HEADER_SIZE,
                          (oa_u16)(OT_RECORD_HEADER_SIZE + size));
    if (rc != 0) return rc;
    next = w->verified;
    rc = ot_archive_verify_block(&next, w->buffer, total);
    if (rc != 0) return rc;             /* Invalid input never reaches the sink. */
    accepted = 0u;
    w->busy = 1u;
    rc = w->sink->write_block(w->sink->context, w->buffer, total, &accepted);
    w->busy = 0u;
    w->last_accepted = accepted;
    if (rc != 0 || accepted != total) {
        w->failed = 1u;                 /* Media position may already have changed. */
        w->sink_error = rc;
        if (rc != 0) return OT_WRITER_SINK_ERROR;
        return accepted > total ? OT_WRITER_BAD_COUNT : OT_WRITER_SHORT_WRITE;
    }
    w->verified = next;
    if (w->observer != 0) {
        w->busy = 1u;
        w->observer(w->observer_context, &w->verified, &r, w->buffer + OT_WRITER_RECORD_OVERHEAD);
        w->busy = 0u;
    }
    return OT_FORMAT_OK;
}

int ot_archive_writer_init(struct ot_archive_writer *w,
                            const struct ot_writer_sink *sink, oa_u8 *buffer, oa_u16 capacity)
{
    if (w == 0 || w->initialized || sink == 0 || buffer == 0 ||
        sink->write_block == 0 || sink->finish_session == 0 ||
        capacity < OT_WRITER_RECORD_OVERHEAD + OT_VOLUME_LABEL_SIZE)
        return OT_WRITER_BAD_ARGUMENT;
    memset(w, 0, sizeof(*w));
    w->sink = sink;
    w->buffer = buffer;
    w->capacity = capacity;
    w->initialized = 1u;
    return OT_FORMAT_OK;
}

int ot_archive_writer_observe(struct ot_archive_writer *w, ot_writer_observer observer, void *context)
{
    int rc = ready(w);
    if (rc != 0) return rc;
    if (w->verified.framing.blocks != 0ul) return OT_WRITER_BAD_ARGUMENT;
    w->observer = observer; w->observer_context = context;
    return 0;
}

int ot_archive_writer_volume(struct ot_archive_writer *w, const struct ot_volume_label *label)
{
    oa_u8 data[OT_VOLUME_LABEL_SIZE];
    int rc = ready(w);
    if (rc != 0) return rc;
    rc = ot_volume_label_encode(data, label);
    if (rc != 0) return rc;
    return emit(w, OT_RECORD_VOLUME_LABEL, 0ul, 3u, 0ul, data, sizeof(data), 0ul, 0ul);
}
int ot_archive_writer_session(struct ot_archive_writer *w, oa_u32 session_id,
                               const struct ot_session_start *start)
{
    oa_u8 data[OT_SESSION_START_SIZE];
    int rc = ready(w);
    if (rc != 0) return rc;
    rc = ot_session_start_encode(data, sizeof(data), start);
    if (rc != 0) return rc;
    return emit(w, OT_RECORD_SESSION_START, 0ul, 3u, 0ul, data, sizeof(data), session_id, start->created_utc);
}
int ot_archive_writer_metadata(struct ot_archive_writer *w, oa_u32 object_id,
                                const struct ot_file_metadata *metadata)
{
    oa_u8 data[OT_FILE_METADATA_PREFIX + OT_FILE_PATH_MAX];
    oa_u16 size;
    int rc = ready(w);
    if (rc != 0) return rc;
    rc = ot_file_metadata_encode(data, sizeof(data), metadata, &size);
    if (rc != 0) return rc;
    return emit(w, OT_RECORD_FILE_METADATA, object_id, 3u, 0ul, data, size,
                 w->verified.last_session, w->verified.session_time);
}
int ot_archive_writer_data(struct ot_archive_writer *w, const oa_u8 *data, oa_u16 size)
{
    oa_u16 flags;
    int rc = ready(w);
    if (rc != 0) return rc;
    if (data == 0 || size == 0u || !w->verified.file_active ||
        w->verified.file_bytes > w->verified.file_size ||
        (oa_u32)size > w->verified.file_size - w->verified.file_bytes)
        return OT_WRITER_BAD_ARGUMENT;
    flags = w->verified.fragments == 0ul ? OT_RECORD_FLAG_FIRST : 0u;
    if (w->verified.file_bytes + size == w->verified.file_size) flags |= OT_RECORD_FLAG_LAST;
    return emit(w, OT_RECORD_FILE_DATA, w->verified.last_object, flags,
                 w->verified.file_bytes, data, size, w->verified.last_session, w->verified.session_time);
}
int ot_archive_writer_file_end(struct ot_archive_writer *w)
{
    struct ot_file_end end;
    oa_u8 data[OT_FILE_END_SIZE];
    int rc = ready(w);
    if (rc != 0) return rc;
    if (!w->verified.file_active || w->verified.file_bytes != w->verified.file_size)
        return OT_VERIFY_INCOMPLETE;
    end.file_size = w->verified.file_size;
    end.crc32 = w->verified.file_crc;
    end.fragments = w->verified.fragments;
    rc = ot_file_end_encode(data, sizeof(data), &end);
    if (rc != 0) return rc;
    return emit(w, OT_RECORD_FILE_END, w->verified.last_object, 3u, 0ul, data, sizeof(data),
                 w->verified.last_session, w->verified.session_time);
}
int ot_archive_writer_session_end(struct ot_archive_writer *w)
{
    struct ot_session_end end;
    oa_u8 data[OT_SESSION_END_SIZE];
    int rc = ready(w);
    if (rc != 0) return rc;
    if (!w->verified.session_active || w->verified.file_active) return OT_VERIFY_INCOMPLETE;
    end.result = OT_SESSION_COMPLETE;
    end.files = w->verified.files;
    end.directories = w->verified.directories;
    end.bytes_low = w->verified.bytes_low;
    end.bytes_high = w->verified.bytes_high;
    ot_session_end_encode(data, sizeof(data), &end);
    rc = emit(w, OT_RECORD_SESSION_END, 0ul, 3u, 0ul, data, sizeof(data),
                w->verified.last_session, w->verified.session_time);
    if (rc != 0) return rc;
    w->pending_finish = 1u;
    w->busy = 1u;
    rc = w->sink->finish_session(w->sink->context);
    w->busy = 0u;
    if (rc != 0) {
        w->failed = 1u;
        w->sink_error = rc;
        return OT_WRITER_SINK_ERROR;
    }
    w->pending_finish = 0u;
    return OT_FORMAT_OK;
}
int ot_archive_writer_complete(const struct ot_archive_writer *w)
{
    int rc = ready(w);
    return rc != 0 ? rc : ot_archive_verify_finish(&w->verified);
}

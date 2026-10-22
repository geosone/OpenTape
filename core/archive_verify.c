#include "opentape/archive_verify.h"
#include "opentape/record_payload.h"

static int verify_record(struct ot_archive_verify *s, const struct ot_block_header *b,
                           const struct ot_record_header *r, const oa_u8 *data)
{
    union {
        struct ot_session_start start;
        struct ot_session_end end;
        struct ot_file_metadata metadata;
        struct ot_file_end file_end;
    } value;
    oa_u32 low;
    unsigned carry;
    oa_u16 size = (oa_u16)r->data_size;

    /* Framing and typed payload validation already succeeded for this block. */
    if (r->type == OT_RECORD_VOLUME_LABEL) return OT_FORMAT_OK;
    if (r->type == OT_RECORD_SESSION_START) {
        ot_session_start_decode(&value.start, data, size);
        if (s->session_active || s->file_active || b->session_id <= s->last_session ||
            b->session_time != value.start.created_utc) return OT_VERIFY_ORDER;
        s->last_session = b->session_id;
        s->session_time = b->session_time;
        s->session_active = 1u;
        s->last_object = 0ul;
        s->files = s->directories = s->bytes_low = s->bytes_high = 0ul;
        return OT_FORMAT_OK;
    }
    if (r->type < OT_RECORD_FILE_METADATA || r->type > OT_RECORD_SESSION_END)
        return OT_VERIFY_UNSUPPORTED;
    if (!s->session_active || b->session_id != s->last_session ||
        b->session_time != s->session_time) return OT_VERIFY_ORDER;
    switch (r->type) {
    case OT_RECORD_FILE_METADATA:
        if (s->file_active || r->object_id <= s->last_object) return OT_VERIFY_ORDER;
        ot_file_metadata_decode(&value.metadata, data, size);
        s->last_object = r->object_id;
        if (value.metadata.kind == OT_FILE_DIRECTORY) {
            if (s->directories == 0xfffffffful) return OT_VERIFY_OVERFLOW;
            ++s->directories;
        } else {
            s->file_active = 1u;
            s->file_size = value.metadata.file_size;
            s->file_last = 0u;
            s->file_bytes = s->file_crc = s->fragments = 0ul;
        }
        return OT_FORMAT_OK;
    case OT_RECORD_FILE_DATA:
        if (!s->file_active || r->object_id != s->last_object || s->file_last ||
            size == 0u || s->file_bytes > s->file_size || r->fragment_offset != s->file_bytes ||
            (((r->flags & OT_RECORD_FLAG_FIRST) != 0u) != (s->fragments == 0ul)) ||
            r->data_size > s->file_size - s->file_bytes) return OT_VERIFY_FILE_DATA;
        if (s->fragments == 0xfffffffful) return OT_VERIFY_OVERFLOW;
        s->file_bytes += r->data_size;
        s->file_last = (r->flags & OT_RECORD_FLAG_LAST) != 0u;
        if ((s->file_bytes == s->file_size) != (s->file_last != 0u)) return OT_VERIFY_FILE_DATA;
        ++s->fragments;
        s->file_crc = ot_crc32_extend(s->file_crc, data, size);
        return OT_FORMAT_OK;
    case OT_RECORD_FILE_END:
        ot_file_end_decode(&value.file_end, data, size);
        if (!s->file_active || r->object_id != s->last_object ||
            s->file_bytes != s->file_size ||
            (s->file_size != 0ul && !s->file_last) ||
            value.file_end.file_size != s->file_size ||
            value.file_end.fragments != s->fragments) return OT_VERIFY_FILE_DATA;
        if (value.file_end.crc32 != s->file_crc) return OT_VERIFY_FILE_CRC;
        if (s->files == 0xfffffffful) return OT_VERIFY_OVERFLOW;
        low = s->bytes_low + s->file_bytes;
        carry = low < s->bytes_low;
        if (carry && s->bytes_high == 0xfffffffful) return OT_VERIFY_OVERFLOW;
        s->bytes_low = low;
        s->bytes_high += carry;
        ++s->files;
        s->file_active = 0u;
        return OT_FORMAT_OK;
    case OT_RECORD_SESSION_END:
        ot_session_end_decode(&value.end, data, size);
        if (value.end.files != s->files || value.end.directories != s->directories ||
            value.end.bytes_low != s->bytes_low || value.end.bytes_high != s->bytes_high)
            return OT_VERIFY_SUMMARY;
        if (value.end.result != OT_SESSION_COMPLETE) return OT_VERIFY_INCOMPLETE;
        if (s->file_active) return OT_VERIFY_INCOMPLETE;
        if (s->sessions == 0xfffffffful) return OT_VERIFY_OVERFLOW;
        s->session_active = 0u;
        ++s->sessions;
        return OT_FORMAT_OK;
    default: return OT_VERIFY_UNSUPPORTED;
    }
}

int ot_archive_verify_block(struct ot_archive_verify *state,
                            const oa_u8 *block, oa_u16 size)
{
    struct ot_archive_verify next;
    struct ot_record_header record;
    oa_u16 offset, i;
    int rc;
    if (state == 0 || block == 0) return OT_FORMAT_BAD_ARGUMENT;
    next = *state;
    rc = ot_native_scan_block(&next.framing, block, size);
    if (rc != OT_FORMAT_OK) return rc;
    if (state->session_active &&
        (next.framing.last_block.session_id != state->last_session ||
         next.framing.last_block.session_time != state->session_time)) return OT_VERIFY_ORDER;
    if (!state->session_active && next.framing.last_block.record_count == 0u &&
        next.framing.last_block.session_id != 0ul) return OT_VERIFY_ORDER;
    offset = OT_BLOCK_HEADER_SIZE;
    for (i = 0u; i < next.framing.last_block.record_count; ++i) {
        ot_record_header_decode(&record, block + offset);
        offset = (oa_u16)(offset + OT_RECORD_HEADER_SIZE);
        rc = verify_record(&next, &next.framing.last_block, &record, block + offset);
        if (rc != OT_FORMAT_OK) return rc;
        offset = (oa_u16)(offset + (oa_u16)record.data_size);
    }
    *state = next;
    return OT_FORMAT_OK;
}

int ot_archive_verify_finish(const struct ot_archive_verify *s)
{
    if (s == 0) return OT_FORMAT_BAD_ARGUMENT;
    if (!s->framing.have_label || s->sessions == 0ul || s->session_active || s->file_active)
        return OT_VERIFY_INCOMPLETE;
    return OT_FORMAT_OK;
}

#include "opentape/native_scan.h"
#include "opentape/record_payload.h"

static int typed_payload(const struct ot_block_header *block,
                           const struct ot_record_header *record, const oa_u8 *data)
{
    union {
        struct ot_session_start start;
        struct ot_session_end end;
        struct ot_file_metadata metadata;
        struct ot_file_end file_end;
    } value;
    unsigned session_record;
    oa_u16 size = (oa_u16)record->data_size;

    if (record->type < OT_RECORD_SESSION_START || record->type > OT_RECORD_SESSION_END)
        return OT_FORMAT_OK;
    session_record = record->type == OT_RECORD_SESSION_START || record->type == OT_RECORD_SESSION_END;
    if (block->session_id == 0ul || (session_record ? record->object_id != 0ul : record->object_id == 0ul))
        return OT_PAYLOAD_BAD_DATA;
    if (record->type == OT_RECORD_FILE_DATA) return OT_FORMAT_OK;
    if (record->fragment_offset != 0ul ||
        record->flags != (OT_RECORD_FLAG_FIRST | OT_RECORD_FLAG_LAST)) return OT_PAYLOAD_BAD_DATA;
    switch (record->type) {
    case OT_RECORD_SESSION_START: return ot_session_start_decode(&value.start, data, size);
    case OT_RECORD_SESSION_END: return ot_session_end_decode(&value.end, data, size);
    case OT_RECORD_FILE_METADATA: return ot_file_metadata_decode(&value.metadata, data, size);
    case OT_RECORD_FILE_END: return ot_file_end_decode(&value.file_end, data, size);
    default: return OT_FORMAT_OK;
    }
}

int ot_native_scan_block(struct ot_native_scan *scan,
                          const oa_u8 *block, oa_u16 size)
{
    struct ot_native_scan next;
    struct ot_block_header header;
    struct ot_record_header record;
    oa_u16 offset, i, available;
    const oa_u8 *data;
    int rc;

    if (scan == 0 || block == 0) return OT_FORMAT_BAD_ARGUMENT;
    rc = ot_block_decode(&header, block, size);
    if (rc != OT_FORMAT_OK) return rc;
    if (header.block_number != scan->blocks || scan->blocks == 0xfffffffful ||
        scan->records > 0xfffffffful - header.record_count)
        return OT_SCAN_BAD_SEQUENCE;
    if ((header.flags & ~(OT_BLOCK_FLAG_FIRST | OT_BLOCK_FLAG_LAST)) != 0u)
        return OT_SCAN_BAD_RECORD;
    if (scan->blocks == 0ul && (header.record_count == 0u || header.session_id != 0ul))
        return OT_SCAN_MISSING_LABEL;

    next = *scan;
    offset = OT_BLOCK_HEADER_SIZE;
    for (i = 0u; i < header.record_count; ++i) {
        available = (oa_u16)(size - offset);
        if (available < OT_RECORD_HEADER_SIZE) return OT_SCAN_BAD_RECORD;
        ot_record_header_decode(&record, block + offset);
        offset = (oa_u16)(offset + OT_RECORD_HEADER_SIZE);
        available = (oa_u16)(size - offset);
        if (record.data_size > (oa_u32)available ||
            record.fragment_offset > 0xfffffffful - record.data_size ||
            (record.flags & ~(OT_RECORD_FLAG_FIRST | OT_RECORD_FLAG_LAST)) != 0u ||
            ((record.flags & OT_RECORD_FLAG_FIRST) != 0u && record.fragment_offset != 0ul))
            return OT_SCAN_BAD_RECORD;
        data = block + offset;
        if (ot_crc32(data, (oa_u16)record.data_size) != record.data_crc32)
            return OT_SCAN_RECORD_CRC;
        if (scan->blocks == 0ul && i == 0u && record.type != OT_RECORD_VOLUME_LABEL)
            return OT_SCAN_MISSING_LABEL;
        rc = typed_payload(&header, &record, data);
        if (rc != OT_FORMAT_OK) return rc;
        if (record.type == OT_RECORD_VOLUME_LABEL) {
            if (next.have_label || scan->blocks != 0ul || i != 0u ||
                record.data_size != OT_VOLUME_LABEL_SIZE || record.fragment_offset != 0ul || record.object_id != 0ul ||
                record.flags != (OT_RECORD_FLAG_FIRST | OT_RECORD_FLAG_LAST))
                return OT_SCAN_BAD_LABEL;
            rc = ot_volume_label_decode(&next.label, data);
            if (rc != OT_FORMAT_OK) return rc;
            if (data[14] != 0u || data[15] != 0u) return OT_SCAN_BAD_LABEL;
            next.have_label = 1u;
        }
        offset = (oa_u16)(offset + (oa_u16)record.data_size);
    }
    if (offset != size) return OT_SCAN_BAD_RECORD;
    next.blocks++;
    next.records += header.record_count;
    next.last_block = header;
    *scan = next;
    return OT_FORMAT_OK;
}

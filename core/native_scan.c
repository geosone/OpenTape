#include "opentape/native_scan.h"

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
        if (record.type == OT_RECORD_VOLUME_LABEL) {
            if (next.have_label || scan->blocks != 0ul || i != 0u ||
                record.data_size != OT_VOLUME_LABEL_SIZE || record.fragment_offset != 0ul ||
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

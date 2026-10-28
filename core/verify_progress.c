/* Progress derives only from whole verified blocks. It keeps payload and path
 * state across session headers, unlike the verifier's per-session counters. */
#include <string.h>
#include "opentape/verify_progress.h"
static int add_bytes(struct ot_verify_progress *p, oa_u32 count)
{
    oa_u32 before = p->bytes_low;
    p->bytes_low += count;
    if (p->bytes_low < before && ++p->bytes_high == 0ul) return OT_VERIFY_OVERFLOW;
    return 0;
}
int ot_verify_progress_block(struct ot_verify_progress *progress, const struct ot_archive_verify *verified,
                              const oa_u8 *block, unsigned size)
{
    struct ot_verify_progress next;
    struct ot_record_header record;
    unsigned offset, i;
    if (progress == 0 || verified == 0 || block == 0 || size < OT_BLOCK_HEADER_SIZE || (oa_u32)size > 65535ul ||
        progress->blocks == 0xfffffffful || verified->framing.blocks != progress->blocks + 1ul)
        return OT_FORMAT_BAD_ARGUMENT;
    next = *progress; offset = OT_BLOCK_HEADER_SIZE;
    for (i = 0u; i < verified->framing.last_block.record_count; ++i) {
        if (size - offset < OT_RECORD_HEADER_SIZE || ot_record_header_decode(&record, block + offset))
            return OT_FORMAT_BAD_SIZE;
        offset += OT_RECORD_HEADER_SIZE;
        if (record.data_size > (oa_u32)(size - offset)) return OT_FORMAT_BAD_SIZE;
        if (record.type == OT_RECORD_FILE_METADATA) {
            if (ot_file_metadata_decode(&next.current, block + offset, (oa_u16)record.data_size)) return OT_PAYLOAD_BAD_DATA;
            next.file_bytes = 0ul;
            if (next.current.kind == OT_FILE_DIRECTORY && ++next.directories == 0ul) return OT_VERIFY_OVERFLOW;
        } else if (record.type == OT_RECORD_FILE_DATA) {
            if (next.current.kind != OT_FILE_REGULAR || next.file_bytes > next.current.file_size ||
                record.data_size > next.current.file_size - next.file_bytes) return OT_VERIFY_FILE_DATA;
            if (add_bytes(&next, record.data_size)) return OT_VERIFY_OVERFLOW;
            next.file_bytes += record.data_size;
        } else if (record.type == OT_RECORD_FILE_END) {
            if (next.current.kind != OT_FILE_REGULAR || next.file_bytes != next.current.file_size) return OT_VERIFY_FILE_DATA;
            if (++next.files == 0ul) return OT_VERIFY_OVERFLOW;
        } else if (record.type != OT_RECORD_VOLUME_LABEL && record.type != OT_RECORD_SESSION_START &&
                   record.type != OT_RECORD_SESSION_END) return OT_VERIFY_UNSUPPORTED;
        offset += (unsigned)record.data_size;
    }
    if (offset != size) return OT_FORMAT_BAD_SIZE;
    next.blocks = verified->framing.blocks; next.records = verified->framing.records;
    next.sessions = verified->sessions; next.boundary = !verified->session_active && verified->sessions != 0ul;
    *progress = next; return 0;
}

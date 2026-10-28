#include <string.h>
#include "opentape/verify_stream.h"

int ot_verify_stream_observed(struct ot_tape_reader *r, struct ot_archive_verify *v,
                       oa_u8 *buffer, unsigned capacity, oa_u32 limit,
                       ot_metadata_visitor visitor, void *context, ot_verify_observer observer, void *observer_context)
{
    unsigned bytes, i;
    oa_u16 offset;
    struct ot_record_header record;
    struct ot_file_metadata metadata;
    oa_u32 reads = 0ul;
    int rc;
    if (r == 0 || !r->opened || r->observing || v == 0 || buffer == 0 || capacity < 2u ||
        (oa_u32)capacity > 65535ul) return OT_READ_BAD_ARGUMENT;
    if ((r->status.tape.device_parameters & OA_MT_PARAM_FIXED_BLOCK) != 0u)
        return OT_READ_BAD_MODE;
    for (;;) {
        if (limit != 0ul && reads >= limit) return OT_VERIFY_STREAM_LIMIT;
        rc = ot_reader_read(r, buffer, capacity, &bytes);
        if (rc == OT_READ_FILEMARK) return ot_archive_verify_finish(v);
        if (rc != OT_READ_BLOCK) return rc;
        rc = ot_archive_verify_block(v, buffer, (oa_u16)bytes);
        if (rc != OT_FORMAT_OK) return rc;
        if (visitor != 0) {
            offset = OT_BLOCK_HEADER_SIZE;
            for (i = 0u; i < v->framing.last_block.record_count; ++i) {
                ot_record_header_decode(&record, buffer + offset);
                offset = (oa_u16)(offset + OT_RECORD_HEADER_SIZE);
                if (record.type == OT_RECORD_FILE_METADATA) {
                    ot_file_metadata_decode(&metadata, buffer + offset, (oa_u16)record.data_size);
                    if (visitor(context, v->framing.last_block.session_id, record.object_id, &metadata) != 0)
                        return OT_VERIFY_VISITOR_FAILED;
                }
                offset = (oa_u16)(offset + (oa_u16)record.data_size);
            }
        }
        if (observer != 0) {
            r->observing = 1u;
            observer(observer_context, v, buffer, bytes);
            r->observing = 0u;
        }
        /* The verifier rejects block-counter overflow before it can wrap. */
        ++reads;
    }
}

int ot_verify_stream_visit(struct ot_tape_reader *r, struct ot_archive_verify *v,
                           oa_u8 *buffer, unsigned capacity, oa_u32 limit,
                           ot_metadata_visitor visitor, void *context)
{
    return ot_verify_stream_observed(r, v, buffer, capacity, limit, visitor, context, 0, 0);
}

int ot_verify_stream(struct ot_tape_reader *r, struct ot_archive_verify *v,
                       oa_u8 *buffer, unsigned capacity, oa_u32 limit)
{
    return ot_verify_stream_visit(r, v, buffer, capacity, limit, 0, 0);
}

int ot_verify_session_observed(const struct ot_reader_ops *ops, const char *name,
                      oa_u8 *buffer, unsigned capacity, ot_metadata_visitor visitor,
                      void *context, ot_verify_observer observer, void *observer_context, struct ot_verify_session_result *result)
{
    int rc;
    if (result == 0) return OT_READ_BAD_ARGUMENT;
    memset(result, 0, sizeof(*result));
    result->transport_error = 1u;
    if (buffer == 0 || capacity < 2u || (oa_u32)capacity > 65535ul ||
        !ot_probe_raw_name_valid(name)) return OT_READ_BAD_ARGUMENT;
    rc = ot_reader_open(&result->reader, ops, name);
    if (rc != 0) return rc;
    return ot_verify_session_opened_observed(buffer, capacity, visitor, context, observer, observer_context, result);
}

int ot_verify_session_opened_observed(oa_u8 *buffer, unsigned capacity, ot_metadata_visitor visitor,
                              void *context, ot_verify_observer observer, void *observer_context, struct ot_verify_session_result *result)
{
    unsigned first_error;
    int rc, close_rc;
    if (result == 0 || !result->reader.opened || result->reader.observing || buffer == 0 || capacity < 2u ||
        (oa_u32)capacity > 65535UL) return OT_READ_BAD_ARGUMENT;
    memset(&result->verify, 0, sizeof(result->verify));
    rc = ot_verify_stream_observed(&result->reader, &result->verify, buffer,
                                 capacity, 0ul, visitor, context, observer, observer_context);
    /* A fixed-framing backend latches malformed OTB1/padding to prevent
     * further reads, but that is archive corruption, not a failed SCSI command.
     * Preserve the parser's result code while still closing the poisoned reader. */
    result->transport_error = (result->reader.failed &&
        !(rc >= OT_FORMAT_BAD_PAYLOAD_CRC && rc <= OT_FORMAT_BAD_ARGUMENT)) ||
        rc == OT_READ_BAD_MODE || rc == OT_READ_BAD_ARGUMENT;
    first_error = result->reader.dos_error;
    close_rc = ot_reader_close(&result->reader);
    if (rc == 0 && close_rc != 0) {
        result->transport_error = 1u;
        rc = close_rc;
    } else if (rc != 0) result->reader.dos_error = first_error;
    return rc;
}

int ot_verify_session(const struct ot_reader_ops *ops, const char *name,
                      oa_u8 *buffer, unsigned capacity, ot_metadata_visitor visitor,
                      void *context, struct ot_verify_session_result *result)
{
    return ot_verify_session_observed(ops, name, buffer, capacity, visitor, context, 0, 0, result);
}
int ot_verify_session_opened(oa_u8 *buffer, unsigned capacity, ot_metadata_visitor visitor,
                             void *context, struct ot_verify_session_result *result)
{
    return ot_verify_session_opened_observed(buffer, capacity, visitor, context, 0, 0, result);
}

/* Native archive transport: variable records pass through; fixed sessions use
 * caller-owned scratch storage to frame self-sized OTB1 blocks independently.
 * Physical padding is validated/stripped below the archive verifier, while
 * writer finalization remains explicit and shared with the variable path.
 */
#include <string.h>
#include "opentape/archive_native.h"

static void read_diagnostics(struct ot_tape_reader *r, const struct ot_native_reader *n)
{
    r->dos_error = n->platform_error;
    r->status.tape.last_scsi_status = n->last.target_status;
    r->status.tape.last_sense_key = n->last.sense_key;
    r->status.tape.last_scsi_opcode = n->last.scsi_opcode;
    if ((n->last.completion_flags & OT_TAPE_COUNT_VALID) != 0u)
        r->status.tape.residual_bytes = (oa_u16)(n->last.requested - n->last.actual);
}
/* Physical fixed blocks frame the existing self-sized OTB1 bytes. Length and
 * header CRC are qualified before requesting the remainder; padding must be
 * zero and never enters the archive checksum or logical accepted count. */
static oa_u32 fixed_u32(const oa_u8 *p)
{
    return (oa_u32)p[0] | ((oa_u32)p[1] << 8) |
        ((oa_u32)p[2] << 16) | ((oa_u32)p[3] << 24);
}
static int fixed_read_error(struct ot_tape_reader *r, int error)
{
    r->failed = 1u;
    if (error != 0) r->backend_result = error;
    return error != 0 ? error : OT_READ_IO_ERROR;
}
static int read_fixed(struct ot_tape_reader *r, void *data, unsigned capacity, unsigned *accepted)
{
    struct ot_native_reader *n = (struct ot_native_reader *)r->backend_context;
    struct ot_block_header header;
    oa_u8 *out = (oa_u8 *)data, *scratch = r->backend_buffer;
    oa_u32 done = 0UL, target = OT_BLOCK_HEADER_SIZE, rounded;
    unsigned unit = (unsigned)n->block_size, maximum, request, got, copy, i;
    int rc, have_header = 0, mark;
    if (capacity < OT_BLOCK_HEADER_SIZE || data == scratch) return OT_READ_BAD_ARGUMENT;
    maximum = r->backend_capacity - r->backend_capacity % unit;
    for (;;) {
        rounded = ((target - done + unit - 1UL) / unit) * unit;
        request = rounded > maximum ? maximum : (unsigned)rounded;
        rc = ot_native_reader_read(n, scratch, request);
        r->backend_result = rc;
        read_diagnostics(r, n);
        if (rc < 0 || (n->last.completion_flags & OT_TAPE_COUNT_VALID) == 0u ||
            (n->last.stream_flags & (OT_TAPE_STREAM_EOM | OT_TAPE_STREAM_EOD | OT_TAPE_STREAM_TRUNCATED)) != 0UL)
            return fixed_read_error(r, 0);
        got = (unsigned)n->last.actual;
        mark = (n->last.stream_flags & OT_TAPE_STREAM_FILEMARK) != 0UL;
        if (got == 0u) {
            if (mark && done == 0UL) return OT_READ_FILEMARK;
            return fixed_read_error(r, OT_FORMAT_BAD_SIZE);
        }
        copy = (unsigned)((oa_u32)capacity - done);
        if (copy > got) copy = got;
        memcpy(out + (unsigned)done, scratch, copy);
        if (!have_header && done + got >= OT_BLOCK_HEADER_SIZE) {
            if (memcmp(out, "OTB1", 4u) != 0) return fixed_read_error(r, OT_FORMAT_BAD_MAGIC);
            if (out[4] != OT_VOLUME_FORMAT_VERSION || out[5] != 0u)
                return fixed_read_error(r, OT_FORMAT_BAD_VERSION);
            if (out[6] != OT_BLOCK_HEADER_SIZE || out[7] != 0u)
                return fixed_read_error(r, OT_FORMAT_BAD_SIZE);
            if (fixed_u32(out + 32u) != ot_crc32(out, 32u))
                return fixed_read_error(r, OT_FORMAT_BAD_HEADER_CRC);
            target = fixed_u32(out + 8u);
            if (target < OT_BLOCK_HEADER_SIZE || target > capacity)
                return fixed_read_error(r, OT_FORMAT_BAD_SIZE);
            have_header = 1;
        }
        if (have_header && done + got >= target) {
            /* This transfer may end inside a padded physical block. */
            for (i = (unsigned)(target - done); i < got; ++i)
                if (scratch[i] != 0u) return fixed_read_error(r, OT_FORMAT_BAD_SIZE);
            rc = ot_block_decode(&header, out, (oa_u16)target);
            if (rc != 0) return fixed_read_error(r, rc);
            *accepted = (unsigned)target;
            if (mark) r->pending_filemark = 1u;
            return OT_READ_BLOCK;
        }
        done += got;
        if (mark) return fixed_read_error(r, OT_FORMAT_BAD_SIZE);
    }
}
static int read_record(struct ot_tape_reader *r, void *data, unsigned capacity, unsigned *accepted)
{
    struct ot_native_reader *n = (struct ot_native_reader *)r->backend_context;
    int rc;
    if (r->pending_filemark) {
        r->pending_filemark = 0u;
        return OT_READ_FILEMARK;
    }
    if (n->block_size != 0UL) return read_fixed(r, data, capacity, accepted);
    rc = ot_native_reader_read(n, data, capacity);
    r->backend_result = rc;
    read_diagnostics(r, n);
    if (rc < 0 || (n->last.completion_flags & OT_TAPE_COUNT_VALID) == 0u ||
        (n->last.stream_flags & (OT_TAPE_STREAM_EOM | OT_TAPE_STREAM_EOD | OT_TAPE_STREAM_TRUNCATED)) != 0UL) {
        r->failed = 1u;
        return OT_READ_IO_ERROR;
    }
    *accepted = (unsigned)n->last.actual;
    if ((n->last.stream_flags & OT_TAPE_STREAM_FILEMARK) != 0UL) {
        if (*accepted == 0u) return OT_READ_FILEMARK;
        r->pending_filemark = 1u; /* archive consumes data before the same mark */
    }
    return OT_READ_BLOCK; /* An empty ordinary record is not EOF. */
}
static int close_reader(struct ot_tape_reader *r)
{
    struct ot_native_reader *n = (struct ot_native_reader *)r->backend_context;
    int rc = ot_native_reader_close(n);
    r->opened = 0u;
    if (!r->failed) { r->dos_error = n->platform_error; r->backend_result = rc; }
    return rc == 0 ? OT_PROBE_OK : OT_PROBE_CLOSE_FAILED;
}
static int attach_reader(struct ot_tape_reader *r, struct ot_native_reader *n, oa_u8 *scratch, unsigned capacity)
{
    if (r == 0 || r->opened || n == 0 || !n->opened || n->failed) return OT_READ_BAD_ARGUMENT;
    if (n->block_size != 0UL &&
        (scratch == 0 || n->block_size > capacity || (oa_u32)capacity > 65535UL))
        return OT_READ_BAD_MODE;
    memset(r, 0, sizeof(*r));
    r->backend_context = n;
    r->backend_buffer = scratch;
    r->backend_capacity = capacity;
    r->backend_read = read_record;
    r->backend_close = close_reader;
    r->opened = 1u;
    /* Higher archive layers see complete logical OTB1 blocks, not raw physical
     * fixed blocks, so the legacy FIXED flag must not trigger their raw-mode gate. */
    r->status.tape.current_state = OA_MT_STATE_UNBUFFERED;
    return 0;
}
int ot_archive_native_reader(struct ot_tape_reader *r, struct ot_native_reader *n)
{ return attach_reader(r, n, 0, 0u); }
int ot_archive_native_reader_fixed(struct ot_tape_reader *r, struct ot_native_reader *n,
                                    oa_u8 *scratch, unsigned capacity)
{
    if (n == 0 || n->block_size == 0UL) return OT_READ_BAD_MODE;
    return attach_reader(r, n, scratch, capacity);
}
static void output_diagnostics(struct ot_tape_output *o, const struct ot_native_writer *n)
{
    o->dos_error = n->platform_error;
    if (n->last_control.result == OT_TAPE_RESULT_ERROR) {
        o->status.tape.last_scsi_status = n->last_control.target_status;
        o->status.tape.last_sense_key = n->last_control.sense_key;
        o->status.tape.last_scsi_opcode = n->last_control.scsi_opcode;
    } else {
        o->status.tape.last_scsi_status = n->last_record.target_status;
        o->status.tape.last_sense_key = n->last_record.sense_key;
        o->status.tape.last_scsi_opcode = n->last_record.scsi_opcode;
        if ((n->last_record.completion_flags & OT_TAPE_COUNT_VALID) != 0u)
            o->status.tape.residual_bytes = (oa_u16)(n->last_record.requested - n->last_record.actual);
    }
}
/* Emit each logical block independently. Never pack the next OTB1 block into
 * its predecessor's padding, and never retry a possibly consumed physical block.
 */
static int write_fixed(struct ot_tape_output *o, const void *data, unsigned bytes, unsigned *accepted)
{
    struct ot_native_writer *n = (struct ot_native_writer *)o->backend_context;
    struct ot_block_header header;
    const oa_u8 *in = (const oa_u8 *)data;
    unsigned unit = (unsigned)n->block_size, maximum, done = 0u, take, physical, count;
    int rc;
    if (data == o->backend_buffer || ot_block_decode(&header, in, (oa_u16)bytes) != 0)
        return OT_OUTPUT_BAD_ARGUMENT;
    maximum = o->backend_capacity - o->backend_capacity % unit;
    while (done < bytes) {
        take = bytes - done;
        if (take > maximum) take = maximum;
        physical = (unsigned)(((oa_u32)take + unit - 1UL) / unit * unit);
        memcpy(o->backend_buffer, in + done, take);
        memset(o->backend_buffer + take, 0, physical - take);
        rc = ot_native_writer_write(n, o->backend_buffer, physical);
        o->backend_result = rc;
        output_diagnostics(o, n);
        count = 0u;
        if ((n->last_record.completion_flags & OT_TAPE_COUNT_VALID) != 0u)
            count = n->last_record.actual > take ? take : (unsigned)n->last_record.actual;
        *accepted = done + count;
        if (rc != 0 || n->last_record.actual != physical) {
            o->failed = 1u;
            return OT_OUTPUT_IO_ERROR;
        }
        done += take;
    }
    return 0;
}
static int write_record(struct ot_tape_output *o, const void *data, unsigned bytes, unsigned *accepted)
{
    struct ot_native_writer *n = (struct ot_native_writer *)o->backend_context;
    int rc;
    if (n->block_size != 0UL) return write_fixed(o, data, bytes, accepted);
    rc = ot_native_writer_write(n, data, bytes);
    o->backend_result = rc;
    output_diagnostics(o, n);
    if ((n->last_record.completion_flags & OT_TAPE_COUNT_VALID) != 0u)
        *accepted = (unsigned)n->last_record.actual;
    if (rc != 0 || *accepted != bytes) {
        o->failed = 1u;
        return OT_OUTPUT_IO_ERROR;
    }
    return 0;
}
static int close_output(struct ot_tape_output *o)
{
    struct ot_native_writer *n = (struct ot_native_writer *)o->backend_context;
    int rc = ot_native_writer_close(n);
    o->opened = 0u;
    if (!o->failed) { o->backend_result = rc; output_diagnostics(o, n); }
    if (rc != 0 || o->failed) { o->failed = 1u; return OT_OUTPUT_IO_ERROR; }
    return 0;
}
static int finish_output(struct ot_tape_output *o)
{
    struct ot_native_writer *n = (struct ot_native_writer *)o->backend_context;
    int rc, close_rc;
    if (!o->opened || o->failed) return OT_OUTPUT_FAILED;
    rc = ot_native_writer_flush(n);
    if (rc == 0) rc = ot_native_writer_filemark(n);
    if (rc == 0) rc = ot_native_writer_finish(n);
    o->backend_result = rc;
    output_diagnostics(o, n);
    if (rc != 0) o->failed = 1u;
    close_rc = close_output(o); /* failure cleanup only aborts ownership */
    if (rc != 0 || close_rc != 0) return OT_OUTPUT_IO_ERROR;
    o->finished = 1u;
    return 0;
}
static int sink_write(void *context, const oa_u8 *data, unsigned bytes, unsigned *accepted)
{ return ot_output_write((struct ot_tape_output *)context, data, bytes, accepted); }
static int sink_finish(void *context)
{ return finish_output((struct ot_tape_output *)context); }
static int attach_output(struct ot_tape_output *o, struct ot_native_writer *n, oa_u8 *scratch, unsigned capacity)
{
    if (o == 0 || o->opened || o->ops != 0 || n == 0 || !n->opened || !n->claimed || n->failure)
        return OT_OUTPUT_BAD_ARGUMENT;
    if (n->block_size != 0UL &&
        (scratch == 0 || n->block_size > capacity || (oa_u32)capacity > 65535UL))
        return OT_OUTPUT_BAD_MODE;
    memset(o, 0, sizeof(*o));
    o->backend_context = n;
    o->backend_buffer = scratch;
    o->backend_capacity = capacity;
    o->backend_write = write_record;
    o->backend_finish = finish_output;
    o->backend_close = close_output;
    o->sink.context = o;
    o->sink.write_block = sink_write;
    o->sink.finish_session = sink_finish;
    o->opened = 1u;
    return 0;
}

int ot_archive_native_output(struct ot_tape_output *o, struct ot_native_writer *n)
{ return attach_output(o, n, 0, 0u); }
int ot_archive_native_output_fixed(struct ot_tape_output *o, struct ot_native_writer *n,
                                    oa_u8 *scratch, unsigned capacity)
{
    if (n == 0 || n->block_size == 0UL) return OT_OUTPUT_BAD_MODE;
    return attach_output(o, n, scratch, capacity);
}

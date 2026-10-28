#include <string.h>
#include "opentape/tape_reader.h"

int ot_reader_open(struct ot_tape_reader *r,
                    const struct ot_reader_ops *ops, const char *name)
{
    unsigned flags, error;
    int rc;

    if (r == 0 || r->opened || ops == 0 ||
        !ot_probe_name_valid(name) || (name[1] != 'R' && name[1] != 'r') ||
        ops->probe.open_readonly == 0 || ops->probe.device_info == 0 ||
        ops->probe.status == 0 || ops->probe.close == 0 ||
        ops->set_raw == 0 || ops->read == 0) return OT_READ_BAD_ARGUMENT;
    memset(r, 0, sizeof(*r));
    r->ops = ops;
    error = ops->probe.open_readonly(ops->probe.context, name, &r->handle);
    r->dos_error = error;
    if (error != 0u) return OT_PROBE_OPEN_FAILED;
    r->opened = 1u;
    error = ops->probe.device_info(ops->probe.context, r->handle, &flags);
    rc = OT_PROBE_IOCTL_FAILED;
    if (error == 0u) {
        if ((flags & 0x80u) == 0u) rc = OT_PROBE_NOT_DEVICE;
        else {
            /* Set only the low-byte device mode; never pass info high bits. */
            error = ops->set_raw(ops->probe.context, r->handle,
                                 (flags & 0xffu) | 0x20u);
            if (error == 0u)
                rc = ot_tape_query(&ops->probe, r->handle, &r->status, &error);
        }
    }
    if (rc == OT_PROBE_OK &&
        ((r->status.tape.current_state & 3u) != 2u ||
         ((r->status.tape.device_parameters & OA_MT_PARAM_FIXED_BLOCK) != 0u &&
          r->status.tape.fixed_block_size == 0u))) rc = OT_READ_BAD_MODE;
    r->dos_error = error;
    if (rc != OT_PROBE_OK) {
        /* Cleanup may not replace the first failure. */
        ops->probe.close(ops->probe.context, r->handle);
        r->opened = 0u;
    }
    return rc;
}

int ot_reader_read(struct ot_tape_reader *r, void *buffer,
                    unsigned capacity, unsigned *accepted)
{
    unsigned got, error, status_error, block_size;
    int rc;

    if (accepted != 0) *accepted = 0u;
    if (r == 0 || !r->opened || r->observing || buffer == 0 || accepted == 0 ||
        capacity < 2u || (oa_u32)capacity > 65535ul) return OT_READ_BAD_ARGUMENT;
    if (r->failed) return OT_READ_FAILED_STATE;
    if (r->backend_read != 0) return r->backend_read(r, buffer, capacity, accepted);
    block_size = r->status.tape.fixed_block_size;
    if ((r->status.tape.device_parameters & OA_MT_PARAM_FIXED_BLOCK) != 0u &&
        capacity % block_size != 0u) return OT_READ_BAD_ARGUMENT;
    got = 0u;
    error = r->ops->read(r->ops->probe.context, r->handle, buffer, capacity, &got);
    rc = ot_tape_query(&r->ops->probe, r->handle, &r->status, &status_error);
    r->dos_error = error != 0u ? error : status_error;
    if (error != 0u || rc != OT_PROBE_OK || got > capacity ||
        ((r->status.tape.device_parameters & OA_MT_PARAM_FIXED_BLOCK) != 0u &&
         (block_size == 0u || got % block_size != 0u)) ||
        (r->status.extended && r->status.tape.driver_error != 0u)) {
        r->failed = 1u;
        return OT_READ_IO_ERROR;
    }
    *accepted = got;
    return got == 0u ? OT_READ_FILEMARK : OT_READ_BLOCK;
}

int ot_reader_close(struct ot_tape_reader *r)
{
    unsigned error;
    if (r == 0 || !r->opened || r->observing) return OT_READ_BAD_ARGUMENT;
    if (r->backend_close != 0) return r->backend_close(r);
    error = r->ops->probe.close(r->ops->probe.context, r->handle);
    r->opened = 0u;
    if (error != 0u) {
        if (!r->failed) r->dos_error = error;
        return OT_PROBE_CLOSE_FAILED;
    }
    return OT_PROBE_OK;
}

#include <string.h>
#include "opentape/tape_output.h"

static int healthy(const struct ot_tape_status *s)
{
    return s->extended && s->tape.driver_error == 0u &&
        (s->tape.current_state & OA_MT16_STATE_WRITE_ERROR) == 0u;
}

static int mode_ok(const struct ot_tape_status *s)
{
    return healthy(s) &&
        (s->tape.current_state & (3u | OA_MT_STATE_REWIND_CLOSE |
         OA_MT_STATE_WRITTEN | OA_MT16_STATE_MTOP |
         OA_MT16_STATE_REWINDING)) == OA_MT_STATE_UNBUFFERED &&
        (s->tape.device_parameters & OA_MT_PARAM_FIXED_BLOCK) == 0u;
}

static int sink_write(void *p, const oa_u8 *b, unsigned n, unsigned *accepted)
{
    return ot_output_write((struct ot_tape_output *)p, b, n, accepted);
}

static int sink_finish(void *p)
{
    struct ot_tape_output *o = (struct ot_tape_output *)p;
    return o->backend_finish != 0 ? o->backend_finish(o) : ot_output_close(o);
}

int ot_output_open(struct ot_tape_output *o, const struct ot_output_ops *ops,
                   const char *name, unsigned authorized)
{
    unsigned flags, error;
    int rc;
    if (o == 0 || o->opened || o->ops != 0 || ops == 0 ||
        !ot_probe_name_valid(name) || (name[1] != 'R' && name[1] != 'r') ||
        ops->probe.open_readonly == 0 || ops->probe.device_info == 0 ||
        ops->probe.status == 0 || ops->probe.close == 0 ||
        ops->open_write == 0 || ops->set_raw == 0 || ops->write == 0)
        return OT_OUTPUT_BAD_ARGUMENT;
    if (!authorized) return OT_OUTPUT_NOT_AUTHORIZED;
    memset(o, 0, sizeof(*o));
    o->ops = ops;
    strcpy(o->name, name);
    /* Reject disk files and incompatible drivers before writable open. */
    rc = ot_tape_probe(&ops->probe, name, &o->status, &o->dos_error);
    if (rc != OT_PROBE_OK) return rc;
    if (!mode_ok(&o->status)) return OT_OUTPUT_BAD_MODE;
    error = ops->open_write(ops->probe.context, name, &o->handle);
    o->dos_error = error;
    if (error != 0u) return OT_PROBE_OPEN_FAILED;
    o->opened = 1u;
    error = ops->probe.device_info(ops->probe.context, o->handle, &flags);
    rc = OT_PROBE_IOCTL_FAILED;
    if (error == 0u) {
        if ((flags & 0x80u) == 0u) rc = OT_PROBE_NOT_DEVICE;
        else {
            error = ops->set_raw(ops->probe.context, o->handle,
                                  (flags & 0xffu) | 0x20u);
            if (error == 0u)
                rc = ot_tape_query(&ops->probe, o->handle, &o->status, &error);
        }
    }
    if (rc == OT_PROBE_OK && !mode_ok(&o->status)) rc = OT_OUTPUT_BAD_MODE;
    o->dos_error = error;
    if (rc != OT_PROBE_OK) {
        ops->probe.close(ops->probe.context, o->handle);
        o->opened = 0u;
        o->failed = 1u;
        return rc;
    }
    o->sink.context = o;
    o->sink.write_block = sink_write;
    o->sink.finish_session = sink_finish;
    return OT_PROBE_OK;
}

int ot_output_write(struct ot_tape_output *o, const void *buffer,
                    unsigned size, unsigned *accepted)
{
    unsigned error, query_error, count = 0u;
    int rc;
    if (accepted != 0) *accepted = 0u;
    if (o == 0 || !o->opened || buffer == 0 || accepted == 0 ||
        size < OT_WRITER_RECORD_OVERHEAD || (oa_u32)size > 65535ul)
        return OT_OUTPUT_BAD_ARGUMENT;
    if (o->failed) return OT_OUTPUT_FAILED;
    if (o->backend_write != 0) return o->backend_write(o, buffer, size, accepted);
    error = o->ops->write(o->ops->probe.context, o->handle, buffer, size, &count);
    *accepted = count;
    rc = ot_tape_query(&o->ops->probe, o->handle, &o->status, &query_error);
    o->dos_error = error != 0u ? error : query_error;
    if (error != 0u || rc != OT_PROBE_OK || !healthy(&o->status) || count != size) {
        o->failed = 1u;
        return OT_OUTPUT_IO_ERROR;
    }
    return OT_PROBE_OK;
}

int ot_output_close(struct ot_tape_output *o)
{
    unsigned error, query_error;
    struct ot_tape_status status;
    int rc;
    if (o == 0 || !o->opened) return OT_OUTPUT_BAD_ARGUMENT;
    if (o->backend_close != 0) return o->backend_close(o);
    /* Driver final close owns both filemarks and no-rewind positioning.
     * Never issue MTWEOF here, and never retry a failed close. */
    error = o->ops->probe.close(o->ops->probe.context, o->handle);
    o->opened = 0u;
    rc = ot_tape_probe(&o->ops->probe, o->name, &status, &query_error);
    if (!o->failed) {
        o->dos_error = error != 0u ? error : query_error;
        if (rc == OT_PROBE_OK) o->status = status;
    }
    if (error != 0u || rc != OT_PROBE_OK || !healthy(&status) || o->failed) {
        o->failed = 1u;
        return OT_OUTPUT_IO_ERROR;
    }
    o->finished = 1u;
    return OT_PROBE_OK;
}

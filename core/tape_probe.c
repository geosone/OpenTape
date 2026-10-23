#include <string.h>
#include "opentape/tape_probe.h"

static char upper_ascii(char c)
{
    return c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c;
}

int ot_probe_name_valid(const char *name)
{
    unsigned n, letter;

    if (name == 0) return 0;
    n = (unsigned)strlen(name);
    if (n != 4u && n != 5u && n != 6u) return 0;
    if (name[n - 1u] == '$') --n;
    if (n != 4u && n != 5u) return 0;
    if (upper_ascii(name[0]) != 'N') return 0;
    letter = 1u;
    if (n == 5u) {
        if (upper_ascii(name[1]) != 'R') return 0;
        letter = 2u;
    }
    if (upper_ascii(name[letter]) < 'A' ||
        upper_ascii(name[letter]) > 'Z') return 0;
    return upper_ascii(name[letter + 1u]) == 'T' &&
           name[letter + 2u] >= '0' && name[letter + 2u] <= '3';
}

int ot_probe_raw_name_valid(const char *name)
{
    unsigned size;
    if (!ot_probe_name_valid(name)) return 0;
    size = (unsigned)strlen(name);
    if (name[size - 1u] == '$') --size;
    return size == 5u && upper_ascii(name[1]) == 'R';
}

static int guard_ok(const unsigned char *buffer, unsigned start)
{
    unsigned i;
    for (i = start; i < 20u; ++i)
        if (buffer[i] != 0xa5u) return 0;
    return 1;
}

int ot_tape_query(const struct ot_probe_ops *ops, unsigned handle,
                  struct ot_tape_status *status, unsigned *dos_error)
{
    unsigned error;
    unsigned char buffer[20];
    struct oa_aspitape_mtget legacy;
    struct ot_tape_status result;
    int rc;

    if (dos_error != 0) *dos_error = 0u;
    if (ops == 0 || status == 0 || dos_error == 0 ||
        ops->status == 0)
        return OT_PROBE_BAD_ARGUMENT;
    memset(&result, 0, sizeof(result));
    rc = OT_PROBE_OK;
    {
        memset(buffer, 0xa5, sizeof(buffer));
        error = ops->status(ops->context, handle, OA_ASPITAPE_IOCTL_MTGET16,
                            buffer, 16u);
        if (!guard_ok(buffer, 16u)) {
            rc = OT_PROBE_UNSUPPORTED_ABI;
        } else if (error == 0u) {
            memcpy(&result.tape, buffer, sizeof(result.tape));
            result.extended = 1u;
        } else if (error == 1u || error == 12u || error == 83u) {
            /* Unsupported function may arrive through INT 24h Fail.
             * Do not silently downgrade unrelated errors (e.g. access). */
            memset(buffer, 0xa5, sizeof(buffer));
            error = ops->status(ops->context, handle, OA_ASPITAPE_IOCTL_MTGET,
                                buffer, 14u);
            if (!guard_ok(buffer, 14u)) {
                rc = OT_PROBE_UNSUPPORTED_ABI;
            } else if (error != 0u) {
                rc = OT_PROBE_IOCTL_FAILED;
            } else {
                memcpy(&legacy, buffer, sizeof(legacy));
                result.tape.host_id = legacy.host_id;
                result.tape.target_id = legacy.target_id;
                result.tape.lun = legacy.lun;
                result.tape.device_parameters = legacy.device_parameters;
                result.tape.current_state = legacy.current_state & 0x1fu;
                result.tape.current_state |=
                    (oa_u16)((legacy.current_state & 0xe0u) << 1);
                result.tape.unit_number = legacy.unit_number;
                result.tape.fixed_block_size = legacy.fixed_block_size;
                result.tape.last_scsi_status = legacy.last_scsi_status;
                result.tape.last_sense_key = legacy.last_sense_key;
                result.tape.last_scsi_opcode = legacy.last_scsi_opcode;
                result.tape.residual_bytes = legacy.residual_bytes;
            }
        } else {
            rc = OT_PROBE_IOCTL_FAILED;
        }
    }
    *dos_error = error;
    if (rc == OT_PROBE_OK) *status = result;
    return rc;
}

int ot_tape_probe(const struct ot_probe_ops *ops, const char *name,
                  struct ot_tape_status *status, unsigned *dos_error)
{
    unsigned handle, flags, error, close_error;
    struct ot_tape_status result;
    int rc;

    if (dos_error != 0) *dos_error = 0u;
    if (ops == 0 || status == 0 || dos_error == 0 ||
        ops->open_readonly == 0 || ops->device_info == 0 ||
        ops->status == 0 || ops->close == 0 || !ot_probe_name_valid(name))
        return OT_PROBE_BAD_ARGUMENT;
    error = ops->open_readonly(ops->context, name, &handle);
    if (error != 0u) {
        *dos_error = error;
        return OT_PROBE_OPEN_FAILED;
    }
    error = ops->device_info(ops->context, handle, &flags);
    if (error != 0u) rc = OT_PROBE_IOCTL_FAILED;
    else if ((flags & 0x80u) == 0u) rc = OT_PROBE_NOT_DEVICE;
    else rc = ot_tape_query(ops, handle, &result, &error);
    close_error = ops->close(ops->context, handle);
    if (rc == OT_PROBE_OK && close_error != 0u) {
        rc = OT_PROBE_CLOSE_FAILED;
        error = close_error;
    }
    *dos_error = error;
    if (rc == OT_PROBE_OK) *status = result;
    return rc;
}

static int rewind_mode_valid(const struct ot_tape_status *status)
{
    unsigned state = status->tape.current_state;
    return (state & 3u) == OA_MT_STATE_UNBUFFERED &&
        (state & (OA_MT_STATE_REWIND_CLOSE | OA_MT_STATE_WRITTEN |
                  OA_MT16_STATE_WRITE_ERROR)) == 0u;
}

int ot_tape_enumerate(const struct ot_probe_ops *ops, char letter, unsigned dollar,
                       ot_device_visitor visitor, void *context, struct ot_device_summary *summary)
{
    struct ot_device_entry entry;
    unsigned i;
    if (summary != 0) memset(summary, 0, sizeof(*summary));
    letter = upper_ascii(letter);
    if (ops == 0 || summary == 0 || visitor == 0 ||
        ops->open_readonly == 0 || ops->device_info == 0 || ops->status == 0 || ops->close == 0 ||
        letter < 'A' || letter > 'Z' || dollar > 1u) return OT_PROBE_BAD_ARGUMENT;
    for (i = 0u; i < 4u; ++i) {
        memset(&entry, 0, sizeof(entry));
        strcpy(entry.name, "NRMT0"); entry.name[2] = letter; entry.name[4] = (char)('0' + i);
        if (dollar) { entry.name[5] = '$'; entry.name[6] = 0; }
        entry.result = ot_tape_probe(ops, entry.name, &entry.status, &entry.dos_error);
        ++summary->examined;
        if (entry.result == OT_PROBE_OK) { entry.kind = OT_DEVICE_FOUND; ++summary->found; }
        else if (entry.result == OT_PROBE_OPEN_FAILED && entry.dos_error == 2u) {
            entry.kind = OT_DEVICE_MISSING; ++summary->missing;
        } else if (entry.result == OT_PROBE_NOT_DEVICE) {
            entry.kind = OT_DEVICE_REJECTED; ++summary->rejected;
        } else { entry.kind = OT_DEVICE_ERROR; ++summary->errors; }
        if (visitor(context, &entry) != 0) return OT_ENUM_VISITOR;
    }
    if (summary->errors != 0u || summary->rejected != 0u) return OT_ENUM_PARTIAL;
    return summary->found != 0u ? OT_PROBE_OK : OT_ENUM_EMPTY;
}

int ot_tape_rewind(const struct ot_rewind_ops *ops, const char *name,
                   struct ot_tape_status *status, unsigned *dos_error)
{
    struct oa_aspitape_mtop request;
    unsigned handle, flags, error, close_error;
    int rc;
    if (dos_error != 0) *dos_error = 0u;
    if (ops == 0 || status == 0 || dos_error == 0 ||
        ops->probe.open_readonly == 0 || ops->probe.device_info == 0 ||
        ops->probe.status == 0 || ops->probe.close == 0 || ops->rewind == 0 ||
        !ot_probe_raw_name_valid(name))
        return OT_PROBE_BAD_ARGUMENT;
    error = ops->probe.open_readonly(ops->probe.context, name, &handle);
    if (error != 0u) { *dos_error = error; return OT_PROBE_OPEN_FAILED; }
    error = ops->probe.device_info(ops->probe.context, handle, &flags);
    rc = OT_PROBE_IOCTL_FAILED;
    if (error == 0u) {
        if ((flags & 0x80u) == 0u) rc = OT_PROBE_NOT_DEVICE;
        else rc = ot_tape_query(&ops->probe, handle, status, &error);
    }
    if (rc == 0 && !rewind_mode_valid(status)) rc = OT_REWIND_BAD_STATE;
    if (rc == 0) {
        request.operation = OA_MTREW; request.count = 1ul;
        error = ops->rewind(ops->probe.context, handle, &request);
        if (error != 0u) rc = OT_REWIND_IO;
        else {
            rc = ot_tape_query(&ops->probe, handle, status, &error);
            if (rc == 0 && (!rewind_mode_valid(status) ||
                (status->tape.current_state & OA_MT16_STATE_REWINDING) != 0u ||
                (status->extended && status->tape.driver_error != 0u) ||
                status->tape.last_scsi_status != 0u ||
                (status->tape.last_scsi_opcode & 0xffu) != 0x01u))
                rc = OT_REWIND_BAD_STATE;
        }
    }
    close_error = ops->probe.close(ops->probe.context, handle);
    if (rc == 0 && close_error != 0u) { rc = OT_PROBE_CLOSE_FAILED; error = close_error; }
    *dos_error = error;
    return rc;
}

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

static int guard_ok(const unsigned char *buffer, unsigned start)
{
    unsigned i;
    for (i = start; i < 20u; ++i)
        if (buffer[i] != 0xa5u) return 0;
    return 1;
}

int ot_tape_probe(const struct ot_probe_ops *ops, const char *name,
                  struct ot_tape_status *status, unsigned *dos_error)
{
    unsigned handle, flags, error, close_error;
    unsigned char buffer[20];
    struct oa_aspitape_mtget legacy;
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
    memset(&result, 0, sizeof(result));
    rc = OT_PROBE_OK;
    error = ops->device_info(ops->context, handle, &flags);
    if (error != 0u) {
        rc = OT_PROBE_IOCTL_FAILED;
    } else if ((flags & 0x80u) == 0u) {
        rc = OT_PROBE_NOT_DEVICE;
    } else {
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
    close_error = ops->close(ops->context, handle);
    if (rc == OT_PROBE_OK && close_error != 0u) {
        rc = OT_PROBE_CLOSE_FAILED;
        error = close_error;
    }
    *dos_error = error;
    if (rc == OT_PROBE_OK) *status = result;
    return rc;
}

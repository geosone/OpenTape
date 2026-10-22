#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/tape_probe.h"

struct fixture {
    unsigned opens, infos, statuses, closes;
    unsigned open_error, info_error, extended_error, legacy_error, close_error;
    unsigned flags, overwrite, legacy_wide;
};

static unsigned fake_open(void *p, const char *name, unsigned *handle)
{
    struct fixture *f = p;
    assert(strcmp(name, "NRMT0") == 0);
    ++f->opens;
    *handle = 9u;
    return f->open_error;
}
static unsigned fake_info(void *p, unsigned handle, unsigned *flags)
{
    struct fixture *f = p;
    assert(handle == 9u);
    ++f->infos;
    *flags = f->flags;
    return f->info_error;
}
static unsigned fake_status(void *p, unsigned handle, unsigned fn,
                             void *buffer, unsigned size)
{
    struct fixture *f = p;
    struct oa_aspitape_mtget16 extended;
    struct oa_aspitape_mtget legacy;
    unsigned char *bytes = buffer;
    assert(handle == 9u);
    ++f->statuses;
    memset(&extended, 0, sizeof(extended));
    extended.host_id = 2u;
    extended.target_id = 4u;
    extended.current_state = 0x01c2u;
    extended.driver_error = 7u;
    extended.fixed_block_size = 512u;
    extended.last_scsi_status = 2u;
    extended.last_sense_key = 7u;
    extended.last_scsi_opcode = 0x100au;
    extended.residual_bytes = 123u;
    if (fn == OA_ASPITAPE_IOCTL_MTGET16) {
        assert(size == 16u);
        if (f->extended_error != 0u) return f->extended_error;
        memcpy(buffer, &extended, sizeof(extended));
    } else {
        assert(fn == OA_ASPITAPE_IOCTL_MTGET && size == 14u);
        if (f->legacy_error != 0u) return f->legacy_error;
        memset(&legacy, 0, sizeof(legacy));
        legacy.host_id = extended.host_id;
        legacy.target_id = extended.target_id;
        legacy.current_state = 0xe2u;
        legacy.fixed_block_size = extended.fixed_block_size;
        legacy.last_scsi_status = extended.last_scsi_status;
        legacy.last_sense_key = extended.last_sense_key;
        legacy.last_scsi_opcode = extended.last_scsi_opcode;
        legacy.residual_bytes = extended.residual_bytes;
        memcpy(buffer, &legacy, sizeof(legacy));
        if (f->legacy_wide) bytes[14] = 0;
    }
    if (f->overwrite) bytes[size] = 0;
    return 0u;
}
static unsigned fake_close(void *p, unsigned handle)
{
    struct fixture *f = p;
    assert(handle == 9u);
    ++f->closes;
    return f->close_error;
}

int main(void)
{
    struct fixture f;
    struct ot_probe_ops ops;
    struct ot_tape_status status;
    unsigned error;

    assert(ot_probe_name_valid("NRMT0"));
    assert(ot_probe_name_valid("nrst3$"));
    assert(ot_probe_name_valid("NMT1"));
    assert(!ot_probe_name_valid("RMT0"));
    assert(!ot_probe_name_valid("MT0"));
    assert(!ot_probe_name_valid("NUL"));
    assert(!ot_probe_name_valid("NRMT4"));
    assert(!ot_probe_name_valid("NRMT0."));
    assert(!ot_probe_name_valid("C:NRMT0"));
    assert(!ot_probe_name_valid("./NRMT0"));
    assert(!ot_probe_name_valid("NRMT0$$"));
    assert(!ot_probe_name_valid(0));
    memset(&f, 0, sizeof(f));
    f.flags = 0x80u;
    ops.context = &f;
    ops.open_readonly = fake_open;
    ops.device_info = fake_info;
    ops.status = fake_status;
    ops.close = fake_close;
    assert(ot_tape_probe(&ops, "RMT0", &status, &error) == OT_PROBE_BAD_ARGUMENT);
    assert(f.opens == 0u);
    assert(ot_tape_probe(&ops, "NRMT0", &status, &error) == OT_PROBE_OK);
    assert(status.extended && status.tape.driver_error == 7u);
    assert(status.tape.current_state == 0x01c2u && error == 0u);
    assert(f.closes == 1u && f.statuses == 1u);
    f.extended_error = 1u;
    assert(ot_tape_probe(&ops, "NRMT0", &status, &error) == OT_PROBE_OK);
    assert(!status.extended && status.tape.driver_error == 0u);
    assert(status.tape.current_state == 0x01c2u);
    assert(status.tape.fixed_block_size == 512u);
    assert(status.tape.last_scsi_opcode == 0x100au);
    assert(status.tape.residual_bytes == 123u);
    f.extended_error = 83u;
    assert(ot_tape_probe(&ops, "NRMT0", &status, &error) == OT_PROBE_OK);
    f.extended_error = 12u;
    assert(ot_tape_probe(&ops, "NRMT0", &status, &error) == OT_PROBE_OK);
    f.legacy_wide = 1u;
    assert(ot_tape_probe(&ops, "NRMT0", &status, &error) == OT_PROBE_UNSUPPORTED_ABI);
    f.legacy_wide = 0u;
    f.legacy_error = 5u;
    f.close_error = 6u;
    assert(ot_tape_probe(&ops, "NRMT0", &status, &error) == OT_PROBE_IOCTL_FAILED);
    assert(error == 5u);
    f.legacy_error = 0u;
    assert(ot_tape_probe(&ops, "NRMT0", &status, &error) == OT_PROBE_CLOSE_FAILED);
    assert(error == 6u);
    f.close_error = 0u;
    f.extended_error = 5u;
    f.statuses = 0u;
    assert(ot_tape_probe(&ops, "NRMT0", &status, &error) == OT_PROBE_IOCTL_FAILED);
    assert(f.statuses == 1u && error == 5u);
    f.extended_error = 0u;
    f.overwrite = 1u;
    assert(ot_tape_probe(&ops, "NRMT0", &status, &error) == OT_PROBE_UNSUPPORTED_ABI);
    f.overwrite = 0u;
    f.flags = 0u;
    f.statuses = 0u;
    assert(ot_tape_probe(&ops, "NRMT0", &status, &error) == OT_PROBE_NOT_DEVICE);
    assert(f.statuses == 0u);
    f.info_error = 6u;
    assert(ot_tape_probe(&ops, "NRMT0", &status, &error) == OT_PROBE_IOCTL_FAILED);
    assert(error == 6u);
    f.open_error = 2u;
    f.closes = 0u;
    status.extended = 123u;
    assert(ot_tape_probe(&ops, "NRMT0", &status, &error) == OT_PROBE_OPEN_FAILED);
    assert(error == 2u && f.closes == 0u);
    assert(status.extended == 123u);
    assert(ot_tape_probe(0, "NRMT0", &status, &error) == OT_PROBE_BAD_ARGUMENT);
    assert(ot_tape_probe(&ops, "NRMT0", 0, &error) == OT_PROBE_BAD_ARGUMENT);
    assert(ot_tape_probe(&ops, "NRMT0", &status, 0) == OT_PROBE_BAD_ARGUMENT);
    ops.status = 0;
    assert(ot_tape_probe(&ops, "NRMT0", &status, &error) == OT_PROBE_BAD_ARGUMENT);
    puts("OpenTape read-only tape probe tests passed");
    return 0;
}

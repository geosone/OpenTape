#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/tape_probe.h"

struct fixture {
    unsigned opens, infos, statuses, closes;
    unsigned open_error, info_error, extended_error, legacy_error, close_error;
    unsigned flags, overwrite, legacy_wide;
    unsigned rewind_test, rewinds, control_error, before, after;
    unsigned post_error, post_wide, post_driver, post_scsi, wrong_opcode;
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
    if (f->rewind_test) {
        extended.current_state = f->rewinds ? f->after : f->before;
        extended.driver_error = f->rewinds ? f->post_driver : 0u;
        extended.last_scsi_status = f->rewinds ? f->post_scsi : 0u;
        extended.last_scsi_opcode = f->rewinds && !f->wrong_opcode ? 0x01u : 0x08u;
        if (f->rewinds && f->post_error) return f->post_error;
    }
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
        if (f->rewind_test) legacy.current_state = (oa_u8)((extended.current_state & 0x1fu) |
                                                    ((extended.current_state >> 1) & 0xe0u));
        legacy.fixed_block_size = extended.fixed_block_size;
        legacy.last_scsi_status = extended.last_scsi_status;
        legacy.last_sense_key = extended.last_sense_key;
        legacy.last_scsi_opcode = extended.last_scsi_opcode;
        legacy.residual_bytes = extended.residual_bytes;
        memcpy(buffer, &legacy, sizeof(legacy));
        if (f->legacy_wide) bytes[14] = 0;
    }
    if (f->overwrite) bytes[size] = 0;
    if (f->rewinds && f->post_wide) bytes[size] = 0;
    return 0u;
}
static unsigned fake_close(void *p, unsigned handle)
{
    struct fixture *f = p;
    assert(handle == 9u);
    ++f->closes;
    return f->close_error;
}

static unsigned fake_rewind(void *p, unsigned handle, const struct oa_aspitape_mtop *request)
{
    struct fixture *f = p;
    assert(handle == 9u && f->closes == 0u && f->rewinds == 0u);
    assert(f->infos == 1u && f->statuses >= 1u);
    assert(sizeof(*request) == 6u && request->operation == OA_MTREW && request->count == 1ul);
    ++f->rewinds;
    return f->control_error;
}

struct enumeration_fixture {
    struct fixture unit[4];
    unsigned opens, visits, current, stop_after, seen[4], dollar;
    char letter;
};
static unsigned enumeration_open(void *p, const char *name, unsigned *handle)
{
    struct enumeration_fixture *f = p;
    char expected[7] = "NRST0";
    assert(f->opens < 4u);
    f->current = f->opens++;
    expected[2] = f->letter; expected[4] = (char)('0' + f->current);
    if (f->dollar) { expected[5] = '$'; expected[6] = 0; }
    assert(strcmp(name, expected) == 0 && ot_probe_raw_name_valid(name));
    *handle = 9u; ++f->unit[f->current].opens;
    return f->unit[f->current].open_error;
}
static unsigned enumeration_info(void *p, unsigned handle, unsigned *flags)
{ struct enumeration_fixture *f = p; return fake_info(&f->unit[f->current], handle, flags); }
static unsigned enumeration_status(void *p, unsigned handle, unsigned fn, void *buffer, unsigned size)
{ struct enumeration_fixture *f = p; return fake_status(&f->unit[f->current], handle, fn, buffer, size); }
static unsigned enumeration_close(void *p, unsigned handle)
{ struct enumeration_fixture *f = p; return fake_close(&f->unit[f->current], handle); }
static int enumeration_visit(void *p, const struct ot_device_entry *entry)
{
    struct enumeration_fixture *f = p;
    struct fixture *unit = &f->unit[f->current];
    assert(unit->closes == (unit->open_error ? 0u : 1u));
    assert(entry->kind <= OT_DEVICE_ERROR && entry->name[4] == (char)('0' + f->current));
    if (entry->kind == OT_DEVICE_FOUND) assert(entry->result == 0 && entry->status.tape.target_id == 4u);
    else assert(entry->status.tape.target_id == 0u && entry->result != 0);
    ++f->seen[entry->kind]; ++f->visits;
    return f->visits == f->stop_after ? -1 : 0;
}
static void test_enumeration(void)
{
    struct enumeration_fixture f;
    struct ot_probe_ops ops;
    struct ot_device_summary summary;
    unsigned test, i;
    int rc, expected;
    ops.context = &f; ops.open_readonly = enumeration_open; ops.device_info = enumeration_info;
    ops.status = enumeration_status; ops.close = enumeration_close;
    for (test = 0u; test < 14u; ++test) {
        memset(&f, 0, sizeof(f)); f.letter = test == 9u ? 'R' : 'S'; f.dollar = test % 2u;
        for (i = 0u; i < 4u; ++i) f.unit[i].flags = 0x80u;
        expected = 0;
        switch (test) {
        case 1: for (i = 0u; i < 4u; ++i) f.unit[i].open_error = 2u; expected = OT_ENUM_EMPTY; break;
        case 2: f.unit[1].open_error = 2u; break;
        case 3: f.unit[1].flags = 0u; expected = OT_ENUM_PARTIAL; break;
        case 4: f.unit[1].open_error = 5u; expected = OT_ENUM_PARTIAL; break;
        case 5: f.unit[1].info_error = 6u; expected = OT_ENUM_PARTIAL; break;
        case 6: f.unit[1].extended_error = 5u; expected = OT_ENUM_PARTIAL; break;
        case 7: f.unit[1].close_error = 6u; expected = OT_ENUM_PARTIAL; break;
        case 8: f.unit[1].overwrite = 1u; expected = OT_ENUM_PARTIAL; break;
        case 9: f.unit[1].extended_error = 1u; break;
        case 10: f.unit[1].open_error = 3u; expected = OT_ENUM_PARTIAL; break;
        case 11: f.stop_after = 2u; expected = OT_ENUM_VISITOR; break;
        case 12: f.stop_after = 2u; f.unit[1].flags = 0u; expected = OT_ENUM_VISITOR; break;
        case 13:
            f.unit[1].open_error = f.unit[2].open_error = 2u;
            f.unit[3].flags = 0u; expected = OT_ENUM_PARTIAL; break;
        }
        rc = ot_tape_enumerate(&ops, (char)(f.letter - 'A' + 'a'), f.dollar, enumeration_visit, &f, &summary);
        assert(rc == expected && summary.examined == f.visits && f.opens == f.visits);
        assert(f.visits == (f.stop_after ? f.stop_after : 4u));
        assert(summary.found == f.seen[OT_DEVICE_FOUND] && summary.missing == f.seen[OT_DEVICE_MISSING]);
        assert(summary.rejected == f.seen[OT_DEVICE_REJECTED] && summary.errors == f.seen[OT_DEVICE_ERROR]);
        if (test == 0u) assert(summary.found == 4u); /* Aliases are not deduplicated by SCSI address. */
        if (test == 13u) assert(summary.found == 1u && summary.missing == 2u && summary.rejected == 1u);
        for (i = 0u; i < 4u; ++i) assert(f.unit[i].rewinds == 0u);
    }
    memset(&f, 0, sizeof(f));
    assert(ot_tape_enumerate(&ops, '/', 0u, enumeration_visit, &f, &summary) == OT_PROBE_BAD_ARGUMENT);
    assert(ot_tape_enumerate(&ops, 'M', 2u, enumeration_visit, &f, &summary) == OT_PROBE_BAD_ARGUMENT);
    assert(ot_tape_enumerate(0, 'M', 0u, enumeration_visit, &f, &summary) == OT_PROBE_BAD_ARGUMENT);
    assert(ot_tape_enumerate(&ops, 'M', 0u, 0, &f, &summary) == OT_PROBE_BAD_ARGUMENT);
    assert(ot_tape_enumerate(&ops, 'M', 0u, enumeration_visit, &f, 0) == OT_PROBE_BAD_ARGUMENT);
    ops.status = 0;
    assert(ot_tape_enumerate(&ops, 'M', 0u, enumeration_visit, &f, &summary) == OT_PROBE_BAD_ARGUMENT);
    assert(f.opens == 0u && f.visits == 0u && summary.examined == 0u);
    puts("OpenTape device enumeration tests passed");
}

static void test_rewind(void)
{
    struct fixture f;
    struct ot_rewind_ops ops;
    struct ot_tape_status status;
    unsigned test, error;
    int rc, expected;
    ops.probe.context = &f; ops.probe.open_readonly = fake_open;
    ops.probe.device_info = fake_info; ops.probe.status = fake_status;
    ops.probe.close = fake_close; ops.rewind = fake_rewind;
    for (test = 0u; test < 22u; ++test) {
        memset(&f, 0, sizeof(f)); memset(&status, 0, sizeof(status));
        f.flags = 0x80u; f.rewind_test = 1u; f.before = f.after = 2u;
        expected = OT_PROBE_OK;
        switch (test) {
        case 1: f.open_error = 2u; expected = OT_PROBE_OPEN_FAILED; break;
        case 2: f.info_error = 6u; expected = OT_PROBE_IOCTL_FAILED; break;
        case 3: f.flags = 0u; expected = OT_PROBE_NOT_DEVICE; break;
        case 4: f.extended_error = 5u; expected = OT_PROBE_IOCTL_FAILED; break;
        case 5: f.before = 1u; expected = OT_REWIND_BAD_STATE; break;
        case 6: f.before |= OA_MT_STATE_WRITTEN; expected = OT_REWIND_BAD_STATE; break;
        case 7: f.before |= OA_MT_STATE_REWIND_CLOSE; expected = OT_REWIND_BAD_STATE; break;
        case 8: f.before |= OA_MT16_STATE_WRITE_ERROR; expected = OT_REWIND_BAD_STATE; break;
        case 9: f.control_error = 5u; f.close_error = 6u; expected = OT_REWIND_IO; break;
        case 10: f.post_error = 5u; f.close_error = 6u; expected = OT_PROBE_IOCTL_FAILED; break;
        case 11: f.post_driver = 21u; expected = OT_REWIND_BAD_STATE; break;
        case 12: f.post_scsi = 2u; expected = OT_REWIND_BAD_STATE; break;
        case 13: f.wrong_opcode = 1u; expected = OT_REWIND_BAD_STATE; break;
        case 14: f.after |= OA_MT16_STATE_REWINDING; expected = OT_REWIND_BAD_STATE; break;
        case 15: f.close_error = 6u; expected = OT_PROBE_CLOSE_FAILED; break;
        case 16: f.extended_error = 1u; break;
        case 17: f.overwrite = 1u; expected = OT_PROBE_UNSUPPORTED_ABI; break;
        case 18: f.post_wide = 1u; expected = OT_PROBE_UNSUPPORTED_ABI; break;
        case 19: f.before |= OA_MT16_STATE_REWINDING; break;
        case 20: f.after = 1u; expected = OT_REWIND_BAD_STATE; break;
        case 21: f.extended_error = 12u; f.legacy_error = 5u; expected = OT_PROBE_IOCTL_FAILED; break;
        }
        rc = ot_tape_rewind(&ops, "NRMT0", &status, &error);
        assert(rc == expected && f.opens == 1u);
        assert(f.closes == (test == 1u ? 0u : 1u));
        assert(f.rewinds == ((test >= 1u && test <= 8u) || test == 17u || test == 21u ? 0u : 1u));
        if (test == 9u || test == 10u) assert(error == 5u);
        if (test == 15u) assert(error == 6u);
        if (test == 0u || test == 16u || test == 19u) assert(error == 0u);
    }
    memset(&f, 0, sizeof(f));
    assert(ot_tape_rewind(&ops, "NMT0", &status, &error) == OT_PROBE_BAD_ARGUMENT);
    assert(ot_tape_rewind(&ops, "NRT0", &status, &error) == OT_PROBE_BAD_ARGUMENT);
    assert(ot_tape_rewind(&ops, "NRT0$", &status, &error) == OT_PROBE_BAD_ARGUMENT);
    assert(ot_tape_rewind(&ops, "RMT0", &status, &error) == OT_PROBE_BAD_ARGUMENT);
    assert(ot_tape_rewind(&ops, "C:NRMT0", &status, &error) == OT_PROBE_BAD_ARGUMENT);
    assert(ot_tape_rewind(&ops, 0, &status, &error) == OT_PROBE_BAD_ARGUMENT);
    assert(ot_tape_rewind(0, "NRMT0", &status, &error) == OT_PROBE_BAD_ARGUMENT);
    assert(ot_tape_rewind(&ops, "NRMT0", 0, &error) == OT_PROBE_BAD_ARGUMENT);
    assert(ot_tape_rewind(&ops, "NRMT0", &status, 0) == OT_PROBE_BAD_ARGUMENT);
    ops.rewind = 0;
    assert(ot_tape_rewind(&ops, "NRMT0", &status, &error) == OT_PROBE_BAD_ARGUMENT);
    assert(f.opens == 0u && f.rewinds == 0u && f.closes == 0u);
    puts("OpenTape explicit rewind tests passed");
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
    assert(ot_probe_raw_name_valid("NRMT0"));
    assert(ot_probe_raw_name_valid("nrst3$"));
    assert(ot_probe_raw_name_valid("NRRT0"));
    assert(!ot_probe_raw_name_valid("NRT0"));
    assert(!ot_probe_raw_name_valid("NRT0$"));
    assert(!ot_probe_raw_name_valid("NMT0"));
    assert(!ot_probe_raw_name_valid("NRMT4"));
    assert(!ot_probe_raw_name_valid(0));
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
    test_rewind();
    test_enumeration();
    puts("OpenTape read-only tape probe tests passed");
    return 0;
}

/* Only run against the memory-only native ASPI fixture, never a real tape. */
#include <assert.h>
#include <dos.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/native_tape.h"

static struct ot_dos_native_writer writer;
static void recover(void)
{
    struct ot_tape_info_v1 info;
    struct ot_tape_control_v1 control;
    unsigned error;
    assert(ot_dos_native_probe("NRMT0", 0, &info, &error) == 0);
    assert(info.session_state == 0x60u);
    assert(ot_dos_native_writer_open(&writer, "NRMT0") == OT_NATIVE_BUSY);
    assert(!writer.writer.opened);
    assert(ot_dos_native_execute("NRMT0", 1, &control, &error) == 0);
    assert(control.session_state == 0 && control.completion_flags == 0x0b);
}
int main(int argc, char **argv)
{
    struct ot_tape_record_v1 saved;
    struct ot_tape_control_v1 saved_control;
    unsigned actual = 8u, completion = 0x25u, diag = 2u, stream = 0u;
    int kind = argc > 1 ? atoi(argv[1]) : 0;
    int fault = argc > 2 ? atoi(argv[2]) : 0;
    int legacy = argc > 3 ? atoi(argv[3]) : 0;
    int rc;
    assert(kind >= 0 && kind <= 10 && fault >= 0 && fault <= 3);
    memset(&writer, 0, sizeof(writer));
    assert(ot_dos_native_writer_open(&writer, "NRMT3") == OT_NATIVE_DEVICE);
    assert(!writer.writer.opened);
    assert(ot_dos_native_writer_open(&writer, "C:\\NRMT0") == OT_NATIVE_ARGUMENT);
    rc = ot_dos_native_writer_open(&writer, "NRMT0");
    if (kind == 10) {
        assert(rc == OT_NATIVE_UNSUPPORTED && !writer.writer.opened);
        puts("OpenTape installed native writer tests passed");
        return 0;
    }
    assert(rc == 0 && writer.writer.claimed);
    assert(ot_dos_native_writer_write(&writer, "RECORD01", 0) == OT_NATIVE_ARGUMENT);
    rc = ot_dos_native_writer_write(&writer, "RECORD01", 8);
    if (kind != 0) {
        assert(rc == OT_NATIVE_COMMAND);
        if (kind == 1) { actual = 4; diag = 5; stream = 8; }
        else if (kind == 2 || kind == 7) { actual = 0; completion = 5; diag = 1; }
        else if (kind == 4 || kind == 8) { actual = 4; stream = 8; }
        else if (kind == 5) { actual = 0; completion = 5; diag = 3; }
        else if (kind == 6) { actual = 0; completion = 5; }
        if (kind == 4 || kind == 9) stream |= 2;
        if (legacy) { actual = 0; completion = 5; }
        saved = writer.writer.last_record;
        assert(saved.actual == actual && saved.completion_flags == completion);
        assert(saved.diagnostic_class == diag && saved.stream_flags == stream);
        assert(saved.session_state == 0x70u);
        assert(ot_dos_native_writer_write(&writer, "RECORD01", 8) == OT_NATIVE_COMMAND);
        assert(ot_dos_native_writer_flush(&writer) == OT_NATIVE_COMMAND);
        assert(ot_dos_native_writer_filemark(&writer) == OT_NATIVE_COMMAND);
        assert(ot_dos_native_writer_finish(&writer) == OT_NATIVE_COMMAND);
        assert(ot_dos_native_writer_close(&writer) == OT_NATIVE_COMMAND);
        assert(memcmp(&saved, &writer.writer.last_record, sizeof(saved)) == 0);
        recover();
    } else {
        assert(rc == 0 && writer.writer.last_record.actual == 8);
        rc = ot_dos_native_writer_flush(&writer);
        if (fault != 1) { assert(rc == 0); rc = ot_dos_native_writer_filemark(&writer); }
        if (fault != 1 && fault != 2) { assert(rc == 0); rc = ot_dos_native_writer_finish(&writer); }
        if (fault != 0) {
            assert(rc == OT_NATIVE_COMMAND);
            saved_control = writer.writer.last_control;
            assert(saved_control.result == 3 && saved_control.completion_flags == 5);
            assert(ot_dos_native_writer_finish(&writer) == OT_NATIVE_COMMAND);
            assert(ot_dos_native_writer_close(&writer) == OT_NATIVE_COMMAND);
            assert(memcmp(&saved_control, &writer.writer.last_control, sizeof(saved_control)) == 0);
            recover();
        } else {
            assert(rc == 0 && !writer.writer.claimed);
            assert(writer.writer.last_control.completion_flags == 0x83);
            assert(ot_dos_native_writer_close(&writer) == 0);
            /* The common close path must report an unfinished stream. */
            assert(ot_dos_native_writer_open(&writer, "NRMT0") == 0);
            assert(ot_dos_native_writer_write(&writer, "RECORD01", 8) == 0);
            assert(ot_dos_native_writer_close(&writer) == OT_NATIVE_UNFINISHED);
            recover();
        }
    }
    /* Simulate a caller bypassing the API / exiting without END or ABORT.
     * DOS may ignore driver CLOSE errors; retained native state must survive.
     */
    assert(ot_dos_native_writer_open(&writer, "NRMT0") == 0);
    assert(ot_dos_native_writer_write(&writer, "RECORD01", 8) ==
        (kind == 0 ? 0 : OT_NATIVE_COMMAND));
    (void)_dos_close(writer.session.handle);
    writer.writer.opened = writer.writer.claimed = 0;
    recover();
    puts("OpenTape installed native writer tests passed");
    return 0;
}

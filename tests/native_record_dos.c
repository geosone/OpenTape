/* Installed DOS provider/consumer test, only with the memory-only ASPI mock. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/native_tape.h"

int main(int argc, char **argv)
{
    struct ot_dos_native_reader r;
    unsigned char data[10];
    unsigned actual = 8u, stream = 0u, completion = 0x23u, diag = 0u, i;
    int kind = argc > 1 ? atoi(argv[1]) : 0;
    int result = OT_NATIVE_OK;
    assert(kind >= 0 && kind <= 13);
    if (kind == 1 || kind == 2 || kind == 3 || kind == 4 || kind == 11) {
        result = OT_NATIVE_STREAM; diag = 2;
    } else if (kind >= 5 && kind <= 10) {
        result = OT_NATIVE_COMMAND; diag = 2; completion = 0x25;
    }
    if (kind == 1 || kind == 3 || kind == 10) { actual = 4; stream = 8; }
    else if (kind == 2 || kind == 4 || kind == 11 || kind == 12) { actual = 0; stream = 8; }
    else if (kind >= 5 && kind <= 8) { actual = 0; completion = 5; }
    if (kind == 1) stream = 12;
    else if (kind == 2 || kind == 3) stream = 9;
    else if (kind == 4) stream = 10;
    else if (kind == 5) diag = 1;
    else if (kind == 6 || kind == 8) diag = 3;
    else if (kind == 9) stream = 0x14;
    else if (kind == 11) stream = 0x28;
    if (kind == 13) { result = OT_NATIVE_UNSUPPORTED; actual = 0; stream = 0; completion = 0; diag = 4; }
    memset(&r, 0, sizeof(r));
    assert(ot_dos_native_reader_open(&r, "NRMT0") == 0);
    assert(ot_dos_native_reader_read(&r, data + 1, 0u) == OT_NATIVE_ARGUMENT);
    for (i = 0; i < 2u; ++i) {
        memset(data, 0xa5, sizeof(data));
        assert(ot_dos_native_reader_read(&r, data + 1, 8u) == result);
        assert(ot_tape_record_validate(&r.reader.last, sizeof(r.reader.last)) == 0);
        assert(r.reader.last.actual == actual && r.reader.last.stream_flags == stream);
        assert(r.reader.last.completion_flags == completion && r.reader.last.diagnostic_class == diag);
        assert(data[0] == 0xa5 && data[9] == 0xa5);
        if (actual != 0u) assert(memcmp(data + 1, "RECORD01", actual) == 0);
        if (result < 0) {
            assert(ot_dos_native_reader_read(&r, data + 1, 8u) == OT_NATIVE_COMMAND);
            break;
        }
    }
    assert(ot_dos_native_reader_close(&r) == 0);
    assert(ot_dos_native_reader_close(&r) == OT_NATIVE_ARGUMENT);
    puts("OpenTape installed native record tests passed");
    return 0;
}

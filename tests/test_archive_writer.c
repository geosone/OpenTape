#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/archive_writer.h"

struct memory_sink {
    unsigned calls, finishes, short_call, fail_call, bad_call;
    int finish_error;
    unsigned sizes[32], boundaries[4];
    oa_u8 blocks[32][512];
    struct ot_archive_writer *reenter;
    int reenter_result;
};
static struct memory_sink memory;
static struct ot_archive_writer writer;
static struct ot_writer_sink sink;
static oa_u8 buffer[528];

static int write_block(void *p, const oa_u8 *data, unsigned size, unsigned *accepted)
{
    struct memory_sink *m = p;
    unsigned i = m->calls++;
    assert(i < 32u && size <= 512u);
    if (m->reenter != 0) m->reenter_result = ot_archive_writer_session_end(m->reenter);
    memcpy(m->blocks[i], data, size);
    m->sizes[i] = size;
    *accepted = size;
    if (m->short_call == m->calls) *accepted = size / 2u;
    if (m->bad_call == m->calls) *accepted = size + 1u;
    return m->fail_call == m->calls ? 83 : 0;
}
static int finish_session(void *p)
{
    struct memory_sink *m = p;
    assert(m->finishes < 4u);
    m->boundaries[m->finishes++] = m->calls;
    return m->finish_error;
}
static void setup(void)
{
    memset(&memory, 0, sizeof(memory));
    memset(&writer, 0, sizeof(writer));
    memset(buffer, 0xa5, sizeof(buffer));
    sink.context = &memory;
    sink.write_block = write_block;
    sink.finish_session = finish_session;
    assert(ot_archive_writer_init(&writer, &sink, buffer, 512u) == 0);
}
static void volume(void)
{
    struct ot_volume_label label;
    memset(&label, 0, sizeof(label));
    label.version = 1u;
    strcpy(label.volume_name, "WRITER-TEST");
    assert(ot_archive_writer_volume(&writer, &label) == 0);
}
static void session(oa_u32 id)
{
    struct ot_session_start start;
    memset(&start, 0, sizeof(start));
    start.created_utc = 123ul;
    strcpy(start.name, "BACKUP");
    assert(ot_archive_writer_session(&writer, id, &start) == 0);
}
static int metadata(oa_u32 id, oa_u32 size, unsigned directory)
{
    struct ot_file_metadata m;
    memset(&m, 0, sizeof(m));
    m.kind = directory ? OT_FILE_DIRECTORY : OT_FILE_REGULAR;
    m.dos_attributes = directory ? 0x10u : 0x20u;
    m.file_size = size;
    strcpy(m.path, directory ? "DATA" : "DATA/FILE.TXT");
    return ot_archive_writer_metadata(&writer, id, &m);
}
static void assert_poisoned(unsigned calls, unsigned finishes)
{
    assert(writer.failed);
    assert(ot_archive_writer_complete(&writer) == OT_WRITER_FAILED);
    assert(ot_archive_writer_data(&writer, (const oa_u8 *)"x", 1u) == OT_WRITER_FAILED);
    assert(ot_archive_writer_file_end(&writer) == OT_WRITER_FAILED);
    assert(ot_archive_writer_session_end(&writer) == OT_WRITER_FAILED);
    assert(memory.calls == calls && memory.finishes == finishes);
}

#ifndef DOS_VERSION
struct large_sink { struct ot_archive_verify readback; unsigned largest, calls, finishes; };
static int large_write(void *p, const oa_u8 *data, unsigned size, unsigned *accepted)
{
    struct large_sink *s = p;
    assert(ot_archive_verify_block(&s->readback, data, (oa_u16)size) == 0);
    if (size > s->largest) s->largest = size;
    ++s->calls;
    *accepted = size;
    return 0;
}
static int large_finish(void *p)
{
    struct large_sink *s = p;
    assert(ot_archive_verify_finish(&s->readback) == 0);
    ++s->finishes;
    return 0;
}
static void test_maximum_record(void)
{
    struct ot_archive_writer w;
    struct ot_writer_sink out;
    struct large_sink capture;
    struct ot_volume_label label;
    struct ot_session_start start;
    struct ot_file_metadata m;
    oa_u8 *storage, *data;
    unsigned i;
    memset(&w, 0, sizeof(w));
    memset(&capture, 0, sizeof(capture));
    memset(&label, 0, sizeof(label)); label.version = 1u;
    memset(&start, 0, sizeof(start)); start.created_utc = 123ul;
    memset(&m, 0, sizeof(m));
    m.kind = OT_FILE_REGULAR; m.file_size = 65479ul;
    strcpy(m.path, "LARGE.BIN");
    storage = malloc(65535u + 16u);
    data = malloc(65479u);
    assert(storage != 0 && data != 0);
    memset(storage, 0xa5, 65535u + 16u);
    for (i = 0u; i < 65479u; ++i) data[i] = (oa_u8)i;
    out.context = &capture; out.write_block = large_write; out.finish_session = large_finish;
    assert(ot_archive_writer_init(&w, &out, storage + 8, 65535u) == 0);
    assert(ot_archive_writer_volume(&w, &label) == 0);
    assert(ot_archive_writer_session(&w, 1ul, &start) == 0);
    assert(ot_archive_writer_metadata(&w, 1ul, &m) == 0);
    assert(ot_archive_writer_data(&w, data, 65479u) == 0);
    assert(ot_archive_writer_file_end(&w) == 0);
    assert(ot_archive_writer_session_end(&w) == 0);
    assert(ot_archive_writer_complete(&w) == 0);
    assert(capture.largest == 65535u && capture.calls == 6u && capture.finishes == 1u);
    assert(storage[0] == 0xa5u && storage[7] == 0xa5u && storage[65543u] == 0xa5u);
    free(storage); free(data);
}
#endif

int main(void)
{
    static const oa_u8 text[] = "123456789";
    struct ot_archive_verify readback, before;
    struct ot_record_header record;
    struct ot_file_end end;
    oa_u8 expected[456];
    unsigned i, calls;

    setup();
    assert(ot_archive_writer_complete(&writer) == OT_VERIFY_INCOMPLETE);
    {
        struct ot_session_start start;
        memset(&start, 0, sizeof(start));
        assert(ot_archive_writer_session(&writer, 1ul, &start) == OT_SCAN_MISSING_LABEL);
        assert(memory.calls == 0u);
    }
    assert(ot_archive_writer_init(&writer, &sink, buffer, 512u) == OT_WRITER_BAD_ARGUMENT);
    volume();
    session(1ul);
    assert(metadata(1ul, 0ul, 1u) == 0);
    assert(metadata(2ul, 9ul, 0u) == 0);
    calls = memory.calls;
    assert(metadata(3ul, 0ul, 0u) == OT_VERIFY_ORDER);
    assert(ot_archive_writer_file_end(&writer) == OT_VERIFY_INCOMPLETE);
    assert(ot_archive_writer_session_end(&writer) == OT_VERIFY_INCOMPLETE);
    assert(ot_archive_writer_data(&writer, text, 10u) == OT_WRITER_BAD_ARGUMENT);
    assert(memory.calls == calls);
    assert(ot_archive_writer_data(&writer, text, 4u) == 0);
    /* Source aliases scratch: the writer must move it before building headers. */
    memcpy(buffer, text + 4, 5u);
    assert(ot_archive_writer_data(&writer, buffer, 5u) == 0);
    assert(ot_archive_writer_file_end(&writer) == 0);
    assert(metadata(3ul, 0ul, 0u) == 0);
    assert(ot_archive_writer_file_end(&writer) == 0);
    assert(ot_archive_writer_session_end(&writer) == 0);
    assert(ot_archive_writer_complete(&writer) == 0);
    session(2ul);
    assert(ot_archive_writer_complete(&writer) == OT_VERIFY_INCOMPLETE);
    assert(ot_archive_writer_session_end(&writer) == 0);
    assert(memory.calls == 12u && memory.finishes == 2u);
    assert(buffer[512] == 0xa5u && buffer[527] == 0xa5u);
    memset(&readback, 0, sizeof(readback));
    for (i = 0u; i < memory.calls; ++i) {
        assert(ot_archive_verify_block(&readback, memory.blocks[i], (oa_u16)memory.sizes[i]) == 0);
        if (i + 1u == memory.boundaries[0] || i + 1u == memory.boundaries[1])
            assert(ot_archive_verify_finish(&readback) == 0);
    }
    assert(readback.sessions == 2ul && readback.framing.blocks == 12ul);
    ot_record_header_decode(&record, memory.blocks[6] + 36);
    assert(record.type == OT_RECORD_FILE_END);
    assert(ot_file_end_decode(&end, memory.blocks[6] + 56, (oa_u16)record.data_size) == 0);
    assert(end.file_size == 9ul && end.fragments == 2ul && end.crc32 == 0xcbf43926ul);
    /* Corruption after a successful sink call is found only by readback. */
    memory.blocks[0][56] ^= 1u;
    memset(&readback, 0, sizeof(readback));
    assert(ot_archive_verify_block(&readback, memory.blocks[0], (oa_u16)memory.sizes[0]) == OT_FORMAT_BAD_PAYLOAD_CRC);

    /* Full buffer, genuinely overlapping source/destination, all byte values. */
    setup(); volume(); session(1ul);
    assert(metadata(1ul, sizeof(expected), 0u) == 0);
    for (i = 0u; i < sizeof(expected); ++i) expected[i] = (oa_u8)i;
    memcpy(buffer, expected, sizeof(expected));
    assert(ot_archive_writer_data(&writer, buffer, sizeof(expected)) == 0);
    assert(memory.sizes[3] == 512u && memcmp(memory.blocks[3] + 56, expected, sizeof(expected)) == 0);
    assert(ot_archive_writer_file_end(&writer) == 0);
    assert(ot_archive_writer_session_end(&writer) == 0);
    memset(&readback, 0, sizeof(readback));
    for (i = 0u; i < memory.calls; ++i)
        assert(ot_archive_verify_block(&readback, memory.blocks[i], (oa_u16)memory.sizes[i]) == 0);
    assert(ot_archive_verify_finish(&readback) == 0);
    assert(buffer[512] == 0xa5u && buffer[527] == 0xa5u);

    setup();
    volume(); session(1ul);
    assert(metadata(1ul, 1ul, 0u) == 0);
    memcpy(&before, &writer.verified, sizeof(before));
    memory.short_call = memory.calls + 1u;
    assert(ot_archive_writer_data(&writer, text, 1u) == OT_WRITER_SHORT_WRITE);
    assert(memcmp(&before, &writer.verified, sizeof(before)) == 0);
    assert_poisoned(memory.calls, 0u);
    setup(); memory.fail_call = 1u;
    {
        struct ot_volume_label label;
        memset(&label, 0, sizeof(label)); label.version = 1u;
        assert(ot_archive_writer_volume(&writer, &label) == OT_WRITER_SINK_ERROR);
        assert(writer.sink_error == 83 && writer.verified.framing.blocks == 0ul);
        assert_poisoned(1u, 0u);
    }
    setup(); memory.bad_call = 1u;
    {
        struct ot_volume_label label;
        memset(&label, 0, sizeof(label)); label.version = 1u;
        assert(ot_archive_writer_volume(&writer, &label) == OT_WRITER_BAD_COUNT);
        assert_poisoned(1u, 0u);
    }
    setup(); volume(); session(1ul);
    memory.finish_error = 7;
    assert(ot_archive_writer_session_end(&writer) == OT_WRITER_SINK_ERROR);
    assert(writer.pending_finish && writer.sink_error == 7);
    assert_poisoned(memory.calls, 1u);
    setup(); memory.reenter = &writer;
    volume();
    assert(memory.reenter_result == OT_WRITER_BAD_ARGUMENT && !writer.failed);
    assert(ot_archive_writer_session(&writer, 1ul, 0) == OT_FORMAT_BAD_ARGUMENT);
    assert(ot_archive_writer_init(0, &sink, buffer, 512u) == OT_WRITER_BAD_ARGUMENT);
#ifndef DOS_VERSION
    test_maximum_record();
#endif
    puts("OpenTape archive writer tests passed");
    return 0;
}

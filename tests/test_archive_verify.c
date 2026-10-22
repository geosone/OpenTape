#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/archive_verify.h"
#include "opentape/record_payload.h"

struct fixture {
    struct ot_archive_verify state;
    oa_u32 sid, time;
};

static void emit(struct fixture *f, oa_u16 type, oa_u32 object, oa_u16 flags,
                    oa_u32 offset, const oa_u8 *data, oa_u16 size, int expected)
{
    struct ot_archive_verify before;
    struct ot_block_header b;
    struct ot_record_header r;
    oa_u8 payload[400], block[450];
    int rc;
    memset(&r, 0, sizeof(r));
    r.type = type;
    r.object_id = object;
    r.flags = flags;
    r.fragment_offset = offset;
    r.data_size = size;
    r.data_crc32 = ot_crc32(data, size);
    ot_record_header_encode(payload, &r);
    if (size != 0u) memcpy(payload + 20, data, size);
    memset(&b, 0, sizeof(b));
    b.block_number = f->state.framing.blocks;
    b.record_count = 1u;
    b.session_id = f->sid;
    b.session_time = f->time;
    assert(ot_block_encode(block, sizeof(block), &b, payload, (oa_u16)(size + 20u)) == 0);
    memcpy(&before, &f->state, sizeof(before));
    rc = ot_archive_verify_block(&f->state, block, (oa_u16)(size + 56u));
    assert(rc == expected);
    if (rc != 0) assert(memcmp(&before, &f->state, sizeof(before)) == 0);
}

static void begin(struct fixture *f)
{
    struct ot_volume_label label;
    struct ot_session_start start;
    oa_u8 bytes[88];
    memset(f, 0, sizeof(*f));
    assert(ot_archive_verify_finish(&f->state) == OT_VERIFY_INCOMPLETE);
    memset(&label, 0, sizeof(label));
    label.version = 1u;
    ot_volume_label_encode(bytes, &label);
    emit(f, OT_RECORD_VOLUME_LABEL, 0ul, 3u, 0ul, bytes, 88u, 0);
    assert(ot_archive_verify_finish(&f->state) == OT_VERIFY_INCOMPLETE);
    f->sid = 1ul;
    f->time = 123ul;
    memset(&start, 0, sizeof(start));
    start.created_utc = f->time;
    ot_session_start_encode(bytes, sizeof(bytes), &start);
    emit(f, OT_RECORD_SESSION_START, 0ul, 3u, 0ul, bytes, 56u, 0);
}

static void metadata(struct fixture *f, oa_u32 object, oa_u32 size, unsigned directory, int expected)
{
    struct ot_file_metadata m;
    oa_u8 bytes[300];
    oa_u16 written;
    memset(&m, 0, sizeof(m));
    m.kind = directory ? OT_FILE_DIRECTORY : OT_FILE_REGULAR;
    m.dos_attributes = directory ? 0x10u : 0x20u;
    m.file_size = size;
    strcpy(m.path, directory ? "DATA" : "DATA/FILE.TXT");
    assert(ot_file_metadata_encode(bytes, sizeof(bytes), &m, &written) == 0);
    emit(f, OT_RECORD_FILE_METADATA, object, 3u, 0ul, bytes, written, expected);
}
static void file_end(struct fixture *f, oa_u32 object, oa_u32 size,
                        oa_u32 crc, oa_u32 fragments, int expected)
{
    struct ot_file_end end;
    oa_u8 bytes[16];
    end.file_size = size;
    end.crc32 = crc;
    end.fragments = fragments;
    assert(ot_file_end_encode(bytes, sizeof(bytes), &end) == 0);
    emit(f, OT_RECORD_FILE_END, object, 3u, 0ul, bytes, sizeof(bytes), expected);
}
static void session_end(struct fixture *f, unsigned result, oa_u32 files,
                           oa_u32 directories, oa_u32 low, oa_u32 high, int expected)
{
    struct ot_session_end end;
    oa_u8 bytes[20];
    end.result = (oa_u16)result;
    end.files = files;
    end.directories = directories;
    end.bytes_low = low;
    end.bytes_high = high;
    assert(ot_session_end_encode(bytes, sizeof(bytes), &end) == 0);
    emit(f, OT_RECORD_SESSION_END, 0ul, 3u, 0ul, bytes, sizeof(bytes), expected);
}

static void test_atomic_block(void)
{
    struct fixture f;
    struct ot_archive_verify before;
    struct ot_session_start start;
    struct ot_session_end end;
    struct ot_block_header b;
    struct ot_record_header r;
    oa_u8 payload[116], block[152], data[56];
    begin(&f);
    session_end(&f, 0u, 0ul, 0ul, 0ul, 0ul, 0);
    memcpy(&before, &f.state, sizeof(before));
    memset(&start, 0, sizeof(start));
    start.created_utc = f.time;
    ot_session_start_encode(data, sizeof(data), &start);
    memset(&r, 0, sizeof(r));
    r.type = OT_RECORD_SESSION_START;
    r.flags = 3u;
    r.data_size = 56ul;
    r.data_crc32 = ot_crc32(data, 56u);
    ot_record_header_encode(payload, &r);
    memcpy(payload + 20, data, 56u);
    memset(&end, 0, sizeof(end));
    end.files = 1ul;                    /* Bad only after the valid START. */
    ot_session_end_encode(data, sizeof(data), &end);
    r.type = OT_RECORD_SESSION_END;
    r.data_size = 20ul;
    r.data_crc32 = ot_crc32(data, 20u);
    ot_record_header_encode(payload + 76, &r);
    memcpy(payload + 96, data, 20u);
    memset(&b, 0, sizeof(b));
    b.block_number = f.state.framing.blocks;
    b.record_count = 2u;
    b.session_id = 2ul;
    b.session_time = f.time;
    ot_block_encode(block, sizeof(block), &b, payload, sizeof(payload));
    assert(ot_archive_verify_block(&f.state, block, sizeof(block)) == OT_VERIFY_SUMMARY);
    assert(memcmp(&f.state, &before, sizeof(before)) == 0);
    assert(ot_archive_verify_finish(&f.state) == 0);
    end.files = 0ul;
    ot_session_end_encode(data, sizeof(data), &end);
    r.data_crc32 = ot_crc32(data, 20u);
    ot_record_header_encode(payload + 76, &r);
    memcpy(payload + 96, data, 20u);
    ot_block_encode(block, sizeof(block), &b, payload, sizeof(payload));
    assert(ot_archive_verify_block(&f.state, block, sizeof(block)) == 0);
    assert(f.state.sessions == 2ul && ot_archive_verify_finish(&f.state) == 0);
}

static void empty_block(struct fixture *f, int expected)
{
    struct ot_archive_verify before;
    struct ot_block_header b;
    oa_u8 block[OT_BLOCK_HEADER_SIZE];
    memset(&b, 0, sizeof(b));
    b.block_number = f->state.framing.blocks;
    b.session_id = f->sid;
    b.session_time = f->time;
    ot_block_encode(block, sizeof(block), &b, 0, 0u);
    memcpy(&before, &f->state, sizeof(before));
    assert(ot_archive_verify_block(&f->state, block, sizeof(block)) == expected);
    if (expected != 0) assert(memcmp(&before, &f->state, sizeof(before)) == 0);
}

int main(void)
{
    static const oa_u8 text[] = "123456789";
    struct fixture f;
    struct ot_session_start start;
    oa_u8 bytes[56];

    begin(&f);
    f.sid = 2ul;
    metadata(&f, 1ul, 0ul, 1u, OT_VERIFY_ORDER);
    empty_block(&f, OT_VERIFY_ORDER);
    f.sid = 1ul;
    ++f.time;
    metadata(&f, 1ul, 0ul, 1u, OT_VERIFY_ORDER);
    --f.time;
    empty_block(&f, 0);
    metadata(&f, 1ul, 0ul, 1u, 0);
    emit(&f, OT_RECORD_FILE_DATA, 1ul, 3u, 0ul, text, 1u, OT_VERIFY_FILE_DATA);
    metadata(&f, 2ul, 9ul, 0u, 0);
    metadata(&f, 3ul, 0ul, 0u, OT_VERIFY_ORDER);
    emit(&f, OT_RECORD_FILE_DATA, 2ul, 1u, 0ul, text, 3u, 0);
    file_end(&f, 2ul, 9ul, 0xcbf43926ul, 1ul, OT_VERIFY_FILE_DATA);
    emit(&f, OT_RECORD_FILE_DATA, 2ul, 0u, 4ul, text + 3, 3u, OT_VERIFY_FILE_DATA);
    emit(&f, OT_RECORD_FILE_DATA, 2ul, 1u, 3ul, text + 3, 3u, OT_SCAN_BAD_RECORD);
    emit(&f, OT_RECORD_FILE_DATA, 2ul, 2u, 3ul, text + 3, 3u, OT_VERIFY_FILE_DATA);
    emit(&f, OT_RECORD_FILE_DATA, 3ul, 0u, 3ul, text + 3, 3u, OT_VERIFY_FILE_DATA);
    emit(&f, OT_RECORD_FILE_DATA, 2ul, 0u, 3ul, text + 3, 3u, 0);
    emit(&f, OT_RECORD_FILE_DATA, 2ul, 0u, 6ul, text + 6, 3u, OT_VERIFY_FILE_DATA);
    emit(&f, OT_RECORD_FILE_DATA, 2ul, 2u, 6ul, text + 6, 3u, 0);
    emit(&f, OT_RECORD_FILE_DATA, 2ul, 2u, 9ul, text, 1u, OT_VERIFY_FILE_DATA);
    file_end(&f, 2ul, 9ul, 1ul, 3ul, OT_VERIFY_FILE_CRC);
    file_end(&f, 2ul, 9ul, 0xcbf43926ul, 2ul, OT_VERIFY_FILE_DATA);
    file_end(&f, 2ul, 9ul, 0xcbf43926ul, 3ul, 0);
    metadata(&f, 2ul, 0ul, 0u, OT_VERIFY_ORDER);
    metadata(&f, 4ul, 0ul, 0u, 0);
    file_end(&f, 4ul, 0ul, 0ul, 0ul, 0);
    session_end(&f, 0u, 1ul, 1ul, 9ul, 0ul, OT_VERIFY_SUMMARY);
    session_end(&f, 0u, 2ul, 1ul, 10ul, 0ul, OT_VERIFY_SUMMARY);
    session_end(&f, 1u, 2ul, 1ul, 9ul, 0ul, OT_VERIFY_INCOMPLETE);
    session_end(&f, 0u, 2ul, 1ul, 9ul, 0ul, 0);
    assert(ot_archive_verify_finish(&f.state) == 0 && f.state.sessions == 1ul);
    empty_block(&f, OT_VERIFY_ORDER);
    session_end(&f, 0u, 2ul, 1ul, 9ul, 0ul, OT_VERIFY_ORDER);
    emit(&f, 0x9000u, 0ul, 3u, 0ul, text, 1u, OT_VERIFY_UNSUPPORTED);
    emit(&f, OT_RECORD_VOLUME_END, 0ul, 3u, 0ul, 0, 0u, OT_VERIFY_UNSUPPORTED);
    memset(&start, 0, sizeof(start));
    start.created_utc = f.time;
    ot_session_start_encode(bytes, sizeof(bytes), &start);
    emit(&f, OT_RECORD_SESSION_START, 0ul, 3u, 0ul, bytes, 56u, OT_VERIFY_ORDER);
    f.sid = 2ul;
    emit(&f, OT_RECORD_SESSION_START, 0ul, 3u, 0ul, bytes, 56u, 0);
    assert(ot_archive_verify_finish(&f.state) == OT_VERIFY_INCOMPLETE);
    session_end(&f, 0u, 0ul, 0ul, 0ul, 0ul, 0);
    assert(ot_archive_verify_finish(&f.state) == 0 && f.state.sessions == 2ul);

    begin(&f);
    metadata(&f, 1ul, 1ul, 0u, 0);
    assert(ot_archive_verify_finish(&f.state) == OT_VERIFY_INCOMPLETE);
    session_end(&f, 0u, 0ul, 0ul, 0ul, 0ul, OT_VERIFY_INCOMPLETE);
    emit(&f, OT_RECORD_FILE_DATA, 1ul, 3u, 0ul, text, 1u, 0);
    f.state.bytes_low = 0xfffffffful; /* Inject only counter boundary conditions. */
    file_end(&f, 1ul, 1ul, ot_crc32(text, 1u), 1ul, 0);
    assert(f.state.bytes_low == 0ul && f.state.bytes_high == 1ul);
    begin(&f);
    metadata(&f, 1ul, 1ul, 0u, 0);
    emit(&f, OT_RECORD_FILE_DATA, 1ul, 3u, 0ul, text, 1u, 0);
    f.state.bytes_low = f.state.bytes_high = 0xfffffffful;
    file_end(&f, 1ul, 1ul, ot_crc32(text, 1u), 1ul, OT_VERIFY_OVERFLOW);
    begin(&f);
    f.state.directories = 0xfffffffful;
    metadata(&f, 1ul, 0ul, 1u, OT_VERIFY_OVERFLOW);
    begin(&f);
    metadata(&f, 1ul, 0ul, 0u, 0);
    f.state.files = 0xfffffffful;
    file_end(&f, 1ul, 0ul, 0ul, 0ul, OT_VERIFY_OVERFLOW);
    begin(&f);
    f.state.sessions = 0xfffffffful;
    session_end(&f, 0u, 0ul, 0ul, 0ul, 0ul, OT_VERIFY_OVERFLOW);
    begin(&f);
    metadata(&f, 1ul, 2ul, 0u, 0);
    emit(&f, OT_RECORD_FILE_DATA, 1ul, 1u, 0ul, text, 1u, 0);
    f.state.fragments = 0xfffffffful;
    emit(&f, OT_RECORD_FILE_DATA, 1ul, 2u, 1ul, text + 1, 1u, OT_VERIFY_OVERFLOW);
    test_atomic_block();
    assert(ot_archive_verify_finish(0) == OT_FORMAT_BAD_ARGUMENT);
    assert(ot_archive_verify_block(0, 0, 0u) == OT_FORMAT_BAD_ARGUMENT);
    assert(ot_archive_verify_block(&f.state, 0, 0u) == OT_FORMAT_BAD_ARGUMENT);
    puts("OpenTape archive state verification tests passed");
    return 0;
}

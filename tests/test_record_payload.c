#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/record_payload.h"
#include "opentape/native_scan.h"

static void test_paths(void)
{
    static const char *bad[] = { "", ".", "..", "../A.TXT", "/A.TXT",
        "A/", "A//B", "C:A.TXT", "A\\B.TXT", "TOOLONG99.TXT", "A.LONG",
        "A.", "A..TXT", "A B.TXT", "CON", "DIR/con.txt", "AUX", "NUL.TXT",
        "LPT9.LOG", "COM0", "CLOCK$", "CONOUT$", "A\033.TXT" };
    unsigned i;
    assert(ot_archive_path_valid("data/file.txt"));
    assert(ot_archive_path_valid("ABCDEFGH.XYZ"));
    assert(ot_archive_path_valid("A_-$~.BIN"));
    assert(!ot_archive_path_valid(0));
    for (i = 0u; i < sizeof(bad) / sizeof(bad[0]); ++i)
        assert(!ot_archive_path_valid(bad[i]));
}

static void test_metadata(void)
{
    struct ot_file_metadata v, decoded, before;
    oa_u8 bytes[300];
    oa_u16 size, i;
    memset(&v, 0, sizeof(v));
    v.kind = OT_FILE_REGULAR;
    v.dos_attributes = 0x20u;
    v.file_size = 0x12345678ul;
    strcpy(v.path, "DATA/FILE.TXT");
    memset(bytes, 0xa5, sizeof(bytes));
    assert(ot_file_metadata_encode(bytes, sizeof(bytes), &v, &size) == OT_FORMAT_OK);
    assert(size == 33u && bytes[33] == 0xa5u);
    assert(bytes[0] == 1u && bytes[2] == 1u && bytes[4] == 0x20u && bytes[6] == 13u);
    assert(bytes[8] == 0x78u && bytes[9] == 0x56u && bytes[10] == 0x34u && bytes[11] == 0x12u);
    assert(ot_file_metadata_decode(&decoded, bytes, size) == OT_FORMAT_OK);
    assert(decoded.file_size == v.file_size && strcmp(decoded.path, v.path) == 0);
    memcpy(&before, &decoded, sizeof(before));
    for (i = 0u; i < size; ++i) {
        assert(ot_file_metadata_decode(&decoded, bytes, i) != OT_FORMAT_OK);
        assert(memcmp(&before, &decoded, sizeof(before)) == 0);
    }
    bytes[16] = 1u;
    assert(ot_file_metadata_decode(&decoded, bytes, size) == OT_PAYLOAD_BAD_DATA);
    bytes[16] = 0u;
    bytes[20] = 0u;
    assert(ot_file_metadata_decode(&decoded, bytes, size) == OT_PAYLOAD_BAD_DATA);
    assert(memcmp(&before, &decoded, sizeof(before)) == 0);
    memset(bytes, 0xa5, sizeof(bytes));
    assert(ot_file_metadata_encode(bytes, 20u, &v, &size) == OT_FORMAT_BAD_SIZE);
    assert(size == 0u && bytes[0] == 0xa5u);

    v.dos_date = (oa_u16)((44u << 9) | (2u << 5) | 29u);
    v.dos_time = (oa_u16)((23u << 11) | (59u << 5) | 29u);
    assert(ot_file_metadata_encode(bytes, sizeof(bytes), &v, &size) == OT_FORMAT_OK);
    v.dos_date = (oa_u16)((43u << 9) | (2u << 5) | 29u);
    assert(ot_file_metadata_encode(bytes, sizeof(bytes), &v, &size) == OT_PAYLOAD_BAD_DATA);
    v.dos_date = (oa_u16)((120u << 9) | (2u << 5) | 29u);
    assert(ot_file_metadata_encode(bytes, sizeof(bytes), &v, &size) == OT_PAYLOAD_BAD_DATA);
    v.dos_date = 0u;
    assert(ot_file_metadata_encode(bytes, sizeof(bytes), &v, &size) == OT_PAYLOAD_BAD_DATA);
    v.dos_time = 0u;
    v.kind = OT_FILE_DIRECTORY;
    v.dos_attributes = 0x10u;
    assert(ot_file_metadata_encode(bytes, sizeof(bytes), &v, &size) == OT_PAYLOAD_BAD_DATA);
    v.file_size = 0ul;
    assert(ot_file_metadata_encode(bytes, sizeof(bytes), &v, &size) == OT_FORMAT_OK);
    assert(ot_file_metadata_decode(&decoded, bytes, size) == OT_FORMAT_OK);
    assert(decoded.kind == OT_FILE_DIRECTORY && decoded.file_size == 0ul);
    v.dos_attributes = 0x18u;
    assert(ot_file_metadata_encode(bytes, sizeof(bytes), &v, &size) == OT_PAYLOAD_BAD_DATA);

    /* Exactly 255 bytes: 31 seven-character directory components and a name. */
    v.dos_attributes = 0x10u;
    for (i = 0u; i < 255u; ++i) v.path[i] = i % 8u == 7u ? '/' : 'A';
    v.path[255] = 0;
    assert(ot_file_metadata_encode(bytes, sizeof(bytes), &v, &size) == OT_FORMAT_OK);
    assert(size == 275u);
    assert(ot_file_metadata_decode(&decoded, bytes, size) == OT_FORMAT_OK);
    memset(v.path, 'A', sizeof(v.path));
    assert(ot_file_metadata_encode(bytes, sizeof(bytes), &v, &size) == OT_PAYLOAD_BAD_DATA);
}

static void test_session_and_end(void)
{
    struct ot_session_start start, decoded;
    struct ot_file_end end, end_decoded;
    struct ot_session_end session, session_decoded;
    oa_u8 bytes[80];
    unsigned i;
    memset(&start, 0, sizeof(start));
    start.created_utc = 0x12345678ul;
    for (i = 0u; i < 16u; ++i) start.uuid[i] = (oa_u8)i;
    strcpy(start.name, "SESSION");
    memset(bytes, 0xa5, sizeof(bytes));
    assert(ot_session_start_encode(bytes, sizeof(bytes), &start) == OT_FORMAT_OK);
    assert(bytes[4] == 0x78u && bytes[7] == 0x12u && bytes[23] == 15u && bytes[56] == 0xa5u);
    assert(ot_session_start_decode(&decoded, bytes, 56u) == OT_FORMAT_OK);
    assert(decoded.created_utc == start.created_utc && strcmp(decoded.name, "SESSION") == 0);
    bytes[55] = 'X';
    assert(ot_session_start_decode(&decoded, bytes, 56u) == OT_PAYLOAD_BAD_DATA);
    assert(ot_session_start_encode(bytes, 55u, &start) == OT_FORMAT_BAD_SIZE);

    end.file_size = 0xfffffffful;
    end.crc32 = 0xcbf43926ul;
    end.fragments = 17ul;
    assert(ot_file_end_encode(bytes, sizeof(bytes), &end) == OT_FORMAT_OK);
    assert(bytes[4] == 0xffu && bytes[8] == 0x26u && bytes[12] == 17u);
    assert(ot_file_end_decode(&end_decoded, bytes, OT_FILE_END_SIZE) == OT_FORMAT_OK);
    assert(end_decoded.file_size == end.file_size && end_decoded.crc32 == end.crc32);
    bytes[2] = 1u;
    assert(ot_file_end_decode(&end_decoded, bytes, OT_FILE_END_SIZE) == OT_PAYLOAD_BAD_DATA);
    memset(&end, 0, sizeof(end));
    assert(ot_file_end_encode(bytes, sizeof(bytes), &end) == OT_FORMAT_OK);
    assert(ot_file_end_decode(&end_decoded, bytes, OT_FILE_END_SIZE) == OT_FORMAT_OK);
    bytes[8] = 1u;
    assert(ot_file_end_decode(&end_decoded, bytes, OT_FILE_END_SIZE) == OT_PAYLOAD_BAD_DATA);
    end.file_size = 1ul;
    assert(ot_file_end_encode(bytes, sizeof(bytes), &end) == OT_PAYLOAD_BAD_DATA);

    memset(&session, 0, sizeof(session));
    session.result = OT_SESSION_INCOMPLETE;
    session.files = 7ul;
    session.directories = 2ul;
    session.bytes_low = 0x12345678ul;
    session.bytes_high = 1ul;
    assert(ot_session_end_encode(bytes, sizeof(bytes), &session) == OT_FORMAT_OK);
    assert(bytes[2] == 1u && bytes[4] == 7u && bytes[8] == 2u && bytes[16] == 1u);
    assert(ot_session_end_decode(&session_decoded, bytes, OT_SESSION_END_SIZE) == OT_FORMAT_OK);
    assert(session_decoded.bytes_high == 1ul && session_decoded.bytes_low == session.bytes_low);
    bytes[2] = 2u;
    assert(ot_session_end_decode(&session_decoded, bytes, OT_SESSION_END_SIZE) == OT_PAYLOAD_BAD_DATA);
    session.result = 2u;
    assert(ot_session_end_encode(bytes, sizeof(bytes), &session) == OT_PAYLOAD_BAD_DATA);
}

static void test_typed_scan(void)
{
    struct ot_native_scan scan, before;
    struct ot_volume_label label;
    struct ot_block_header block_header;
    struct ot_record_header record;
    struct ot_file_metadata metadata;
    struct ot_session_start start;
    oa_u8 data[300], payload[320], block[400];
    oa_u16 size;
    memset(&scan, 0, sizeof(scan));
    memset(&label, 0, sizeof(label));
    label.version = OT_VOLUME_FORMAT_VERSION;
    ot_volume_label_encode(data, &label);
    memset(&record, 0, sizeof(record));
    record.type = OT_RECORD_VOLUME_LABEL;
    record.flags = OT_RECORD_FLAG_FIRST | OT_RECORD_FLAG_LAST;
    record.data_size = OT_VOLUME_LABEL_SIZE;
    record.data_crc32 = ot_crc32(data, OT_VOLUME_LABEL_SIZE);
    ot_record_header_encode(payload, &record);
    memcpy(payload + OT_RECORD_HEADER_SIZE, data, OT_VOLUME_LABEL_SIZE);
    memset(&block_header, 0, sizeof(block_header));
    block_header.record_count = 1u;
    ot_block_encode(block, sizeof(block), &block_header, payload, 108u);
    assert(ot_native_scan_block(&scan, block, 144u) == OT_FORMAT_OK);

    memset(&start, 0, sizeof(start));
    strcpy(start.name, "SESSION");
    ot_session_start_encode(data, sizeof(data), &start);
    record.type = OT_RECORD_SESSION_START;
    record.data_size = OT_SESSION_START_SIZE;
    record.data_crc32 = ot_crc32(data, OT_SESSION_START_SIZE);
    ot_record_header_encode(payload, &record);
    memcpy(payload + 20, data, OT_SESSION_START_SIZE);
    block_header.block_number = 1ul;
    block_header.session_id = 1ul;
    ot_block_encode(block, sizeof(block), &block_header, payload, 76u);
    assert(ot_native_scan_block(&scan, block, 112u) == OT_FORMAT_OK);

    memset(&metadata, 0, sizeof(metadata));
    metadata.kind = OT_FILE_REGULAR;
    strcpy(metadata.path, "FILE.TXT");
    ot_file_metadata_encode(data, sizeof(data), &metadata, &size);
    record.type = OT_RECORD_FILE_METADATA;
    record.object_id = 1ul;
    record.data_size = size;
    record.data_crc32 = ot_crc32(data, size);
    ot_record_header_encode(payload, &record);
    memcpy(payload + OT_RECORD_HEADER_SIZE, data, size);
    block_header.block_number = 2ul;
    ot_block_encode(block, sizeof(block), &block_header, payload, (oa_u16)(size + 20u));
    assert(ot_native_scan_block(&scan, block, (oa_u16)(size + 56u)) == OT_FORMAT_OK);

    /* All CRCs remain valid: the typed metadata path itself is malicious. */
    memcpy(&before, &scan, sizeof(before));
    data[OT_FILE_METADATA_PREFIX] = '/';
    record.data_crc32 = ot_crc32(data, size);
    ot_record_header_encode(payload, &record);
    memcpy(payload + 20, data, size);
    block_header.block_number = 3ul;
    ot_block_encode(block, sizeof(block), &block_header, payload, (oa_u16)(size + 20u));
    assert(ot_native_scan_block(&scan, block, (oa_u16)(size + 56u)) == OT_PAYLOAD_BAD_DATA);
    assert(memcmp(&before, &scan, sizeof(before)) == 0);
}

int main(void)
{
    test_paths();
    test_metadata();
    test_session_and_end();
    test_typed_scan();
    puts("OpenTape typed payload tests passed");
    return 0;
}

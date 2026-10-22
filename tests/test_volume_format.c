#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "opentape/volume_format.h"

static void test_crc32(void)
{
    static const oa_u8 check[] = "123456789";

    assert(ot_crc32(check, 9u) == 0xcbf43926ul);
    assert(ot_crc32(check, 0u) == 0ul);
    assert(ot_crc32_extend(ot_crc32(check, 3u), check + 3, 6u) == 0xcbf43926ul);
    assert(ot_crc32_extend(0xcbf43926ul, 0, 0u) == 0xcbf43926ul);
}

static void test_block(void)
{
    static const oa_u8 payload[] = "OpenTape block payload";
    oa_u8 block[128];
    struct ot_block_header source;
    struct ot_block_header decoded;
    oa_u16 block_size;

    source.block_number = 0x01020304ul;
    source.session_id = 0x11223344ul;
    source.session_time = 0x55667788ul;
    source.record_count = 3u;
    source.flags = OT_BLOCK_FLAG_FIRST;
    block_size = (oa_u16)(OT_BLOCK_HEADER_SIZE + sizeof(payload) - 1u);

    assert(ot_block_encode(block, sizeof(block), &source, payload,
                           (oa_u16)(sizeof(payload) - 1u)) == OT_FORMAT_OK);
    assert(memcmp(block, "OTB1", 4u) == 0);
    assert(block[4] == 1u && block[5] == 0u);
    assert(block[6] == OT_BLOCK_HEADER_SIZE && block[7] == 0u);
    assert(block[12] == 4u && block[13] == 3u &&
           block[14] == 2u && block[15] == 1u);
    assert(ot_block_decode(&decoded, block, block_size) == OT_FORMAT_OK);
    assert(decoded.block_number == source.block_number);
    assert(decoded.session_id == source.session_id);
    assert(decoded.session_time == source.session_time);
    assert(decoded.record_count == source.record_count);
    assert(decoded.flags == source.flags);

    block[OT_BLOCK_HEADER_SIZE + 1u] ^= 0x01u;
    assert(ot_block_decode(&decoded, block, block_size) ==
           OT_FORMAT_BAD_PAYLOAD_CRC);
    block[OT_BLOCK_HEADER_SIZE + 1u] ^= 0x01u;
    block[12] ^= 0x01u;
    assert(ot_block_decode(&decoded, block, block_size) ==
           OT_FORMAT_BAD_HEADER_CRC);
}

static void test_record_header(void)
{
    oa_u8 encoded[OT_RECORD_HEADER_SIZE];
    struct ot_record_header source;
    struct ot_record_header decoded;

    source.type = OT_RECORD_FILE_DATA;
    source.flags = OT_RECORD_FLAG_FIRST | OT_RECORD_FLAG_LAST;
    source.data_size = 0x12345678ul;
    source.object_id = 42ul;
    source.fragment_offset = 0x01020304ul;
    source.data_crc32 = 0xaabbccddul;

    assert(ot_record_header_encode(encoded, &source) == OT_FORMAT_OK);
    assert(encoded[0] == OT_RECORD_FILE_DATA && encoded[1] == 0u);
    assert(encoded[4] == 0x78u && encoded[5] == 0x56u &&
           encoded[6] == 0x34u && encoded[7] == 0x12u);
    assert(ot_record_header_decode(&decoded, encoded) == OT_FORMAT_OK);
    assert(decoded.type == source.type);
    assert(decoded.flags == source.flags);
    assert(decoded.data_size == source.data_size);
    assert(decoded.object_id == source.object_id);
    assert(decoded.fragment_offset == source.fragment_offset);
    assert(decoded.data_crc32 == source.data_crc32);
}

static void test_volume_label(void)
{
    oa_u8 encoded[OT_VOLUME_LABEL_SIZE];
    struct ot_volume_label source;
    struct ot_volume_label decoded;
    unsigned i;

    memset(&source, 0, sizeof(source));
    source.version = OT_VOLUME_FORMAT_VERSION;
    source.flags = 1u;
    source.volume_sequence = 7ul;
    source.created_utc = 0x12345678ul;
    for (i = 0u; i < sizeof(source.volume_uuid); ++i) {
        source.volume_uuid[i] = (oa_u8)i;
        source.volume_set_uuid[i] = (oa_u8)(0xf0u + i);
    }
    strcpy(source.volume_name, "DDS-TEST-0001");

    assert(ot_volume_label_encode(encoded, &source) == OT_FORMAT_OK);
    assert(memcmp(encoded, "OPENTAPE", 8u) == 0);
    assert(ot_volume_label_decode(&decoded, encoded) == OT_FORMAT_OK);
    assert(decoded.version == source.version);
    assert(decoded.flags == source.flags);
    assert(decoded.volume_sequence == source.volume_sequence);
    assert(decoded.created_utc == source.created_utc);
    assert(memcmp(decoded.volume_uuid, source.volume_uuid, 16u) == 0);
    assert(memcmp(decoded.volume_set_uuid, source.volume_set_uuid, 16u) == 0);
    assert(memcmp(decoded.volume_name, source.volume_name, 32u) == 0);
    source.volume_name[31] = 'X';
    assert(ot_volume_label_encode(encoded, &source) == OT_FORMAT_OK);
    assert(encoded[87] == 0u);          /* No stale bytes after the name's NUL. */

    encoded[0] = 'X';
    assert(ot_volume_label_decode(&decoded, encoded) == OT_FORMAT_BAD_MAGIC);
}

int main(void)
{
    test_crc32();
    test_block();
    test_record_header();
    test_volume_label();
    puts("OpenTape volume-format tests passed");
    return 0;
}

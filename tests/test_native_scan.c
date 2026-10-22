#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/native_scan.h"

static oa_u16 put_record(oa_u8 *out, oa_u16 type, const oa_u8 *data, oa_u16 size)
{
    struct ot_record_header h;
    memset(&h, 0, sizeof(h));
    h.type = type;
    h.flags = OT_RECORD_FLAG_FIRST | OT_RECORD_FLAG_LAST;
    h.data_size = size;
    h.data_crc32 = ot_crc32(data, size);
    assert(ot_record_header_encode(out, &h) == OT_FORMAT_OK);
    memcpy(out + OT_RECORD_HEADER_SIZE, data, size);
    return (oa_u16)(OT_RECORD_HEADER_SIZE + size);
}

static oa_u16 wrap(oa_u8 *out, const oa_u8 *payload, oa_u16 size,
                    oa_u32 number, oa_u16 count, oa_u16 flags)
{
    struct ot_block_header h;
    memset(&h, 0, sizeof(h));
    h.block_number = number;
    h.record_count = count;
    h.flags = flags;
    assert(ot_block_encode(out, 512u, &h, payload, size) == OT_FORMAT_OK);
    return (oa_u16)(OT_BLOCK_HEADER_SIZE + size);
}

static oa_u16 label_payload(oa_u8 *out)
{
    struct ot_volume_label label;
    oa_u8 data[OT_VOLUME_LABEL_SIZE];
    memset(&label, 0, sizeof(label));
    label.version = OT_VOLUME_FORMAT_VERSION;
    label.volume_sequence = 7ul;
    strcpy(label.volume_name, "NATIVE-TEST");
    assert(ot_volume_label_encode(data, &label) == OT_FORMAT_OK);
    return put_record(out, OT_RECORD_VOLUME_LABEL, data, sizeof(data));
}

static void reject(struct ot_native_scan *scan, const oa_u8 *block,
                    oa_u16 size, int expected)
{
    struct ot_native_scan before;
    memcpy(&before, scan, sizeof(before));
    assert(ot_native_scan_block(scan, block, size) == expected);
    assert(memcmp(scan, &before, sizeof(before)) == 0);
}

#ifndef DOS_VERSION
static void test_maximum_block(void)
{
    struct ot_native_scan scan;
    struct ot_block_header header;
    struct ot_record_header record;
    oa_u8 first[512], label[256];
    oa_u8 *storage, *payload;
    oa_u16 length;

    memset(&scan, 0, sizeof(scan));
    length = label_payload(label);
    length = wrap(first, label, length, 0ul, 1u, 0u);
    assert(ot_native_scan_block(&scan, first, length) == OT_FORMAT_OK);
    storage = malloc(65535u + 16u);
    payload = malloc(65499u);
    assert(storage != 0 && payload != 0);
    memset(storage, 0xa5, 65535u + 16u);
    memset(payload, 0x5c, 65499u);
    memset(&record, 0, sizeof(record));
    record.type = 0x9000u;
    record.flags = OT_RECORD_FLAG_FIRST | OT_RECORD_FLAG_LAST;
    record.data_size = 65479ul;
    record.data_crc32 = ot_crc32(payload + OT_RECORD_HEADER_SIZE, 65479u);
    ot_record_header_encode(payload, &record);
    memset(&header, 0, sizeof(header));
    header.block_number = 1ul;
    header.record_count = 1u;
    assert(ot_block_encode(storage + 8, 65535u, &header, payload, 65499u) == OT_FORMAT_OK);
    assert(ot_native_scan_block(&scan, storage + 8, 65535u) == OT_FORMAT_OK);
    assert(scan.blocks == 2ul && scan.records == 2ul);
    assert(storage[0] == 0xa5u && storage[7] == 0xa5u);
    assert(storage[65543u] == 0xa5u && storage[65550u] == 0xa5u);
    free(storage);
    free(payload);
}
#endif

int main(void)
{
    struct ot_native_scan scan, empty;
    struct ot_record_header record;
    oa_u8 block[512], payload[256], original[256];
    oa_u16 length, size, i;
    static const oa_u8 data[4] = { 0, 0x1a, 0xff, 0x0a };

    memset(&empty, 0, sizeof(empty));
    memset(block, 0, sizeof(block));
    scan = empty;
    assert(ot_native_scan_block(0, block, 0u) == OT_FORMAT_BAD_ARGUMENT);
    assert(ot_native_scan_block(&scan, 0, 0u) == OT_FORMAT_BAD_ARGUMENT);
    length = label_payload(payload);
    memcpy(original, payload, length);
    size = wrap(block, payload, length, 0ul, 1u, OT_BLOCK_FLAG_FIRST);
    for (i = 0u; i < size; ++i) {
        assert(ot_native_scan_block(&scan, block, i) != OT_FORMAT_OK);
        assert(scan.blocks == 0ul && !scan.have_label);
    }
    assert(ot_native_scan_block(&scan, block, size) == OT_FORMAT_OK);
    assert(scan.have_label && scan.blocks == 1ul && scan.records == 1ul);
    assert(scan.label.volume_sequence == 7ul);
    assert(strcmp(scan.label.volume_name, "NATIVE-TEST") == 0);
    reject(&scan, block, size, OT_SCAN_BAD_SEQUENCE);
    block[12] ^= 1u;
    reject(&empty, block, size, OT_FORMAT_BAD_HEADER_CRC);
    block[12] ^= 1u;
    block[56] ^= 1u;
    reject(&empty, block, size, OT_FORMAT_BAD_PAYLOAD_CRC);

    /* Valid parent checksums cannot hide a corrupt record checksum. */
    payload[OT_RECORD_HEADER_SIZE + 1u] ^= 1u;
    size = wrap(block, payload, length, 0ul, 1u, 0u);
    reject(&empty, block, size, OT_SCAN_RECORD_CRC);
    memcpy(payload, original, length);
    size = wrap(block, payload, length, 0ul, 2u, 0u);
    reject(&empty, block, size, OT_SCAN_BAD_RECORD);
    payload[length] = 0u;
    size = wrap(block, payload, (oa_u16)(length + 1u), 0ul, 1u, 0u);
    reject(&empty, block, size, OT_SCAN_BAD_RECORD);
    size = wrap(block, payload, length, 0ul, 1u, 0x8000u);
    reject(&empty, block, size, OT_SCAN_BAD_RECORD);
    size = wrap(block, payload, length, 1ul, 1u, 0u);
    reject(&empty, block, size, OT_SCAN_BAD_SEQUENCE);
    reject(&scan, block, size, OT_SCAN_BAD_LABEL);
    size = wrap(block, payload, 0u, 0ul, 0u, 0u);
    reject(&empty, block, size, OT_SCAN_MISSING_LABEL);

    /* A bad second record must not publish even the valid preceding label. */
    memcpy(payload, original, length);
    i = put_record(payload + length, 0x9000u, data, sizeof(data));
    payload[length + OT_RECORD_HEADER_SIZE] ^= 1u;
    size = wrap(block, payload, (oa_u16)(length + i), 0ul, 2u, 0u);
    reject(&empty, block, size, OT_SCAN_RECORD_CRC);
    payload[length + OT_RECORD_HEADER_SIZE] ^= 1u;
    size = wrap(block, payload, (oa_u16)(length + i), 0ul, 2u, 0u);
    assert(ot_native_scan_block(&empty, block, size) == OT_FORMAT_OK);
    assert(empty.blocks == 1ul && empty.records == 2ul && empty.have_label);
    memset(&empty, 0, sizeof(empty));

    /* Unknown record types are skippable, but must obey all framing/CRCs. */
    length = put_record(payload, 0x9000u, data, sizeof(data));
    memcpy(original, payload, length);
    size = wrap(block, payload, length, 0ul, 1u, 0u);
    reject(&empty, block, size, OT_SCAN_MISSING_LABEL);
    size = wrap(block, payload, length, 3ul, 1u, 0u);
    reject(&scan, block, size, OT_SCAN_BAD_SEQUENCE);
    size = wrap(block, payload, length, 1ul, 1u, 0u);
    assert(ot_native_scan_block(&scan, block, size) == OT_FORMAT_OK);
    assert(scan.blocks == 2ul && scan.records == 2ul);
    ot_record_header_decode(&record, payload);
    record.data_size = 0xfffffffful;
    ot_record_header_encode(payload, &record);
    size = wrap(block, payload, length, 2ul, 1u, 0u);
    reject(&scan, block, size, OT_SCAN_BAD_RECORD);
    memcpy(payload, original, length);
    ot_record_header_decode(&record, payload);
    record.fragment_offset = 1ul;
    ot_record_header_encode(payload, &record);
    size = wrap(block, payload, length, 2ul, 1u, 0u);
    reject(&scan, block, size, OT_SCAN_BAD_RECORD);
    record.flags = OT_RECORD_FLAG_LAST;
    record.fragment_offset = 0xfffffffeul;
    ot_record_header_encode(payload, &record);
    size = wrap(block, payload, length, 2ul, 1u, 0u);
    reject(&scan, block, size, OT_SCAN_BAD_RECORD);

    /* Label size, fragment flags, payload magic/version and reserved bytes. */
    length = label_payload(payload);
    payload[2] = OT_RECORD_FLAG_FIRST;
    size = wrap(block, payload, length, 0ul, 1u, 0u);
    reject(&empty, block, size, OT_SCAN_BAD_LABEL);
    length = label_payload(payload);
    payload[OT_RECORD_HEADER_SIZE] = 'X';
    ot_record_header_decode(&record, payload);
    record.data_crc32 = ot_crc32(payload + OT_RECORD_HEADER_SIZE, OT_VOLUME_LABEL_SIZE);
    ot_record_header_encode(payload, &record);
    size = wrap(block, payload, length, 0ul, 1u, 0u);
    reject(&empty, block, size, OT_FORMAT_BAD_MAGIC);
    length = label_payload(payload);
    payload[OT_RECORD_HEADER_SIZE + 8u] = 2u;
    ot_record_header_decode(&record, payload);
    record.data_crc32 = ot_crc32(payload + OT_RECORD_HEADER_SIZE, OT_VOLUME_LABEL_SIZE);
    ot_record_header_encode(payload, &record);
    size = wrap(block, payload, length, 0ul, 1u, 0u);
    reject(&empty, block, size, OT_FORMAT_BAD_VERSION);
    length = label_payload(payload);
    payload[OT_RECORD_HEADER_SIZE + 14u] = 1u;
    ot_record_header_decode(&record, payload);
    record.data_crc32 = ot_crc32(payload + OT_RECORD_HEADER_SIZE, OT_VOLUME_LABEL_SIZE);
    ot_record_header_encode(payload, &record);
    size = wrap(block, payload, length, 0ul, 1u, 0u);
    reject(&empty, block, size, OT_SCAN_BAD_LABEL);

    scan.blocks = 0xfffffffful;
    size = wrap(block, payload, length, 0xfffffffful, 1u, 0u);
    reject(&scan, block, size, OT_SCAN_BAD_SEQUENCE);
#ifndef DOS_VERSION
    test_maximum_block();
#endif
    scan.blocks = 1ul;
    scan.records = 0xfffffffful;
    size = wrap(block, payload, length, 1ul, 1u, 0u);
    reject(&scan, block, size, OT_SCAN_BAD_SEQUENCE);
    puts("OpenTape native framing tests passed");
    return 0;
}

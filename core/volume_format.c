#include <string.h>

#include "opentape/volume_format.h"

static const oa_u8 ot_block_magic[4] = { 'O', 'T', 'B', '1' };
static const oa_u8 ot_volume_magic[8] =
    { 'O', 'P', 'E', 'N', 'T', 'A', 'P', 'E' };

static void ot_put_u16(oa_u8 *out, oa_u16 value)
{
    out[0] = (oa_u8)(value & 0xffu);
    out[1] = (oa_u8)((value >> 8) & 0xffu);
}

static void ot_put_u32(oa_u8 *out, oa_u32 value)
{
    out[0] = (oa_u8)(value & 0xfful);
    out[1] = (oa_u8)((value >> 8) & 0xfful);
    out[2] = (oa_u8)((value >> 16) & 0xfful);
    out[3] = (oa_u8)((value >> 24) & 0xfful);
}

static oa_u16 ot_get_u16(const oa_u8 *in)
{
    return (oa_u16)((oa_u16)in[0] | ((oa_u16)in[1] << 8));
}

static oa_u32 ot_get_u32(const oa_u8 *in)
{
    return (oa_u32)in[0] |
           ((oa_u32)in[1] << 8) |
           ((oa_u32)in[2] << 16) |
           ((oa_u32)in[3] << 24);
}

oa_u32 ot_crc32(const oa_u8 *data, oa_u16 size)
{
    oa_u32 crc;
    oa_u16 i;
    unsigned bit;

    if (data == 0 && size != 0u) {
        return 0ul;
    }

    crc = 0xfffffffful;
    for (i = 0u; i < size; ++i) {
        crc ^= (oa_u32)data[i];
        for (bit = 0u; bit < 8u; ++bit) {
            if ((crc & 1ul) != 0ul) {
                crc = (crc >> 1) ^ 0xedb88320ul;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc ^ 0xfffffffful;
}

int ot_block_encode(oa_u8 *block, oa_u16 block_capacity,
                    const struct ot_block_header *header,
                    const oa_u8 *payload, oa_u16 payload_size)
{
    oa_u32 total_size;

    if (block == 0 || header == 0 ||
        (payload == 0 && payload_size != 0u)) {
        return OT_FORMAT_BAD_ARGUMENT;
    }

    total_size = (oa_u32)OT_BLOCK_HEADER_SIZE + (oa_u32)payload_size;
    if (total_size > (oa_u32)block_capacity ||
        total_size > (oa_u32)OT_BLOCK_MAX_SIZE) {
        return OT_FORMAT_BAD_SIZE;
    }

    memcpy(block, ot_block_magic, sizeof(ot_block_magic));
    ot_put_u16(block + 4, (oa_u16)OT_VOLUME_FORMAT_VERSION);
    ot_put_u16(block + 6, (oa_u16)OT_BLOCK_HEADER_SIZE);
    ot_put_u32(block + 8, total_size);
    ot_put_u32(block + 12, header->block_number);
    ot_put_u32(block + 16, header->session_id);
    ot_put_u32(block + 20, header->session_time);
    ot_put_u16(block + 24, header->record_count);
    ot_put_u16(block + 26, header->flags);
    ot_put_u32(block + 28, ot_crc32(payload, payload_size));
    ot_put_u32(block + 32, ot_crc32(block, 32u));
    if (payload_size != 0u) {
        memcpy(block + OT_BLOCK_HEADER_SIZE, payload, payload_size);
    }
    return OT_FORMAT_OK;
}

int ot_block_decode(struct ot_block_header *header,
                    const oa_u8 *block, oa_u16 block_size)
{
    oa_u32 stored_size;
    oa_u16 payload_size;

    if (header == 0 || block == 0) {
        return OT_FORMAT_BAD_ARGUMENT;
    }
    if (block_size < (oa_u16)OT_BLOCK_HEADER_SIZE) {
        return OT_FORMAT_BAD_SIZE;
    }
    if (memcmp(block, ot_block_magic, sizeof(ot_block_magic)) != 0) {
        return OT_FORMAT_BAD_MAGIC;
    }
    if (ot_get_u16(block + 4) != (oa_u16)OT_VOLUME_FORMAT_VERSION) {
        return OT_FORMAT_BAD_VERSION;
    }
    if (ot_get_u16(block + 6) != (oa_u16)OT_BLOCK_HEADER_SIZE) {
        return OT_FORMAT_BAD_SIZE;
    }
    stored_size = ot_get_u32(block + 8);
    if (stored_size != (oa_u32)block_size) {
        return OT_FORMAT_BAD_SIZE;
    }
    if (ot_get_u32(block + 32) != ot_crc32(block, 32u)) {
        return OT_FORMAT_BAD_HEADER_CRC;
    }

    payload_size = (oa_u16)(block_size - (oa_u16)OT_BLOCK_HEADER_SIZE);
    if (ot_get_u32(block + 28) !=
        ot_crc32(block + OT_BLOCK_HEADER_SIZE, payload_size)) {
        return OT_FORMAT_BAD_PAYLOAD_CRC;
    }

    header->block_number = ot_get_u32(block + 12);
    header->session_id = ot_get_u32(block + 16);
    header->session_time = ot_get_u32(block + 20);
    header->record_count = ot_get_u16(block + 24);
    header->flags = ot_get_u16(block + 26);
    return OT_FORMAT_OK;
}

int ot_record_header_encode(oa_u8 out[OT_RECORD_HEADER_SIZE],
                            const struct ot_record_header *header)
{
    if (out == 0 || header == 0) {
        return OT_FORMAT_BAD_ARGUMENT;
    }
    ot_put_u16(out, header->type);
    ot_put_u16(out + 2, header->flags);
    ot_put_u32(out + 4, header->data_size);
    ot_put_u32(out + 8, header->object_id);
    ot_put_u32(out + 12, header->fragment_offset);
    ot_put_u32(out + 16, header->data_crc32);
    return OT_FORMAT_OK;
}

int ot_record_header_decode(struct ot_record_header *header,
                            const oa_u8 in[OT_RECORD_HEADER_SIZE])
{
    if (header == 0 || in == 0) {
        return OT_FORMAT_BAD_ARGUMENT;
    }
    header->type = ot_get_u16(in);
    header->flags = ot_get_u16(in + 2);
    header->data_size = ot_get_u32(in + 4);
    header->object_id = ot_get_u32(in + 8);
    header->fragment_offset = ot_get_u32(in + 12);
    header->data_crc32 = ot_get_u32(in + 16);
    return OT_FORMAT_OK;
}

int ot_volume_label_encode(oa_u8 out[OT_VOLUME_LABEL_SIZE],
                           const struct ot_volume_label *label)
{
    if (out == 0 || label == 0) {
        return OT_FORMAT_BAD_ARGUMENT;
    }
    if (label->version != (oa_u16)OT_VOLUME_FORMAT_VERSION) {
        return OT_FORMAT_BAD_VERSION;
    }

    memset(out, 0, OT_VOLUME_LABEL_SIZE);
    memcpy(out, ot_volume_magic, sizeof(ot_volume_magic));
    ot_put_u16(out + 8, label->version);
    ot_put_u16(out + 10, (oa_u16)OT_VOLUME_LABEL_SIZE);
    ot_put_u16(out + 12, label->flags);
    ot_put_u16(out + 14, 0u);
    ot_put_u32(out + 16, label->volume_sequence);
    ot_put_u32(out + 20, label->created_utc);
    memcpy(out + 24, label->volume_uuid, 16u);
    memcpy(out + 40, label->volume_set_uuid, 16u);
    memcpy(out + 56, label->volume_name, 32u);
    return OT_FORMAT_OK;
}

int ot_volume_label_decode(struct ot_volume_label *label,
                           const oa_u8 in[OT_VOLUME_LABEL_SIZE])
{
    if (label == 0 || in == 0) {
        return OT_FORMAT_BAD_ARGUMENT;
    }
    if (memcmp(in, ot_volume_magic, sizeof(ot_volume_magic)) != 0) {
        return OT_FORMAT_BAD_MAGIC;
    }
    if (ot_get_u16(in + 8) != (oa_u16)OT_VOLUME_FORMAT_VERSION) {
        return OT_FORMAT_BAD_VERSION;
    }
    if (ot_get_u16(in + 10) != (oa_u16)OT_VOLUME_LABEL_SIZE) {
        return OT_FORMAT_BAD_SIZE;
    }

    label->version = ot_get_u16(in + 8);
    label->flags = ot_get_u16(in + 12);
    label->volume_sequence = ot_get_u32(in + 16);
    label->created_utc = ot_get_u32(in + 20);
    memcpy(label->volume_uuid, in + 24, 16u);
    memcpy(label->volume_set_uuid, in + 40, 16u);
    memcpy(label->volume_name, in + 56, 32u);
    return OT_FORMAT_OK;
}

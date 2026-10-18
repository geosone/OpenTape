#ifndef OPENTAPE_VOLUME_FORMAT_H
#define OPENTAPE_VOLUME_FORMAT_H

#include "openaspi/types.h"

#define OT_VOLUME_FORMAT_VERSION 1u

#define OT_BLOCK_HEADER_SIZE 36u
#define OT_RECORD_HEADER_SIZE 20u
#define OT_VOLUME_LABEL_SIZE 88u

#define OT_BLOCK_RECOMMENDED_SIZE 32768u
#define OT_BLOCK_MAX_SIZE 65535u

#define OT_BLOCK_FLAG_FIRST 0x0001u
#define OT_BLOCK_FLAG_LAST  0x0002u

#define OT_RECORD_FLAG_FIRST 0x0001u
#define OT_RECORD_FLAG_LAST  0x0002u

enum ot_record_type {
    OT_RECORD_VOLUME_LABEL = 1,
    OT_RECORD_SESSION_START,
    OT_RECORD_FILE_METADATA,
    OT_RECORD_FILE_DATA,
    OT_RECORD_FILE_END,
    OT_RECORD_SESSION_END,
    OT_RECORD_VOLUME_END,
    OT_RECORD_CATALOG_CHECKPOINT
};

enum ot_format_result {
    OT_FORMAT_OK = 0,
    OT_FORMAT_BAD_ARGUMENT = -1,
    OT_FORMAT_BAD_MAGIC = -2,
    OT_FORMAT_BAD_VERSION = -3,
    OT_FORMAT_BAD_SIZE = -4,
    OT_FORMAT_BAD_HEADER_CRC = -5,
    OT_FORMAT_BAD_PAYLOAD_CRC = -6
};

struct ot_block_header {
    oa_u32 block_number;
    oa_u32 session_id;
    oa_u32 session_time;
    oa_u16 record_count;
    oa_u16 flags;
};

struct ot_record_header {
    oa_u16 type;
    oa_u16 flags;
    oa_u32 data_size;
    oa_u32 object_id;
    oa_u32 fragment_offset;
    oa_u32 data_crc32;
};

struct ot_volume_label {
    oa_u16 version;
    oa_u16 flags;
    oa_u32 volume_sequence;
    oa_u32 created_utc;
    oa_u8 volume_uuid[16];
    oa_u8 volume_set_uuid[16];
    char volume_name[32];
};

oa_u32 ot_crc32(const oa_u8 *data, oa_u16 size);

int ot_block_encode(oa_u8 *block, oa_u16 block_capacity,
                    const struct ot_block_header *header,
                    const oa_u8 *payload, oa_u16 payload_size);
int ot_block_decode(struct ot_block_header *header,
                    const oa_u8 *block, oa_u16 block_size);

int ot_record_header_encode(oa_u8 out[OT_RECORD_HEADER_SIZE],
                            const struct ot_record_header *header);
int ot_record_header_decode(struct ot_record_header *header,
                            const oa_u8 in[OT_RECORD_HEADER_SIZE]);

int ot_volume_label_encode(oa_u8 out[OT_VOLUME_LABEL_SIZE],
                           const struct ot_volume_label *label);
int ot_volume_label_decode(struct ot_volume_label *label,
                           const oa_u8 in[OT_VOLUME_LABEL_SIZE]);

#endif

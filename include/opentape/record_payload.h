#ifndef OPENTAPE_RECORD_PAYLOAD_H
#define OPENTAPE_RECORD_PAYLOAD_H
#include "opentape/volume_format.h"

#define OT_SESSION_START_SIZE 56u
#define OT_FILE_METADATA_PREFIX 20u
#define OT_FILE_PATH_MAX 255u
#define OT_FILE_END_SIZE 16u
#define OT_SESSION_END_SIZE 20u
#define OT_FILE_REGULAR 1u
#define OT_FILE_DIRECTORY 2u
#define OT_SESSION_COMPLETE 0u
#define OT_SESSION_INCOMPLETE 1u
#define OT_PAYLOAD_BAD_DATA (-60)

struct ot_session_start {
    oa_u32 created_utc;
    oa_u8 uuid[16];
    char name[32];
};
struct ot_file_metadata {
    oa_u16 kind;
    oa_u16 dos_attributes;
    oa_u32 file_size;
    oa_u16 dos_time;
    oa_u16 dos_date;
    char path[OT_FILE_PATH_MAX + 1u];
};
struct ot_file_end {
    oa_u32 file_size;
    oa_u32 crc32;
    oa_u32 fragments;
};
struct ot_session_end {
    oa_u16 result;
    oa_u32 files;
    oa_u32 directories;
    oa_u32 bytes_low;
    oa_u32 bytes_high;
};

int ot_archive_path_valid(const char *path);
int ot_session_start_encode(oa_u8 *out, oa_u16 capacity, const struct ot_session_start *value);
int ot_session_start_decode(struct ot_session_start *value, const oa_u8 *data, oa_u16 size);
int ot_file_metadata_encode(oa_u8 *out, oa_u16 capacity,
                            const struct ot_file_metadata *value, oa_u16 *written);
int ot_file_metadata_decode(struct ot_file_metadata *value, const oa_u8 *data, oa_u16 size);
int ot_file_end_encode(oa_u8 *out, oa_u16 capacity, const struct ot_file_end *value);
int ot_file_end_decode(struct ot_file_end *value, const oa_u8 *data, oa_u16 size);
int ot_session_end_encode(oa_u8 *out, oa_u16 capacity, const struct ot_session_end *value);
int ot_session_end_decode(struct ot_session_end *value, const oa_u8 *data, oa_u16 size);
#endif

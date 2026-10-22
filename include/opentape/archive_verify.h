#ifndef OPENTAPE_ARCHIVE_VERIFY_H
#define OPENTAPE_ARCHIVE_VERIFY_H

#include "opentape/native_scan.h"

enum ot_verify_result {
    OT_VERIFY_ORDER = -70,
    OT_VERIFY_FILE_DATA = -71,
    OT_VERIFY_FILE_CRC = -72,
    OT_VERIFY_SUMMARY = -73,
    OT_VERIFY_INCOMPLETE = -74,
    OT_VERIFY_UNSUPPORTED = -75,
    OT_VERIFY_OVERFLOW = -76
};

/* Zero-initialize at the volume beginning. Strict sequential profile:
 * one session/file at a time, increasing session/object IDs. No extraction.
 * A rejected block leaves the entire state unchanged. */
struct ot_archive_verify {
    struct ot_native_scan framing;
    oa_u32 sessions;
    oa_u32 last_session;
    oa_u32 session_time;
    unsigned session_active;
    oa_u32 last_object;
    unsigned file_active;
    unsigned file_last;
    oa_u32 file_size;
    oa_u32 file_bytes;
    oa_u32 file_crc;
    oa_u32 fragments;
    oa_u32 files;
    oa_u32 directories;
    oa_u32 bytes_low;
    oa_u32 bytes_high;
};

int ot_archive_verify_block(struct ot_archive_verify *state,
                            const oa_u8 *block, oa_u16 size);
/* Verifies closed sessions through a boundary, not the whole volume. */
int ot_archive_verify_finish(const struct ot_archive_verify *state);
#endif

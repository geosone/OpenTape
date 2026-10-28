#ifndef OPENTAPE_VERIFY_PROGRESS_H
#define OPENTAPE_VERIFY_PROGRESS_H
#include "opentape/verify_stream.h"
/* Zero initialize at the first block. Count across session boundaries; never
 * infer total tape size, whole-volume verification or successful close. The
 * verifier's 32-bit aggregate record bound also bounds files/directories. */
struct ot_verify_progress {
    oa_u32 blocks, records, sessions, files, directories;
    oa_u32 bytes_low, bytes_high, file_bytes;
    struct ot_file_metadata current;
    unsigned boundary;
};
/* Consume a borrowed, already verified block, in sequence, after visitors.
 * Bounds/decoders/overflow are checked; malformed observation preserves the
 * previous snapshot. Does not replace framing/order/CRC verification. No I/O. */
int ot_verify_progress_block(struct ot_verify_progress *, const struct ot_archive_verify *, const oa_u8 *, unsigned);
#ifdef DOS_VERSION
#include "opentape/dos_ui.h"
struct ot_verify_progress_view {
    struct ot_verify_progress progress;
    const char *device;
    ot_dos_ui_line line;
    unsigned cataloguing, initialized, failed;
};
void ot_dos_verify_progress(void *, const struct ot_archive_verify *, const oa_u8 *, unsigned);
#endif
#endif

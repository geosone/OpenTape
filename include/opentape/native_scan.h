#ifndef OPENTAPE_NATIVE_SCAN_H
#define OPENTAPE_NATIVE_SCAN_H

#include "opentape/volume_format.h"

enum ot_scan_result {
    OT_SCAN_BAD_RECORD = -40,
    OT_SCAN_RECORD_CRC = -41,
    OT_SCAN_BAD_SEQUENCE = -42,
    OT_SCAN_MISSING_LABEL = -43,
    OT_SCAN_BAD_LABEL = -44
};

/* Strict sequential framing validation, not file/session recovery.
 * Initialize to zero at BOT. A rejected block never changes this state. */
struct ot_native_scan {
    oa_u32 blocks;
    oa_u32 records;
    unsigned have_label;
    struct ot_volume_label label;
    struct ot_block_header last_block;
};

int ot_native_scan_block(struct ot_native_scan *scan,
                          const oa_u8 *block, oa_u16 size);

#endif

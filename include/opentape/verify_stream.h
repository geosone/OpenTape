#ifndef OPENTAPE_VERIFY_STREAM_H
#define OPENTAPE_VERIFY_STREAM_H
#include "opentape/tape_reader.h"
#include "opentape/archive_verify.h"
#include "opentape/record_payload.h"
#define OT_VERIFY_STREAM_LIMIT (-120)
#define OT_VERIFY_VISITOR_FAILED (-121)
/* Called only after the complete containing block passed verification.
 * Metadata is provisional until session/filemark completion; do not retain
 * the pointer or reenter. A nonzero result stops input immediately. */
typedef int (*ot_metadata_visitor)(void *, oa_u32 session, oa_u32 object,
                                  const struct ot_file_metadata *);
/* Read-only observer after complete block verification and successful metadata
 * visitors. Borrowed state/block remain valid only during the call. No filesystem/tape I/O,
 * mutation, retained pointers or reentry (including session constructors).
 * There is no filemark/finish/close event and no success authority. */
typedef void (*ot_verify_observer)(void *, const struct ot_archive_verify *, const oa_u8 *, unsigned);
int ot_verify_stream_observed(struct ot_tape_reader *, struct ot_archive_verify *,
    oa_u8 *, unsigned, oa_u32 limit, ot_metadata_visitor, void *, ot_verify_observer, void *);
/* Reader is already open, raw variable-block and positioned at the label;
 * verifier state must be zero initialized there.
 * limit=0 reads until the first filemark. The caller owns close and buffers.
 * A limit never implies completeness; no read is issued beyond that limit. */
int ot_verify_stream(struct ot_tape_reader *, struct ot_archive_verify *,
                       oa_u8 *, unsigned capacity, oa_u32 limit);
int ot_verify_stream_visit(struct ot_tape_reader *, struct ot_archive_verify *,
                       oa_u8 *, unsigned capacity, oa_u32 limit, ot_metadata_visitor, void *);

/* One complete, synchronous read-only session attempt. Owns open/close, never
 * rewinds or retries. Buffer remains caller-owned. Result reader is diagnostic
 * only and is always closed on return. Counts/visitor metadata are provisional
 * unless the return value is zero, including successful close. Do not reenter. */
struct ot_verify_session_result {
    struct ot_tape_reader reader;
    struct ot_archive_verify verify;
    unsigned transport_error;
};
int ot_verify_session(const struct ot_reader_ops *, const char *, oa_u8 *, unsigned,
                      ot_metadata_visitor, void *, struct ot_verify_session_result *);
/* Run and close a reader already opened by a selected platform backend.
 * Invalid arguments leave ownership with the caller. */
int ot_verify_session_opened(oa_u8 *, unsigned, ot_metadata_visitor, void *, struct ot_verify_session_result *);
/* Observed equivalents preserve the ordinary session ownership/error rules.
 * A failed visitor, block, read or close cannot create a completion event. */
int ot_verify_session_observed(const struct ot_reader_ops *, const char *, oa_u8 *, unsigned,
    ot_metadata_visitor, void *, ot_verify_observer, void *, struct ot_verify_session_result *);
int ot_verify_session_opened_observed(oa_u8 *, unsigned, ot_metadata_visitor, void *,
    ot_verify_observer, void *, struct ot_verify_session_result *);
#endif

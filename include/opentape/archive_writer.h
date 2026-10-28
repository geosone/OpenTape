#ifndef OPENTAPE_ARCHIVE_WRITER_H
#define OPENTAPE_ARCHIVE_WRITER_H

#include "opentape/archive_verify.h"
#include "opentape/record_payload.h"
#define OT_WRITER_RECORD_OVERHEAD (OT_BLOCK_HEADER_SIZE + OT_RECORD_HEADER_SIZE)

enum ot_writer_result {
    OT_WRITER_BAD_ARGUMENT = -80,
    OT_WRITER_FAILED = -81,
    OT_WRITER_SINK_ERROR = -82,
    OT_WRITER_SHORT_WRITE = -83,
    OT_WRITER_BAD_COUNT = -84
};

/* One callback = one physical variable record; no automatic retries.
 * finish_session must finalize/flush the session and report delayed errors.
 * Callbacks must not retain the buffer or reenter the writer. */
struct ot_writer_sink {
    void *context;
    int (*write_block)(void *, const oa_u8 *, unsigned, unsigned *);
    int (*finish_session)(void *);
};

/* Notification after a whole record is accepted and verifier state commits.
 * Transient borrowed arguments, no return value or control over transport.
 * Called with writer busy: no retention, reentry, source/tape I/O or mutation. */
typedef void (*ot_writer_observer)(void *, const struct ot_archive_verify *,
                                    const struct ot_record_header *, const oa_u8 *);

/* Zero-initialize before init; caller owns a separate buffer and sink for the lifetime.
 * A failed sink permanently poisons this writer, including finalization. */
struct ot_archive_writer {
    struct ot_archive_verify verified;
    const struct ot_writer_sink *sink;
    oa_u8 *buffer;
    oa_u16 capacity;
    unsigned initialized, failed, busy, pending_finish, last_accepted;
    int sink_error;
    ot_writer_observer observer;
    void *observer_context;
};

int ot_archive_writer_init(struct ot_archive_writer *writer,
                            const struct ot_writer_sink *sink, oa_u8 *buffer, oa_u16 capacity);
/* Attach/detach only before the first record. Reentrant/late changes fail. */
int ot_archive_writer_observe(struct ot_archive_writer *, ot_writer_observer, void *);
int ot_archive_writer_volume(struct ot_archive_writer *writer, const struct ot_volume_label *label);
int ot_archive_writer_session(struct ot_archive_writer *writer, oa_u32 session_id,
                               const struct ot_session_start *start);
int ot_archive_writer_metadata(struct ot_archive_writer *writer, oa_u32 object_id,
                                const struct ot_file_metadata *metadata);
int ot_archive_writer_data(struct ot_archive_writer *writer, const oa_u8 *data, oa_u16 size);
int ot_archive_writer_file_end(struct ot_archive_writer *writer);
int ot_archive_writer_session_end(struct ot_archive_writer *writer);
/* Closed, sink-finalized session prefix only; not whole-volume/readback proof. */
int ot_archive_writer_complete(const struct ot_archive_writer *writer);
#endif

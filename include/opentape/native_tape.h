#ifndef OPENTAPE_NATIVE_TAPE_H
#define OPENTAPE_NATIVE_TAPE_H
#include "openaspi/tape_api.h"
/* Older provider headers can still build the SDK. Removal controls need the
 * matching wire enums/capability bits; never map an unknown CLI verb to READY. */
#ifdef OT_TAPE_CONTROL_LOCK
#define OT_NATIVE_TAPE_REMOVAL 1
#endif

#ifdef OT_TAPE_API_POSITION
#define OT_NATIVE_TAPE_POSITION 1
/* One-shot position observation. Coordinate validity and transfer provenance
 * are part of the returned packet, separately from successful delivery. */
#endif

#ifdef OT_TAPE_API_LOCATE
#define OT_NATIVE_TAPE_LOCATE 1
#endif

#ifdef OT_TAPE_OP_POSITION64
#define OT_NATIVE_TAPE_WIDE 1
#endif

#ifdef OT_TAPE_CONTROL_SPACE_SETMARKS
#define OT_NATIVE_TAPE_EXTENDED_SPACE 1
#endif

/* Classify the noncontiguous SPACE enums supported by the selected headers. */
int ot_native_space_operation(oa_u16);

#ifdef OT_TAPE_CONTROL_SETMARK
#define OT_NATIVE_TAPE_WRITE_SETMARK 1
#endif

enum ot_native_result {
    OT_NATIVE_OK = 0,
    OT_NATIVE_ARGUMENT = -200,
    OT_NATIVE_OPEN = -201,
    OT_NATIVE_DEVICE = -202,
    OT_NATIVE_IOCTL = -203,
    OT_NATIVE_RESPONSE = -204,
    OT_NATIVE_CLOSE = -205,
    OT_NATIVE_PARTIAL = -206,
    OT_NATIVE_BUSY = -207,
    OT_NATIVE_UNSUPPORTED = -208,
    OT_NATIVE_COMMAND = -209,
    OT_NATIVE_UNFINISHED = -210
};

/* Successful read with a stream condition; the validated record contains
 * both actual bytes and FILEMARK/SETMARK/EOM/ILI/EOD (as supported by provider
 * headers). Setmarks are not archive EOF. Never infer EOF from zero alone.
 */
#define OT_NATIVE_STREAM 1

/* One synchronous session per context. Platform handles stay in the context,
 * so the shared interface does not encode DOS descriptors or NT pointers.
 * Callbacks return a native platform error (zero on success); is_device
 * returns a boolean through its output. No fallback or retry is implicit.
 */
struct ot_native_ops {
    void *context;
    int (*valid_name)(void *, const char *);
    unsigned (*open_readonly)(void *, const char *);
    unsigned (*is_device)(void *, unsigned *);
    unsigned (*query)(void *, struct ot_tape_info_v1 *);
    unsigned (*close)(void *);
};
#ifdef OT_TAPE_API_ALERTS
#define OT_NATIVE_TAPE_ALERTS 1
/* Explicit retrieval can acknowledge device warnings; flags are mandatory. */
struct ot_native_alerts_ops {
    struct ot_native_ops query;
    unsigned (*alerts)(void *,struct ot_tape_alerts_v1 *);
};
int ot_native_alerts(const struct ot_native_alerts_ops *,const char *,oa_u16,
                      struct ot_tape_alerts_v1 *,unsigned *);
#ifdef DOS_VERSION
int ot_dos_native_alerts(const char *,oa_u16,struct ot_tape_alerts_v1 *,unsigned *);
#endif
#endif

#ifdef OT_TAPE_API_PARTITIONS
#define OT_NATIVE_TAPE_PARTITIONS 1
/* Reported MODE SENSE layout can be pending; it is not a LOCATE whitelist. */
struct ot_native_partitions_ops {
    struct ot_native_ops query;
    unsigned (*partitions)(void *, struct ot_tape_partitions_v1 *);
};
int ot_native_partitions(const struct ot_native_partitions_ops *, const char *,
                         struct ot_tape_partitions_v1 *, unsigned *);
#ifdef DOS_VERSION
int ot_dos_native_partitions(const char *, struct ot_tape_partitions_v1 *, unsigned *);
#endif
#endif

#ifdef OT_NATIVE_TAPE_POSITION
struct ot_native_position_ops {
    struct ot_native_ops query;
    unsigned (*position)(void *, struct ot_tape_position_v1 *);
};
int ot_native_position(const struct ot_native_position_ops *, const char *,
                       struct ot_tape_position_v1 *, unsigned *);
#ifdef DOS_VERSION
int ot_dos_native_position(const char *, struct ot_tape_position_v1 *, unsigned *);
#endif
#endif

#ifdef OT_NATIVE_TAPE_LOCATE
/* Explicit logical movement. Flags select current/explicit partition; no
 * automatic retry, emulation or recovery after an uncertain completion. */
struct ot_native_locate_ops {
    struct ot_native_ops query;
    unsigned (*locate)(void *, struct ot_tape_locate_v1 *);
};
int ot_native_locate(const struct ot_native_locate_ops *, const char *, oa_u16 flags,
                     oa_u32 partition, oa_u32 block, struct ot_tape_locate_v1 *, unsigned *);
#ifdef DOS_VERSION
int ot_dos_native_locate(const char *, oa_u16, oa_u32, oa_u32, struct ot_tape_locate_v1 *, unsigned *);
#endif
#endif

#ifdef OT_NATIVE_TAPE_WIDE
struct ot_native_position64_ops {
    struct ot_native_ops query;
    unsigned (*position)(void *,struct ot_tape_position64_v1 *);
};
struct ot_native_locate64_ops {
    struct ot_native_ops query;
    unsigned (*locate)(void *,struct ot_tape_locate64_v1 *);
};
int ot_native_position64(const struct ot_native_position64_ops *,const char *,struct ot_tape_position64_v1 *,unsigned *);
int ot_native_locate64(const struct ot_native_locate64_ops *,const char *,oa_u16,oa_u32,oa_u32,oa_u32,struct ot_tape_locate64_v1 *,unsigned *);
#ifdef DOS_VERSION
int ot_dos_native_position64(const char *,struct ot_tape_position64_v1 *,unsigned *);
int ot_dos_native_locate64(const char *,oa_u16,oa_u32,oa_u32,oa_u32,struct ot_tape_locate64_v1 *,unsigned *);
#endif
#endif

struct ot_native_control_ops {
    struct ot_native_ops query;
    unsigned (*control)(void *, struct ot_tape_control_v1 *);
};

struct ot_native_reader_ops {
    struct ot_native_ops query;
    unsigned (*read)(void *, void *buffer, unsigned capacity,
                      struct ot_tape_record_v1 *);
};
/* Zero-initialize before first open. The callback context and ops must outlive
 * the open reader. After a read failure, close explicitly; no record replay.
 * A validated error packet may retain partial data/count alongside its error.
 */
struct ot_native_reader {
    const struct ot_native_reader_ops *ops;
    unsigned opened, failed, platform_error;
    oa_u32 block_size; /* zero variable, otherwise explicit bytes per physical block */
    struct ot_tape_record_v1 last;
};
int ot_native_reader_open(struct ot_native_reader *, const struct ot_native_reader_ops *, const char *);
int ot_native_reader_open_fixed(struct ot_native_reader *, const struct ot_native_reader_ops *, const char *, oa_u32);
int ot_native_reader_read(struct ot_native_reader *, void *, unsigned capacity);
int ot_native_reader_close(struct ot_native_reader *);

struct ot_native_writer_ops {
    struct ot_native_control_ops control;
    unsigned (*open_write)(void *, const char *);
    unsigned (*write)(void *, const void *, unsigned,
                       struct ot_tape_record_v1 *);
};
/* Explicit BEGIN -> WRITE/FLUSH/FILEMARK -> END -> CLOSE. Closing an unfinished
 * writer only aborts ownership; it never writes implicit records or marks.
 * Keep the first failed operation and its diagnostics through cleanup.
 */
struct ot_native_writer {
    const struct ot_native_writer_ops *ops;
    unsigned opened, claimed, unfinished, platform_error;
    int failure;
    oa_u32 block_size; /* zero variable, otherwise explicit bytes per physical block */
    oa_u32 operations; /* provider capabilities observed on this writer handle */
    struct ot_tape_record_v1 last_record;
    struct ot_tape_control_v1 last_control;
};
int ot_native_writer_open(struct ot_native_writer *, const struct ot_native_writer_ops *, const char *);
int ot_native_writer_open_fixed(struct ot_native_writer *, const struct ot_native_writer_ops *, const char *, oa_u32);
int ot_native_writer_write(struct ot_native_writer *, const void *, unsigned);
int ot_native_writer_flush(struct ot_native_writer *);
int ot_native_writer_filemark(struct ot_native_writer *);
#ifdef OT_NATIVE_TAPE_WRITE_SETMARK
/* Exactly one setmark in an active writer; unsupported provider does no I/O. */
int ot_native_writer_setmark(struct ot_native_writer *);
#endif
int ot_native_writer_finish(struct ot_native_writer *);
int ot_native_writer_abort(struct ot_native_writer *);
int ot_native_writer_close(struct ot_native_writer *);

/* Output is cleared before opening; only validated native responses are
 * copied out, including PARTIAL/BUSY/command-error diagnostics. A successful
 * open gets exactly one close attempt. The first failure takes precedence.
 */
int ot_native_probe(const struct ot_native_ops *, const char *, oa_u16 flags,
                    struct ot_tape_info_v1 *, unsigned *platform_error);
/* READY/REWIND, explicit LOAD/UNLOAD and removal LOCK/UNLOCK.
 * LOCK controls cartridge removal, not writer ownership or write protection.
 * Movement counts use ot_native_space. */
int ot_native_execute(const struct ot_native_control_ops *, const char *,
                      oa_u16 operation, struct ot_tape_control_v1 *, unsigned *);
/* Bounded signed decimal input for the shared CLI/UI SPACE count editor.
 * Accept -8388608..8388607; malformed input preserves the output, with no I/O. */
int ot_native_space_count_parse(const char *, oa_s32 *);
/* One-shot signed SPACE, with capability/ownership checks and echoed count.
 * No fallback, retry or inferred absolute position. */
int ot_native_space(const struct ot_native_control_ops *, const char *, oa_u16,
                    oa_s32, struct ot_tape_control_v1 *, unsigned *);
#ifdef DOS_VERSION
int ot_dos_native_space(const char *, oa_u16, oa_s32, struct ot_tape_control_v1 *, unsigned *);
int ot_dos_native_probe(const char *, oa_u16, struct ot_tape_info_v1 *, unsigned *);
int ot_dos_native_execute(const char *, oa_u16, struct ot_tape_control_v1 *, unsigned *);
/* DOS reader and its callbacks are owned by the caller for the whole session. */
struct ot_dos_native_session { unsigned handle; };
struct ot_dos_native_reader {
    struct ot_native_reader reader;
    struct ot_native_reader_ops ops;
    struct ot_dos_native_session session;
};
int ot_dos_native_reader_open(struct ot_dos_native_reader *, const char *);
int ot_dos_native_reader_open_fixed(struct ot_dos_native_reader *, const char *, oa_u32);
int ot_dos_native_reader_read(struct ot_dos_native_reader *, void *, unsigned);
int ot_dos_native_reader_close(struct ot_dos_native_reader *);
struct ot_dos_native_writer {
    struct ot_native_writer writer;
    struct ot_native_writer_ops ops;
    struct ot_dos_native_session session;
};
int ot_dos_native_writer_open(struct ot_dos_native_writer *, const char *);
int ot_dos_native_writer_open_fixed(struct ot_dos_native_writer *, const char *, oa_u32);
int ot_dos_native_writer_write(struct ot_dos_native_writer *, const void *, unsigned);
int ot_dos_native_writer_flush(struct ot_dos_native_writer *);
int ot_dos_native_writer_filemark(struct ot_dos_native_writer *);
#ifdef OT_NATIVE_TAPE_WRITE_SETMARK
int ot_dos_native_writer_setmark(struct ot_dos_native_writer *);
#endif
int ot_dos_native_writer_finish(struct ot_dos_native_writer *);
int ot_dos_native_writer_abort(struct ot_dos_native_writer *);
int ot_dos_native_writer_close(struct ot_dos_native_writer *);
int ot_dos_native_command(int, char **);
#endif
#endif

#ifndef OPENTAPE_RESTORE_H
#define OPENTAPE_RESTORE_H
#include "opentape/archive_verify.h"
#include "opentape/record_payload.h"
#define OT_RESTORE_IO (-130)
#define OT_RESTORE_PATH (-131)
#define OT_RESTORE_LIMIT (-132)
#define OT_RESTORE_FAILED (-133)
#define OT_RESTORE_MAX_ENTRIES 256u
#define OT_RESTORE_TEMP "OT$PART.TMP"
#define OT_RESTORE_OLD "OT$OLD.TMP"
#define OT_RESTORE_SKIP 65534u
#define OT_RESTORE_SELECTION_MAX 256u
#define OT_RESTORE_SELECTION_BYTES 131072ul
int ot_restore_selection_valid(const char *const *, unsigned);
unsigned ot_restore_selected(const char *const *, unsigned, const char *);
struct ot_restore_selection {
    const char *paths[OT_RESTORE_SELECTION_MAX];
    unsigned count;
};
struct ot_source_ops;
/* Zero initialize. Load closes an already-open source once and rolls back
 * entries it added on failure. Free all owned entries after use. */
int ot_restore_selection_add(struct ot_restore_selection *, const char *);
int ot_restore_selection_load(struct ot_restore_selection *, const struct ot_source_ops *);
void ot_restore_selection_free(struct ot_restore_selection *);
struct ot_selection_output {
    void *context;
    unsigned (*write)(void *, const oa_u8 *, unsigned, unsigned *);
    unsigned (*close)(void *);
    unsigned (*publish)(void *);
};
/* Already-open exclusive staging file. Close once even after write failure;
 * publish only after every exact write and close succeeds. No retry/cleanup. */
int ot_restore_selection_save(const struct ot_restore_selection *, const struct ot_selection_output *);
struct ot_restore_exchange_ops {
    void *context;
    unsigned (*attributes)(void *, const char *, unsigned);
    unsigned (*rename)(void *, const char *, const char *);
    unsigned (*remove)(void *, const char *);
};
/* Caller checked destination identity/permission and absence of previous.
 * Paths must be distinct and confined. Staging data/time/close already passed.
 * Old data is removed only after new publication/attributes succeed. */
unsigned ot_restore_exchange(const struct ot_restore_exchange_ops *, const char *stage,
    const char *destination, const char *previous, unsigned replacing,
    unsigned old_attributes, unsigned new_attributes, unsigned *recovery_error);
struct ot_restore_ops {
    /* Synchronous callbacks; do not retain buffers or reenter the core. */
    void *context;
    unsigned (*directory)(void *, const struct ot_file_metadata *);
    unsigned (*begin)(void *, const struct ot_file_metadata *);
    unsigned (*write)(void *, const oa_u8 *, unsigned, unsigned *);
    unsigned (*commit)(void *, const struct ot_file_metadata *);
    unsigned (*abort)(void *);
};
struct ot_restore_entry;
struct ot_restore;
/* Successful block observation after archive validation and all output callbacks.
 * Borrowed snapshot; no reentry, mutation, file/device I/O or retained pointers.
 * Published files can still precede later archive/filemark/close failure. */
typedef void (*ot_restore_observer)(void *, const struct ot_restore *);
/* Zero initialize. abort/free after any outcome; no rollback or automatic retry.
 * Backend must stage files, enforce per-file replacement policy and confine paths.
 * begin may return OT_RESTORE_SKIP: input is still verified without output. */
struct ot_restore {
    struct ot_archive_verify verified;
    const struct ot_restore_ops *ops;
    struct ot_restore_entry *entries;
    struct ot_file_metadata active;
    oa_u32 restored_files, restored_directories, skipped_files;
    oa_u32 excluded_files;
    /* FILE_DATA bytes in fully processed blocks, across session boundaries. */
    oa_u32 checked_low, checked_high;
    /* Current/last regular-file bytes, retained across session headers. */
    oa_u32 active_bytes;
    const char *const *filter;
    unsigned filter_count, filter_enabled, excluding;
    oa_u8 filter_seen[OT_RESTORE_SELECTION_MAX];
    unsigned count, failed, staging, finished, backend_error, busy, skipping;
    ot_restore_observer observer;
    void *observer_context;
};
int ot_restore_init(struct ot_restore *, const struct ot_restore_ops *);
/* Optional nonempty exact-file filter, configured once before any block.
 * Caller retains immutable paths until free. Does not grant replacement rights.
 * Directory callbacks are suppressed; selected-file begin creates its parents.
 * All archive records, paths and CRCs are still verified, including exclusions. */
int ot_restore_set_filter(struct ot_restore *, const char *const *, unsigned);
/* Attach/detach only before the first archive block. No completion callback. */
int ot_restore_observe(struct ot_restore *, ot_restore_observer, void *);
int ot_restore_block(struct ot_restore *, const oa_u8 *, oa_u16);
int ot_restore_finish(struct ot_restore *);
void ot_restore_free(struct ot_restore *);

#ifdef DOS_VERSION
struct ot_dos_selection_export {
    char destination[128], temporary[128];
    unsigned handle, dos_error;
};
/* Prepare is read-only; save creates staging only after archive verification. */
int ot_dos_selection_prepare(struct ot_dos_selection_export *, const char *);
int ot_dos_selection_save(struct ot_dos_selection_export *, const struct ot_restore_selection *);
/* New-file-only bounded blob output using the same prepared staging paths.
 * Exact write/close and independent source readback precede rename. Failed
 * staging is retained for inspection; existing targets are never replaced. */
int ot_dos_file_save_blob(struct ot_dos_selection_export *, const oa_u8 *, unsigned);
/* Exclusive streaming staging with checked close and independent readback before
 * publication. Owns one handle; failed staging is retained. Not tape output. */
struct ot_dos_file_stream {
    struct ot_dos_selection_export file;
    struct ot_selection_output ops;
    oa_u32 bytes, crc;
    unsigned opened;
    int result;
};
int ot_dos_file_stream_open(struct ot_dos_file_stream *, const char *);
struct ot_dos_restore {
    char root[128], temporary[128], previous[128];
    char checked_path[128];
    unsigned handle, opened, dos_error, created, preview, comparing, quiet, root_created;
    const char *const *selection;
    unsigned selection_count, replacing, original_attributes, replaced, recovery_error;
    oa_u32 output_crc;
    unsigned readback_files;
    int readback_error;
    oa_u8 selection_seen[OT_RESTORE_SELECTION_MAX];
    struct ot_restore_ops ops;
};
int ot_dos_restore_create(struct ot_dos_restore *, const char *);
int ot_dos_restore_open(struct ot_dos_restore *, const char *, const char *const *, unsigned);
/* Read-only destination inspection; no mkdir, staging, writes or attributes.
 * created means prepared, not necessarily existing, for this mode only. */
int ot_dos_restore_preview(struct ot_dos_restore *, const char *, const char *const *, unsigned);
/* Quiet, read-only preview; an existing root is allowed without granting
 * replacement permission. All existing regular files remain KEEP. */
int ot_dos_restore_inspect(struct ot_dos_restore *, const char *);
/* Shared, non-wire result for the confirmed DOS batches. A preview counts
 * planned output files; a writable batch counts published files; comparison counts
 * successful size/CRC/EOF file checks. Counts remain provisional until
 * result==0, including session completion and close. Diagnostics survive cleanup. */
struct ot_dos_restore_result {
    int result;
    unsigned tape_error, disk_error, unmatched;
    oa_u32 files, skipped, excluded;
    /* destination_created means mkdir succeeded, not an existing root reused.
     * output_prepared means the real writable backend was prepared (including
     * reused roots); both are zero for preview. replaced is planned/published.
     * readback_files counts successful staging/existing-file checks, even if publication or
     * a later archive/session check fails. checked_path retains full DOS path. */
    unsigned destination_created, output_prepared, replaced, readback_files, recovery_error;
    int readback_error;
    char checked_path[128];
};
/* Explicit batch: validate destination/filter, rewind once, then verify/read to
 * first filemark and close. No console output, destination writes or retry.
 * Caller confirms tape movement and holds immutable nonempty selected paths.
 * Counts are provisional unless result==0; every selected path must be seen. */
int ot_dos_preview_selected(const char *, const char *, const struct ot_restore_selection *,
                            struct ot_dos_restore_result *);
/* Confirmed writable counterpart: same checked rewind/read batch and exact
 * filter, but requires a NEW root and stages/readbacks/publishes selected files.
 * Existing roots/files are never granted replacement rights. Preparation is
 * read-only; root creation waits until rewind and tape open/mode checks pass.
 * On failure published files and staging may remain; never retry implicitly.
 * The result retains disk/readback diagnostics after abort and tape close. */
int ot_dos_restore_selected(const char *, const char *, const struct ot_restore_selection *,
                            struct ot_dos_restore_result *);
/* Explicit replacement variants. Optional immutable replacement paths must be
 * a subset of the extraction filter. A nonempty list allows an existing root;
 * only named files may be replaced, using the shared staging/readback/exchange
 * callbacks. Empty replacement input retains the selected-only service policy.
 * Preview remains read-only and reports planned replacements in result.replaced.
 * Caller must separately confirm replacement intent for each writable batch. */
int ot_dos_preview_replacing(const char *, const char *, const struct ot_restore_selection *,
                            const struct ot_restore_selection *, struct ot_dos_restore_result *);
int ot_dos_restore_replacing(const char *, const char *, const struct ot_restore_selection *,
                            const struct ot_restore_selection *, struct ot_dos_restore_result *);
/* Confirmed read-only batch: inspect an existing root, rewind once, compare
 * selected file lengths/CRCs/exact EOF and verify the complete archive prefix.
 * Stops at the first mismatch/I/O error. No replacement input, writes or retry.
 * Counts remain provisional until result==0, including tape close. Retains the
 * full checked path and source error; output_prepared/destination_created stay 0. */
int ot_dos_compare_selected(const char *, const char *, const struct ot_restore_selection *,
                            struct ot_dos_restore_result *);
enum ot_dos_file_action { OT_DOS_FILE_PREVIEW, OT_DOS_FILE_RESTORE, OT_DOS_FILE_COMPARE };
/* Same explicit selected batch with optional read-only observation. Compare
 * accepts no replacement authority. Result after close/cleanup is authoritative. */
int ot_dos_selected_observed(const char *, const char *, const struct ot_restore_selection *,
    const struct ot_restore_selection *, unsigned action, ot_restore_observer, void *, struct ot_dos_restore_result *);
/* Existing root only; content comparison with no replacement authority/writes. */
int ot_dos_restore_compare(struct ot_dos_restore *, const char *);
#endif
#endif

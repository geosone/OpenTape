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
    const char *const *filter;
    unsigned filter_count, filter_enabled, excluding;
    oa_u8 filter_seen[OT_RESTORE_SELECTION_MAX];
    unsigned count, failed, staging, finished, backend_error, busy, skipping;
};
int ot_restore_init(struct ot_restore *, const struct ot_restore_ops *);
/* Optional nonempty exact-file filter, configured once before any block.
 * Caller retains immutable paths until free. Does not grant replacement rights.
 * Directory callbacks are suppressed; selected-file begin creates its parents.
 * All archive records, paths and CRCs are still verified, including exclusions. */
int ot_restore_set_filter(struct ot_restore *, const char *const *, unsigned);
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
struct ot_dos_restore {
    char root[128], temporary[128], previous[128];
    char checked_path[128];
    unsigned handle, opened, dos_error, created, preview, comparing;
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
/* Existing root only; content comparison with no replacement authority/writes. */
int ot_dos_restore_compare(struct ot_dos_restore *, const char *);
#endif
#endif

#ifndef OPENTAPE_BACKUP_PLAN_H
#define OPENTAPE_BACKUP_PLAN_H
#include "opentape/source_file.h"
#define OT_PLAN_LIMIT (-110)
#define OT_PLAN_DUPLICATE (-111)
#define OT_PLAN_MAX_ENTRIES 256u
#define OT_PLAN_MAX_DEPTH 16u
struct ot_plan_entry {
    struct ot_plan_entry *next;
    char source[128];
    struct ot_file_metadata metadata;
};
struct ot_backup_plan {
    struct ot_plan_entry *first, *last;
    unsigned count;
};
struct ot_plan_ops {
    void *context;
    int (*directory)(void *, const struct ot_plan_entry *);
    int (*open_file)(void *, const struct ot_plan_entry *, struct ot_file_metadata *, struct ot_source_ops *);
};
/* Zero initialize; free on all paths. No output occurs while building a plan.
 * Reject case-insensitive duplicate targets and any file used as an ancestor
 * of another file/directory. Failure leaves the existing plan unchanged. */
int ot_plan_add(struct ot_backup_plan *, const char *, const struct ot_file_metadata *);
void ot_plan_free(struct ot_backup_plan *);
struct ot_plan_summary { oa_u32 files, directories, bytes_low, bytes_high; };
/* Call after successful plan construction. Validate list/metadata and count
 * without opening sources or output.
 * Payload bytes use a checked 64-bit pair, independent of archive overhead.
 * Failure preserves the caller's summary. No ownership or I/O side effects. */
int ot_plan_summarize(const struct ot_backup_plan *, struct ot_plan_summary *);
int ot_plan_emit(const struct ot_backup_plan *, const struct ot_plan_ops *,
                  struct ot_archive_writer *, oa_u8 *, unsigned);
#ifdef DOS_VERSION
int ot_dos_plan_tree(struct ot_backup_plan *, const char *, const char *, unsigned *);
void ot_dos_plan_ops(struct ot_plan_ops *, struct ot_dos_source *);
#endif
#endif

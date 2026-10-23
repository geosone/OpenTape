#ifndef OPENTAPE_ARCHIVE_CATALOG_H
#define OPENTAPE_ARCHIVE_CATALOG_H
#include "opentape/verify_stream.h"
#define OT_CATALOG_MAX_ENTRIES 256u
#define OT_CATALOG_LIMIT (-160)
#define OT_CATALOG_MEMORY (-161)
#define OT_CATALOG_ARGUMENT (-162)
#define OT_CATALOG_AMBIGUOUS (-163)
struct ot_catalog_entry {
    struct ot_catalog_entry *next;
    oa_u32 session, object;
    unsigned marked;
    struct ot_file_metadata metadata;
};
struct ot_catalog {
    struct ot_catalog_entry *first, *last;
    unsigned count, marked;
    int error;
};
/* Zero initialize. Visitor copies transient metadata; everything remains
 * provisional until the enclosing ot_verify_session succeeds, including close.
 * A failure is sticky. Free on every path; no disk output or restore authority. */
int ot_catalog_capture(void *, oa_u32, oa_u32, const struct ot_file_metadata *);
const struct ot_catalog_entry *ot_catalog_at(const struct ot_catalog *, unsigned);
/* Memory-only inspection marks, per entry (session/object), not path-based
 * extraction/replacement permission. Directories cannot be marked. Repeated
 * requests are idempotent. Invalid requests never poison capture error state. */
int ot_catalog_mark(struct ot_catalog *, unsigned index, unsigned marked);
int ot_catalog_mark_all(struct ot_catalog *, unsigned marked);
struct ot_restore_selection;
/* Copy marked paths into an initially empty owned selection. Reject empty
 * marks and any marked path matching another entry (DOS case-insensitive),
 * because a plain path list cannot identify session/object versions. Roll back
 * on failure. No filesystem/tape I/O; marks remain unchanged. */
int ot_catalog_selection(const struct ot_catalog *, struct ot_restore_selection *);
void ot_catalog_free(struct ot_catalog *);
#endif

#ifndef OPENTAPE_ARCHIVE_CATALOG_H
#define OPENTAPE_ARCHIVE_CATALOG_H
#include "opentape/verify_stream.h"
#define OT_CATALOG_MAX_ENTRIES 256u
#define OT_CATALOG_LIMIT (-160)
#define OT_CATALOG_MEMORY (-161)
#define OT_CATALOG_ARGUMENT (-162)
#define OT_CATALOG_AMBIGUOUS (-163)
#define OT_CATALOG_MISSING (-164)
struct ot_catalog_entry {
    struct ot_catalog_entry *next;
    oa_u32 session, object;
    unsigned marked, replace;
    struct ot_file_metadata metadata;
};
struct ot_catalog {
    struct ot_catalog_entry *first, *last;
    unsigned count, marked, replacements;
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
#define OT_CATALOG_TREE_TOGGLE 2u
struct ot_catalog_tree_result { unsigned files, selected; };
/* Select/deselect regular descendants of one directory in its own session.
 * marked=0/1 explicitly clears/adds; TOGGLE clears only if all are selected.
 * ASCII case-insensitive path boundary matching, not a bare prefix. No I/O.
 * Validate the complete snapshot before mutation; errors preserve all state
 * and result. Adding never grants replacement; clearing revokes only affected
 * grants. Directories stay unmarked; an empty tree succeeds with files=0. */
int ot_catalog_mark_tree(struct ot_catalog *, unsigned directory, unsigned marked,
                         struct ot_catalog_tree_result *);
/* Separate, per-entry intent for a later confirmed batch. Only a marked
 * regular file may receive it. Deselecting a file clears its replacement grant;
 * selecting/all never grants replacement. Clear leaves extraction marks intact. */
int ot_catalog_replace(struct ot_catalog *, unsigned index, unsigned replace);
void ot_catalog_clear_replacements(struct ot_catalog *);
struct ot_restore_selection;
/* Copy marked paths into an initially empty owned selection. Reject empty
 * marks and any marked path matching another entry (DOS case-insensitive),
 * because a plain path list cannot identify session/object versions. Roll back
 * on failure. No filesystem/tape I/O; marks remain unchanged. */
int ot_catalog_selection(const struct ot_catalog *, struct ot_restore_selection *);
/* Copy only replacement grants, with the same ambiguity/rollback checks.
 * An empty replacement list succeeds; it grants nothing. */
int ot_catalog_replacements(const struct ot_catalog *, struct ot_restore_selection *);
/* Replace extraction marks atomically from a nonempty, unique DOS path list.
 * Each name must identify exactly one regular entry (ASCII case-insensitive).
 * Unknown, directory, ambiguous or invalid input leaves ALL marks/grants intact.
 * Success clears every replacement grant and stores no pointers to the input.
 * No tape/filesystem I/O; the caller must hold a validated catalogue snapshot. Offline file metadata
 * may be marked but cannot authorize a tape operation. */
int ot_catalog_apply_selection(struct ot_catalog *, const struct ot_restore_selection *);
#ifdef DOS_VERSION
/* Load/read/recheck/close the regular list file with the shared CLI parser,
 * then apply atomically. Input is read-only; no tape access or retry. File,
 * allocation, parse, snapshot or close errors leave the catalogue unchanged. */
int ot_dos_catalog_import(struct ot_catalog *, const char *, unsigned *);
#endif
void ot_catalog_free(struct ot_catalog *);
#endif

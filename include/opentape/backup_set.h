#ifndef OPENTAPE_BACKUP_SET_H
#define OPENTAPE_BACKUP_SET_H
#include "opentape/backup_plan.h"
#define OT_SET_FORMAT (-230)
#define OT_SET_LIMIT (-231)
#define OT_SET_MAX_ROOTS 64u
#define OT_SET_MAX_BYTES 32768ul
/* A source manifest is configuration only. Each F/T line maps a regular file
 * or directory tree to a relative archive name below the backup's base path.
 * Nodes own both strings; no filesystem handle or write grant is retained. */
struct ot_backup_root {
    struct ot_backup_root *next;
    unsigned tree;
    char source[128], path[OT_FILE_PATH_MAX + 1u];
};
struct ot_backup_set { struct ot_backup_root *first, *last; unsigned count; };
/* Already-open regular source; valid ops close once on every outcome. Accept
 * blank/# comment lines, F/T source archivepath separated by spaces/tabs,
 * LF/CRLF and a final unterminated line. Reject controls/NUL/extra tokens,
 * oversized lines/files/root lists, wildcards, device stems and invalid paths.
 * EOF/recheck/close precede atomic replacement of an initialized owned set.
 * On failure all provisional nodes are freed and the old set stays intact. */
int ot_backup_set_load(struct ot_backup_set *, const struct ot_source_ops *);
void ot_backup_set_free(struct ot_backup_set *);
#ifdef DOS_VERSION
/* Build a complete owned plan before tape acquisition. Manifest paths resolve
 * against DOS's current directory (not the manifest directory). Each expanded
 * member uses the existing file/tree planner; collisions, source/close errors
 * or combined limits discard the whole provisional plan. Destination is empty.
 * No file payloads are read and no tape/device operation occurs here. */
int ot_dos_plan_set(struct ot_backup_plan *, const char *manifest, const char *base, unsigned *dos_error);
#endif
#endif

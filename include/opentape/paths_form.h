#ifndef OPENTAPE_PATHS_FORM_H
#define OPENTAPE_PATHS_FORM_H
#define OT_PATHS_FORM_MAX 40u
#define OT_PATHS_FORM_COUNT 5u
enum ot_paths_field { OT_PATH_PREVIEW, OT_PATH_RESTORE, OT_PATH_COMPARE,
                      OT_PATH_IMPORT, OT_PATH_EXPORT };
/* Owned session settings. An empty field disables that action; no configuration
 * change grants restore/replacement/export permission or performs any I/O. */
struct ot_paths_form { char names[OT_PATHS_FORM_COUNT][OT_PATHS_FORM_MAX + 1u]; };
/* Empty is valid (disabled); reject wildcards, controls and overlong text. */
int ot_paths_form_value_valid(const char *);
/* Initialize one field from argv; reject invalid/oversized input unchanged. */
int ot_paths_form_set(struct ot_paths_form *, unsigned, const char *);
int ot_paths_form_commit(struct ot_paths_form *, const struct ot_paths_form *);
/* Borrowed nonempty name or NULL for disabled/out-of-range. */
const char *ot_paths_form_name(const struct ot_paths_form *, unsigned);
#endif

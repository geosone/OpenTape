#ifndef OPENTAPE_BACKUP_FORM_H
#define OPENTAPE_BACKUP_FORM_H
#include "opentape/backup_dos.h"
#define OT_BACKUP_FORM_PATH_MAX 40u
#define OT_BACKUP_FORM_VOLUME_MAX 31u
/* Session-owned configuration, independent of argv and editor scratch storage.
 * A saved form grants no tape operation. No routine here performs I/O. */
struct ot_backup_form {
    char source[OT_BACKUP_FORM_PATH_MAX + 1u];
    char path[OT_BACKUP_FORM_PATH_MAX + 1u];
    char volume[OT_BACKUP_FORM_VOLUME_MAX + 1u];
    unsigned tree, configured;
};
void ot_backup_form_init(struct ot_backup_form *, const struct ot_dos_backup_request *);
/* Borrowed strings remain valid while the form lives and is unchanged.
 * Failure leaves the request untouched. Does not require configured=1. */
int ot_backup_form_request(const struct ot_backup_form *, struct ot_dos_backup_request *);
/* Validate before copying. Failure/cancel preserves the committed form. */
int ot_backup_form_commit(struct ot_backup_form *, const struct ot_backup_form *);
#endif

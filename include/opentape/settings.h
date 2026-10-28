#ifndef OPENTAPE_SETTINGS_H
#define OPENTAPE_SETTINGS_H
#include "opentape/backup_form.h"
#include "opentape/paths_form.h"
#define OT_SETTINGS_ARGUMENT (-180)
#define OT_SETTINGS_FORMAT (-181)
#define OT_SETTINGS_WIRE_SIZE 337u
/* Configuration only: no device/backend, marks, grants, positioning or action
 * permissions. An unconfigured backup has an empty source and configured=0. */
struct ot_settings {
    struct ot_backup_form backup;
    struct ot_paths_form paths;
};
/* Fixed little-endian codec: v1 FILE/TREE, v2 SET; same 337-byte layout,
 * canonical zero padding and two CRCs. Old readers safely reject v2.
 * Encode validates before touching output; decode commits only after every
 * field and checksum succeeds. There is no file/device I/O in these routines. */
int ot_settings_valid(const struct ot_settings *);
/* DOS short filename (optional local path), excluding tape-device stems even
 * with extensions: opening/closing a rewind alias must never be profile I/O. */
int ot_settings_filename_valid(const char *);
int ot_settings_encode(const struct ot_settings *, oa_u8 *, unsigned capacity);
int ot_settings_decode(struct ot_settings *, const oa_u8 *, unsigned size);
#ifdef DOS_VERSION
/* Read/recheck/close a regular file before committing; all failures retain
 * caller settings. Export refuses existing targets, stages and reads back
 * bytes before publication through the existing protected DOS file backend. */
int ot_dos_settings_load(const char *, struct ot_settings *, unsigned *dos_error);
int ot_dos_settings_save(const char *, const struct ot_settings *, unsigned *dos_error);
#endif
#endif

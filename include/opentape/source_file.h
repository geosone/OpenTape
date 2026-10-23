#ifndef OPENTAPE_SOURCE_FILE_H
#define OPENTAPE_SOURCE_FILE_H
#include "opentape/archive_writer.h"

#define OT_SOURCE_IO (-100)
#define OT_SOURCE_CHANGED (-101)
#define OT_SOURCE_ARGUMENT (-102)
#define OT_SOURCE_CONTENT (-103)

/* A source is already open and described by metadata before output starts.
 * Callbacks return zero on success. read may return short chunks, never more
 * than requested. recheck compares the current source with the open snapshot.
 * This operation always closes the source once after valid arguments. */
struct ot_source_ops {
    void *context;
    unsigned (*read)(void *, void *, unsigned, unsigned *);
    unsigned (*recheck)(void *);
    unsigned (*close)(void *);
};
int ot_backup_source(struct ot_archive_writer *, oa_u32 object,
                      const struct ot_file_metadata *, const struct ot_source_ops *,
                      oa_u8 *buffer, unsigned capacity);
/* Verify an already-open regular source against expected length and CRC.
 * Requires exact EOF, snapshot recheck and successful close; closes once after
 * valid arguments even on failure. No retry, writes or publication callbacks. */
int ot_source_verify(const struct ot_source_ops *, oa_u32, oa_u32, oa_u8 *, unsigned);

#ifdef DOS_VERSION
struct ot_dos_source {
    unsigned handle, opened, dos_error;
    struct ot_file_metadata metadata;
    struct ot_source_ops ops;
    char name[128];
};
int ot_dos_source_open(struct ot_dos_source *, const char *name, const char *archive_path);
#endif
#endif

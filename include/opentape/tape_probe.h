#ifndef OPENTAPE_TAPE_PROBE_H
#define OPENTAPE_TAPE_PROBE_H

#include "openaspi/aspitape.h"

enum ot_probe_result {
    OT_PROBE_OK = 0,
    OT_PROBE_BAD_ARGUMENT = -1,
    OT_PROBE_OPEN_FAILED = -2,
    OT_PROBE_NOT_DEVICE = -3,
    OT_PROBE_IOCTL_FAILED = -4,
    OT_PROBE_UNSUPPORTED_ABI = -5,
    OT_PROBE_CLOSE_FAILED = -6
};

/* All callbacks return zero on success and a DOS error number on failure.
 * No data or media-control callback is available to a read-only probe. */
struct ot_probe_ops {
    void *context;
    unsigned (*open_readonly)(void *, const char *, unsigned *);
    unsigned (*device_info)(void *, unsigned, unsigned *);
    unsigned (*status)(void *, unsigned, unsigned, void *, unsigned);
    unsigned (*close)(void *, unsigned);
};

struct ot_tape_status {
    struct oa_aspitape_mtget16 tape;
    unsigned extended;
};

/* Only no-rewind aliases, including -tX and -$ names, are accepted. */
int ot_probe_name_valid(const char *name);
int ot_probe_raw_name_valid(const char *name);
int ot_tape_query(const struct ot_probe_ops *ops, unsigned handle,
                  struct ot_tape_status *status, unsigned *dos_error);
int ot_tape_probe(const struct ot_probe_ops *ops, const char *name,
                  struct ot_tape_status *status, unsigned *dos_error);

#define OT_ENUM_EMPTY 1
#define OT_ENUM_VISITOR (-150)
#define OT_ENUM_PARTIAL (-151)
#define OT_DEVICE_FOUND 0u
#define OT_DEVICE_MISSING 1u
#define OT_DEVICE_REJECTED 2u
#define OT_DEVICE_ERROR 3u
struct ot_device_entry {
    char name[7];
    unsigned kind, dos_error;
    int result;
    struct ot_tape_status status;
};
struct ot_device_summary { unsigned examined, found, missing, rejected, errors; };
typedef int (*ot_device_visitor)(void *, const struct ot_device_entry *);
/* Inspect exactly four raw no-rewind aliases in one configured namespace.
 * Each successful open gets one close attempt before the visitor. No read/control callbacks,
 * selection, address deduplication or retry; status need not imply readiness.
 * Entry pointers are transient; copy entries if retained. A failed visitor stops
 * before the next alias. Summary retains partial counts. */
int ot_tape_enumerate(const struct ot_probe_ops *, char letter, unsigned dollar,
                       ot_device_visitor, void *, struct ot_device_summary *);

#define OT_REWIND_BAD_STATE (-140)
#define OT_REWIND_IO (-141)
/* Separate from the strictly status-only probe. Only a fixed MTREW request
 * is exposed here; no write, filemark, unload or arbitrary operation API. */
struct ot_rewind_ops {
    struct ot_probe_ops probe;
    unsigned (*rewind)(void *, unsigned, const struct oa_aspitape_mtop *);
};
/* Explicit synchronous rewind, raw no-rewind alias only. Close once after
 * opening, preserve first failure and never retry. Status is diagnostic even
 * on failure after a successful query; initialize it before calling. */
int ot_tape_rewind(const struct ot_rewind_ops *, const char *,
                   struct ot_tape_status *, unsigned *);

#ifdef DOS_VERSION
int ot_dos_tape_probe(const char *name, struct ot_tape_status *status,
                      unsigned *dos_error);
int ot_dos_tape_rewind(const char *, struct ot_tape_status *, unsigned *);
#endif

#endif

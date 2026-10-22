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
int ot_tape_query(const struct ot_probe_ops *ops, unsigned handle,
                  struct ot_tape_status *status, unsigned *dos_error);
int ot_tape_probe(const struct ot_probe_ops *ops, const char *name,
                  struct ot_tape_status *status, unsigned *dos_error);

#ifdef DOS_VERSION
int ot_dos_tape_probe(const char *name, struct ot_tape_status *status,
                      unsigned *dos_error);
#endif

#endif

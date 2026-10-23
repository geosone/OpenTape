#ifndef OPENTAPE_TAPE_OUTPUT_H
#define OPENTAPE_TAPE_OUTPUT_H

#include "opentape/tape_probe.h"
#include "opentape/archive_writer.h"

enum ot_output_result {
    OT_OUTPUT_BAD_ARGUMENT = -90,
    OT_OUTPUT_NOT_AUTHORIZED = -91,
    OT_OUTPUT_BAD_MODE = -92,
    OT_OUTPUT_IO_ERROR = -93,
    OT_OUTPUT_FAILED = -94
};

struct ot_output_ops {
    struct ot_probe_ops probe;
    unsigned (*open_write)(void *, const char *, unsigned *);
    unsigned (*set_raw)(void *, unsigned, unsigned);
    unsigned (*write)(void *, unsigned, const void *, unsigned, unsigned *);
};

/* Zero initialize. One session, current position, no rewind or retry.
 * Authorization is mandatory even for open; it does not establish media identity.
 * The legacy open path requires extended-status, variable-block, raw no-rewind
 * devices; native attachments enforce their separate session/framing contract.
 * Close must be called for cleanup even after a failed write. */
struct ot_tape_output {
    const struct ot_output_ops *ops;
    struct ot_writer_sink sink;
    struct ot_tape_status status;
    char name[7];
    unsigned handle, opened, failed, finished, dos_error;
    void *backend_context;
    int (*backend_write)(struct ot_tape_output *, const void *, unsigned, unsigned *);
    int (*backend_finish)(struct ot_tape_output *);
    int (*backend_close)(struct ot_tape_output *);
    /* Optional caller-owned workspace for archive backend framing. */
    oa_u8 *backend_buffer;
    unsigned backend_capacity;
    int backend_result;
};

int ot_output_open(struct ot_tape_output *, const struct ot_output_ops *,
                   const char *name, unsigned overwrite_authorized);
int ot_output_write(struct ot_tape_output *, const void *, unsigned, unsigned *);
int ot_output_close(struct ot_tape_output *);

#ifdef DOS_VERSION
const struct ot_output_ops *ot_dos_output_ops(void);
#endif
#endif

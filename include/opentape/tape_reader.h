#ifndef OPENTAPE_TAPE_READER_H
#define OPENTAPE_TAPE_READER_H

#include "opentape/tape_probe.h"

enum ot_read_result {
    OT_READ_BLOCK = 0,
    OT_READ_FILEMARK = 1,
    OT_READ_BAD_ARGUMENT = -20,
    OT_READ_BAD_MODE = -21,
    OT_READ_IO_ERROR = -22,
    OT_READ_FAILED_STATE = -23
};

struct ot_reader_ops {
    struct ot_probe_ops probe;
    unsigned (*set_raw)(void *, unsigned, unsigned);
    unsigned (*read)(void *, unsigned, void *, unsigned, unsigned *);
};

/* Initialize to zero before first open. No implicit rewind or read retry. */
struct ot_tape_reader {
    const struct ot_reader_ops *ops;
    unsigned handle;
    unsigned opened;
    unsigned failed;
    unsigned dos_error;
    struct ot_tape_status status;
};

int ot_reader_open(struct ot_tape_reader *reader,
                    const struct ot_reader_ops *ops, const char *name);
int ot_reader_read(struct ot_tape_reader *reader, void *buffer,
                    unsigned capacity, unsigned *accepted);
int ot_reader_close(struct ot_tape_reader *reader);

#ifdef DOS_VERSION
const struct ot_reader_ops *ot_dos_reader_ops(void);
#endif
#endif

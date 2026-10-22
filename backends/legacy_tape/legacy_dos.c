#include <dos.h>
#include <fcntl.h>
#include <string.h>
#include "opentape/tape_probe.h"
#include "opentape/tape_reader.h"

typedef void (__interrupt __far *critical_vector)(void);

static int __far critical_fail(unsigned device, unsigned error,
                               unsigned __far *header)
{
    (void)device;
    (void)error;
    (void)header;
    return _HARDERR_FAIL;
}

static critical_vector fail_begin(void)
{
    critical_vector old = _dos_getvect(0x24);
    _harderr(critical_fail);
    return old;
}

static unsigned dos_open(void *context, const char *name, unsigned *handle)
{
    critical_vector old;
    unsigned error;
    int opened_handle;
    (void)context;
    old = fail_begin();
    error = _dos_open(name, O_RDONLY, &opened_handle);
    _dos_setvect(0x24, old);
    if (error == 0u) *handle = (unsigned)opened_handle;
    return error;
}

static unsigned dos_info(void *context, unsigned handle, unsigned *flags)
{
    union REGS in, out;
    critical_vector old;
    (void)context;
    memset(&in, 0, sizeof(in));
    in.x.ax = 0x4400u;
    in.x.bx = handle;
    old = fail_begin();
    intdos(&in, &out);
    _dos_setvect(0x24, old);
    if (out.x.cflag) return out.x.ax;
    *flags = out.x.dx;
    return 0u;
}

static unsigned dos_status(void *context, unsigned handle, unsigned function,
                            void *buffer, unsigned size)
{
    union REGS in, out;
    struct SREGS segments;
    critical_vector old;
    (void)context;
    (void)size;
    memset(&in, 0, sizeof(in));
    segread(&segments);
    in.x.ax = 0x440cu;
    in.x.bx = handle;
    in.x.cx = (OA_ASPITAPE_IOCTL_CATEGORY << 8) | function;
    in.x.dx = FP_OFF(buffer);
    segments.ds = FP_SEG(buffer);
    old = fail_begin();
    intdosx(&in, &out, &segments);
    _dos_setvect(0x24, old);
    return out.x.cflag ? out.x.ax : 0u;
}

static unsigned dos_close(void *context, unsigned handle)
{
    critical_vector old;
    unsigned error;
    (void)context;
    old = fail_begin();
    error = _dos_close(handle);
    _dos_setvect(0x24, old);
    return error;
}

static unsigned dos_set_raw(void *context, unsigned handle, unsigned flags)
{
    union REGS in, out;
    critical_vector old;
    (void)context;
    memset(&in, 0, sizeof(in));
    in.x.ax = 0x4401u;
    in.x.bx = handle;
    in.x.dx = flags;
    old = fail_begin();
    intdos(&in, &out);
    _dos_setvect(0x24, old);
    return out.x.cflag ? out.x.ax : 0u;
}

static unsigned dos_read(void *context, unsigned handle, void *buffer,
                           unsigned capacity, unsigned *accepted)
{
    critical_vector old;
    unsigned error;
    (void)context;
    old = fail_begin();
    error = _dos_read(handle, buffer, capacity, accepted);
    _dos_setvect(0x24, old);
    return error;
}

const struct ot_reader_ops *ot_dos_reader_ops(void)
{
    static const struct ot_reader_ops ops = {
        { 0, dos_open, dos_info, dos_status, dos_close }, dos_set_raw, dos_read
    };
    return &ops;
}

int ot_dos_tape_probe(const char *name, struct ot_tape_status *status,
                      unsigned *dos_error)
{
    return ot_tape_probe(&ot_dos_reader_ops()->probe, name, status, dos_error);
}

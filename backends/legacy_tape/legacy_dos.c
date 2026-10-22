#include <dos.h>
#include <fcntl.h>
#include <string.h>
#include "opentape/tape_probe.h"

static unsigned dos_open(void *context, const char *name, unsigned *handle)
{
    (void)context;
    return _dos_open(name, O_RDONLY, handle);
}

static unsigned dos_info(void *context, unsigned handle, unsigned *flags)
{
    union REGS in, out;
    (void)context;
    memset(&in, 0, sizeof(in));
    in.x.ax = 0x4400u;
    in.x.bx = handle;
    intdos(&in, &out);
    if (out.x.cflag) return out.x.ax;
    *flags = out.x.dx;
    return 0u;
}

static unsigned dos_status(void *context, unsigned handle, unsigned function,
                            void *buffer, unsigned size)
{
    union REGS in, out;
    struct SREGS segments;
    (void)context;
    (void)size;
    memset(&in, 0, sizeof(in));
    segread(&segments);
    in.x.ax = 0x440cu;
    in.x.bx = handle;
    in.x.cx = (OA_ASPITAPE_IOCTL_CATEGORY << 8) | function;
    in.x.dx = FP_OFF(buffer);
    segments.ds = FP_SEG(buffer);
    intdosx(&in, &out, &segments);
    return out.x.cflag ? out.x.ax : 0u;
}

static unsigned dos_close(void *context, unsigned handle)
{
    (void)context;
    return _dos_close(handle);
}

/* Never display DOS's interactive Abort/Retry prompt during a probe. */
static int __far critical_fail(unsigned device, unsigned error,
                               unsigned __far *header)
{
    (void)device;
    (void)error;
    (void)header;
    return _HARDERR_FAIL;
}

int ot_dos_tape_probe(const char *name, struct ot_tape_status *status,
                      unsigned *dos_error)
{
    struct ot_probe_ops ops;
    void (__interrupt __far *old_handler)(void);
    int rc;

    memset(&ops, 0, sizeof(ops));
    ops.open_readonly = dos_open;
    ops.device_info = dos_info;
    ops.status = dos_status;
    ops.close = dos_close;
    old_handler = _dos_getvect(0x24);
    _harderr(critical_fail);
    rc = ot_tape_probe(&ops, name, status, dos_error);
    _dos_setvect(0x24, old_handler);
    return rc;
}

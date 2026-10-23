#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/tape_output.h"

struct fixture {
    unsigned opens, writable, closes, writes;
    unsigned flags, mode, count, error, delayed, close_error, legacy, fixed;
    unsigned query_error, driver_error, post_close_query_error;
};
static unsigned ro(void *p, const char *name, unsigned *h)
{
    struct fixture *f = p;
    assert(strcmp(name, "NRMT0") == 0);
    ++f->opens;
    *h = 1u;
    return 0u;
}
static unsigned wo(void *p, const char *name, unsigned *h)
{
    ++((struct fixture *)p)->writable;
    return ro(p, name, h);
}
static unsigned info(void *p, unsigned h, unsigned *flags)
{
    assert(h == 1u);
    *flags = ((struct fixture *)p)->flags;
    return 0u;
}
static unsigned query(void *p, unsigned h, unsigned fn, void *b, unsigned n)
{
    struct fixture *f = p;
    struct oa_aspitape_mtget16 s;
    struct oa_aspitape_mtget old;
    assert(h == 1u);
    if (f->writes && f->query_error) return f->query_error;
    if (f->closes >= 2u && f->post_close_query_error)
        return f->post_close_query_error;
    memset(&s, 0, sizeof(s));
    s.current_state = (oa_u16)f->mode;
    s.device_parameters = (oa_u8)f->fixed;
    if (f->writes) s.driver_error = (oa_u8)f->driver_error;
    if (f->delayed && f->closes >= 2u)
        s.current_state |= OA_MT16_STATE_WRITE_ERROR;
    if (fn == OA_ASPITAPE_IOCTL_MTGET16) {
        assert(n == sizeof(s));
        if (f->legacy) return 1u;
        memcpy(b, &s, n);
    } else {
        assert(n == sizeof(old));
        memset(&old, 0, sizeof(old));
        old.current_state = (oa_u8)f->mode;
        memcpy(b, &old, n);
    }
    return 0u;
}
static unsigned raw(void *p, unsigned h, unsigned flags)
{
    (void)p;
    assert(h == 1u && flags == 0xa0u);
    return 0u;
}
static unsigned write_block(void *p, unsigned h, const void *b,
                             unsigned n, unsigned *accepted)
{
    struct fixture *f = p;
    assert(h == 1u && b != 0 && n >= 56u);
    ++f->writes;
    *accepted = f->count;
    return f->error;
}
static unsigned close_handle(void *p, unsigned h)
{
    struct fixture *f = p;
    assert(h == 1u);
    ++f->closes;
    return f->closes == 2u ? f->close_error : 0u;
}
static void setup(struct fixture *f, struct ot_tape_output *o,
                   struct ot_output_ops *ops)
{
    memset(f, 0, sizeof(*f));
    memset(o, 0, sizeof(*o));
    memset(ops, 0, sizeof(*ops));
    f->flags = 0x8080u;
    f->mode = OA_MT_STATE_UNBUFFERED;
    f->count = 64u;
    ops->probe.context = f;
    ops->probe.open_readonly = ro;
    ops->probe.device_info = info;
    ops->probe.status = query;
    ops->probe.close = close_handle;
    ops->open_write = wo;
    ops->set_raw = raw;
    ops->write = write_block;
}
int main(void)
{
    struct fixture f;
    struct ot_tape_output o;
    struct ot_output_ops ops;
    unsigned char b[64];
    unsigned n, i;
    memset(b, 0, sizeof(b));
    setup(&f, &o, &ops);
    assert(ot_output_open(&o, &ops, "NRMT0", 0u) == OT_OUTPUT_NOT_AUTHORIZED);
    assert(f.opens == 0u && f.writable == 0u);
    assert(ot_output_open(&o, &ops, "RMT0", 1u) == OT_OUTPUT_BAD_ARGUMENT);
    assert(ot_output_open(&o, &ops, "NMT0", 1u) == OT_OUTPUT_BAD_ARGUMENT);
    assert(ot_output_open(&o, &ops, "C:NRMT0", 1u) == OT_OUTPUT_BAD_ARGUMENT);
    for (i = 0u; i < 7u; ++i) {
        setup(&f, &o, &ops);
        if (i == 0u) f.flags = 0u;
        if (i == 1u) f.legacy = 1u;
        if (i == 2u) f.fixed = OA_MT_PARAM_FIXED_BLOCK;
        if (i == 3u) f.mode |= OA_MT_STATE_REWIND_CLOSE;
        if (i == 4u) f.mode |= OA_MT_STATE_WRITTEN;
        if (i == 5u) f.mode |= OA_MT16_STATE_WRITE_ERROR;
        if (i == 6u) f.mode = OA_MT_STATE_BUFFERED;
        assert(ot_output_open(&o, &ops, "NRMT0", 1u) != 0);
        assert(f.writable == 0u && f.writes == 0u && f.closes == 1u);
    }
    setup(&f, &o, &ops);
    assert(ot_output_open(&o, &ops, "NRMT0", 1u) == 0);
    assert(ot_output_write(&o, b, 1u, &n) == OT_OUTPUT_BAD_ARGUMENT);
    assert(f.writes == 0u);
    assert(o.sink.write_block(o.sink.context, b, sizeof(b), &n) == 0);
    assert(n == sizeof(b));
    assert(o.sink.finish_session(o.sink.context) == 0);
    assert(o.finished && !o.failed && f.writes == 1u && f.closes == 3u);
    assert(ot_output_close(&o) == OT_OUTPUT_BAD_ARGUMENT);
    for (i = 0u; i < 8u; ++i) {
        setup(&f, &o, &ops);
        assert(ot_output_open(&o, &ops, "NRMT0", 1u) == 0);
        if (i == 0u) f.count = 63u;
        if (i == 1u) f.count = 65u;
        if (i == 2u) f.error = 83u;
        if (i == 3u) f.delayed = 1u;
        if (i == 4u) f.close_error = 6u;
        if (i == 5u) f.query_error = 5u;
        if (i == 6u) f.driver_error = OA_MT_ERROR_UNSPECIFIED;
        if (i == 7u) f.post_close_query_error = 5u;
        if (i < 3u || i == 5u || i == 6u) {
            assert(ot_output_write(&o, b, sizeof(b), &n) == OT_OUTPUT_IO_ERROR);
            assert(ot_output_write(&o, b, sizeof(b), &n) == OT_OUTPUT_FAILED);
            assert(f.writes == 1u);
        } else assert(ot_output_write(&o, b, sizeof(b), &n) == 0);
        assert(ot_output_close(&o) == OT_OUTPUT_IO_ERROR);
        assert(o.failed && !o.finished && !o.opened);
        if (i == 2u) assert(o.dos_error == 83u);
        if (i == 3u) assert(o.status.tape.current_state & OA_MT16_STATE_WRITE_ERROR);
        if (i == 4u) assert(o.dos_error == 6u);
    }
    puts("Tape output tests passed");
    return 0;
}

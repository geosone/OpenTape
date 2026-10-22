#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/tape_reader.h"

struct fixture {
    unsigned opens, closes, raw_calls, reads, queries;
    unsigned info_flags, raw_error, read_error, post_status_error, close_error;
    unsigned count, mode, block_size, driver_error;
    unsigned legacy;
};
static unsigned open_tape(void *p, const char *name, unsigned *handle)
{
    struct fixture *f = p;
    assert(strcmp(name, "NRMT0") == 0);
    ++f->opens;
    *handle = 1u;
    return 0u;
}
static unsigned info(void *p, unsigned h, unsigned *flags)
{
    struct fixture *f = p;
    assert(h == 1u);
    *flags = f->info_flags;
    return 0u;
}
static unsigned set_raw(void *p, unsigned h, unsigned flags)
{
    struct fixture *f = p;
    assert(h == 1u && flags == 0xa0u);
    ++f->raw_calls;
    return f->raw_error;
}
static unsigned query(void *p, unsigned h, unsigned fn, void *out, unsigned size)
{
    struct fixture *f = p;
    struct oa_aspitape_mtget16 status;
    struct oa_aspitape_mtget legacy;
    assert(h == 1u);
    ++f->queries;
    if (f->reads && f->post_status_error) return f->post_status_error;
    memset(&status, 0, sizeof(status));
    status.current_state = (oa_u16)f->mode;
    status.fixed_block_size = (oa_u16)f->block_size;
    if (f->block_size) status.device_parameters = OA_MT_PARAM_FIXED_BLOCK;
    if (f->reads) status.driver_error = (oa_u8)f->driver_error;
    if (fn == OA_ASPITAPE_IOCTL_MTGET16) {
        assert(size == 16u);
        if (f->legacy) return 1u;
        memcpy(out, &status, sizeof(status));
    } else {
        assert(fn == OA_ASPITAPE_IOCTL_MTGET && size == 14u);
        memset(&legacy, 0, sizeof(legacy));
        legacy.current_state = (oa_u8)f->mode;
        memcpy(out, &legacy, sizeof(legacy));
    }
    return 0u;
}
static unsigned read_tape(void *p, unsigned h, void *buffer,
                            unsigned capacity, unsigned *count)
{
    struct fixture *f = p;
    unsigned copied;
    assert(h == 1u);
    ++f->reads;
    copied = f->count > capacity ? capacity : f->count;
    memset(buffer, 0x5c, copied);
    *count = f->count;
    return f->read_error;
}
static unsigned close_tape(void *p, unsigned h)
{
    struct fixture *f = p;
    assert(h == 1u);
    ++f->closes;
    return f->close_error;
}
static void setup(struct fixture *f, struct ot_tape_reader *r,
                    struct ot_reader_ops *ops)
{
    memset(f, 0, sizeof(*f));
    memset(r, 0, sizeof(*r));
    memset(ops, 0, sizeof(*ops));
    f->info_flags = 0x8080u;
    f->mode = 2u;
    f->count = 63u;
    ops->probe.context = f;
    ops->probe.open_readonly = open_tape;
    ops->probe.device_info = info;
    ops->probe.status = query;
    ops->probe.close = close_tape;
    ops->set_raw = set_raw;
    ops->read = read_tape;
}
int main(void)
{
    struct fixture f;
    struct ot_tape_reader r;
    struct ot_reader_ops ops;
    unsigned char buffer[1040];
    unsigned count, queries;

    setup(&f, &r, &ops);
    assert(ot_reader_open(&r, &ops, "RMT0") == OT_READ_BAD_ARGUMENT);
    assert(ot_reader_open(&r, &ops, "NMT0") == OT_READ_BAD_ARGUMENT);
    assert(f.opens == 0u);
    assert(ot_reader_open(&r, &ops, "NRMT0") == 0);
    assert(ot_reader_open(&r, &ops, "NRMT0") == OT_READ_BAD_ARGUMENT);
    assert(f.opens == 1u && f.raw_calls == 1u);
    memset(buffer, 0xa5, sizeof(buffer));
    assert(ot_reader_read(&r, buffer, 1024u, &count) == OT_READ_BLOCK);
    assert(count == 63u && buffer[62] == 0x5cu && buffer[63] == 0xa5u);
    assert(buffer[1024] == 0xa5u && f.reads == 1u);
    f.count = 0u;
    assert(ot_reader_read(&r, buffer, 1024u, &count) == OT_READ_FILEMARK);
    assert(count == 0u && !r.failed);
    f.count = 512u;                 /* callers may explicitly continue after EOF */
    assert(ot_reader_read(&r, buffer, 1024u, &count) == OT_READ_BLOCK);
    assert(count == 512u);
    assert(ot_reader_close(&r) == 0 && f.closes == 1u);
    assert(ot_reader_close(&r) == OT_READ_BAD_ARGUMENT);

    setup(&f, &r, &ops);
    f.info_flags = 0u;
    assert(ot_reader_open(&r, &ops, "NRMT0") == OT_PROBE_NOT_DEVICE);
    assert(f.raw_calls == 0u && f.closes == 1u && !r.opened);
    setup(&f, &r, &ops);
    f.raw_error = 5u;
    f.close_error = 6u;
    assert(ot_reader_open(&r, &ops, "NRMT0") == OT_PROBE_IOCTL_FAILED);
    assert(r.dos_error == 5u && f.closes == 1u && f.queries == 0u);
    setup(&f, &r, &ops);
    f.mode = 1u;
    assert(ot_reader_open(&r, &ops, "NRMT0") == OT_READ_BAD_MODE);
    assert(f.closes == 1u);

    setup(&f, &r, &ops);
    f.block_size = 512u;
    f.count = 512u;
    assert(ot_reader_open(&r, &ops, "NRMT0") == 0);
    queries = f.queries;
    assert(ot_reader_read(&r, buffer, 513u, &count) == OT_READ_BAD_ARGUMENT);
    assert(f.reads == 0u && f.queries == queries && count == 0u);
    assert(ot_reader_read(&r, buffer, 1024u, &count) == OT_READ_BLOCK);
    f.count = 63u;
    assert(ot_reader_read(&r, buffer, 1024u, &count) == OT_READ_IO_ERROR);
    assert(count == 0u && r.failed);
    assert(ot_reader_close(&r) == 0);

    setup(&f, &r, &ops);
    assert(ot_reader_open(&r, &ops, "NRMT0") == 0);
    f.read_error = 83u;
    f.post_status_error = 5u;
    assert(ot_reader_read(&r, buffer, 1024u, &count) == OT_READ_IO_ERROR);
    assert(count == 0u && r.dos_error == 83u && r.failed);
    queries = f.queries;
    assert(ot_reader_read(&r, buffer, 1024u, &count) == OT_READ_FAILED_STATE);
    assert(f.reads == 1u && f.queries == queries);
    f.close_error = 6u;
    assert(ot_reader_close(&r) == OT_PROBE_CLOSE_FAILED && r.dos_error == 83u);

    setup(&f, &r, &ops);
    assert(ot_reader_open(&r, &ops, "NRMT0") == 0);
    assert(ot_reader_read(&r, buffer, 1u, &count) == OT_READ_BAD_ARGUMENT);
    assert(f.reads == 0u && count == 0u);
    f.count = 1025u;
    assert(ot_reader_read(&r, buffer, 1024u, &count) == OT_READ_IO_ERROR);
    assert(count == 0u);
    assert(ot_reader_close(&r) == 0);
    setup(&f, &r, &ops);
    assert(ot_reader_open(&r, &ops, "NRMT0") == 0);
    f.post_status_error = 5u;
    assert(ot_reader_read(&r, buffer, 1024u, &count) == OT_READ_IO_ERROR);
    assert(count == 0u && r.dos_error == 5u && r.failed);
    assert(ot_reader_close(&r) == 0);
    setup(&f, &r, &ops);
    f.legacy = 1u;
    assert(ot_reader_open(&r, &ops, "NRMT0") == 0 && !r.status.extended);
    assert(ot_reader_read(&r, buffer, 1024u, &count) == OT_READ_BLOCK);
    assert(ot_reader_close(&r) == 0);
    setup(&f, &r, &ops);
    assert(ot_reader_open(&r, &ops, "NRMT0") == 0);
    f.driver_error = 7u;
    assert(ot_reader_read(&r, buffer, 1024u, &count) == OT_READ_IO_ERROR);
    assert(ot_reader_close(&r) == 0);
    puts("OpenTape raw tape reader tests passed");
    return 0;
}

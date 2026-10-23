#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/native_tape.h"

struct fixture {
    char trace[32];
    unsigned count, open_error, device_error, query_error, control_error, close_error;
    unsigned device, bad_query, query_guard, bad_control, control_guard, mismatch;
    unsigned read_kind, read_error, write_kind, write_error, fail_operation, count_mismatch;
    oa_u16 query_result, control_result, session;
    oa_u32 operations;
};
static void event(struct fixture *f, char c)
{
    assert(f->count < sizeof(f->trace)-1u);
    f->trace[f->count++] = c;
    f->trace[f->count] = 0;
}
static int valid(void *v, const char *name)
{ (void)v; return strcmp(name, "drive-A") == 0; }
static unsigned opened(void *v, const char *name)
{ struct fixture *f = (struct fixture *)v; assert(valid(v,name)); event(f,'O'); return f->open_error; }
static unsigned device(void *v, unsigned *result)
{ struct fixture *f = (struct fixture *)v; event(f,'I'); *result=f->device; return f->device_error; }
static unsigned query(void *v, struct ot_tape_info_v1 *p)
{
    struct fixture *f = (struct fixture *)v;
    event(f,'Q');
    assert(memcmp(p->magic,"OTAP",4)==0 && p->size==128);
    if (f->query_error) return f->query_error;
    p->operations=f->operations;
    p->valid_fields=OT_TAPE_VALID_SESSION;
    p->result=f->query_result;
    p->session_state=f->session;
    if (p->result==OT_TAPE_RESULT_PARTIAL) {
        p->diagnostic_class=OT_TAPE_DIAG_UNSUPPORTED;
        p->target_status=2; p->sense_key=5; p->sense_response=0x70; p->asc=0x24;
    }
    if (f->bad_query) p->version=0x0200;
    if (f->query_guard) ((unsigned char *)p)[sizeof(*p)]=0;
    return 0;
}
static unsigned controlled(void *v, struct ot_tape_control_v1 *p)
{
    struct fixture *f = (struct fixture *)v;
    event(f,'X');
    assert(p->flags==0 && p->size==64);
    if (f->control_error) return f->control_error;
    if (f->mismatch) p->operation=p->operation==0 ? 1 : 0;
    p->result=f->control_result;
    if (p->result!=OT_TAPE_RESULT_BUSY) {
        p->scsi_opcode=p->operation>=10 ? 0x1b : p->operation>=7 ? 0x11 : (oa_u8)p->operation;
        p->completion_flags=OT_TAPE_COMPLETION_SUBMITTED;
        if (p->result==OT_TAPE_RESULT_OK) {
            p->completion_flags|=OT_TAPE_COMPLETION_DONE |
                (p->operation==OT_TAPE_CONTROL_REWIND ? OT_TAPE_POSITION_BOT :
                 p->operation==OT_TAPE_CONTROL_READY ? OT_TAPE_COMPLETION_READY : 0u);
        } else {
            if (p->operation!=OT_TAPE_CONTROL_READY) p->completion_flags|=OT_TAPE_POSITION_UNKNOWN;
            if (p->operation>=7) p->session_state=OT_TAPE_SESSION_NATIVE_FAILED;
            p->diagnostic_class=OT_TAPE_DIAG_TARGET;
            p->target_status=2; p->sense_key=2; p->sense_response=0x70; p->asc=0x3a;
        }
    }
    if (p->operation>=10) {
        if (p->result==0) p->session_state=f->session;
        else if (p->result==3) p->session_state=f->session|OT_TAPE_SESSION_NATIVE_FAILED;
    }
    if (f->count_mismatch) ++p->count;
    if (f->bad_control) p->completion_flags|=0x8000u;
    if (f->control_guard) ((unsigned char *)p)[sizeof(*p)]=0;
    return 0;
}
static unsigned read_record(void *v, void *data, unsigned capacity, struct ot_tape_record_v1 *p)
{
    struct fixture *f = (struct fixture *)v;
    event(f, 'R');
    assert(capacity == 8u && p->requested == 8UL);
    if (f->read_error) return f->read_error;
    memcpy(data, "RECORD01", 8u);
    p->scsi_opcode = 8; p->actual = 8;
    p->completion_flags = 0x23;
    if (f->read_kind == 1u || f->read_kind == 2u) {
        p->actual = 4; p->stream_flags = OT_TAPE_STREAM_SHORT;
        p->result = OT_TAPE_RESULT_STREAM; p->diagnostic_class = OT_TAPE_DIAG_TARGET;
        p->target_status = 2; p->sense_response = 0x70;
        if (f->read_kind == 1u) p->stream_flags |= OT_TAPE_STREAM_FILEMARK;
        else { p->result = OT_TAPE_RESULT_ERROR; p->sense_key = 3; p->completion_flags = 0x25; }
    } else if (f->read_kind == 3u) p->requested = 9;
    else if (f->read_kind == 4u) ((unsigned char *)p)[sizeof(*p)] = 0;
    else if (f->read_kind == 5u) p->actual = 0; /* missing SHORT flag */
    else if (f->read_kind == 7u) p->block_size = 2;
    else if (f->read_kind == 6u) {
        p->actual = 0; p->stream_flags = OT_TAPE_STREAM_SHORT;
    }
    return 0u;
}
static unsigned writer_controlled(void *v, struct ot_tape_control_v1 *p)
{
    struct fixture *f = (struct fixture *)v;
    static const char events[] = "??BFMEA";
    unsigned op = p->operation;
    assert(op >= 2u && op <= 6u);
    event(f, events[op]);
    if (op == f->fail_operation && f->control_error) return f->control_error;
    if (op == f->fail_operation && f->control_result) {
        f->session |= OT_TAPE_SESSION_NATIVE_FAILED;
        p->result = OT_TAPE_RESULT_ERROR; p->diagnostic_class = OT_TAPE_DIAG_TARGET;
        p->target_status = 2; p->sense_response = 0x70; p->sense_key = 3;
        p->scsi_opcode = 0x10; p->completion_flags = 5;
    } else if (op == OT_TAPE_CONTROL_BEGIN_WRITE) {
        f->session = OT_TAPE_SESSION_NATIVE_WRITER; p->completion_flags = 0x42;
    } else if (op == OT_TAPE_CONTROL_ABORT_WRITE) {
        f->session &= ~OT_TAPE_SESSION_NATIVE_WRITER;
        if (f->session) f->session |= OT_TAPE_SESSION_NATIVE_FAILED;
        p->completion_flags = 0x82;
    } else {
        p->scsi_opcode = 0x10; p->completion_flags = 3;
        if (op == OT_TAPE_CONTROL_FILEMARK) f->session |= OT_TAPE_SESSION_NATIVE_DIRTY;
        if (op == OT_TAPE_CONTROL_END_WRITE) { f->session = 0; p->completion_flags |= 0x80; }
    }
    p->session_state = f->session;
    if (op == f->fail_operation && f->bad_control) p->completion_flags |= 0x100;
    if (op == f->fail_operation && f->control_guard) ((unsigned char *)p)[sizeof(*p)] = 0;
    return 0;
}
static unsigned write_record(void *v, const void *data, unsigned bytes, struct ot_tape_record_v1 *p)
{
    struct fixture *f = (struct fixture *)v;
    event(f, 'W');
    assert(bytes == 8u && memcmp(data, "RECORD01", 8u) == 0 && p->operation == 1);
    if (f->write_error) return f->write_error;
    f->session |= OT_TAPE_SESSION_NATIVE_DIRTY;
    p->scsi_opcode = 0x0a; p->actual = 8; p->completion_flags = 0x23;
    if (f->write_kind == 1) {
        p->result = OT_TAPE_RESULT_ERROR; p->diagnostic_class = OT_TAPE_DIAG_INCOMPLETE;
        p->actual = 4; p->stream_flags = OT_TAPE_STREAM_SHORT; p->completion_flags = 0x25;
        f->session |= OT_TAPE_SESSION_NATIVE_FAILED;
    } else if (f->write_kind == 2) p->actual = 7; /* inconsistent short success */
    else if (f->write_kind == 3) p->operation = 0;
    else if (f->write_kind == 4) ((unsigned char *)p)[sizeof(*p)] = 0;
    else if (f->write_kind == 5) p->block_size = 8;
    p->session_state = f->session;
    return 0;
}
static unsigned closed(void *v)
{ struct fixture *f=(struct fixture *)v; event(f,'C'); return f->close_error; }
static void reset(struct fixture *f)
{
    memset(f,0,sizeof(*f)); f->device=1;
    f->operations=OT_TAPE_OP_QUERY|OT_TAPE_OP_READY|OT_TAPE_OP_REWIND;
}

static void readers(struct fixture *f, const struct ot_native_ops *base)
{
    struct ot_native_reader_ops ops;
    struct ot_native_reader r;
    unsigned char data[8];
    unsigned n;
    ops.query = *base;
    ops.read = read_record;
    memset(&r, 0, sizeof(r));
    reset(f); f->operations |= OT_TAPE_OP_READ;
    assert(ot_native_reader_open(&r, &ops, "drive-A") == 0);
    assert(ot_native_reader_open(&r, &ops, "drive-A") == OT_NATIVE_ARGUMENT);
    assert(ot_native_reader_read(&r, data, 0u) == OT_NATIVE_ARGUMENT);
    assert(!r.failed && strcmp(f->trace, "OIQ") == 0);
    assert(ot_native_reader_read(&r, data, 8u) == 0);
    assert(r.last.actual == 8UL && memcmp(data, "RECORD01", 8u) == 0);
    f->read_kind = 1;
    assert(ot_native_reader_read(&r, data, 8u) == OT_NATIVE_STREAM);
    assert(r.last.actual == 4UL && (r.last.stream_flags & OT_TAPE_STREAM_FILEMARK));
    assert(ot_native_reader_read(&r, data, 8u) == OT_NATIVE_STREAM);
    assert(ot_native_reader_close(&r) == 0);
    assert(strcmp(f->trace, "OIQRRRC") == 0);
    assert(ot_native_reader_close(&r) == OT_NATIVE_ARGUMENT);
    reset(f);
    assert(ot_native_reader_open(&r, &ops, "drive-A") == OT_NATIVE_UNSUPPORTED);
    assert(!r.opened && strcmp(f->trace, "OIQC") == 0);
    reset(f); f->operations |= OT_TAPE_OP_READ; f->session = OT_TAPE_SESSION_WRITE_ERROR;
    assert(ot_native_reader_open(&r, &ops, "drive-A") == OT_NATIVE_BUSY);
    assert(!r.opened && strcmp(f->trace, "OIQC") == 0);
    for (n = 2; n <= 5; ++n) {
        reset(f); f->operations |= OT_TAPE_OP_READ; f->read_kind = n;
        assert(ot_native_reader_open(&r, &ops, "drive-A") == 0);
        assert(ot_native_reader_read(&r, data, 8u) == (n == 2 ? OT_NATIVE_COMMAND : OT_NATIVE_RESPONSE));
        assert(r.failed);
        if (n == 2) assert(r.last.actual == 4UL && r.last.completion_flags == 0x25);
        else assert(r.last.size == 0u);
        assert(ot_native_reader_read(&r, data, 8u) == OT_NATIVE_COMMAND);
        assert(ot_native_reader_close(&r) == 0);
        assert(strcmp(f->trace, "OIQRC") == 0);
    }
    reset(f); f->operations |= OT_TAPE_OP_READ; f->read_error = 12; f->close_error = 53;
    assert(ot_native_reader_open(&r, &ops, "drive-A") == 0);
    assert(ot_native_reader_read(&r, data, 8u) == OT_NATIVE_IOCTL);
    assert(ot_native_reader_close(&r) == OT_NATIVE_CLOSE);
    assert(r.platform_error == 12 && strcmp(f->trace, "OIQRC") == 0);
    reset(f); f->operations |= OT_TAPE_OP_READ; f->read_kind = 6;
    assert(ot_native_reader_open(&r, &ops, "drive-A") == 0);
    assert(ot_native_reader_read(&r, data, 8u) == 0);
    assert(r.last.actual == 0 && r.last.stream_flags == OT_TAPE_STREAM_SHORT); /* empty record, not filemark */
    f->close_error = 53;
    assert(ot_native_reader_close(&r) == OT_NATIVE_CLOSE);
    assert(r.last.actual == 0 && r.platform_error == 53);
}

static void writers(struct fixture *f, const struct ot_native_ops *base)
{
    struct ot_native_writer_ops ops;
    struct ot_native_writer w;
    struct ot_tape_record_v1 saved;
    unsigned n;
    memset(&w, 0, sizeof(w));
    ops.control.query = *base; ops.control.control = writer_controlled;
    ops.open_write = opened; ops.write = write_record;
    reset(f); f->operations = 0x3ffUL;
    assert(ot_native_writer_open(&w, &ops, "drive-A") == 0);
    assert(strcmp(f->trace, "OIQB") == 0);
    assert(ot_native_writer_write(&w, "RECORD01", 0u) == OT_NATIVE_ARGUMENT);
    assert(w.failure == 0 && strcmp(f->trace, "OIQB") == 0);
    assert(ot_native_writer_write(&w, "RECORD01", 8u) == 0);
    assert(ot_native_writer_flush(&w) == 0);
    assert(ot_native_writer_filemark(&w) == 0);
    assert(ot_native_writer_finish(&w) == 0);
    assert(w.last_control.completion_flags == 0x83);
    assert(ot_native_writer_close(&w) == 0);
    assert(strcmp(f->trace, "OIQBWFMEC") == 0);
    assert(ot_native_writer_close(&w) == OT_NATIVE_ARGUMENT);
    reset(f);
    assert(ot_native_writer_open(&w, &ops, "drive-A") == OT_NATIVE_UNSUPPORTED);
    assert(strcmp(f->trace, "OIQC") == 0);
    reset(f); f->operations = 0x3ffUL; f->session = OT_TAPE_SESSION_NATIVE_FAILED;
    assert(ot_native_writer_open(&w, &ops, "drive-A") == OT_NATIVE_BUSY);
    assert(strcmp(f->trace, "OIQC") == 0);
    for (n = 1; n <= 4; ++n) {
        reset(f); f->operations = 0x3ffUL; f->write_kind = n;
        assert(ot_native_writer_open(&w, &ops, "drive-A") == 0);
        assert(ot_native_writer_write(&w, "RECORD01", 8u) == (n == 1 ? OT_NATIVE_COMMAND : OT_NATIVE_RESPONSE));
        saved = w.last_record;
        if (n == 1) assert(saved.actual == 4 && saved.completion_flags == 0x25);
        else assert(saved.size == 0);
        assert(ot_native_writer_write(&w, "RECORD01", 8u) == w.failure);
        assert(ot_native_writer_flush(&w) == w.failure);
        assert(ot_native_writer_finish(&w) == w.failure);
        assert(ot_native_writer_close(&w) == (n == 1 ? OT_NATIVE_COMMAND : OT_NATIVE_RESPONSE));
        assert(strcmp(f->trace, "OIQBWAC") == 0);
        assert(memcmp(&saved, &w.last_record, sizeof(saved)) == 0);
    }
    for (n = 3; n <= 5; ++n) {
        reset(f); f->operations = 0x3ffUL; f->fail_operation = n; f->control_result = 3;
        assert(ot_native_writer_open(&w, &ops, "drive-A") == 0);
        assert(ot_native_writer_write(&w, "RECORD01", 8u) == 0);
        assert((n == 3 ? ot_native_writer_flush(&w) : n == 4 ? ot_native_writer_filemark(&w) :
                ot_native_writer_finish(&w)) == OT_NATIVE_COMMAND);
        assert(ot_native_writer_finish(&w) == OT_NATIVE_COMMAND);
        assert(ot_native_writer_close(&w) == OT_NATIVE_COMMAND);
        assert(w.last_control.result == 3 && w.last_control.operation == n);
    }
    reset(f); f->operations = 0x3ffUL;
    assert(ot_native_writer_open(&w, &ops, "drive-A") == 0);
    assert(ot_native_writer_write(&w, "RECORD01", 8u) == 0);
    assert(ot_native_writer_close(&w) == OT_NATIVE_UNFINISHED);
    assert(strcmp(f->trace, "OIQBWAC") == 0);
    reset(f); f->operations = 0x3ffUL;
    assert(ot_native_writer_open(&w, &ops, "drive-A") == 0);
    assert(ot_native_writer_abort(&w) == 0);
    assert(ot_native_writer_close(&w) == OT_NATIVE_UNFINISHED);
    assert(strcmp(f->trace, "OIQBAC") == 0);
    reset(f); f->operations = 0x3ffUL; f->fail_operation = 2; f->control_error = 12; f->close_error = 53;
    assert(ot_native_writer_open(&w, &ops, "drive-A") == OT_NATIVE_IOCTL);
    assert(!w.opened && w.platform_error == 12 && strcmp(f->trace, "OIQBC") == 0);
    for (n = 0; n < 2; ++n) {
        reset(f); f->operations = 0x3ffUL; f->fail_operation = 3;
        f->bad_control = n == 0; f->control_guard = n == 1;
        assert(ot_native_writer_open(&w, &ops, "drive-A") == 0);
        assert(ot_native_writer_flush(&w) == OT_NATIVE_RESPONSE);
        assert(w.last_control.size == 0);
        assert(ot_native_writer_close(&w) == OT_NATIVE_RESPONSE);
        assert(strcmp(f->trace, "OIQBFAC") == 0);
    }
    reset(f); f->operations = 0x3ffUL; f->write_error = 12; f->close_error = 53;
    assert(ot_native_writer_open(&w, &ops, "drive-A") == 0);
    assert(ot_native_writer_write(&w, "RECORD01", 8u) == OT_NATIVE_IOCTL);
    assert(ot_native_writer_close(&w) == OT_NATIVE_IOCTL);
    assert(w.platform_error == 12);
    reset(f); f->operations = 0x3ffUL;
    assert(ot_native_writer_open(&w, &ops, "drive-A") == 0);
    assert(ot_native_writer_finish(&w) == 0);
    f->close_error = 53;
    assert(ot_native_writer_close(&w) == OT_NATIVE_CLOSE && w.platform_error == 53);
}

static void fixed_sessions(struct fixture *f, const struct ot_native_ops *base)
{
    struct ot_native_reader_ops ro;
    struct ot_native_writer_ops wo;
    struct ot_native_reader r;
    struct ot_native_writer w;
    unsigned char data[8];
    memset(&r,0,sizeof(r)); memset(&w,0,sizeof(w));
    ro.query=*base; ro.read=read_record;
    wo.control.query=*base; wo.control.control=writer_controlled; wo.open_write=opened; wo.write=write_record;
    reset(f);
    assert(ot_native_reader_open_fixed(&r,&ro,"drive-A",0)==OT_NATIVE_ARGUMENT && f->count==0);
    assert(ot_native_reader_open_fixed(&r,&ro,"drive-A",65536UL)==OT_NATIVE_ARGUMENT && f->count==0);
    f->operations|=OT_TAPE_OP_READ;
    assert(ot_native_reader_open_fixed(&r,&ro,"drive-A",4)==OT_NATIVE_UNSUPPORTED);
    assert(strcmp(f->trace,"OIQC")==0);
    reset(f); f->operations=0x7ffUL;
    assert(ot_native_reader_open_fixed(&r,&ro,"drive-A",4)==0);
    assert(ot_native_reader_read(&r,data,7)==OT_NATIVE_ARGUMENT && !r.failed);
    assert(strcmp(f->trace,"OIQ")==0);
    assert(ot_native_reader_read(&r,data,8)==0 && r.last.block_size==4 && r.last.flags==1);
    f->read_kind=1;
    assert(ot_native_reader_read(&r,data,8)==OT_NATIVE_STREAM && r.last.actual==4);
    assert(ot_native_reader_close(&r)==0);
    reset(f); f->operations=0x7ffUL; f->read_kind=7;
    assert(ot_native_reader_open_fixed(&r,&ro,"drive-A",4)==0);
    assert(ot_native_reader_read(&r,data,8)==OT_NATIVE_RESPONSE);
    assert(ot_native_reader_read(&r,data,8)==OT_NATIVE_COMMAND);
    assert(ot_native_reader_close(&r)==0 && strcmp(f->trace,"OIQRC")==0);
    reset(f); f->operations=0x3ffUL;
    assert(ot_native_writer_open_fixed(&w,&wo,"drive-A",4)==OT_NATIVE_UNSUPPORTED);
    assert(strcmp(f->trace,"OIQC")==0);
    reset(f); f->operations=0x7ffUL;
    assert(ot_native_writer_open_fixed(&w,&wo,"drive-A",4)==0);
    assert(w.last_control.flags==1 && w.last_control.block_size==4);
    assert(ot_native_writer_write(&w,"RECORD01",7)==OT_NATIVE_ARGUMENT && !w.failure);
    assert(strcmp(f->trace,"OIQB")==0);
    assert(ot_native_writer_write(&w,"RECORD01",8)==0 && w.last_record.flags==1);
    assert(ot_native_writer_filemark(&w)==0);
    assert(ot_native_writer_finish(&w)==0 && w.last_control.flags==0 && w.last_control.block_size==0);
    assert(ot_native_writer_close(&w)==0);
    reset(f); f->operations=0x7ffUL; f->write_kind=1;
    assert(ot_native_writer_open_fixed(&w,&wo,"drive-A",4)==0);
    assert(ot_native_writer_write(&w,"RECORD01",8)==OT_NATIVE_COMMAND && w.last_record.actual==4);
    assert(ot_native_writer_write(&w,"RECORD01",8)==OT_NATIVE_COMMAND);
    assert(ot_native_writer_close(&w)==OT_NATIVE_COMMAND && strcmp(f->trace,"OIQBWAC")==0);
    reset(f); f->operations=0x7ffUL; f->write_kind=5;
    assert(ot_native_writer_open_fixed(&w,&wo,"drive-A",4)==0);
    assert(ot_native_writer_write(&w,"RECORD01",8)==OT_NATIVE_RESPONSE);
    assert(ot_native_writer_close(&w)==OT_NATIVE_RESPONSE);
}

static void spaces(struct fixture *f, const struct ot_native_control_ops *ops)
{
    struct ot_tape_control_v1 c;
    unsigned error, op, i;
    const oa_s32 counts[]={0,1,-1,8388607L,-8388608L};
    for(op=7;op<=9;++op) for(i=0;i<(op==9?1u:5u);++i) {
        reset(f); f->operations=0x3fffUL;
        assert(ot_native_space(ops,"drive-A",(oa_u16)op,counts[i],&c,&error)==0);
        assert(c.count==counts[i] && c.completion_flags==3 && strcmp(f->trace,"OIQXC")==0);
    }
    for(op=7;op<=9;++op) {
        oa_u32 bit = 1UL << (op+4u);
        reset(f); f->operations=OT_TAPE_OP_QUERY|bit;
        assert(ot_native_space(ops,"drive-A",(oa_u16)op,0,&c,&error)==0);
        reset(f); f->operations=0x3fffUL & ~bit;
        assert(ot_native_space(ops,"drive-A",(oa_u16)op,0,&c,&error)==OT_NATIVE_UNSUPPORTED);
        assert(strcmp(f->trace,"OIQC")==0);
    }
    reset(f); f->operations=0x7ffUL;
    assert(ot_native_space(ops,"drive-A",7,1,&c,&error)==OT_NATIVE_UNSUPPORTED);
    assert(strcmp(f->trace,"OIQC")==0);
    reset(f);
    assert(ot_native_space(ops,"drive-A",7,8388608L,&c,&error)==OT_NATIVE_ARGUMENT && f->count==0);
    assert(ot_native_space(ops,"drive-A",8,-8388609L,&c,&error)==OT_NATIVE_ARGUMENT && f->count==0);
    assert(ot_native_space(ops,"drive-A",9,1,&c,&error)==OT_NATIVE_ARGUMENT && f->count==0);
    assert(ot_native_space(ops,"drive-A",0,0,&c,&error)==OT_NATIVE_ARGUMENT && f->count==0);
    for(i=0;i<7;++i) {
        reset(f); f->operations=0x3fffUL; f->session=(oa_u16)(1u<<i);
        assert(ot_native_space(ops,"drive-A",7,-1,&c,&error)==OT_NATIVE_BUSY);
        assert(strcmp(f->trace,"OIQC")==0);
    }
    reset(f); f->operations=0x3fffUL; f->count_mismatch=1;
    assert(ot_native_space(ops,"drive-A",7,-1,&c,&error)==OT_NATIVE_RESPONSE);
    assert(c.size==0 && strcmp(f->trace,"OIQXC")==0);
    reset(f); f->operations=0x3fffUL; f->control_result=3; f->close_error=53;
    assert(ot_native_space(ops,"drive-A",7,1,&c,&error)==OT_NATIVE_COMMAND);
    assert(c.session_state==OT_TAPE_SESSION_NATIVE_FAILED && c.completion_flags==5 && error==0);
    assert(strcmp(f->trace,"OIQXC")==0);
}

static void media(struct fixture *f, const struct ot_native_control_ops *ops)
{
    struct ot_tape_control_v1 c;
    unsigned error,op,state;
    for(op=10;op<=11;++op) {
        oa_u32 bit=op==10 ? OT_TAPE_OP_LOAD : OT_TAPE_OP_UNLOAD;
        reset(f); f->operations=OT_TAPE_OP_QUERY|bit;
        assert(ot_native_execute(ops,"drive-A",(oa_u16)op,&c,&error)==0);
        assert(c.scsi_opcode==0x1b && c.completion_flags==3 && c.count==0 && strcmp(f->trace,"OIQXC")==0);
        reset(f); f->operations=0xffffUL & ~bit;
        assert(ot_native_execute(ops,"drive-A",(oa_u16)op,&c,&error)==OT_NATIVE_UNSUPPORTED);
        assert(strcmp(f->trace,"OIQC")==0);
        for(state=0x40;state<=0x60;state+=0x20) {
            reset(f); f->operations=0xffffUL; f->session=(oa_u16)state;
            assert(ot_native_execute(ops,"drive-A",(oa_u16)op,&c,&error)==0 && c.session_state==state);
            reset(f); f->operations=0xffffUL; f->session=(oa_u16)state; f->control_result=3; f->close_error=53;
            assert(ot_native_execute(ops,"drive-A",(oa_u16)op,&c,&error)==OT_NATIVE_COMMAND);
            assert(c.session_state==state && c.completion_flags==5 && strcmp(f->trace,"OIQXC")==0);
        }
        for(state=1;state<=0x10;state<<=1) {
            reset(f); f->operations=0xffffUL; f->session=(oa_u16)state;
            assert(ot_native_execute(ops,"drive-A",(oa_u16)op,&c,&error)==OT_NATIVE_BUSY);
            assert(strcmp(f->trace,"OIQC")==0);
        }
    }
    reset(f); f->operations=0xffffUL;
    assert(ot_native_execute(ops,"drive-A",7,&c,&error)==OT_NATIVE_ARGUMENT && f->count==0);
    assert(ot_native_space(ops,"drive-A",10,0,&c,&error)==OT_NATIVE_ARGUMENT && f->count==0);
    assert(ot_native_execute(ops,"drive-A",0xffffu,&c,&error)==OT_NATIVE_ARGUMENT && f->count==0);
}

int main(void)
{
    struct fixture f;
    struct ot_native_control_ops ops;
    struct ot_tape_info_v1 info;
    struct ot_tape_control_v1 result;
    unsigned error;
    ops.query.context=&f; ops.query.valid_name=valid;
    ops.query.open_readonly=opened; ops.query.is_device=device;
    ops.query.query=query; ops.query.close=closed; ops.control=controlled;
    reset(&f);
    assert(ot_native_probe(&ops.query,"drive-A",0,&info,&error)==0);
    assert(strcmp(f.trace,"OIQC")==0 && error==0 && info.size==128);
    reset(&f); f.query_result=OT_TAPE_RESULT_PARTIAL;
    assert(ot_native_probe(&ops.query,"drive-A",1,&info,&error)==OT_NATIVE_PARTIAL);
    assert(info.diagnostic_class==OT_TAPE_DIAG_UNSUPPORTED && strcmp(f.trace,"OIQC")==0);
    reset(&f); f.open_error=5;
    assert(ot_native_probe(&ops.query,"drive-A",0,&info,&error)==OT_NATIVE_OPEN);
    assert(strcmp(f.trace,"O")==0 && error==5 && info.size==0);
    reset(&f); f.device=0;
    assert(ot_native_probe(&ops.query,"drive-A",0,&info,&error)==OT_NATIVE_DEVICE);
    assert(strcmp(f.trace,"OIC")==0);
    reset(&f); f.query_error=12; f.close_error=53;
    assert(ot_native_probe(&ops.query,"drive-A",0,&info,&error)==OT_NATIVE_IOCTL);
    assert(error==12 && strcmp(f.trace,"OIQC")==0);
    reset(&f); f.bad_query=1;
    assert(ot_native_probe(&ops.query,"drive-A",0,&info,&error)==OT_NATIVE_RESPONSE);
    reset(&f); f.query_guard=1;
    assert(ot_native_probe(&ops.query,"drive-A",0,&info,&error)==OT_NATIVE_RESPONSE);
    reset(&f); f.close_error=53;
    assert(ot_native_probe(&ops.query,"drive-A",0,&info,&error)==OT_NATIVE_CLOSE);
    assert(error==53 && info.size==128);
    reset(&f);
    assert(ot_native_probe(&ops.query,"NRMT0",0,&info,&error)==OT_NATIVE_ARGUMENT);
    assert(f.count==0);
    assert(ot_native_probe(&ops.query,"drive-A",2,&info,&error)==OT_NATIVE_ARGUMENT);
    assert(f.count==0);

    reset(&f);
    assert(ot_native_execute(&ops,"drive-A",OT_TAPE_CONTROL_REWIND,&result,&error)==0);
    assert(strcmp(f.trace,"OIQXC")==0 && result.completion_flags==0x0b);
    reset(&f); f.control_result=OT_TAPE_RESULT_ERROR;
    assert(ot_native_execute(&ops,"drive-A",1,&result,&error)==OT_NATIVE_COMMAND);
    assert(result.completion_flags==5 && strcmp(f.trace,"OIQXC")==0);
    reset(&f); f.control_result=OT_TAPE_RESULT_BUSY;
    assert(ot_native_execute(&ops,"drive-A",1,&result,&error)==OT_NATIVE_BUSY);
    assert(result.completion_flags==0);
    reset(&f); f.session=OT_TAPE_SESSION_WRITTEN;
    assert(ot_native_execute(&ops,"drive-A",1,&result,&error)==OT_NATIVE_BUSY);
    assert(strcmp(f.trace,"OIQC")==0 && result.size==0);
    reset(&f); f.session=OT_TAPE_SESSION_REWINDING;
    assert(ot_native_execute(&ops,"drive-A",1,&result,&error)==OT_NATIVE_BUSY);
    assert(strcmp(f.trace,"OIQC")==0);
    reset(&f); f.session=OT_TAPE_SESSION_REWINDING;
    assert(ot_native_execute(&ops,"drive-A",0,&result,&error)==0);
    assert(result.completion_flags==0x13 && strcmp(f.trace,"OIQXC")==0);
    reset(&f); f.session=OT_TAPE_SESSION_WRITE_ERROR;
    assert(ot_native_execute(&ops,"drive-A",0,&result,&error)==OT_NATIVE_BUSY);
    assert(strcmp(f.trace,"OIQC")==0);
    reset(&f); f.operations=OT_TAPE_OP_QUERY;
    assert(ot_native_execute(&ops,"drive-A",1,&result,&error)==OT_NATIVE_UNSUPPORTED);
    assert(strcmp(f.trace,"OIQC")==0);
    reset(&f); f.mismatch=1;
    assert(ot_native_execute(&ops,"drive-A",1,&result,&error)==OT_NATIVE_RESPONSE);
    assert(result.size==0);
    reset(&f); f.control_guard=1;
    assert(ot_native_execute(&ops,"drive-A",1,&result,&error)==OT_NATIVE_RESPONSE);
    reset(&f); f.bad_control=1;
    assert(ot_native_execute(&ops,"drive-A",1,&result,&error)==OT_NATIVE_RESPONSE);
    reset(&f); f.control_error=5; f.close_error=53;
    assert(ot_native_execute(&ops,"drive-A",1,&result,&error)==OT_NATIVE_IOCTL);
    assert(error==5 && strcmp(f.trace,"OIQXC")==0);
    reset(&f); f.close_error=53;
    assert(ot_native_execute(&ops,"drive-A",1,&result,&error)==OT_NATIVE_CLOSE);
    assert(error==53 && result.completion_flags==0x0b);
    readers(&f, &ops.query);
    writers(&f, &ops.query);
    fixed_sessions(&f,&ops.query);
    spaces(&f, &ops);
    media(&f, &ops);
    puts("OpenTape native API lifecycle/guard/control/record/writer/fixed tests passed");
    return 0;
}

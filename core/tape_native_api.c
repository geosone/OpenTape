#include <string.h>
#include "opentape/native_tape.h"

static int ops_valid(const struct ot_native_ops *ops, const char *name)
{
    return ops != 0 && name != 0 && *name != 0 && ops->valid_name != 0 &&
        ops->open_readonly != 0 && ops->is_device != 0 && ops->query != 0 &&
        ops->close != 0 && ops->valid_name(ops->context, name);
}

static int guards_ok(const unsigned char *before, const unsigned char *after)
{
    unsigned i;
    for (i = 0; i < 8u; ++i)
        if (before[i] != 0xa5u || after[i] != 0xa5u) return 0;
    return 1;
}

static int query_opened(const struct ot_native_ops *ops, oa_u16 flags,
                        struct ot_tape_info_v1 *info, unsigned *error)
{
    struct { unsigned char before[8]; struct ot_tape_info_v1 packet; unsigned char after[8]; } buffer;
    struct ot_tape_info_v1 *p = &buffer.packet;
    memset(&buffer, 0xa5, sizeof(buffer));
    ot_tape_info_init(p, flags);
    *error = ops->query(ops->context, p);
    if (!guards_ok(buffer.before, buffer.after)) return OT_NATIVE_RESPONSE;
    if (*error != 0u) return OT_NATIVE_IOCTL;
    if (ot_tape_info_validate(p, sizeof(*p)) != 0 || p->flags != flags)
        return OT_NATIVE_RESPONSE;
    memcpy(info, p, sizeof(*p));
    return p->result == OT_TAPE_RESULT_BUSY ? OT_NATIVE_BUSY :
        p->result == OT_TAPE_RESULT_PARTIAL ? OT_NATIVE_PARTIAL : OT_NATIVE_OK;
}

static int verify_opened(const struct ot_native_ops *ops, unsigned *error)
{
    unsigned is_device = 0u;
    *error = ops->is_device(ops->context, &is_device);
    return *error != 0u || !is_device ? OT_NATIVE_DEVICE : OT_NATIVE_OK;
}

static int close_opened(const struct ot_native_ops *ops, int result, unsigned *error)
{
    unsigned close_error = ops->close(ops->context);
    if (result == OT_NATIVE_OK && close_error != 0u) {
        *error = close_error;
        return OT_NATIVE_CLOSE;
    }
    return result;
}

int ot_native_probe(const struct ot_native_ops *ops, const char *name, oa_u16 flags,
                    struct ot_tape_info_v1 *info, unsigned *error)
{
    int rc;
    if (error != 0) *error = 0u;
    if (info != 0) memset(info, 0, sizeof(*info));
    if (info == 0 || error == 0 || (flags & ~OT_TAPE_API_REFRESH) != 0u ||
        !ops_valid(ops, name)) return OT_NATIVE_ARGUMENT;
    *error = ops->open_readonly(ops->context, name);
    if (*error != 0u) return OT_NATIVE_OPEN;
    rc = verify_opened(ops, error);
    if (rc == OT_NATIVE_OK) rc = query_opened(ops, flags, info, error);
    return close_opened(ops, rc, error);
}

/* Keep SPACE classification explicit across old provider headers. Media and
 * removal enums occupy the gap and must never acquire SPACE count semantics. */
int ot_native_space_operation(oa_u16 operation)
{
    return (operation>=OT_TAPE_CONTROL_SPACE_RECORDS && operation<=OT_TAPE_CONTROL_SPACE_EOD)
#ifdef OT_NATIVE_TAPE_EXTENDED_SPACE
        || (operation>=OT_TAPE_CONTROL_SPACE_SEQ_FILEMARKS && operation<=OT_TAPE_CONTROL_SPACE_SEQ_SETMARKS)
#endif
        ;
}

static int execute(const struct ot_native_control_ops *ops, const char *name,
                      oa_u16 operation, oa_s32 count, struct ot_tape_control_v1 *result, unsigned *error)
{
    struct { unsigned char before[8]; struct ot_tape_control_v1 packet; unsigned char after[8]; } buffer;
    struct ot_tape_control_v1 *p = &buffer.packet;
    struct ot_tape_info_v1 info;
    oa_u32 required;
    unsigned blocked;
    int rc;
    if (error != 0) *error = 0u;
    if (result != 0) memset(result, 0, sizeof(*result));
    if (ops == 0 || ops->control == 0 || result == 0 || error == 0 ||
#ifdef OT_NATIVE_TAPE_EXTENDED_SPACE
        operation > OT_TAPE_CONTROL_SPACE_SEQ_SETMARKS ||
#elif defined(OT_NATIVE_TAPE_REMOVAL)
        operation > OT_TAPE_CONTROL_UNLOCK ||
#else
        operation > OT_TAPE_CONTROL_UNLOAD ||
#endif
        (operation > OT_TAPE_CONTROL_REWIND && operation < OT_TAPE_CONTROL_SPACE_RECORDS) ||
        !ops_valid(&ops->query, name)) return OT_NATIVE_ARGUMENT;
    if (ot_native_space_operation(operation) &&
        ot_tape_space_init(p, operation, count) != 0)
        return OT_NATIVE_ARGUMENT;
    *error = ops->query.open_readonly(ops->query.context, name);
    if (*error != 0u) return OT_NATIVE_OPEN;
    rc = verify_opened(&ops->query, error);
    if (rc == OT_NATIVE_OK) rc = query_opened(&ops->query, 0, &info, error);
    required = operation == OT_TAPE_CONTROL_REWIND ? OT_TAPE_OP_REWIND : OT_TAPE_OP_READY;
    if (operation == OT_TAPE_CONTROL_SPACE_RECORDS) required = OT_TAPE_OP_SPACE_RECORDS;
    else if (operation == OT_TAPE_CONTROL_SPACE_FILEMARKS) required = OT_TAPE_OP_SPACE_FILEMARKS;
    else if (operation == OT_TAPE_CONTROL_SPACE_EOD) required = OT_TAPE_OP_SPACE_EOD;
    else if (operation == OT_TAPE_CONTROL_LOAD) required = OT_TAPE_OP_LOAD;
    else if (operation == OT_TAPE_CONTROL_UNLOAD) required = OT_TAPE_OP_UNLOAD;
#ifdef OT_NATIVE_TAPE_REMOVAL
    else if (operation == OT_TAPE_CONTROL_LOCK) required = OT_TAPE_OP_LOCK;
    else if (operation == OT_TAPE_CONTROL_UNLOCK) required = OT_TAPE_OP_UNLOCK;
#endif
#ifdef OT_NATIVE_TAPE_EXTENDED_SPACE
    if(operation==OT_TAPE_CONTROL_SPACE_SEQ_FILEMARKS) required=OT_TAPE_OP_SPACE_SEQ_FILEMARKS;
    else if(operation==OT_TAPE_CONTROL_SPACE_SETMARKS) required=OT_TAPE_OP_SPACE_SETMARKS;
    else if(operation==OT_TAPE_CONTROL_SPACE_SEQ_SETMARKS) required=OT_TAPE_OP_SPACE_SEQ_SETMARKS;
#endif
    blocked = OT_TAPE_SESSION_WRITTEN | OT_TAPE_SESSION_BUFFERED | OT_TAPE_SESSION_WRITE_ERROR |
        OT_TAPE_SESSION_NATIVE_WRITER;
    if (operation == OT_TAPE_CONTROL_REWIND) blocked |= OT_TAPE_SESSION_REWINDING;
    if (operation >= OT_TAPE_CONTROL_LOAD) blocked |= OT_TAPE_SESSION_REWINDING;
    if (ot_native_space_operation(operation))
        blocked |= OT_TAPE_SESSION_REWINDING |
        OT_TAPE_SESSION_NATIVE_DIRTY | OT_TAPE_SESSION_NATIVE_FAILED;
    if (rc == OT_NATIVE_OK && (info.operations & required) == 0UL) rc = OT_NATIVE_UNSUPPORTED;
    if (rc == OT_NATIVE_OK && (info.session_state & blocked) != 0u) rc = OT_NATIVE_BUSY;
    if (rc == OT_NATIVE_OK) {
        memset(&buffer, 0xa5, sizeof(buffer));
        if (ot_native_space_operation(operation))
            ot_tape_space_init(p, operation, count);
        else ot_tape_control_init(p, operation);
        *error = ops->control(ops->query.context, p);
        if (!guards_ok(buffer.before, buffer.after)) rc = OT_NATIVE_RESPONSE;
        else if (*error != 0u) rc = OT_NATIVE_IOCTL;
        else if (ot_tape_control_validate(p, sizeof(*p)) != 0 || p->operation != operation || p->count != count)
            rc = OT_NATIVE_RESPONSE;
        else {
            memcpy(result, p, sizeof(*p));
            if (p->result == OT_TAPE_RESULT_BUSY) rc = OT_NATIVE_BUSY;
            else if (p->result == OT_TAPE_RESULT_ERROR) rc = OT_NATIVE_COMMAND;
        }
    }
    return close_opened(&ops->query, rc, error);
}

int ot_native_execute(const struct ot_native_control_ops *ops, const char *name,
                      oa_u16 operation, struct ot_tape_control_v1 *result, unsigned *error)
{
    return execute(ops, name, (operation <= OT_TAPE_CONTROL_REWIND ||
#ifdef OT_NATIVE_TAPE_REMOVAL
                   (operation >= OT_TAPE_CONTROL_LOAD && operation <= OT_TAPE_CONTROL_UNLOCK)
#else
                   (operation >= OT_TAPE_CONTROL_LOAD && operation <= OT_TAPE_CONTROL_UNLOAD)
#endif
                   ) ? operation : 0xffffu,
                   0, result, error);
}
int ot_native_space(const struct ot_native_control_ops *ops, const char *name,
                    oa_u16 operation, oa_s32 count, struct ot_tape_control_v1 *result, unsigned *error)
{
    return execute(ops, name, ot_native_space_operation(operation) ? operation : 0xffffu,
                   count, result, error);
}

static int reader_open(struct ot_native_reader *r,
                          const struct ot_native_reader_ops *ops, const char *name, oa_u32 block_size)
{
    struct ot_tape_info_v1 info;
    int rc;
    if (r == 0 || r->opened || block_size > 65535UL || ops == 0 || ops->read == 0 ||
        !ops_valid(&ops->query, name)) return OT_NATIVE_ARGUMENT;
    memset(r, 0, sizeof(*r));
    r->ops = ops;
    r->platform_error = ops->query.open_readonly(ops->query.context, name);
    if (r->platform_error != 0u) return OT_NATIVE_OPEN;
    rc = verify_opened(&ops->query, &r->platform_error);
    if (rc == OT_NATIVE_OK) rc = query_opened(&ops->query, 0u, &info, &r->platform_error);
    if (rc == OT_NATIVE_OK && (info.operations & OT_TAPE_OP_READ) == 0UL)
        rc = OT_NATIVE_UNSUPPORTED;
    if (rc == OT_NATIVE_OK && block_size != 0UL && (info.operations & OT_TAPE_OP_FIXED_IO) == 0UL)
        rc = OT_NATIVE_UNSUPPORTED;
    if (rc == OT_NATIVE_OK && info.session_state != 0u) rc = OT_NATIVE_BUSY;
    if (rc != OT_NATIVE_OK) return close_opened(&ops->query, rc, &r->platform_error);
    r->block_size = block_size;
    r->opened = 1u;
    return OT_NATIVE_OK;
}

int ot_native_reader_open(struct ot_native_reader *r, const struct ot_native_reader_ops *ops, const char *name)
{ return reader_open(r, ops, name, 0UL); }
int ot_native_reader_open_fixed(struct ot_native_reader *r, const struct ot_native_reader_ops *ops,
                                 const char *name, oa_u32 block_size)
{
    if (block_size == 0UL) return OT_NATIVE_ARGUMENT;
    return reader_open(r, ops, name, block_size);
}

int ot_native_reader_read(struct ot_native_reader *r, void *data, unsigned capacity)
{
    struct { unsigned char before[8]; struct ot_tape_record_v1 packet; unsigned char after[8]; } buffer;
    struct ot_tape_record_v1 *p = &buffer.packet;
    int rc = OT_NATIVE_OK;
    if (r == 0 || !r->opened || data == 0 || capacity == 0u ||
        (oa_u32)capacity > 65535UL ||
        (r->block_size != 0UL && (oa_u32)capacity % r->block_size != 0UL)) return OT_NATIVE_ARGUMENT;
    if (r->failed) return OT_NATIVE_COMMAND;
    memset(&r->last, 0, sizeof(r->last));
    memset(&buffer, 0xa5, sizeof(buffer));
    if (r->block_size != 0UL) ot_tape_record_fixed_init(p, OT_TAPE_RECORD_READ, (oa_u32)capacity, r->block_size);
    else ot_tape_record_init(p, (oa_u32)capacity);
    r->platform_error = r->ops->read(r->ops->query.context, data, capacity, p);
    if (!guards_ok(buffer.before, buffer.after)) rc = OT_NATIVE_RESPONSE;
    else if (r->platform_error != 0u) rc = OT_NATIVE_IOCTL;
    else if (ot_tape_record_validate(p, sizeof(*p)) != 0 || p->requested != (oa_u32)capacity ||
             p->operation != OT_TAPE_RECORD_READ || p->block_size != r->block_size ||
             p->flags != (r->block_size ? OT_TAPE_IO_FIXED : 0u))
        rc = OT_NATIVE_RESPONSE;
    else {
        memcpy(&r->last, p, sizeof(*p));
        if (p->result == OT_TAPE_RESULT_STREAM) rc = OT_NATIVE_STREAM;
        else if (p->result == OT_TAPE_RESULT_BUSY) rc = OT_NATIVE_BUSY;
        else if (p->result == OT_TAPE_RESULT_UNSUPPORTED) rc = OT_NATIVE_UNSUPPORTED;
        else if (p->result == OT_TAPE_RESULT_ERROR) rc = OT_NATIVE_COMMAND;
    }
    if (rc < 0) r->failed = 1u;
    return rc;
}

int ot_native_reader_close(struct ot_native_reader *r)
{
    unsigned error;
    if (r == 0 || !r->opened) return OT_NATIVE_ARGUMENT;
    r->opened = 0u; /* One attempt, even if the platform reports failure. */
    error = r->ops->query.close(r->ops->query.context);
    if (error == 0u) return OT_NATIVE_OK;
    if (!r->failed) r->platform_error = error;
    return OT_NATIVE_CLOSE;
}

static int writer_control(struct ot_native_writer *w, oa_u16 operation)
{
    struct { unsigned char before[8]; struct ot_tape_control_v1 packet; unsigned char after[8]; } buffer;
    struct ot_tape_control_v1 *p = &buffer.packet;
    unsigned error;
    int rc = OT_NATIVE_OK;
    if (operation != OT_TAPE_CONTROL_ABORT_WRITE || w->failure == 0)
        memset(&w->last_control, 0, sizeof(w->last_control));
    memset(&buffer, 0xa5, sizeof(buffer));
    if (operation == OT_TAPE_CONTROL_BEGIN_WRITE && w->block_size != 0UL)
        ot_tape_control_fixed_init(p, w->block_size);
    else ot_tape_control_init(p, operation);
    error = w->ops->control.control(w->ops->control.query.context, p);
    if (!guards_ok(buffer.before, buffer.after)) rc = OT_NATIVE_RESPONSE;
    else if (error != 0u) rc = OT_NATIVE_IOCTL;
    else if (ot_tape_control_validate(p, sizeof(*p)) != 0 || p->operation != operation ||
             p->block_size != (operation == OT_TAPE_CONTROL_BEGIN_WRITE ? w->block_size : 0UL) ||
             p->flags != (operation == OT_TAPE_CONTROL_BEGIN_WRITE && w->block_size ? OT_TAPE_IO_FIXED : 0u))
        rc = OT_NATIVE_RESPONSE;
    else {
        if (operation != OT_TAPE_CONTROL_ABORT_WRITE || w->failure == 0)
            memcpy(&w->last_control, p, sizeof(*p));
        if (p->result == OT_TAPE_RESULT_BUSY) rc = OT_NATIVE_BUSY;
        else if (p->result == OT_TAPE_RESULT_UNSUPPORTED) rc = OT_NATIVE_UNSUPPORTED;
        else if (p->result == OT_TAPE_RESULT_ERROR) rc = OT_NATIVE_COMMAND;
    }
    if (w->failure == 0) {
        w->platform_error = error;
        if (rc < 0) w->failure = rc;
    }
    return rc;
}

static int writer_open(struct ot_native_writer *w,
                          const struct ot_native_writer_ops *ops, const char *name, oa_u32 block_size)
{
    struct ot_tape_info_v1 info;
    oa_u32 required = OT_TAPE_OP_WRITE | OT_TAPE_OP_BEGIN_WRITE | OT_TAPE_OP_FLUSH |
        OT_TAPE_OP_FILEMARK | OT_TAPE_OP_END_WRITE | OT_TAPE_OP_ABORT_WRITE;
    int rc;
    if (w == 0 || w->opened || block_size > 65535UL || ops == 0 || ops->open_write == 0 || ops->write == 0 ||
        ops->control.control == 0 || !ops_valid(&ops->control.query, name)) return OT_NATIVE_ARGUMENT;
    memset(w, 0, sizeof(*w));
    w->ops = ops;
    w->block_size = block_size;
    if (block_size != 0UL) required |= OT_TAPE_OP_FIXED_IO;
    w->platform_error = ops->open_write(ops->control.query.context, name);
    if (w->platform_error != 0u) return OT_NATIVE_OPEN;
    rc = verify_opened(&ops->control.query, &w->platform_error);
    if (rc == OT_NATIVE_OK) rc = query_opened(&ops->control.query, 0u, &info, &w->platform_error);
    if (rc == OT_NATIVE_OK && (info.operations & required) != required) rc = OT_NATIVE_UNSUPPORTED;
    if (rc == OT_NATIVE_OK && info.session_state != 0u) rc = OT_NATIVE_BUSY;
    if (rc == OT_NATIVE_OK) {
        w->operations=info.operations;
        rc = writer_control(w, OT_TAPE_CONTROL_BEGIN_WRITE);
    }
    if (rc != OT_NATIVE_OK) return close_opened(&ops->control.query, rc, &w->platform_error);
    w->opened = w->claimed = 1u;
    return OT_NATIVE_OK;
}

int ot_native_writer_open(struct ot_native_writer *w, const struct ot_native_writer_ops *ops, const char *name)
{ return writer_open(w, ops, name, 0UL); }
int ot_native_writer_open_fixed(struct ot_native_writer *w, const struct ot_native_writer_ops *ops,
                                 const char *name, oa_u32 block_size)
{
    if (block_size == 0UL) return OT_NATIVE_ARGUMENT;
    return writer_open(w, ops, name, block_size);
}

int ot_native_writer_write(struct ot_native_writer *w, const void *data, unsigned bytes)
{
    struct { unsigned char before[8]; struct ot_tape_record_v1 packet; unsigned char after[8]; } buffer;
    struct ot_tape_record_v1 *p = &buffer.packet;
    int rc = OT_NATIVE_OK;
    if (w == 0 || !w->opened || !w->claimed || data == 0 || bytes == 0u ||
        (oa_u32)bytes > 65535UL ||
        (w->block_size != 0UL && (oa_u32)bytes % w->block_size != 0UL)) return OT_NATIVE_ARGUMENT;
    if (w->failure != 0) return w->failure;
    memset(&w->last_record, 0, sizeof(w->last_record));
    memset(&buffer, 0xa5, sizeof(buffer));
    if (w->block_size != 0UL) ot_tape_record_fixed_init(p, OT_TAPE_RECORD_WRITE, (oa_u32)bytes, w->block_size);
    else ot_tape_write_init(p, (oa_u32)bytes);
    w->platform_error = w->ops->write(w->ops->control.query.context, data, bytes, p);
    if (!guards_ok(buffer.before, buffer.after)) rc = OT_NATIVE_RESPONSE;
    else if (w->platform_error != 0u) rc = OT_NATIVE_IOCTL;
    else if (ot_tape_record_validate(p, sizeof(*p)) != 0 || p->requested != (oa_u32)bytes ||
             p->operation != OT_TAPE_RECORD_WRITE || p->block_size != w->block_size ||
             p->flags != (w->block_size ? OT_TAPE_IO_FIXED : 0u)) rc = OT_NATIVE_RESPONSE;
    else {
        memcpy(&w->last_record, p, sizeof(*p));
        if (p->result == OT_TAPE_RESULT_BUSY) rc = OT_NATIVE_BUSY;
        else if (p->result == OT_TAPE_RESULT_UNSUPPORTED) rc = OT_NATIVE_UNSUPPORTED;
        else if (p->result == OT_TAPE_RESULT_ERROR) rc = OT_NATIVE_COMMAND;
    }
    if (rc < 0) w->failure = rc;
    return rc;
}

static int writer_operation(struct ot_native_writer *w, oa_u16 operation)
{
    int rc;
    if (w == 0 || !w->opened || !w->claimed) return OT_NATIVE_ARGUMENT;
    if (w->failure != 0 && operation != OT_TAPE_CONTROL_ABORT_WRITE) return w->failure;
#ifdef OT_NATIVE_TAPE_WRITE_SETMARK
    if(operation==OT_TAPE_CONTROL_SETMARK && !(w->operations & OT_TAPE_OP_SETMARK))
        return OT_NATIVE_UNSUPPORTED; /* No command, failure latch or changed diagnostics. */
#endif
    rc = writer_control(w, operation);
    if (rc == OT_NATIVE_OK && (operation == OT_TAPE_CONTROL_END_WRITE || operation == OT_TAPE_CONTROL_ABORT_WRITE)) {
        w->claimed = 0u;
        if (operation == OT_TAPE_CONTROL_ABORT_WRITE) w->unfinished = 1u;
    }
    return rc;
}
int ot_native_writer_flush(struct ot_native_writer *w)
{ return writer_operation(w, OT_TAPE_CONTROL_FLUSH); }
int ot_native_writer_filemark(struct ot_native_writer *w)
{ return writer_operation(w, OT_TAPE_CONTROL_FILEMARK); }
#ifdef OT_NATIVE_TAPE_WRITE_SETMARK
int ot_native_writer_setmark(struct ot_native_writer *w)
{ return writer_operation(w, OT_TAPE_CONTROL_SETMARK); }
#endif
int ot_native_writer_finish(struct ot_native_writer *w)
{ return writer_operation(w, OT_TAPE_CONTROL_END_WRITE); }
int ot_native_writer_abort(struct ot_native_writer *w)
{ return writer_operation(w, OT_TAPE_CONTROL_ABORT_WRITE); }

int ot_native_writer_close(struct ot_native_writer *w)
{
    unsigned error;
    int rc;
    if (w == 0 || !w->opened) return OT_NATIVE_ARGUMENT;
    if (w->claimed) {
        w->unfinished = 1u;
        (void)ot_native_writer_abort(w); /* ownership only, never a data/mark CDB */
    }
    w->opened = 0u;
    error = w->ops->control.query.close(w->ops->control.query.context);
    rc = w->failure != 0 ? w->failure : w->unfinished ? OT_NATIVE_UNFINISHED : OT_NATIVE_OK;
    if (rc == OT_NATIVE_OK && error != 0u) {
        w->platform_error = error;
        rc = OT_NATIVE_CLOSE;
    }
    return rc;
}

#ifdef OT_NATIVE_TAPE_POSITION
/* Observation has a distinct packet/callback: never route it through a moving
 * control or treat a reported address as recovery of abandoned native output. */
int ot_native_position(const struct ot_native_position_ops *ops, const char *name,
                       struct ot_tape_position_v1 *result, unsigned *error)
{
    struct { unsigned char before[8]; struct ot_tape_position_v1 packet; unsigned char after[8]; } buffer;
    struct ot_tape_position_v1 *p=&buffer.packet;
    struct ot_tape_info_v1 info;
    int rc;
    if(error!=0) *error=0;
    if(result!=0) memset(result,0,sizeof(*result));
    if(ops==0 || ops->position==0 || result==0 || error==0 || !ops_valid(&ops->query,name))
        return OT_NATIVE_ARGUMENT;
    *error=ops->query.open_readonly(ops->query.context,name);
    if(*error!=0) return OT_NATIVE_OPEN;
    rc=verify_opened(&ops->query,error);
    if(rc==0) rc=query_opened(&ops->query,0,&info,error);
    if(rc==0 && (info.operations & OT_TAPE_OP_POSITION)==0UL) rc=OT_NATIVE_UNSUPPORTED;
    if(rc==0 && (info.session_state & 0x1fu)!=0u) rc=OT_NATIVE_BUSY;
    if(rc==0) {
        memset(&buffer,0xa5,sizeof(buffer)); ot_tape_position_init(p);
        *error=ops->position(ops->query.context,p);
        if(!guards_ok(buffer.before,buffer.after)) rc=OT_NATIVE_RESPONSE;
        else if(*error!=0) rc=OT_NATIVE_IOCTL;
        else if(ot_tape_position_validate(p,sizeof(*p))!=0) rc=OT_NATIVE_RESPONSE;
        else {
            *result=*p;
            if(p->result==OT_TAPE_RESULT_BUSY) rc=OT_NATIVE_BUSY;
            else if(p->result==OT_TAPE_RESULT_ERROR) rc=OT_NATIVE_COMMAND;
            else if(p->result==OT_TAPE_RESULT_PARTIAL || p->valid_fields==0UL) rc=OT_NATIVE_PARTIAL;
        }
    }
    return close_opened(&ops->query,rc,error);
}
#endif

#ifdef OT_NATIVE_TAPE_LOCATE
/* Match the echoed target as well as the completion packet. A successful
 * command cannot silently substitute another partition, block or CP policy. */
int ot_native_locate(const struct ot_native_locate_ops *ops,const char *name,oa_u16 flags,
                     oa_u32 partition,oa_u32 block,struct ot_tape_locate_v1 *result,unsigned *error)
{
    struct { unsigned char before[8]; struct ot_tape_locate_v1 packet; unsigned char after[8]; } buffer;
    struct ot_tape_locate_v1 *p=&buffer.packet;
    struct ot_tape_info_v1 info;
    int rc;
    if(error!=0) *error=0;
    if(result!=0) memset(result,0,sizeof(*result));
    if(ops==0 || ops->locate==0 || result==0 || error==0 || !ops_valid(&ops->query,name) ||
       ot_tape_locate_init(p,flags,partition,block)!=0) return OT_NATIVE_ARGUMENT;
    *error=ops->query.open_readonly(ops->query.context,name);
    if(*error!=0) return OT_NATIVE_OPEN;
    rc=verify_opened(&ops->query,error);
    if(rc==0) rc=query_opened(&ops->query,0,&info,error);
    if(rc==0 && (info.operations & OT_TAPE_OP_LOCATE)==0UL) rc=OT_NATIVE_UNSUPPORTED;
    if(rc==0 && info.session_state!=0u) rc=OT_NATIVE_BUSY;
    if(rc==0) {
        memset(&buffer,0xa5,sizeof(buffer)); ot_tape_locate_init(p,flags,partition,block);
        *error=ops->locate(ops->query.context,p);
        if(!guards_ok(buffer.before,buffer.after)) rc=OT_NATIVE_RESPONSE;
        else if(*error!=0) rc=OT_NATIVE_IOCTL;
        else if(ot_tape_locate_validate(p,sizeof(*p))!=0 || p->flags!=flags ||
                p->partition!=partition || p->block!=block) rc=OT_NATIVE_RESPONSE;
        else {
            *result=*p;
            if(p->result==OT_TAPE_RESULT_BUSY) rc=OT_NATIVE_BUSY;
            else if(p->result==OT_TAPE_RESULT_ERROR) rc=OT_NATIVE_COMMAND;
        }
    }
    return close_opened(&ops->query,rc,error);
}
#endif

#ifdef OT_NATIVE_TAPE_WIDE
/* Wide coordinates keep explicit low/high DWORDs through callback and echo
 * checks; no truncation or implicit short-form fallback is allowed. */
int ot_native_position64(const struct ot_native_position64_ops *ops, const char *name,
                       struct ot_tape_position64_v1 *result, unsigned *error)
{
    struct { unsigned char before[8]; struct ot_tape_position64_v1 packet; unsigned char after[8]; } buffer;
    struct ot_tape_position64_v1 *p=&buffer.packet;
    struct ot_tape_info_v1 info;
    int rc;
    if(error!=0) *error=0;
    if(result!=0) memset(result,0,sizeof(*result));
    if(ops==0 || ops->position==0 || result==0 || error==0 || !ops_valid(&ops->query,name))
        return OT_NATIVE_ARGUMENT;
    *error=ops->query.open_readonly(ops->query.context,name);
    if(*error!=0) return OT_NATIVE_OPEN;
    rc=verify_opened(&ops->query,error);
    if(rc==0) rc=query_opened(&ops->query,0,&info,error);
    if(rc==0 && (info.operations & OT_TAPE_OP_POSITION64)==0UL) rc=OT_NATIVE_UNSUPPORTED;
    if(rc==0 && (info.session_state & 0x1fu)!=0u) rc=OT_NATIVE_BUSY;
    if(rc==0) {
        memset(&buffer,0xa5,sizeof(buffer)); ot_tape_position64_init(p);
        *error=ops->position(ops->query.context,p);
        if(!guards_ok(buffer.before,buffer.after)) rc=OT_NATIVE_RESPONSE;
        else if(*error!=0) rc=OT_NATIVE_IOCTL;
        else if(ot_tape_position64_validate(p,sizeof(*p))!=0) rc=OT_NATIVE_RESPONSE;
        else {
            *result=*p;
            if(p->result==OT_TAPE_RESULT_BUSY) rc=OT_NATIVE_BUSY;
            else if(p->result==OT_TAPE_RESULT_ERROR) rc=OT_NATIVE_COMMAND;
            else if(p->result==OT_TAPE_RESULT_PARTIAL || (p->valid_fields & 1UL)==0UL) rc=OT_NATIVE_PARTIAL;
        }
    }
    return close_opened(&ops->query,rc,error);
}
int ot_native_locate64(const struct ot_native_locate64_ops *ops,const char *name,oa_u16 flags,
                     oa_u32 partition,oa_u32 block,oa_u32 block_high,struct ot_tape_locate64_v1 *result,unsigned *error)
{
    struct { unsigned char before[8]; struct ot_tape_locate64_v1 packet; unsigned char after[8]; } buffer;
    struct ot_tape_locate64_v1 *p=&buffer.packet;
    struct ot_tape_info_v1 info;
    int rc;
    if(error!=0) *error=0;
    if(result!=0) memset(result,0,sizeof(*result));
    if(ops==0 || ops->locate==0 || result==0 || error==0 || !ops_valid(&ops->query,name) ||
       ot_tape_locate64_init(p,flags,partition,block,block_high)!=0) return OT_NATIVE_ARGUMENT;
    *error=ops->query.open_readonly(ops->query.context,name);
    if(*error!=0) return OT_NATIVE_OPEN;
    rc=verify_opened(&ops->query,error);
    if(rc==0) rc=query_opened(&ops->query,0,&info,error);
    if(rc==0 && (info.operations & OT_TAPE_OP_LOCATE64)==0UL) rc=OT_NATIVE_UNSUPPORTED;
    if(rc==0 && info.session_state!=0u) rc=OT_NATIVE_BUSY;
    if(rc==0) {
        memset(&buffer,0xa5,sizeof(buffer)); ot_tape_locate64_init(p,flags,partition,block,block_high);
        *error=ops->locate(ops->query.context,p);
        if(!guards_ok(buffer.before,buffer.after)) rc=OT_NATIVE_RESPONSE;
        else if(*error!=0) rc=OT_NATIVE_IOCTL;
        else if(ot_tape_locate64_validate(p,sizeof(*p))!=0 || p->flags!=flags ||
                p->partition!=partition || p->block!=block || p->block_high!=block_high) rc=OT_NATIVE_RESPONSE;
        else {
            *result=*p;
            if(p->result==OT_TAPE_RESULT_BUSY) rc=OT_NATIVE_BUSY;
            else if(p->result==OT_TAPE_RESULT_ERROR) rc=OT_NATIVE_COMMAND;
        }
    }
    return close_opened(&ops->query,rc,error);
}
#endif

#ifdef OT_NATIVE_TAPE_PARTITIONS
/* Layout reports have their own checked packet/callback. Pending/stale counts
 * never authorize movement or recover an abandoned native writer. */
int ot_native_partitions(const struct ot_native_partitions_ops *ops, const char *name,
                       struct ot_tape_partitions_v1 *result, unsigned *error)
{
    struct { unsigned char before[8]; struct ot_tape_partitions_v1 packet; unsigned char after[8]; } buffer;
    struct ot_tape_partitions_v1 *p=&buffer.packet;
    struct ot_tape_info_v1 info;
    int rc;
    if(error!=0) *error=0;
    if(result!=0) memset(result,0,sizeof(*result));
    if(ops==0 || ops->partitions==0 || result==0 || error==0 || !ops_valid(&ops->query,name))
        return OT_NATIVE_ARGUMENT;
    *error=ops->query.open_readonly(ops->query.context,name);
    if(*error!=0) return OT_NATIVE_OPEN;
    rc=verify_opened(&ops->query,error);
    if(rc==0) rc=query_opened(&ops->query,0,&info,error);
    if(rc==0 && (info.operations & OT_TAPE_OP_PARTITIONS)==0UL) rc=OT_NATIVE_UNSUPPORTED;
    if(rc==0 && (info.session_state & 0x1fu)!=0u) rc=OT_NATIVE_BUSY;
    if(rc==0) {
        memset(&buffer,0xa5,sizeof(buffer)); ot_tape_partitions_init(p);
        *error=ops->partitions(ops->query.context,p);
        if(!guards_ok(buffer.before,buffer.after)) rc=OT_NATIVE_RESPONSE;
        else if(*error!=0) rc=OT_NATIVE_IOCTL;
        else if(ot_tape_partitions_validate(p,sizeof(*p))!=0) rc=OT_NATIVE_RESPONSE;
        else {
            *result=*p;
            if(p->result==OT_TAPE_RESULT_BUSY) rc=OT_NATIVE_BUSY;
            else if(p->result==OT_TAPE_RESULT_ERROR) rc=OT_NATIVE_COMMAND;
            else if(p->result==OT_TAPE_RESULT_PARTIAL || p->valid_fields==0UL) rc=OT_NATIVE_PARTIAL;
        }
    }
    return close_opened(&ops->query,rc,error);
}
#endif

/* Decimal parsing is bounded before multiplication; even LONG_MIN-like input
 * is rejected without signed overflow or opening a device. */
int ot_native_space_count_parse(const char *s, oa_s32 *out)
{
    oa_u32 value = 0UL, maximum;
    int negative;
    if(s==0 || out==0) return -1;
    negative = *s == '-';
    if (negative) ++s;
    if (*s == 0) return -1;
    maximum = negative ? 8388608UL : 8388607UL;
    while (*s) {
        unsigned digit;
        if (*s < '0' || *s > '9') return -1;
        digit = (unsigned)(*s++ - '0');
        if (value > (maximum - digit) / 10UL) return -1;
        value = value * 10UL + digit;
    }
    *out = negative ? -(oa_s32)value : (oa_s32)value;
    return 0;
}

#ifdef OT_NATIVE_TAPE_ALERTS
/* Explicit acknowledged log retrieval, without implicit status polling or
 * retry. Preserve first failure through the single close attempt. */
int ot_native_alerts(const struct ot_native_alerts_ops *ops, const char *name,oa_u16 flags,
                       struct ot_tape_alerts_v1 *result, unsigned *error)
{
    struct { unsigned char before[8]; struct ot_tape_alerts_v1 packet; unsigned char after[8]; } buffer;
    struct ot_tape_alerts_v1 *p=&buffer.packet;
    struct ot_tape_info_v1 info;
    int rc;
    if(error!=0) *error=0;
    if(result!=0) memset(result,0,sizeof(*result));
    if(ops==0 || ops->alerts==0 || result==0 || error==0 || !ops_valid(&ops->query,name) ||
       ot_tape_alerts_init(p,flags)!=0)
        return OT_NATIVE_ARGUMENT;
    *error=ops->query.open_readonly(ops->query.context,name);
    if(*error!=0) return OT_NATIVE_OPEN;
    rc=verify_opened(&ops->query,error);
    if(rc==0) rc=query_opened(&ops->query,0,&info,error);
    if(rc==0 && (info.operations & OT_TAPE_OP_ALERTS)==0UL) rc=OT_NATIVE_UNSUPPORTED;
    if(rc==0 && (info.session_state & 0x1fu)!=0u) rc=OT_NATIVE_BUSY;
    if(rc==0) {
        memset(&buffer,0xa5,sizeof(buffer)); ot_tape_alerts_init(p,flags);
        *error=ops->alerts(ops->query.context,p);
        if(!guards_ok(buffer.before,buffer.after)) rc=OT_NATIVE_RESPONSE;
        else if(*error!=0) rc=OT_NATIVE_IOCTL;
        else if(ot_tape_alerts_validate(p,sizeof(*p))!=0) rc=OT_NATIVE_RESPONSE;
        else {
            *result=*p;
            if(p->result==OT_TAPE_RESULT_BUSY) rc=OT_NATIVE_BUSY;
            else if(p->result==OT_TAPE_RESULT_ERROR) rc=OT_NATIVE_COMMAND;
            else if(p->result==OT_TAPE_RESULT_PARTIAL || p->valid_fields==0UL) rc=OT_NATIVE_PARTIAL;
        }
    }
    return close_opened(&ops->query,rc,error);
}
#endif

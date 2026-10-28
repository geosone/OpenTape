#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <share.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "opentape/tape_probe.h"
#include "opentape/tape_reader.h"
#include "opentape/tape_output.h"
#include "opentape/source_file.h"
#include "opentape/backup_plan.h"
#include "opentape/restore.h"
#include "opentape/messages.h"
#include "opentape/local_name.h"
#ifdef OT_NATIVE_TAPE_API
#include "opentape/native_tape.h"
#endif

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

static unsigned dos_ioctl(void *context, unsigned handle, unsigned category,
                           unsigned function, void *buffer, unsigned size)
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
    in.x.cx = (category << 8) | function;
    in.x.dx = FP_OFF(buffer);
    segments.ds = FP_SEG(buffer);
    old = fail_begin();
    intdosx(&in, &out, &segments);
    _dos_setvect(0x24, old);
    return out.x.cflag ? out.x.ax : 0u;
}

static unsigned dos_status(void *context, unsigned handle, unsigned function,
                            void *buffer, unsigned size)
{
    return dos_ioctl(context, handle, OA_ASPITAPE_IOCTL_CATEGORY, function, buffer, size);
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

#ifdef OT_NATIVE_TAPE_API
static int native_dos_name(void *context, const char *name)
{
    (void)context;
    return ot_probe_raw_name_valid(name);
}
static unsigned native_dos_open(void *context, const char *name)
{
    struct ot_dos_native_session *s = (struct ot_dos_native_session *)context;
    return dos_open(0, name, &s->handle);
}
static unsigned native_dos_device(void *context, unsigned *device)
{
    struct ot_dos_native_session *s = (struct ot_dos_native_session *)context;
    unsigned flags = 0u, error = dos_info(0, s->handle, &flags);
    *device = (flags & 0x80u) != 0u;
    return error;
}
static unsigned native_dos_query(void *context, struct ot_tape_info_v1 *packet)
{
    struct ot_dos_native_session *s = (struct ot_dos_native_session *)context;
    return dos_ioctl(0, s->handle, OT_TAPE_API_CATEGORY, OT_TAPE_API_QUERY, packet, sizeof(*packet));
}
static unsigned native_dos_close(void *context)
{
    struct ot_dos_native_session *s = (struct ot_dos_native_session *)context;
    return dos_close(0, s->handle);
}
static unsigned native_dos_control(void *context, struct ot_tape_control_v1 *packet)
{
    struct ot_dos_native_session *s = (struct ot_dos_native_session *)context;
    return dos_ioctl(0, s->handle, OT_TAPE_API_CATEGORY, OT_TAPE_API_CONTROL, packet, sizeof(*packet));
}
static void native_dos_ops(struct ot_native_ops *ops, struct ot_dos_native_session *session)
{
    ops->context = session;
    ops->valid_name = native_dos_name;
    ops->open_readonly = native_dos_open;
    ops->is_device = native_dos_device;
    ops->query = native_dos_query;
    ops->close = native_dos_close;
}
#ifdef OT_NATIVE_TAPE_POSITION
static unsigned native_dos_position(void *context, struct ot_tape_position_v1 *packet)
{
    struct ot_dos_native_session *s=(struct ot_dos_native_session *)context;
    return dos_ioctl(0,s->handle,OT_TAPE_API_CATEGORY,OT_TAPE_API_POSITION,packet,sizeof(*packet));
}
int ot_dos_native_position(const char *name, struct ot_tape_position_v1 *packet, unsigned *error)
{
    struct ot_dos_native_session session;
    struct ot_native_position_ops ops;
    native_dos_ops(&ops.query,&session); ops.position=native_dos_position;
    return ot_native_position(&ops,name,packet,error);
}
#endif
#ifdef OT_NATIVE_TAPE_LOCATE
static unsigned native_dos_locate(void *context,struct ot_tape_locate_v1 *packet)
{
    struct ot_dos_native_session *s=(struct ot_dos_native_session *)context;
    return dos_ioctl(0,s->handle,OT_TAPE_API_CATEGORY,OT_TAPE_API_LOCATE,packet,sizeof(*packet));
}
int ot_dos_native_locate(const char *name,oa_u16 flags,oa_u32 partition,oa_u32 block,
                         struct ot_tape_locate_v1 *packet,unsigned *error)
{
    struct ot_dos_native_session session;
    struct ot_native_locate_ops ops;
    native_dos_ops(&ops.query,&session); ops.locate=native_dos_locate;
    return ot_native_locate(&ops,name,flags,partition,block,packet,error);
}
#endif
#ifdef OT_NATIVE_TAPE_WIDE
static unsigned native_dos_position64(void *context, struct ot_tape_position64_v1 *packet)
{
    struct ot_dos_native_session *s=(struct ot_dos_native_session *)context;
    return dos_ioctl(0,s->handle,OT_TAPE_API_CATEGORY,OT_TAPE_API_POSITION,packet,sizeof(*packet));
}
int ot_dos_native_position64(const char *name, struct ot_tape_position64_v1 *packet, unsigned *error)
{
    struct ot_dos_native_session session;
    struct ot_native_position64_ops ops;
    native_dos_ops(&ops.query,&session); ops.position=native_dos_position64;
    return ot_native_position64(&ops,name,packet,error);
}
static unsigned native_dos_locate64(void *context,struct ot_tape_locate64_v1 *packet)
{
    struct ot_dos_native_session *s=(struct ot_dos_native_session *)context;
    return dos_ioctl(0,s->handle,OT_TAPE_API_CATEGORY,OT_TAPE_API_LOCATE,packet,sizeof(*packet));
}
int ot_dos_native_locate64(const char *name,oa_u16 flags,oa_u32 partition,oa_u32 block,oa_u32 block_high,
                         struct ot_tape_locate64_v1 *packet,unsigned *error)
{
    struct ot_dos_native_session session;
    struct ot_native_locate64_ops ops;
    native_dos_ops(&ops.query,&session); ops.locate=native_dos_locate64;
    return ot_native_locate64(&ops,name,flags,partition,block,block_high,packet,error);
}
#endif
int ot_dos_native_probe(const char *name, oa_u16 flags, struct ot_tape_info_v1 *info, unsigned *error)
{
    struct ot_dos_native_session session;
    struct ot_native_ops ops;
    native_dos_ops(&ops, &session);
    return ot_native_probe(&ops, name, flags, info, error);
}
int ot_dos_native_execute(const char *name, oa_u16 operation, struct ot_tape_control_v1 *result, unsigned *error)
{
    struct ot_dos_native_session session;
    struct ot_native_control_ops ops;
    native_dos_ops(&ops.query, &session);
    ops.control = native_dos_control;
    return ot_native_execute(&ops, name, operation, result, error);
}

int ot_dos_native_space(const char *name, oa_u16 operation, oa_s32 count,
                        struct ot_tape_control_v1 *result, unsigned *error)
{
    struct ot_dos_native_session session;
    struct ot_native_control_ops ops;
    native_dos_ops(&ops.query, &session);
    ops.control = native_dos_control;
    return ot_native_space(&ops, name, operation, count, result, error);
}

static unsigned native_dos_read_record(void *context, void *buffer, unsigned capacity,
                                       struct ot_tape_record_v1 *packet)
{
    struct ot_dos_native_session *s = (struct ot_dos_native_session *)context;
    (void)capacity;
    packet->buffer_offset = FP_OFF(buffer);
    packet->buffer_segment = FP_SEG(buffer);
    return dos_ioctl(0, s->handle, OT_TAPE_API_CATEGORY, OT_TAPE_API_READ, packet, sizeof(*packet));
}
int ot_dos_native_reader_open(struct ot_dos_native_reader *r, const char *name)
{
    if (r == 0 || r->reader.opened) return OT_NATIVE_ARGUMENT;
    native_dos_ops(&r->ops.query, &r->session);
    r->ops.read = native_dos_read_record;
    return ot_native_reader_open(&r->reader, &r->ops, name);
}
int ot_dos_native_reader_open_fixed(struct ot_dos_native_reader *r, const char *name, oa_u32 block_size)
{
    if (r == 0 || r->reader.opened || block_size == 0UL || block_size > 65535UL) return OT_NATIVE_ARGUMENT;
    native_dos_ops(&r->ops.query, &r->session);
    r->ops.read = native_dos_read_record;
    return ot_native_reader_open_fixed(&r->reader, &r->ops, name, block_size);
}
int ot_dos_native_reader_read(struct ot_dos_native_reader *r, void *buffer, unsigned capacity)
{
    if (r == 0) return OT_NATIVE_ARGUMENT;
    return ot_native_reader_read(&r->reader, buffer, capacity);
}
int ot_dos_native_reader_close(struct ot_dos_native_reader *r)
{
    if (r == 0) return OT_NATIVE_ARGUMENT;
    return ot_native_reader_close(&r->reader);
}

static unsigned native_dos_open_write(void *context, const char *name)
{
    struct ot_dos_native_session *s = (struct ot_dos_native_session *)context;
    critical_vector old = fail_begin();
    int handle;
    unsigned error = _dos_open(name, O_RDWR, &handle);
    _dos_setvect(0x24, old);
    if (error == 0u) s->handle = (unsigned)handle;
    return error;
}
static unsigned native_dos_write_record(void *context, const void *buffer, unsigned bytes,
                                        struct ot_tape_record_v1 *packet)
{
    struct ot_dos_native_session *s = (struct ot_dos_native_session *)context;
    (void)bytes;
    packet->buffer_offset = FP_OFF(buffer);
    packet->buffer_segment = FP_SEG(buffer);
    return dos_ioctl(0, s->handle, OT_TAPE_API_CATEGORY, OT_TAPE_API_WRITE, packet, sizeof(*packet));
}
int ot_dos_native_writer_open(struct ot_dos_native_writer *w, const char *name)
{
    if (w == 0 || w->writer.opened) return OT_NATIVE_ARGUMENT;
    native_dos_ops(&w->ops.control.query, &w->session);
    w->ops.control.control = native_dos_control;
    w->ops.open_write = native_dos_open_write;
    w->ops.write = native_dos_write_record;
    return ot_native_writer_open(&w->writer, &w->ops, name);
}
int ot_dos_native_writer_open_fixed(struct ot_dos_native_writer *w, const char *name, oa_u32 block_size)
{
    if (w == 0 || w->writer.opened || block_size == 0UL || block_size > 65535UL) return OT_NATIVE_ARGUMENT;
    native_dos_ops(&w->ops.control.query, &w->session);
    w->ops.control.control = native_dos_control;
    w->ops.open_write = native_dos_open_write;
    w->ops.write = native_dos_write_record;
    return ot_native_writer_open_fixed(&w->writer, &w->ops, name, block_size);
}
int ot_dos_native_writer_write(struct ot_dos_native_writer *w, const void *buffer, unsigned bytes)
{
    if (w == 0) return OT_NATIVE_ARGUMENT;
    return ot_native_writer_write(&w->writer, buffer, bytes);
}
int ot_dos_native_writer_flush(struct ot_dos_native_writer *w)
{ return w == 0 ? OT_NATIVE_ARGUMENT : ot_native_writer_flush(&w->writer); }
int ot_dos_native_writer_filemark(struct ot_dos_native_writer *w)
{ return w == 0 ? OT_NATIVE_ARGUMENT : ot_native_writer_filemark(&w->writer); }
#ifdef OT_NATIVE_TAPE_WRITE_SETMARK
int ot_dos_native_writer_setmark(struct ot_dos_native_writer *w)
{ return w == 0 ? OT_NATIVE_ARGUMENT : ot_native_writer_setmark(&w->writer); }
#endif
int ot_dos_native_writer_finish(struct ot_dos_native_writer *w)
{ return w == 0 ? OT_NATIVE_ARGUMENT : ot_native_writer_finish(&w->writer); }
int ot_dos_native_writer_abort(struct ot_dos_native_writer *w)
{ return w == 0 ? OT_NATIVE_ARGUMENT : ot_native_writer_abort(&w->writer); }
int ot_dos_native_writer_close(struct ot_dos_native_writer *w)
{ return w == 0 ? OT_NATIVE_ARGUMENT : ot_native_writer_close(&w->writer); }

#endif

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

static unsigned dos_open_write(void *context, const char *name, unsigned *handle)
{
    critical_vector old;
    unsigned error;
    int opened_handle;
    (void)context;
    old = fail_begin();
    error = _dos_open(name, O_WRONLY, &opened_handle);
    _dos_setvect(0x24, old);
    if (error == 0u) *handle = (unsigned)opened_handle;
    return error;
}

static unsigned dos_write(void *context, unsigned handle, const void *buffer,
                           unsigned size, unsigned *accepted)
{
    critical_vector old;
    unsigned error;
    (void)context;
    old = fail_begin();
    error = _dos_write(handle, buffer, size, accepted);
    _dos_setvect(0x24, old);
    return error;
}

const struct ot_output_ops *ot_dos_output_ops(void)
{
    static const struct ot_output_ops ops = {
        { 0, dos_open, dos_info, dos_status, dos_close },
        dos_open_write, dos_set_raw, dos_write
    };
    return &ops;
}

int ot_dos_tape_probe(const char *name, struct ot_tape_status *status,
                      unsigned *dos_error)
{
    return ot_tape_probe(&ot_dos_reader_ops()->probe, name, status, dos_error);
}

static unsigned dos_rewind(void *context, unsigned handle,
                           const struct oa_aspitape_mtop *request)
{
    struct oa_aspitape_mtop parameter = *request;
    return dos_status(context, handle, OA_ASPITAPE_IOCTL_MTOP,
                      &parameter, sizeof(parameter));
}

int ot_dos_tape_rewind(const char *name, struct ot_tape_status *status, unsigned *error)
{
    static const struct ot_rewind_ops ops = {
        { 0, dos_open, dos_info, dos_status, dos_close }, dos_rewind
    };
    return ot_tape_rewind(&ops, name, status, error);
}

static unsigned source_snapshot(struct ot_dos_source *s, struct ot_file_metadata *m)
{
    critical_vector old;
    unsigned error, attributes, date, time;
    long length;
    old = fail_begin();
    error = _dos_getfileattr(s->name, &attributes);
    if (error == 0u) error = _dos_getftime(s->handle, &date, &time);
    length = error == 0u ? filelength(s->handle) : -1L;
    _dos_setvect(0x24, old);
    if (error == 0u && (length < 0L || (attributes & ~0x27u) != 0u)) error = 5u;
    if (error != 0u) return error;
    *m = s->metadata;
    m->kind = OT_FILE_REGULAR;
    m->dos_attributes = (oa_u16)attributes;
    m->file_size = (oa_u32)length;
    m->dos_date = (oa_u16)date;
    m->dos_time = (oa_u16)time;
    return 0u;
}

static unsigned source_read(void *p, void *buffer, unsigned size, unsigned *got)
{
    struct ot_dos_source *s = p;
    unsigned error = dos_read(0, s->handle, buffer, size, got);
    if (error != 0u) s->dos_error = error;
    return error;
}

static unsigned source_recheck(void *p)
{
    struct ot_dos_source *s = p;
    struct ot_file_metadata now;
    unsigned error = source_snapshot(s, &now);
    if (error != 0u) s->dos_error = error;
    if (error != 0u) return error;
    return now.file_size != s->metadata.file_size ||
        now.dos_date != s->metadata.dos_date || now.dos_time != s->metadata.dos_time ||
        now.dos_attributes != s->metadata.dos_attributes;
}

static unsigned source_close(void *p)
{
    struct ot_dos_source *s = p;
    unsigned error = dos_close(0, s->handle);
    s->opened = 0u;
    if (s->dos_error == 0u) s->dos_error = error;
    return error;
}

int ot_dos_source_open(struct ot_dos_source *s, const char *name, const char *path)
{
    critical_vector old;
    int handle;
    unsigned error, flags, length;
    union { oa_u8 encoded[OT_FILE_METADATA_PREFIX + OT_FILE_PATH_MAX]; char canonical[128]; } scratch;
    oa_u16 size;
    if (s == 0 || s->opened || name == 0 || strlen(name) >= sizeof(s->name) ||
        strchr(name, '*') != 0 || strchr(name, '?') != 0 || ot_local_tape_name(name) || !ot_archive_path_valid(path))
        return OT_SOURCE_ARGUMENT;
    /* Resolve dot/parent components before applying the same device-stem
     * rule. Reuse codec scratch storage; recursive tree planning stays bounded. */
    if (_fullpath(scratch.canonical, name, sizeof(scratch.canonical)) == 0) return OT_SOURCE_ARGUMENT;
    length = (unsigned)strlen(scratch.canonical);
    while (length > 3u && (scratch.canonical[length - 1u] == '\\' || scratch.canonical[length - 1u] == '/'))
        scratch.canonical[--length] = 0;
    if (ot_local_tape_name(scratch.canonical)) return OT_SOURCE_ARGUMENT;
    memset(s, 0, sizeof(*s));
    strcpy(s->name, name);
    strcpy(s->metadata.path, path);
    old = fail_begin();
    error = _dos_open(name, O_RDONLY | SH_DENYWR, &handle);
    _dos_setvect(0x24, old);
    if (error != 0u) { s->dos_error = error; return OT_SOURCE_IO; }
    s->handle = (unsigned)handle;
    s->opened = 1u;
    error = dos_info(0, s->handle, &flags);
    if (error == 0u && (flags & 0x80u) != 0u) error = 5u;
    if (error == 0u) error = source_snapshot(s, &s->metadata);
    s->dos_error = error;
    if (error != 0u || ot_file_metadata_encode(scratch.encoded, sizeof(scratch.encoded), &s->metadata, &size) != 0) {
        source_close(s);
        return OT_SOURCE_IO;
    }
    s->ops.context = s;
    s->ops.read = source_read;
    s->ops.recheck = source_recheck;
    s->ops.close = source_close;
    return 0;
}

static int plan_file(void *p, const struct ot_plan_entry *e,
                      struct ot_file_metadata *m, struct ot_source_ops *ops)
{
    struct ot_dos_source *s = p;
    int rc = ot_dos_source_open(s, e->source, e->metadata.path);
    if (rc == 0) { *m = s->metadata; *ops = s->ops; }
    return rc;
}

static int plan_directory(void *p, const struct ot_plan_entry *e)
{
    struct ot_dos_source *s = p;
    critical_vector old = fail_begin();
    unsigned flags, error = _dos_getfileattr(e->source, &flags);
    _dos_setvect(0x24, old);
    if (error != 0u) s->dos_error = error;
    return error != 0u || flags != e->metadata.dos_attributes ? OT_SOURCE_CHANGED : 0;
}

void ot_dos_plan_ops(struct ot_plan_ops *ops, struct ot_dos_source *source)
{
    ops->context = source;
    ops->directory = plan_directory;
    ops->open_file = plan_file;
}

struct walk_frame {
    struct find_t find;
    char disk[128], path[OT_FILE_PATH_MAX + 1u], pattern[128];
};

static int walk_tree(struct ot_backup_plan *plan, const char *disk, const char *path,
                       const struct ot_file_metadata *directory, unsigned depth, unsigned *dos_error)
{
    struct walk_frame *f;
    struct ot_file_metadata m;
    struct ot_dos_source *source;
    critical_vector old;
    unsigned error;
    int rc;
    if (depth > OT_PLAN_MAX_DEPTH || strlen(disk) + 4u >= 128u) return OT_PLAN_LIMIT;
    rc = ot_plan_add(plan, disk, directory);
    if (rc != 0) return rc;
    f = malloc(sizeof(*f));
    if (f == 0) return OT_PLAN_LIMIT;
    strcpy(f->pattern, disk);
    if (f->pattern[strlen(f->pattern) - 1u] != '\\') strcat(f->pattern, "\\");
    strcat(f->pattern, "*.*");
    old = fail_begin();
    error = _dos_findfirst(f->pattern, 0x37u, &f->find);
    _dos_setvect(0x24, old);
    while (error == 0u && rc == 0) {
        if (strcmp(f->find.name, ".") != 0 && strcmp(f->find.name, "..") != 0) {
            if (strlen(disk) + strlen(f->find.name) + 2u > sizeof(f->disk) ||
                strlen(path) + strlen(f->find.name) + 2u > sizeof(f->path)) { rc = OT_PLAN_LIMIT; break; }
            strcpy(f->disk, disk);
            if (f->disk[strlen(f->disk) - 1u] != '\\') strcat(f->disk, "\\");
            strcat(f->disk, f->find.name);
            strcpy(f->path, path); strcat(f->path, "/"); strcat(f->path, f->find.name);
            memset(&m, 0, sizeof(m));
            m.kind = OT_FILE_DIRECTORY;
            m.dos_attributes = f->find.attrib;
            m.dos_date = f->find.wr_date;
            m.dos_time = f->find.wr_time;
            strcpy(m.path, f->path);
            if ((f->find.attrib & 0x10u) != 0u)
                rc = walk_tree(plan, f->disk, f->path, &m, depth + 1u, dos_error);
            else {
                source = calloc(1u, sizeof(*source));
                if (source == 0) rc = OT_PLAN_LIMIT;
                else {
                    rc = ot_dos_source_open(source, f->disk, f->path);
                    if (rc == 0) {
                        rc = ot_plan_add(plan, f->disk, &source->metadata);
                        if (source_close(source) != 0u && rc == 0) rc = OT_SOURCE_IO;
                    }
                    if (source->dos_error != 0u) *dos_error = source->dos_error;
                    free(source);
                }
            }
        }
        if (rc == 0) {
            old = fail_begin();
            error = _dos_findnext(&f->find);
            _dos_setvect(0x24, old);
        }
    }
    if (error != 0u && error != 18u) { *dos_error = error; if (rc == 0) rc = OT_SOURCE_IO; }
    free(f);
    return rc;
}

int ot_dos_plan_tree(struct ot_backup_plan *plan, const char *name, const char *path, unsigned *dos_error)
{
    char disk[128];
    struct ot_file_metadata m;
    critical_vector old;
    unsigned flags, error;
    if (plan == 0 || plan->count != 0u || name == 0 || name[0] == 0 ||
        strlen(name) >= sizeof(disk) || dos_error == 0 ||
        strchr(name, '*') != 0 || strchr(name, '?') != 0 || !ot_archive_path_valid(path) ||
        _fullpath(disk, name, sizeof(disk)) == 0 || ot_local_tape_name(disk)) return OT_SOURCE_ARGUMENT;
    *dos_error = 0u;
    old = fail_begin();
    error = _dos_getfileattr(disk, &flags);
    _dos_setvect(0x24, old);
    if (error != 0u) { *dos_error = error; return OT_SOURCE_IO; }
    if ((flags & 0x10u) == 0u) return OT_SOURCE_ARGUMENT;
    memset(&m, 0, sizeof(m));
    m.kind = OT_FILE_DIRECTORY;
    m.dos_attributes = (oa_u16)flags;
    strcpy(m.path, path);
    return walk_tree(plan, disk, path, &m, 0u, dos_error);
}

static unsigned restore_error(struct ot_dos_restore *r, unsigned error)
{
    if (r->dos_error == 0u) r->dos_error = error;
    return error;
}

static unsigned make_directory(const char *path)
{
    union REGS in, out;
    struct SREGS segments;
    critical_vector old;
    memset(&in, 0, sizeof(in)); segread(&segments);
    in.x.ax = 0x3900u; in.x.dx = FP_OFF(path); segments.ds = FP_SEG(path);
    old = fail_begin(); intdosx(&in, &out, &segments); _dos_setvect(0x24, old);
    return out.x.cflag ? out.x.ax : 0u;
}

static unsigned path_attributes(const char *path, unsigned *attributes)
{
    critical_vector old = fail_begin();
    unsigned error = _dos_getfileattr(path, attributes);
    _dos_setvect(0x24, old);
    return error;
}

static unsigned restore_path(struct ot_dos_restore *r, const char *relative, char *out, unsigned directory)
{
    unsigned i, flags, error;
    char saved;
    if (!r->created || !ot_archive_path_valid(relative) ||
        strlen(r->root) + strlen(relative) + 2u > 128u) return restore_error(r, 3u);
    strcpy(out, r->root); strcat(out, "\\"); strcat(out, relative);
    for (i = (unsigned)strlen(r->root) + 1u; out[i] != 0; ++i)
        if (out[i] == '/') out[i] = '\\';
    for (i = (unsigned)strlen(r->root) + 1u;; ++i) {
        if (out[i] == '\\' || (directory && out[i] == 0)) {
            saved = out[i]; out[i] = 0;
            error = path_attributes(out, &flags);
            if (error == 2u || error == 3u) {
                if (!r->comparing) error = r->preview ? 0u : make_directory(out);
            }
            else if (error == 0u && (flags & 0x10u) == 0u) error = 5u;
            out[i] = saved;
            if (error != 0u) return restore_error(r, error);
        }
        if (out[i] == 0) break;
    }
    return 0u;
}

static unsigned restore_directory(void *p, const struct ot_file_metadata *m)
{
    struct ot_dos_restore *r = p;
    char path[128];
    unsigned error = restore_path(r, m->path, path, 1u);
    if (error == 0u && r->preview && !r->quiet && printf("DIR %10lu %s\n", 0ul, m->path) < 0)
        error = restore_error(r, 29u);
    return error;
}

static unsigned restore_begin(void *p, const struct ot_file_metadata *m)
{
    struct ot_dos_restore *r = p;
    char path[128];
    unsigned error, flags, selected;
    int handle;
    critical_vector old;
    if (r->opened) return restore_error(r, 5u);
    error = restore_path(r, m->path, path, 0u);
    if (error != 0u) return error;
    if (r->comparing) { r->output_crc = 0ul; return 0u; }
    selected = ot_restore_selected(r->selection, r->selection_count, m->path);
    if (selected != 0u) r->selection_seen[selected - 1u] = 1u;
    r->replacing = 0u;
    error = path_attributes(path, &flags);
    if (error == 0u) {
        if ((flags & 0x10u) != 0u) return restore_error(r, 5u);
        if (selected == 0u) {
            if (r->preview && !r->quiet && printf("KEEP %10lu %s\n", (unsigned long)m->file_size, m->path) < 0)
                return restore_error(r, 29u);
            return OT_RESTORE_SKIP;
        }
        r->replacing = 1u; r->original_attributes = flags;
    } else if (error != 2u && !(r->preview && error == 3u)) return restore_error(r, error);
    if (r->preview) return 0u;
    r->output_crc = 0ul;
    old = fail_begin();
    error = _dos_creatnew(r->temporary, 0u, &handle);
    _dos_setvect(0x24, old);
    if (error == 0u) { r->handle = (unsigned)handle; r->opened = 1u; }
    return restore_error(r, error);
}

static unsigned restore_write(void *p, const oa_u8 *data, unsigned size, unsigned *accepted)
{
    struct ot_dos_restore *r = p;
    unsigned error;
    if (r->comparing) {
        r->output_crc = ot_crc32_extend(r->output_crc, data, (oa_u16)size);
        *accepted = size; return 0u;
    }
    if (r->preview) { (void)data; *accepted = size; return 0u; }
    if (!r->opened) return restore_error(r, 6u);
    error = dos_write(0, r->handle, data, size, accepted);
    if (error == 0u && *accepted != size) error = 29u;
    if (error == 0u) r->output_crc = ot_crc32_extend(r->output_crc, data, (oa_u16)size);
    return restore_error(r, error);
}

static unsigned restore_abort(void *p)
{
    struct ot_dos_restore *r = p;
    unsigned error = 0u;
    if (r->preview) return 0u;
    if (r->opened) {
        error = dos_close(0, r->handle);
        r->opened = 0u;
    }
    return restore_error(r, error);
}

static unsigned rename_path(const char *from, const char *to)
{
    union REGS in, out;
    struct SREGS segments;
    critical_vector old;
    memset(&in, 0, sizeof(in)); segread(&segments);
    in.x.ax = 0x5600u; in.x.dx = FP_OFF(from); segments.ds = FP_SEG(from);
    in.x.di = FP_OFF(to); segments.es = FP_SEG(to);
    old = fail_begin(); intdosx(&in, &out, &segments); _dos_setvect(0x24, old);
    return out.x.cflag ? out.x.ax : 0u;
}

static unsigned set_attributes(const char *path, unsigned flags)
{
    critical_vector old = fail_begin();
    unsigned error = _dos_setfileattr(path, flags);
    _dos_setvect(0x24, old);
    return error;
}

int ot_dos_selection_prepare(struct ot_dos_selection_export *out, const char *name)
{
    char relative[128], *last;
    unsigned i, flags, error;
    if (out == 0) return OT_RESTORE_PATH;
    memset(out, 0, sizeof(*out));
    if (name == 0 || *name == 0 || strlen(name) >= 128u) return OT_RESTORE_PATH;
    if (_fullpath(out->destination, name, sizeof(out->destination)) == 0 ||
        out->destination[1] != ':' || out->destination[2] != '\\') return OT_RESTORE_PATH;
    strcpy(relative, out->destination + 3);
    for (i = 0u; relative[i] != 0; ++i) if (relative[i] == '\\') relative[i] = '/';
    if (!ot_archive_path_valid(relative)) return OT_RESTORE_PATH;
    strcpy(out->temporary, out->destination);
    last = strrchr(out->temporary, '\\');
    if (last == 0 || (unsigned)(last - out->temporary) + 1u + sizeof("OT$LIST.TMP") > sizeof(out->temporary))
        return OT_RESTORE_PATH;
    strcpy(last + 1, "OT$LIST.TMP");
    if (stricmp(out->temporary, out->destination) == 0) return OT_RESTORE_PATH;
    error = path_attributes(out->destination, &flags);
    if (error == 2u) error = path_attributes(out->temporary, &flags);
    if (error != 2u) { out->dos_error = error == 0u ? 80u : error; return OT_RESTORE_IO; }
    /* Parent must already exist; neither missing directories nor devices are created. */
    strcpy(relative, out->temporary); last = strrchr(relative, '\\');
    if (last == relative + 2) last[1] = 0; else *last = 0;
    error = path_attributes(relative, &flags);
    if (error != 0u || (flags & 0x10u) == 0u) {
        out->dos_error = error == 0u ? 3u : error; return OT_RESTORE_IO;
    }
    return 0;
}

static unsigned selection_write(void *p, const oa_u8 *data, unsigned size, unsigned *accepted)
{
    struct ot_dos_selection_export *out = p;
    unsigned error = dos_write(0, out->handle, data, size, accepted);
    if (error == 0u && *accepted != size) error = 29u;
    if (error != 0u) out->dos_error = error;
    return error;
}
static unsigned selection_close(void *p)
{
    struct ot_dos_selection_export *out = p;
    unsigned error = dos_close(0, out->handle);
    if (out->dos_error == 0u) out->dos_error = error;
    return error;
}
static unsigned selection_publish(void *p)
{
    struct ot_dos_selection_export *out = p;
    out->dos_error = rename_path(out->temporary, out->destination);
    return out->dos_error;
}
int ot_dos_selection_save(struct ot_dos_selection_export *out, const struct ot_restore_selection *list)
{
    struct ot_selection_output ops;
    int handle;
    critical_vector old = fail_begin();
    out->dos_error = _dos_creatnew(out->temporary, 0u, &handle);
    _dos_setvect(0x24, old);
    if (out->dos_error != 0u) return OT_RESTORE_IO;
    out->handle = (unsigned)handle;
    ops.context = out; ops.write = selection_write;
    ops.close = selection_close; ops.publish = selection_publish;
    return ot_restore_selection_save(list, &ops);
}

/* Common independent readback for both small blobs and streamed metadata.
 * ot_source_verify owns EOF/recheck/close; rename occurs only after success. */
static int selection_readback_publish(struct ot_dos_selection_export *out, oa_u32 size, oa_u32 crc)
{
    struct ot_dos_source source;
    oa_u8 buffer[256];
    int rc;
    memset(&source, 0, sizeof(source));
    rc = ot_dos_source_open(&source, out->temporary, "VERIFY.TMP");
    if (rc == 0) rc = ot_source_verify(&source.ops, size, crc, buffer, sizeof(buffer));
    if (source.dos_error != 0u) out->dos_error = source.dos_error;
    if (rc != 0) return rc;
    return selection_publish(out) != 0u ? OT_RESTORE_IO : 0;
}

/* Generic small profile publication shares selection export's DOS guards and
 * staging name, adding independent length/CRC/readback before the rename. */
int ot_dos_file_save_blob(struct ot_dos_selection_export *out, const oa_u8 *data, unsigned size)
{
    unsigned accepted, error;
    int handle, rc;
    critical_vector old;
    if (out == 0 || data == 0 || size == 0u || size > 4096u) return OT_RESTORE_PATH;
    old = fail_begin();
    out->dos_error = _dos_creatnew(out->temporary, 0u, &handle);
    _dos_setvect(0x24, old);
    if (out->dos_error != 0u) return OT_RESTORE_IO;
    out->handle = (unsigned)handle;
    error = selection_write(out, data, size, &accepted);
    rc = error != 0u ? OT_RESTORE_IO : 0;
    error = selection_close(out);
    if (rc == 0 && error != 0u) rc = OT_RESTORE_IO;
    if (rc != 0) return rc;
    return selection_readback_publish(out, (oa_u32)size, ot_crc32(data, (oa_u16)size));
}

/* Streaming metadata uses the same exclusive paths as small profile output.
 * Counts are 32-bit; every DOS transfer remains inside one small chunk. */
static unsigned stream_write(void *p, const oa_u8 *data, unsigned size, unsigned *accepted)
{
    struct ot_dos_file_stream *s = p;
    unsigned error;
    *accepted = 0u;
    if (!s->opened || s->bytes > 131072ul || (oa_u32)size > 131072ul - s->bytes) return 87u;
    error = selection_write(&s->file, data, size, accepted);
    if (!error) { s->bytes += size; s->crc = ot_crc32_extend(s->crc, data, (oa_u16)size); }
    return error;
}
static unsigned stream_close(void *p)
{
    struct ot_dos_file_stream *s = p;
    if (!s->opened) return 6u;
    s->opened = 0u;
    return selection_close(&s->file);
}
static unsigned stream_publish(void *p)
{
    struct ot_dos_file_stream *s = p;
    if (s->opened || s->file.dos_error) return 6u;
    s->result = selection_readback_publish(&s->file, s->bytes, s->crc);
    return s->result ? (s->file.dos_error ? s->file.dos_error : 23u) : 0u;
}

int ot_dos_file_stream_open(struct ot_dos_file_stream *s, const char *name)
{
    int handle, rc;
    critical_vector old;
    if (s == 0) return OT_RESTORE_PATH;
    memset(s, 0, sizeof(*s));
    rc = ot_dos_selection_prepare(&s->file, name);
    if (rc) return rc;
    old = fail_begin();
    s->file.dos_error = _dos_creatnew(s->file.temporary, 0u, &handle);
    _dos_setvect(0x24, old);
    if (s->file.dos_error) return OT_RESTORE_IO;
    s->file.handle = (unsigned)handle; s->opened = 1u;
    s->ops.context = s; s->ops.write = stream_write;
    s->ops.close = stream_close; s->ops.publish = stream_publish;
    return 0;
}

static unsigned remove_previous(const char *path)
{
    union REGS in, out;
    struct SREGS segments;
    critical_vector old;
    memset(&in, 0, sizeof(in)); segread(&segments);
    in.x.ax = 0x4100u; in.x.dx = FP_OFF(path); segments.ds = FP_SEG(path);
    old = fail_begin(); intdosx(&in, &out, &segments); _dos_setvect(0x24, old);
    return out.x.cflag ? out.x.ax : 0u;
}

static unsigned exchange_attributes(void *p, const char *path, unsigned flags)
{ (void)p; return set_attributes(path, flags); }
static unsigned exchange_rename(void *p, const char *from, const char *to)
{ (void)p; return rename_path(from, to); }
static unsigned exchange_remove(void *p, const char *path)
{ (void)p; return remove_previous(path); }

static unsigned verify_disk_file(struct ot_dos_restore *r, const struct ot_file_metadata *m,
                                  const char *path)
{
    struct ot_dos_source source;
    oa_u8 *buffer;
    int rc;
    strcpy(r->checked_path, path);
    memset(&source, 0, sizeof(source));
    buffer = malloc(4096u);
    if (buffer == 0) {
        r->readback_error = OT_SOURCE_IO; return restore_error(r, 8u);
    }
    rc = ot_dos_source_open(&source, path, "VERIFY.TMP");
    if (rc == 0) rc = ot_source_verify(&source.ops, m->file_size, r->output_crc, buffer, 4096u);
    free(buffer);
    r->readback_error = rc;
    if (rc != 0) return restore_error(r, source.dos_error != 0u ? source.dos_error : 23u);
    ++r->readback_files;
    return 0u;
}

static unsigned restore_commit(void *p, const struct ot_file_metadata *m)
{
    struct ot_dos_restore *r = p;
    char path[128];
    unsigned flags, error, close_error;
    struct ot_restore_exchange_ops exchange;
    critical_vector old;
    if (r->comparing) {
        error = restore_path(r, m->path, path, 0u);
        if (error == 0u) error = verify_disk_file(r, m, path);
        if (error == 0u && !r->quiet && printf("MATCH %10lu %s\n", (unsigned long)m->file_size, m->path) < 0)
            error = restore_error(r, 29u);
        return error;
    }
    if (r->preview) {
        if (!r->quiet && printf("%s %10lu %s\n", r->replacing ? "REPLACE" : "NEW",
                   (unsigned long)m->file_size, m->path) < 0) return restore_error(r, 29u);
        if (r->replacing) ++r->replaced;
        return 0u;
    }
    if (!r->opened) return restore_error(r, 6u);
    old = fail_begin();
    error = m->dos_date != 0u ? _dos_setftime(r->handle, m->dos_date, m->dos_time) : 0u;
    _dos_setvect(0x24, old);
    restore_error(r, error);
    close_error = restore_abort(r);
    if (error == 0u) error = close_error;
    if (error != 0u) return restore_error(r, error);
    error = verify_disk_file(r, m, r->temporary);
    if (error != 0u) return error;
    error = restore_path(r, m->path, path, 0u);
    if (error != 0u) return error;
    error = path_attributes(path, &flags);
    if (r->replacing) {
        if (error != 0u || flags != r->original_attributes || (flags & 0x10u) != 0u)
            return restore_error(r, error != 0u ? error : 5u);
        error = path_attributes(r->previous, &close_error);
        if (error != 2u) return restore_error(r, error == 0u ? 80u : error);
    } else if (error != 2u) return restore_error(r, error == 0u ? 80u : error);
    exchange.context = 0; exchange.attributes = exchange_attributes;
    exchange.rename = exchange_rename; exchange.remove = exchange_remove;
    error = ot_restore_exchange(&exchange, r->temporary, path, r->previous,
        r->replacing, r->original_attributes, m->dos_attributes, &r->recovery_error);
    if (error == 0u && r->replacing) ++r->replaced;
    return restore_error(r, error);
}

static int restore_open(struct ot_dos_restore *r, const char *name,
                         const char *const *selection, unsigned count, unsigned mode)
{
    char relative[128], *last;
    unsigned i, flags, error, missing = 0u, preview = mode != 0u;
    if (r == 0 || r->created || name == 0 || name[0] == 0 || strlen(name) >= 128u ||
        !ot_restore_selection_valid(selection, count))
        return OT_RESTORE_PATH;
    memset(r, 0, sizeof(*r));
    r->preview = preview;
    r->comparing = mode == 2u;
    r->quiet = mode == 3u;
    r->selection = selection; r->selection_count = count;
    if (_fullpath(r->root, name, sizeof(r->root)) == 0 || r->root[1] != ':' || r->root[2] != '\\' ||
        strlen(r->root) + strlen(OT_RESTORE_TEMP) + 2u > sizeof(r->temporary)) return OT_RESTORE_PATH;
    strcpy(relative, r->root + 3);
    for (i = 0u; relative[i] != 0; ++i) if (relative[i] == '\\') relative[i] = '/';
    if (!ot_archive_path_valid(relative)) return OT_RESTORE_PATH;
    error = path_attributes(r->root, &flags);
    if (error == 0u) {
        if ((!r->comparing && !r->quiet && count == 0u) || (flags & 0x10u) == 0u) return (int)restore_error(r, 80u);
    } else {
        if (r->comparing) return (int)restore_error(r, error);
        if (error != 2u && error != 3u) return (int)restore_error(r, error);
        missing = 1u;
        if (preview) {
            strcpy(relative, r->root); last = strrchr(relative, '\\');
            if (last == relative + 2) last[1] = 0; else *last = 0;
            error = path_attributes(relative, &flags);
            if (error == 0u && (flags & 0x10u) == 0u) error = 3u;
        } else {
            error = make_directory(r->root);
            if (error == 0u) r->root_created = 1u;
        }
        if (error != 0u) return (int)restore_error(r, error);
    }
    r->created = 1u;
    strcpy(r->temporary, r->root); strcat(r->temporary, "\\" OT_RESTORE_TEMP);
    strcpy(r->previous, r->root); strcat(r->previous, "\\" OT_RESTORE_OLD);
    error = path_attributes(r->temporary, &flags);
    if (error != 2u && !(preview && missing && error == 3u))
        return (int)restore_error(r, error == 0u ? 80u : error);
    error = path_attributes(r->previous, &flags);
    if (error != 2u && !(preview && missing && error == 3u))
        return (int)restore_error(r, error == 0u ? 80u : error);
    r->ops.context = r; r->ops.directory = restore_directory; r->ops.begin = restore_begin;
    r->ops.write = restore_write; r->ops.commit = restore_commit; r->ops.abort = restore_abort;
    return 0;
}

int ot_dos_restore_open(struct ot_dos_restore *r, const char *name,
                         const char *const *selection, unsigned count)
{ return restore_open(r, name, selection, count, 0u); }

int ot_dos_restore_preview(struct ot_dos_restore *r, const char *name,
                            const char *const *selection, unsigned count)
{ return restore_open(r, name, selection, count, 1u); }

int ot_dos_restore_inspect(struct ot_dos_restore *r, const char *name)
{ return restore_open(r, name, 0, 0u, 3u); }

int ot_dos_restore_compare(struct ot_dos_restore *r, const char *name)
{ return restore_open(r, name, 0, 0u, 2u); }

int ot_dos_restore_create(struct ot_dos_restore *r, const char *name)
{
    return ot_dos_restore_open(r, name, 0, 0u);
}

#ifdef OT_NATIVE_TAPE_PARTITIONS
static unsigned native_dos_partitions(void *context, struct ot_tape_partitions_v1 *packet)
{
    struct ot_dos_native_session *s=(struct ot_dos_native_session *)context;
    return dos_ioctl(0,s->handle,OT_TAPE_API_CATEGORY,OT_TAPE_API_PARTITIONS,packet,sizeof(*packet));
}
int ot_dos_native_partitions(const char *name, struct ot_tape_partitions_v1 *packet, unsigned *error)
{
    struct ot_dos_native_session session;
    struct ot_native_partitions_ops ops;
    native_dos_ops(&ops.query,&session); ops.partitions=native_dos_partitions;
    return ot_native_partitions(&ops,name,packet,error);
}
#endif

#ifdef OT_NATIVE_TAPE_ALERTS
static unsigned native_dos_alerts(void *context, struct ot_tape_alerts_v1 *packet)
{
    struct ot_dos_native_session *s=(struct ot_dos_native_session *)context;
    return dos_ioctl(0,s->handle,OT_TAPE_API_CATEGORY,OT_TAPE_API_ALERTS,packet,sizeof(*packet));
}
int ot_dos_native_alerts(const char *name,oa_u16 flags, struct ot_tape_alerts_v1 *packet, unsigned *error)
{
    struct ot_dos_native_session session;
    struct ot_native_alerts_ops ops;
    native_dos_ops(&ops.query,&session); ops.alerts=native_dos_alerts;
    return ot_native_alerts(&ops,name,flags,packet,error);
}
#endif

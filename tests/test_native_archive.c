#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/archive_native.h"
#include "opentape/verify_stream.h"

struct fixture {
    unsigned count, cursor, opened, closes, reads, writes, flushes, marks, aborts;
    unsigned combined, fail_write, fail_control, read_fault, close_error;
    unsigned unit, workspace, offset;
    oa_u16 state, length[128];
    unsigned char data[128][1024];
};
static struct fixture f;
static unsigned char large_block[65535], large_copy[65535];
static int name_ok(void *c, const char *name)
{ (void)c; return strcmp(name, "tape") == 0; }
static unsigned opened(void *c, const char *name)
{ struct fixture *p = c; assert(name_ok(c,name) && !p->opened); p->opened=1; return 0; }
static unsigned device(void *c, unsigned *is_device)
{ (void)c; *is_device=1; return 0; }
static unsigned query(void *c, struct ot_tape_info_v1 *p)
{ struct fixture *q=c; p->operations=0x7ffUL; p->valid_fields=OT_TAPE_VALID_SESSION; p->session_state=q->state; return 0; }
static unsigned closed(void *c)
{ struct fixture *p=c; assert(p->opened); p->opened=0; ++p->closes; return p->close_error; }
static unsigned control(void *c, struct ot_tape_control_v1 *p)
{
    struct fixture *q=c;
    unsigned op=p->operation;
    assert(q->opened && op>=2 && op<=6);
    if (op==2) { assert(q->state==0); q->state=0x10; p->completion_flags=0x42; }
    else if (op==6) {
        ++q->aborts; q->state &= ~0x10u;
        if (q->state) q->state |= 0x40;
        p->completion_flags=0x82;
    } else {
        p->scsi_opcode=0x10; p->completion_flags=3;
        if (op==4) { ++q->marks; q->length[q->count++]=0xffff; q->state|=0x20; }
        else ++q->flushes;
        if ((q->fail_control==1 && op==3) || (q->fail_control==2 && op==4) || (q->fail_control==3 && op==5)) {
            q->state|=0x40; p->result=3; p->diagnostic_class=2; p->target_status=2;
            p->sense_response=0x70; p->sense_key=3; p->completion_flags=5;
        } else if (op==5) { q->state=0; p->completion_flags|=0x80; }
    }
    p->session_state=q->state;
    return 0;
}
static unsigned write_record(void *c, const void *data, unsigned bytes, struct ot_tape_record_v1 *p)
{
    struct fixture *q=c;
    assert(q->opened && q->count<128 && bytes<=1024);
    if (q->unit) assert(p->flags == 1 && p->block_size == q->unit && bytes % q->unit == 0);
    ++q->writes; q->state=0x30;
    memcpy(q->data[q->count],data,bytes); q->length[q->count++]=(oa_u16)bytes;
    p->scsi_opcode=0x0a; p->actual=bytes; p->completion_flags=0x23;
    if (q->fail_write && q->writes==q->fail_write) {
        q->state=0x70; p->result=3; p->diagnostic_class=5;
        p->actual=bytes-(q->unit ? q->unit : 1u); p->stream_flags=8; p->completion_flags=0x25;
    }
    p->session_state=q->state;
    return 0;
}
static unsigned read_record(void *c, void *data, unsigned capacity, struct ot_tape_record_v1 *p)
{
    struct fixture *q=c;
    unsigned bytes;
    assert(q->opened && q->cursor<q->count);
    ++q->reads;
    if (q->unit) {
        assert(p->flags == 1 && p->block_size == q->unit && capacity % q->unit == 0);
        bytes = q->length[q->cursor];
        p->scsi_opcode=8; p->completion_flags=0x23;
        if (bytes == 0xffff) {
            ++q->cursor; p->actual=0; p->result=5; p->diagnostic_class=2;
            p->target_status=2; p->sense_response=0x70; p->stream_flags=9;
            if (q->read_fault==1) p->stream_flags=10;
            if (q->read_fault==2) { p->stream_flags=0x28; p->sense_key=8; p->ascq=5; }
            return 0;
        }
        bytes -= q->offset;
        if (bytes > capacity) bytes = capacity;
        if (q->read_fault==8 && bytes > q->unit) bytes=q->unit;
        memcpy(data,q->data[q->cursor]+q->offset,bytes);
        q->offset += bytes;
        if (q->offset == q->length[q->cursor]) { q->offset=0; ++q->cursor; }
        p->actual=bytes;
        if (bytes < capacity) p->stream_flags=8;
        if (q->read_fault==8 || (q->combined && q->offset==0 && q->cursor<q->count && q->length[q->cursor]==0xffff)) {
            if (q->read_fault!=8) ++q->cursor;
            p->result=5; p->diagnostic_class=2; p->target_status=2; p->sense_response=0x70;
            p->stream_flags |= 1;
        }
        if (q->read_fault==3) {
            p->result=3; p->diagnostic_class=2; p->target_status=2; p->sense_response=0x70;
            p->sense_key=3; p->completion_flags=0x25;
        } else if (q->read_fault==4) { p->actual=0; p->stream_flags=8; }
        return 0;
    }
    bytes=q->length[q->cursor++];
    p->scsi_opcode=8; p->completion_flags=0x23;
    p->result=5; p->diagnostic_class=2; p->target_status=2; p->sense_response=0x70;
    if (bytes==0xffff) {
        p->actual=0; p->stream_flags=9;
        if (q->read_fault==1) p->stream_flags=10; /* EOM cannot prove archive EOF */
        else if (q->read_fault==2) { p->stream_flags=0x28; p->sense_key=8; p->ascq=5; }
    } else {
        assert(bytes<capacity); memcpy(data,q->data[q->cursor-1],bytes);
        p->actual=bytes; p->stream_flags=12;
        if (q->combined && q->cursor<q->count && q->length[q->cursor]==0xffff) {
            p->stream_flags|=1; ++q->cursor;
        }
        if (q->read_fault==3) {
            p->result=3; p->sense_key=3; p->completion_flags=0x25;
            p->stream_flags=8;
        } else if (q->read_fault==4) {
            p->result=0; p->diagnostic_class=0; p->target_status=0; p->sense_response=0;
            p->actual=0; p->stream_flags=8; /* ordinary empty record, not EOF */
        }
    }
    return 0;
}
static void ops_init(struct ot_native_writer_ops *w, struct ot_native_reader_ops *r)
{
    memset(w,0,sizeof(*w));
    w->control.query.context=&f; w->control.query.valid_name=name_ok;
    w->control.query.open_readonly=opened; w->control.query.is_device=device;
    w->control.query.query=query; w->control.query.close=closed;
    w->control.control=control; w->open_write=opened; w->write=write_record;
    r->query=w->control.query; r->read=read_record;
}
static int backup(const struct ot_native_writer_ops *ops)
{
    struct ot_native_writer n;
    struct ot_tape_output output;
    struct ot_archive_writer archive;
    struct ot_volume_label label;
    struct ot_session_start start;
    struct ot_file_metadata metadata;
    unsigned char buffer[1024], workspace[1024];
    int rc;
    memset(&n,0,sizeof(n)); memset(&output,0,sizeof(output)); memset(&archive,0,sizeof(archive));
    memset(&label,0,sizeof(label)); memset(&start,0,sizeof(start)); memset(&metadata,0,sizeof(metadata));
    label.version=OT_VOLUME_FORMAT_VERSION; label.volume_sequence=1; strcpy(label.volume_name,"TEST");
    strcpy(start.name,"native archive");
    metadata.kind=OT_FILE_REGULAR; metadata.file_size=3; strcpy(metadata.path,"FILE.TXT");
    if (f.unit) {
        assert(ot_native_writer_open_fixed(&n,ops,"tape",f.unit)==0);
        assert(ot_archive_native_output_fixed(&output,&n,workspace,f.workspace)==0);
    } else {
        assert(ot_native_writer_open(&n,ops,"tape")==0);
        assert(ot_archive_native_output(&output,&n)==0);
    }
    assert(ot_archive_writer_init(&archive,&output.sink,buffer,sizeof(buffer))==0);
    rc=ot_archive_writer_volume(&archive,&label);
    if (!rc) rc=ot_archive_writer_session(&archive,1,&start);
    if (!rc) rc=ot_archive_writer_metadata(&archive,1,&metadata);
    if (!rc) rc=ot_archive_writer_data(&archive,(const oa_u8 *)"abc",3);
    if (!rc) rc=ot_archive_writer_file_end(&archive);
    if (!rc) rc=ot_archive_writer_session_end(&archive);
    if (!rc) { assert(ot_archive_writer_complete(&archive)==0); assert(output.finished && !output.opened); }
    else {
        if (output.opened) assert(ot_output_close(&output)!=0);
        assert(!output.finished && !output.opened && f.aborts==1);
        if (f.fail_write) assert(n.last_record.result==3 && n.last_record.actual>0);
        else assert(n.last_control.result==3);
    }
    return rc;
}
int main(void)
{
    struct ot_native_writer_ops w;
    struct ot_native_reader_ops r;
    struct ot_native_reader n;
    struct ot_native_writer nw;
    struct ot_tape_output out;
    struct ot_verify_session_result result;
    unsigned char buffer[1024], workspace[1024];
    unsigned i, records, mode;
    const unsigned units[] = {1u,17u,512u,1024u};
    const unsigned spaces[] = {31u,51u,512u,1024u};
    ops_init(&w,&r);
    for (i=0;i<2;++i) {
        memset(&f,0,sizeof(f)); assert(backup(&w)==0);
        assert(f.flushes==2 && f.marks==1 && f.closes==1 && f.aborts==0);
        records=f.count-1; f.combined=i;
        memset(&n,0,sizeof(n)); memset(&result,0,sizeof(result));
        assert(ot_native_reader_open(&n,&r,"tape")==0);
        assert(ot_archive_native_reader(&result.reader,&n)==0);
        assert(ot_verify_session_opened(buffer,sizeof(buffer),0,0,&result)==0);
        assert(result.verify.files==1 && result.verify.bytes_low==3 && f.closes==2);
        assert(f.reads==records+(i?0:1));
    }
    for (i=1;i<=4;++i) {
        memset(&f,0,sizeof(f)); assert(backup(&w)==0); f.read_fault=i;
        memset(&n,0,sizeof(n)); memset(&result,0,sizeof(result));
        assert(ot_native_reader_open(&n,&r,"tape")==0);
        assert(ot_archive_native_reader(&result.reader,&n)==0);
        assert(ot_verify_session_opened(buffer,sizeof(buffer),0,0,&result)!=0);
        assert(f.closes==2 && !result.reader.opened);
        if (i>=3) assert(result.verify.framing.blocks==0 && f.reads==1);
    }
    for (i=1;i<=3;++i) {
        memset(&f,0,sizeof(f)); f.fail_control=i;
        assert(backup(&w)!=0 && f.aborts==1 && f.closes==1);
        assert(f.marks==(i==1?0u:1u));
    }
    memset(&f,0,sizeof(f)); f.fail_write=3;
    assert(backup(&w)!=0 && f.writes==3 && f.marks==0 && f.flushes==0 && f.aborts==1);
    memset(&f,0,sizeof(f)); memset(&n,0,sizeof(n)); memset(&result,0,sizeof(result));
    assert(ot_native_reader_open_fixed(&n,&r,"tape",512)==0);
    assert(ot_archive_native_reader(&result.reader,&n)==OT_READ_BAD_MODE && n.opened);
    assert(ot_native_reader_close(&n)==0 && f.reads==0);
    memset(&f,0,sizeof(f)); memset(&nw,0,sizeof(nw)); memset(&out,0,sizeof(out));
    assert(ot_native_writer_open_fixed(&nw,&w,"tape",512)==0);
    assert(ot_archive_native_output(&out,&nw)==OT_OUTPUT_BAD_MODE && nw.claimed);
    assert(ot_native_writer_close(&nw)==OT_NATIVE_UNFINISHED && f.writes==0 && f.flushes==0 && f.marks==0);
    /* Fixed mode uses the same six OTB1 logical blocks, independently padded.
     * Small units split the header itself; a larger unit pads short metadata.
     */
    for (mode=0;mode<4;++mode) for (i=0;i<2;++i) {
        memset(&f,0,sizeof(f)); f.unit=units[mode]; f.workspace=spaces[mode];
        assert(backup(&w)==0);
        assert(f.flushes==2 && f.marks==1 && f.aborts==0);
        f.combined=i;
        memset(&n,0,sizeof(n)); memset(&result,0,sizeof(result));
        assert(ot_native_reader_open_fixed(&n,&r,"tape",f.unit)==0);
        assert(ot_archive_native_reader_fixed(&result.reader,&n,workspace,f.workspace)==0);
        assert(ot_verify_session_opened(buffer,sizeof(buffer),0,0,&result)==0);
        assert(result.verify.files==1 && result.verify.bytes_low==3 && result.verify.framing.blocks==6);
        assert(f.closes==2 && f.cursor==f.count);
    }
    /* Corruption/truncation cannot become archive EOF or a padded good block. */
    for (i=1;i<=9;++i) {
        oa_u32 crc;
        memset(&f,0,sizeof(f)); f.unit=512; f.workspace=512;
        assert(backup(&w)==0);
        if (i<=4 || i==8) f.read_fault=i;
        if (i==5) f.data[0][144]=1;              /* nonzero transport padding */
        if (i==6) f.data[0][12]^=1;             /* header CRC */
        if (i==7) f.data[0][60]^=1;             /* payload CRC */
        if (i==9) {                            /* forged length with valid CRC */
            f.data[0][8]=0; f.data[0][9]=0; f.data[0][10]=1; f.data[0][11]=0;
            crc=ot_crc32(f.data[0],32);
            f.data[0][32]=(oa_u8)crc; f.data[0][33]=(oa_u8)(crc>>8);
            f.data[0][34]=(oa_u8)(crc>>16); f.data[0][35]=(oa_u8)(crc>>24);
        }
        memset(&n,0,sizeof(n)); memset(&result,0,sizeof(result));
        assert(ot_native_reader_open_fixed(&n,&r,"tape",512)==0);
        assert(ot_archive_native_reader_fixed(&result.reader,&n,workspace,512)==0);
        assert(ot_verify_session_opened(buffer,sizeof(buffer),0,0,&result)!=0);
        assert(f.closes==2 && !result.reader.opened);
        if (i==5 || i==6 || i==7 || i==9) assert(!result.transport_error);
        if (i==3) assert(result.transport_error);
        if (i>=3 && i!=8) assert(result.verify.framing.blocks==0);
    }
    /* A filemark before a header is complete is a truncated logical block. */
    memset(&f,0,sizeof(f)); f.unit=17; f.workspace=51;
    assert(backup(&w)==0); f.read_fault=8;
    memset(&n,0,sizeof(n)); memset(&result,0,sizeof(result));
    assert(ot_native_reader_open_fixed(&n,&r,"tape",17)==0);
    assert(ot_archive_native_reader_fixed(&result.reader,&n,workspace,51)==0);
    assert(ot_verify_session_opened(buffer,sizeof(buffer),0,0,&result)!=0);
    assert(f.reads==1 && f.closes==2);
    /* No write replay, and no marks/flush after a failed physical chunk. */
    memset(&f,0,sizeof(f)); f.unit=17; f.workspace=51; f.fail_write=2;
    assert(backup(&w)!=0 && f.writes==2 && f.aborts==1 && !f.marks && !f.flushes);
    for (i=1;i<=3;++i) {
        memset(&f,0,sizeof(f)); f.unit=512; f.workspace=512; f.fail_control=i;
        assert(backup(&w)!=0 && f.aborts==1 && f.closes==1);
    }
    /* Capacity rejection happens after the first bounded header read, not an
     * unbounded copy or a speculative read of its claimed large payload. */
    memset(&f,0,sizeof(f)); f.unit=512; f.workspace=512; assert(backup(&w)==0);
    memset(&n,0,sizeof(n)); memset(&result,0,sizeof(result));
    assert(ot_native_reader_open_fixed(&n,&r,"tape",512)==0);
    assert(ot_archive_native_reader_fixed(&result.reader,&n,workspace,511)==OT_READ_BAD_MODE);
    assert(!result.reader.opened && n.opened && f.reads==0);
    assert(ot_archive_native_reader_fixed(&result.reader,&n,workspace,512)==0);
    assert(ot_verify_session_opened(buffer,80,0,0,&result)!=0);
    assert(f.reads==1 && result.verify.framing.blocks==0 && f.closes==2);
    /* 65535 logical bytes require 65536 physical bytes: never wrap WORD
     * counters, copy padding beyond caller capacity or submit a 65536-byte SRB. */
    {
        struct ot_block_header header;
        unsigned accepted;
        memset(&header,0,sizeof(header));
        memset(large_block,0x5a,sizeof(large_block));
        assert(ot_block_encode(large_block,sizeof(large_block),&header,
            large_block+OT_BLOCK_HEADER_SIZE,sizeof(large_block)-OT_BLOCK_HEADER_SIZE)==0);
        memset(&f,0,sizeof(f)); f.unit=512;
        memset(&nw,0,sizeof(nw)); memset(&out,0,sizeof(out));
        assert(ot_native_writer_open_fixed(&nw,&w,"tape",512)==0);
        assert(ot_archive_native_output_fixed(&out,&nw,workspace,sizeof(workspace))==0);
        assert(ot_output_write(&out,large_block,sizeof(large_block),&accepted)==0 && accepted==65535u);
        assert(f.writes==64 && f.length[63]==1024 && f.data[63][1023]==0);
        assert(out.sink.finish_session(out.sink.context)==0);
        memset(&n,0,sizeof(n)); memset(&result,0,sizeof(result));
        assert(ot_native_reader_open_fixed(&n,&r,"tape",512)==0);
        assert(ot_archive_native_reader_fixed(&result.reader,&n,workspace,sizeof(workspace))==0);
        assert(ot_reader_read(&result.reader,large_copy,sizeof(large_copy),&accepted)==0 && accepted==65535u);
        assert(memcmp(large_copy,large_block,sizeof(large_copy))==0);
        assert(ot_reader_read(&result.reader,large_copy,sizeof(large_copy),&accepted)==OT_READ_FILEMARK && accepted==0);
        assert(ot_reader_close(&result.reader)==0 && f.closes==2);
    }
    puts("Native archive variable/fixed framing and finalization integration tests passed");
    return 0;
}

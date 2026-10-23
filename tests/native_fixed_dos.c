/* Memory-only fixed-mode provider/consumer integration: 2 x 512 bytes. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/native_tape.h"

static struct ot_dos_native_reader reader;
static struct ot_dos_native_writer writer;
static unsigned char data[1026];
static void check_packet(const struct ot_tape_record_v1 *p, unsigned actual,
                          unsigned completion, unsigned stream, unsigned diag)
{
    assert(ot_tape_record_validate(p,sizeof(*p))==0);
    assert(p->flags==OT_TAPE_IO_FIXED && p->block_size==512 && p->requested==1024);
    assert(p->actual==actual && p->completion_flags==completion);
    assert(p->stream_flags==stream && p->diagnostic_class==diag);
    assert(data[0]==0xa5 && data[1025]==0xa5);
}
int main(int argc, char **argv)
{
    struct ot_tape_control_v1 control;
    struct ot_tape_record_v1 saved;
    struct ot_tape_info_v1 info;
    unsigned error, actual=1024, completion=0x23, stream=0, diag=0;
    unsigned i;
    int rc, expected=0;
    int read_case=argc>1?atoi(argv[1]):0;
    int write_case=argc>2?atoi(argv[2]):0;
    int legacy=argc>3?atoi(argv[3]):0;
    assert(read_case>=0 && read_case<=12 && write_case>=0 && write_case<=8);
    if (read_case) {
        expected=OT_NATIVE_COMMAND; completion=5; actual=0; diag=2;
        if (read_case<=3) {
            expected=OT_NATIVE_STREAM; completion=0x23; stream=9;
            if (read_case!=1) actual=512;
            if (read_case==3) stream=10;
        } else if (read_case==4 || read_case==5 || read_case==6 || read_case==11) diag=3;
        else if (read_case==7 || read_case==8) { actual=512; completion=0x25; stream=read_case==7?12:8; }
        else if (read_case==9) diag=1;
        else if (read_case==12) stream=1;
    }
    memset(data,0xa5,sizeof(data));
    assert(ot_dos_native_reader_open_fixed(&reader,"NRMT0",512)==0);
    assert(ot_dos_native_reader_read(&reader,data+1,1023)==OT_NATIVE_ARGUMENT);
    assert(!reader.reader.failed);
    rc=ot_dos_native_reader_read(&reader,data+1,1024);
    assert(rc==expected);
    check_packet(&reader.reader.last,actual,completion,stream,diag);
    for (i=0;i<actual;++i) assert(data[i+1]==0x5a);
    if (rc<0) assert(ot_dos_native_reader_read(&reader,data+1,1024)==OT_NATIVE_COMMAND);
    assert(ot_dos_native_reader_close(&reader)==0);
    assert(ot_dos_native_execute("NRMT0",1,&control,&error)==0);
    assert(ot_dos_native_writer_open_fixed(&writer,"NRMT0",512)==0);
    assert(writer.writer.last_control.flags==1 && writer.writer.last_control.block_size==512);
    assert(ot_dos_native_writer_write(&writer,data+1,1023)==OT_NATIVE_ARGUMENT);
    assert(writer.writer.failure==0);
    memset(data+1,0x5a,1024);
    actual=1024; completion=0x23; stream=0; diag=0; expected=0;
    if (write_case) {
        expected=OT_NATIVE_COMMAND; completion=0x25; diag=2;
        if (write_case==1) { actual=512; diag=5; stream=8; }
        else if (write_case==2) { actual=0; stream=8; if (legacy) { completion=5; stream=0; } }
        else if (write_case==3 || write_case==4 || write_case==5) {
            actual=0; completion=5; diag=write_case==3?3:write_case==4?1:2;
        } else if (write_case==6) stream=2;
        else { actual=512; stream=write_case==7?10:8; }
    }
    assert(ot_dos_native_writer_write(&writer,data+1,1024)==expected);
    check_packet(&writer.writer.last_record,actual,completion,stream,diag);
    if (expected) {
        saved=writer.writer.last_record;
        assert(ot_dos_native_writer_write(&writer,data+1,1024)==OT_NATIVE_COMMAND);
        assert(ot_dos_native_writer_flush(&writer)==OT_NATIVE_COMMAND);
        assert(ot_dos_native_writer_finish(&writer)==OT_NATIVE_COMMAND);
        assert(ot_dos_native_writer_close(&writer)==OT_NATIVE_COMMAND);
        assert(memcmp(&saved,&writer.writer.last_record,sizeof(saved))==0);
        assert(ot_dos_native_probe("NRMT0",0,&info,&error)==0 && info.session_state==0x60);
        assert(ot_dos_native_execute("NRMT0",1,&control,&error)==0);
    } else {
        assert(ot_dos_native_writer_flush(&writer)==0);
        assert(ot_dos_native_writer_filemark(&writer)==0);
        assert(ot_dos_native_writer_finish(&writer)==0);
        assert(ot_dos_native_writer_close(&writer)==0);
    }
    puts("OpenTape installed native fixed-block tests passed");
    return 0;
}

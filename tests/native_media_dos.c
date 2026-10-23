/* Installed provider tests: media motion, empty-drive errors and recovery.
 * A memory-only manager validates every CDB and counts hidden submissions. */
#include <assert.h>
#include <dos.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/native_tape.h"
/* stdout is redirected to the serial log by the disposable DOS harness. */
#undef assert
#define assert(test) ((test) ? (void)0 : (printf("Native media assertion line %u: %s\n",(unsigned)__LINE__,#test),exit(1)))
static unsigned raw(unsigned h,struct ot_tape_control_v1 *p)
{
    union REGS r; struct SREGS s;
    memset(&r,0,sizeof(r)); segread(&s);
    r.x.ax=0x440c; r.x.bx=h; r.x.cx=0x8001;
    r.x.dx=FP_OFF(p); s.ds=FP_SEG(p); intdosx(&r,&r,&s);
    return r.x.cflag;
}
static void invalid(void)
{
    struct ot_tape_control_v1 p,saved;
    unsigned h,other,i;
    assert(_dos_open("NRMT0",O_RDONLY,&h)==0);
    for(i=0;i<4;++i) {
        assert(ot_tape_control_init(&p,10)==0);
        if(i==0) p.count=1;
        if(i==1) p.flags=1;
        if(i==2) p.block_size=512;
        if(i==3) p.operation=0xffffu;
        saved=p;
        assert(raw(h,&p)!=0 && memcmp(&saved,&p,sizeof(p))==0);
    }
    assert(_dos_open("NRMT0",O_RDONLY,&other)==0);
    for(i=10;i<=11;++i) {
        assert(ot_tape_control_init(&p,(oa_u16)i)==0);
        assert(raw(h,&p)==0 && p.result==OT_TAPE_RESULT_BUSY && p.completion_flags==0);
    }
    assert(_dos_close(other)==0 && _dos_close(h)==0);
}
static void execute(unsigned op,int result,unsigned flags,unsigned session)
{
    struct ot_tape_control_v1 p; unsigned error=0;
    assert(ot_dos_native_execute("NRMT0",(oa_u16)op,&p,&error)==result);
    assert(p.completion_flags==flags && p.session_state==session);
    assert(p.count==0 && p.scsi_opcode==(op>=10 ? 0x1b : op));
}
int main(int argc,char **argv)
{
    struct ot_tape_control_v1 p;
    struct ot_dos_native_reader reader;
    struct ot_dos_native_writer writer;
    unsigned error=0;
    char data[8];
    int fault=argc>1;
    memset(&reader,0,sizeof(reader)); memset(&writer,0,sizeof(writer));
    invalid();
    if(fault) {
        assert(argc==3);
        assert(ot_dos_native_execute("NRMT0",(oa_u16)atoi(argv[1]),&p,&error)==OT_NATIVE_COMMAND);
        assert(p.completion_flags==5 && p.session_state==0x40 && p.scsi_opcode==0x1b);
        assert(p.diagnostic_class==(unsigned)atoi(argv[2]));
        assert(ot_dos_native_space("NRMT0",7,1,&p,&error)==OT_NATIVE_BUSY);
        assert(ot_dos_native_writer_open(&writer,"NRMT0")==OT_NATIVE_BUSY);
    } else {
        execute(11,0,3,0);
        execute(0,OT_NATIVE_COMMAND,1,0);
        execute(1,OT_NATIVE_COMMAND,5,0);
        assert(ot_dos_native_reader_open(&reader,"NRMT0")==0);
        assert(ot_dos_native_reader_read(&reader,data,sizeof(data))==OT_NATIVE_COMMAND);
        assert(reader.reader.last.sense_key==2);
        assert(ot_dos_native_reader_close(&reader)==0);
        execute(10,0,3,0);
        execute(0,0,0x13,0);
        execute(1,0,0xb,0);
        assert(ot_dos_native_writer_open(&writer,"NRMT0")==0);
        assert(ot_dos_native_execute("NRMT0",10,&p,&error)==OT_NATIVE_BUSY);
        assert(ot_dos_native_execute("NRMT0",11,&p,&error)==OT_NATIVE_BUSY);
        assert(ot_dos_native_writer_close(&writer)==OT_NATIVE_UNFINISHED);
    }
    execute(10,0,3,0x40);
    execute(11,0,3,0x40);
    if(fault) execute(0,OT_NATIVE_COMMAND,1,0x40);
    execute(1,OT_NATIVE_COMMAND,5,0x40);
    execute(10,0,3,0x40);
    execute(1,0,0xb,0);
    if(!fault) {
        /* Abandoned bytes remain DIRTY even across successful media commands. */
        memset(&writer,0,sizeof(writer));
        assert(ot_dos_native_writer_open(&writer,"NRMT0")==0);
        assert(ot_dos_native_writer_write(&writer,"RECORD01",8)==0);
        assert(ot_dos_native_execute("NRMT0",11,&p,&error)==OT_NATIVE_BUSY);
        assert(ot_dos_native_writer_close(&writer)==OT_NATIVE_UNFINISHED);
        execute(10,0,3,0x60);
        execute(11,0,3,0x60);
        execute(10,0,3,0x60);
        execute(1,0,0xb,0);
    }
    puts("OpenTape installed native media tests passed");
    return 0;
}

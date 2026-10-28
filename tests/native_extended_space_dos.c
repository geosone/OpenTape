/* Installed native SPACE consumer/provider tests. The harness supplies a
 * memory-only manager; exact CDB/count assertions live in mockspace.inc. */
#include <assert.h>
#include <dos.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/native_tape.h"

static unsigned raw_control(unsigned handle, struct ot_tape_control_v1 *p)
{
    union REGS r;
    struct SREGS s;
    memset(&r,0,sizeof(r)); segread(&s);
    r.x.ax=0x440c; r.x.bx=handle; r.x.cx=0x8001;
    r.x.dx=FP_OFF(p); s.ds=FP_SEG(p);
    intdosx(&r,&r,&s);
    return r.x.cflag;
}
static void invalid_packets(void)
{
    struct ot_tape_control_v1 p,saved;
    const oa_s32 invalid[]={8388608L,-8388609L};
    unsigned handle,i;
    assert(_dos_open("NRMT0",O_RDONLY,&handle)==0);
    for(i=0;i<2;++i) {
        assert(ot_tape_control_init(&p,7)==0); p.count=invalid[i]; saved=p;
        assert(raw_control(handle,&p)!=0 && memcmp(&p,&saved,sizeof(p))==0);
    }
    assert(ot_tape_control_init(&p,9)==0); p.count=1; saved=p;
    assert(raw_control(handle,&p)!=0 && memcmp(&p,&saved,sizeof(p))==0);
    assert(ot_tape_control_init(&p,0)==0); p.count=1; saved=p;
    assert(raw_control(handle,&p)!=0 && memcmp(&p,&saved,sizeof(p))==0);
    assert(ot_tape_control_init(&p,0)==0); p.operation=0xffffu; saved=p;
    assert(raw_control(handle,&p)!=0 && memcmp(&p,&saved,sizeof(p))==0);
    assert(_dos_close(handle)==0);
}
/* Driver-side gates repeat SDK checks, including abandoned dirty output. */
static void busy_all(unsigned state)
{
    struct ot_tape_control_v1 p;
    unsigned h,op,error=0;
    for(op=14;op<=16;++op) {
        assert(ot_dos_native_space("NRMT0",(oa_u16)op,1,&p,&error)==OT_NATIVE_BUSY);
        assert(_dos_open("NRMT0",O_RDONLY,&h)==0);
        assert(ot_tape_space_init(&p,(oa_u16)op,-1)==0);
        assert(raw_control(h,&p)==0 && p.result==2 && p.completion_flags==0 && p.session_state==state);
        assert(_dos_close(h)==0);
    }
}
int main(int argc,char **argv)
{
    struct ot_tape_control_v1 p;
    struct ot_tape_info_v1 info;
    struct ot_dos_native_writer writer;
    const oa_s32 counts[]={1,-1,8388607L,-8388608L,0};
    unsigned error=0,handle,other,i,op;
    int fault=argc>1;
    memset(&writer,0,sizeof(writer));
    invalid_packets();
    for(op=14;op<=16;++op) {
        assert(ot_dos_native_space("NRMT0",(oa_u16)op,8388608L,&p,&error)==OT_NATIVE_ARGUMENT);
        assert(ot_dos_native_space("NRMT0",(oa_u16)op,-8388609L,&p,&error)==OT_NATIVE_ARGUMENT);
        assert(_dos_open("NRMT0",O_RDONLY,&handle)==0);
        assert(ot_tape_space_init(&p,(oa_u16)op,1)==0);
        p.flags=1; assert(raw_control(handle,&p)!=0);
        p.flags=0; p.count=8388608L; assert(raw_control(handle,&p)!=0);
        p.count=-8388609L; assert(raw_control(handle,&p)!=0);
        assert(_dos_open("NRMT0",O_RDONLY,&other)==0);
        assert(ot_tape_space_init(&p,(oa_u16)op,1)==0);
        assert(raw_control(handle,&p)==0 && p.result==2 && p.completion_flags==0);
        assert(_dos_close(other)==0 && _dos_close(handle)==0);
    }
    if(fault) {
        assert(argc==3); op=(unsigned)atoi(argv[1]);
        assert(ot_dos_native_space("NRMT0",(oa_u16)op,1,&p,&error)==OT_NATIVE_COMMAND);
        assert(p.diagnostic_class==(unsigned)atoi(argv[2]));
        assert(p.scsi_opcode==0x11 && p.count==1 && p.completion_flags==5 && p.session_state==0x40);
        busy_all(0x40);
        assert(ot_dos_native_execute("NRMT0",0,&p,&error)==0 && p.session_state==0x40);
        assert(ot_dos_native_probe("NRMT0",0,&info,&error)==0 && info.session_state==0x40);
        assert(ot_dos_native_execute("NRMT0",1,&p,&error)==0 && p.session_state==0);
        assert(ot_dos_native_space("NRMT0",(oa_u16)op,1,&p,&error)==0 && p.completion_flags==3);
    } else {
        for(op=14;op<=16;++op) for(i=0;i<5;++i) {
            assert(ot_dos_native_space("NRMT0",(oa_u16)op,counts[i],&p,&error)==0);
            assert(p.count==counts[i] && p.scsi_opcode==0x11 && p.completion_flags==3 && p.session_state==0);
        }
        assert(ot_dos_native_writer_open(&writer,"NRMT0")==0);
        for(op=14;op<=16;++op) assert(ot_dos_native_space("NRMT0",(oa_u16)op,1,&p,&error)==OT_NATIVE_BUSY);
        assert(ot_dos_native_writer_close(&writer)==OT_NATIVE_UNFINISHED);
        busy_all(0x40);
        assert(ot_dos_native_execute("NRMT0",0,&p,&error)==0 && p.session_state==0x40);
        assert(ot_dos_native_execute("NRMT0",1,&p,&error)==0 && p.session_state==0);
        memset(&writer,0,sizeof(writer));
        assert(ot_dos_native_writer_open(&writer,"NRMT0")==0);
        assert(ot_dos_native_writer_write(&writer,"RECORD01",8)==0);
        assert(ot_dos_native_writer_close(&writer)==OT_NATIVE_UNFINISHED);
        busy_all(0x60);
        assert(ot_dos_native_execute("NRMT0",1,&p,&error)==0 && p.session_state==0);
    }
    puts("OpenTape installed extended SPACE tests passed");
    return 0;
}

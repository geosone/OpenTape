/* Installed native SPACE consumer/provider tests. The harness supplies a
 * memory-only manager; exact CDB/count assertions live in mockspace.inc. */
#include <assert.h>
#include <dos.h>
#include <fcntl.h>
#include <stdio.h>
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
int main(int argc,char **argv)
{
    struct ot_tape_control_v1 p;
    struct ot_tape_info_v1 info;
    struct ot_dos_native_writer writer;
    const oa_s32 counts[]={1,-1,8388607L,-8388608L,0,2,-2,0};
    unsigned error=0,handle,other,i;
    int fault=argc>1 && strcmp(argv[1],"fault")==0;
    memset(&writer,0,sizeof(writer));
    assert(ot_dos_native_space("NRMT0",7,8388608L,&p,&error)==OT_NATIVE_ARGUMENT);
    assert(ot_dos_native_space("NRMT0",8,-8388609L,&p,&error)==OT_NATIVE_ARGUMENT);
    assert(ot_dos_native_space("NRMT0",9,1,&p,&error)==OT_NATIVE_ARGUMENT);
    invalid_packets();
    if(fault) {
        assert(ot_dos_native_space("NRMT0",7,1,&p,&error)==OT_NATIVE_COMMAND);
        assert(argc==3 && p.diagnostic_class==(unsigned)(argv[2][0]-'0'));
        assert(p.scsi_opcode==0x11 && p.count==1 && p.completion_flags==5 && p.session_state==0x40);
        assert(ot_dos_native_space("NRMT0",7,1,&p,&error)==OT_NATIVE_BUSY);
        assert(ot_dos_native_execute("NRMT0",0,&p,&error)==0 && p.session_state==0x40);
        assert(ot_dos_native_probe("NRMT0",0,&info,&error)==0 && info.session_state==0x40);
        assert(ot_dos_native_execute("NRMT0",1,&p,&error)==0 && p.session_state==0 && p.completion_flags==0xb);
        assert(ot_dos_native_space("NRMT0",7,1,&p,&error)==0 && p.completion_flags==3);
    } else {
        for(i=0;i<8;++i) {
            unsigned op=i<5 ? 7 : i<7 ? 8 : 9;
            assert(ot_dos_native_space("NRMT0",(oa_u16)op,counts[i],&p,&error)==0);
            assert(p.count==counts[i] && p.scsi_opcode==0x11 && p.completion_flags==3 && p.session_state==0);
        }
        /* Driver-side handle guard cannot be inferred from the snapshot. */
        assert(_dos_open("NRMT0",O_RDONLY,&handle)==0);
        assert(_dos_open("NRMT0",O_RDONLY,&other)==0);
        assert(ot_tape_space_init(&p,7,1)==0);
        assert(raw_control(handle,&p)==0 && p.result==OT_TAPE_RESULT_BUSY && p.completion_flags==0 && p.count==1);
        assert(_dos_close(other)==0 && _dos_close(handle)==0);
        assert(ot_dos_native_writer_open(&writer,"NRMT0")==0);
        assert(ot_dos_native_space("NRMT0",7,1,&p,&error)==OT_NATIVE_BUSY);
        assert(ot_dos_native_writer_close(&writer)==OT_NATIVE_UNFINISHED);
        assert(ot_dos_native_space("NRMT0",8,-1,&p,&error)==OT_NATIVE_BUSY);
        assert(ot_dos_native_execute("NRMT0",0,&p,&error)==0 && p.session_state==0x40);
        assert(ot_dos_native_execute("NRMT0",1,&p,&error)==0 && p.session_state==0);
    }
    puts("OpenTape installed native SPACE tests passed");
    return 0;
}

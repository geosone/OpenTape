/* Installed PREVENT/ALLOW tests. A memory-only manager models removal locks;
 * they survive rewind but remain independent of native writer ownership. */
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/native_tape.h"
#define CHECK(test) ((test) ? (void)0 : (printf("Native lock assertion line %u: %s\n",(unsigned)__LINE__,#test),exit(1)))
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
    unsigned h,other,i,op;
    CHECK(_dos_open("NRMT0",O_RDONLY,&h)==0);
    for(op=12;op<=13;++op) for(i=0;i<3;++i) {
        CHECK(ot_tape_control_init(&p,(oa_u16)op)==0);
        if(i==0) p.count=1;
        if(i==1) p.flags=1;
        if(i==2) p.block_size=512;
        saved=p;
        CHECK(raw(h,&p)!=0 && memcmp(&saved,&p,sizeof(p))==0);
    }
    CHECK(_dos_open("NRMT0",O_RDONLY,&other)==0);
    for(op=12;op<=13;++op) {
        CHECK(ot_tape_control_init(&p,(oa_u16)op)==0);
        CHECK(raw(h,&p)==0 && p.result==OT_TAPE_RESULT_BUSY && p.completion_flags==0);
    }
    CHECK(_dos_close(other)==0 && _dos_close(h)==0);
}
static void execute(unsigned op,int result,unsigned flags,unsigned session)
{
    struct ot_tape_control_v1 p; unsigned error=0;
    CHECK(ot_dos_native_execute("NRMT0",(oa_u16)op,&p,&error)==result);
    CHECK(p.completion_flags==flags && p.session_state==session);
    CHECK(p.count==0 && p.scsi_opcode==(op>=12 ? 0x1e : op>=10 ? 0x1b : op));
    if(op==11 && result==OT_NATIVE_COMMAND) CHECK(p.asc==0x53 && p.ascq==2);
}
/* Raw DOS failures must return to the test instead of opening a DOS prompt. */
static int __far critical_fail(unsigned device,unsigned error,unsigned __far *header)
{ (void)device; (void)error; (void)header; return _HARDERR_FAIL; }
/* A partial legacy record queues one zero-byte EOF. Removal controls must
 * preserve it; consuming it must not read another tape record. */
static void legacy_eof(void)
{
    struct ot_tape_control_v1 p;
    unsigned h,actual,op;
    union REGS regs;
    void (__interrupt __far *old)(void)=_dos_getvect(0x24);
    char data[8];
    _harderr(critical_fail);
    CHECK(_dos_open("NRMT0",O_RDONLY,&h)==0);
    memset(&regs,0,sizeof(regs)); regs.x.ax=0x4400; regs.x.bx=h;
    intdos(&regs,&regs); CHECK(!regs.x.cflag);
    regs.x.dx=(regs.x.dx&0xffu)|0x20u; regs.x.ax=0x4401;
    intdos(&regs,&regs); CHECK(!regs.x.cflag);
    CHECK(_dos_read(h,data,sizeof(data),&actual)==0 && actual==4);
    CHECK(memcmp(data,"RECO",4)==0);
    for(op=12;op<=13;++op) {
        CHECK(ot_tape_control_init(&p,(oa_u16)op)==0);
        CHECK(raw(h,&p)==0 && ot_tape_control_validate(&p,sizeof(p))==0);
        CHECK(p.result==0 && p.completion_flags==3);
    }
    CHECK(_dos_read(h,data,sizeof(data),&actual)==0 && actual==0);
    CHECK(_dos_close(h)==0);
    _dos_setvect(0x24,old);
}
int main(int argc,char **argv)
{
    struct ot_tape_control_v1 p;
    struct ot_tape_info_v1 info;
    struct ot_dos_native_writer writer;
    unsigned error=0;
    memset(&writer,0,sizeof(writer));
    invalid();
    if(argc>1 && strcmp(argv[1],"eof")==0) {
        legacy_eof();
    } else if(argc>1 && strcmp(argv[1],"attention")==0) {
        /* A current UNIT ATTENTION latches native FAILED even for a
         * non-moving command; only explicit REWIND clears it. */
        CHECK(ot_dos_native_execute("NRMT0",12,&p,&error)==OT_NATIVE_COMMAND);
        CHECK(p.completion_flags==1 && p.session_state==0x40 && p.scsi_opcode==0x1e);
        CHECK(p.diagnostic_class==OT_TAPE_DIAG_TARGET && p.sense_key==6 && p.asc==0x29);
        CHECK(p.sense_response==0x70 && p.target_status==2);
        CHECK(ot_dos_native_probe("NRMT0",0,&info,&error)==0 && info.session_state==0x40);
        CHECK(ot_dos_native_space("NRMT0",7,1,&p,&error)==OT_NATIVE_BUSY);
        CHECK(ot_dos_native_writer_open(&writer,"NRMT0")!=0);
        execute(0,0,0x13,0x40);
        execute(1,0,0xb,0);
        execute(13,0,3,0);
        CHECK(ot_dos_native_probe("NRMT0",0,&info,&error)==0 && info.session_state==0);
    } else if(argc>1) {
        CHECK(argc==3);
        CHECK(ot_dos_native_execute("NRMT0",(oa_u16)atoi(argv[1]),&p,&error)==OT_NATIVE_COMMAND);
        CHECK(p.completion_flags==1 && p.session_state==0 && p.scsi_opcode==0x1e);
        CHECK(p.diagnostic_class==(unsigned)atoi(argv[2]));
        CHECK(ot_dos_native_probe("NRMT0",0,&info,&error)==0 && info.session_state==0);
        execute(0,0,0x13,0);
        execute(12,0,3,0);
        execute(13,0,3,0);
    } else {
        execute(12,0,3,0);
        execute(0,0,0x13,0);
        execute(1,0,0xb,0); /* rewind must not silently unlock */
        execute(11,OT_NATIVE_COMMAND,5,0x40);
        execute(12,0,3,0x40);
        execute(13,0,3,0x40);
        execute(0,0,0x13,0x40);
        CHECK(ot_dos_native_space("NRMT0",7,1,&p,&error)==OT_NATIVE_BUSY);
        execute(11,0,3,0x40);
        execute(10,0,3,0x40);
        execute(12,0,3,0x40);
        execute(13,0,3,0x40);
        execute(1,0,0xb,0);
        CHECK(ot_dos_native_writer_open(&writer,"NRMT0")==0);
        CHECK(ot_dos_native_execute("NRMT0",12,&p,&error)==OT_NATIVE_BUSY);
        CHECK(ot_dos_native_execute("NRMT0",13,&p,&error)==OT_NATIVE_BUSY);
        CHECK(ot_dos_native_writer_close(&writer)==OT_NATIVE_UNFINISHED);
        execute(13,0,3,0x40);
        execute(1,0,0xb,0);
        execute(12,0,3,0);
        memset(&writer,0,sizeof(writer));
        CHECK(ot_dos_native_writer_open(&writer,"NRMT0")==0);
        CHECK(ot_dos_native_writer_write(&writer,"RECORD01",8)==0);
        CHECK(ot_dos_native_execute("NRMT0",13,&p,&error)==OT_NATIVE_BUSY);
        CHECK(ot_dos_native_writer_close(&writer)==OT_NATIVE_UNFINISHED);
        execute(13,0,3,0x60);
        execute(12,0,3,0x60);
        execute(13,0,3,0x60);
        execute(1,0,0xb,0);
    }
    puts("OpenTape installed native lock tests passed");
    return 0;
}

/* Installed synchronous LOCATE and independent READ POSITION qualification.
 * The manager models a cursor, partition changes and an ambiguous failed move. */
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/native_tape.h"
#define CHECK(x) ((x) ? (void)0 : (printf("LOCATE assertion line %u: %s\n",(unsigned)__LINE__,#x),exit(1)))
static int legacy;
static unsigned raw(unsigned h,struct ot_tape_locate_v1 *p)
{
    union REGS r; struct SREGS s;
    memset(&r,0,sizeof(r)); segread(&s);
    r.x.ax=0x440c; r.x.bx=h; r.x.cx=0x8005;
    r.x.dx=FP_OFF(p); s.ds=FP_SEG(p); intdosx(&r,&r,&s);
    return r.x.cflag;
}
static void invalid(void)
{
    struct ot_tape_locate_v1 p,saved;
    unsigned h,other,i;
    CHECK(_dos_open("NRMT0",O_RDONLY,&h)==0);
    for(i=0;i<7;++i) {
        CHECK(ot_tape_locate_init(&p,0,0,0xffffffffUL)==0);
        if(i==0) p.format=2;
        if(i==1) p.flags=2;
        if(i==2) p.partition=1;
        if(i==3) { p.flags=1; p.partition=256; }
        if(i==4) p.size=63;
        if(i==5) p.version=0x0200;
        if(i==6) p.magic[0]='X';
        saved=p; CHECK(raw(h,&p)!=0 && memcmp(&saved,&p,sizeof(p))==0);
    }
    CHECK(_dos_open("NRMT0",O_RDONLY,&other)==0);
    CHECK(ot_tape_locate_init(&p,1,255,0xffffffffUL)==0);
    CHECK(raw(h,&p)==0 && ot_tape_locate_validate(&p,sizeof(p))==0 && p.result==2 && p.completion_flags==0);
    CHECK(p.flags==1 && p.partition==255 && p.block==0xffffffffUL);
    CHECK(_dos_close(other)==0 && _dos_close(h)==0);
}
static void observe(oa_u32 part,oa_u32 block,unsigned state)
{
    struct ot_tape_position_v1 p;
    unsigned error;
    CHECK(ot_dos_native_position("NRMT0",&p,&error)==(legacy ? OT_NATIVE_PARTIAL : 0));
    CHECK(p.session_state==state && p.completion_flags==3);
    if(legacy) CHECK(p.valid_fields==0 && p.probe_flags==1);
    else CHECK(p.valid_fields==1 && p.partition==part && p.first_block==block && p.last_block==block);
}
static void locate(unsigned flags,oa_u32 part,oa_u32 block)
{
    struct ot_tape_locate_v1 p; unsigned error;
    CHECK(ot_dos_native_locate("NRMT0",(oa_u16)flags,part,block,&p,&error)==0);
    CHECK(p.flags==flags && p.partition==part && p.block==block && p.completion_flags==3 && p.session_state==0 && p.scsi_opcode==0x2b);
}
static int __far critical_fail(unsigned dev,unsigned err,unsigned __far *header)
{ (void)dev; (void)err; (void)header; return _HARDERR_FAIL; }
static void eof_test(void)
{
    unsigned h,actual;
    union REGS r;
    struct ot_tape_locate_v1 p;
    char data[8];
    void (__interrupt __far *old)(void)=_dos_getvect(0x24);
    _harderr(critical_fail);
    CHECK(_dos_open("NRMT0",O_RDONLY,&h)==0);
    memset(&r,0,sizeof(r)); r.x.ax=0x4400; r.x.bx=h; intdos(&r,&r); CHECK(!r.x.cflag);
    r.x.dx=(r.x.dx&0xffu)|0x20u; r.x.ax=0x4401; intdos(&r,&r); CHECK(!r.x.cflag);
    CHECK(_dos_read(h,data,8,&actual)==0 && actual==4);
    CHECK(ot_tape_locate_init(&p,0,0,0)==0 && raw(h,&p)==0 && p.result==0 && p.completion_flags==3);
    /* Successful movement discards the old queued EOF; this is another READ. */
    CHECK(_dos_read(h,data,8,&actual)==0 && actual==4);
    CHECK(_dos_close(h)==0); _dos_setvect(0x24,old);
}
int main(int argc,char **argv)
{
    const oa_u32 blocks[]={0UL,1UL,65535UL,65536UL,0x7fffffffUL,0x80000000UL,0x89abcdefUL,0xffffffffUL};
    const unsigned flags[]={0,1,0,1,1,0,1,0};
    const oa_u32 parts[]={0,0,0,255,2,0,255,0};
    struct ot_dos_native_writer writer;
    struct ot_tape_locate_v1 p;
    struct ot_tape_control_v1 c;
    unsigned error,i;
    oa_u32 current=2;
    CHECK(argc>=2); legacy=!strcmp(argv[1],"legacy");
    invalid();
    if(!strcmp(argv[1],"eof")) eof_test();
    else if(!strcmp(argv[1],"fault")) {
        CHECK(argc==3);
        CHECK(ot_dos_native_locate("NRMT0",1,255,0xffffffffUL,&p,&error)==OT_NATIVE_COMMAND);
        CHECK(p.completion_flags==5 && p.session_state==0x40 && p.scsi_opcode==0x2b && p.block==0xffffffffUL && p.partition==255);
        CHECK(p.diagnostic_class==(unsigned)atoi(argv[2]));
        CHECK(ot_dos_native_locate("NRMT0",0,0,0,&p,&error)==OT_NATIVE_BUSY);
        CHECK(ot_dos_native_space("NRMT0",7,1,&c,&error)==OT_NATIVE_BUSY);
        observe(2,0xdeadbeefUL,0x40);
        CHECK(ot_dos_native_execute("NRMT0",0,&c,&error)==0 && c.session_state==0x40);
        CHECK(ot_dos_native_execute("NRMT0",1,&c,&error)==0 && c.session_state==0);
        locate(1,255,0xffffffffUL); observe(255,0xffffffffUL,0);
    } else {
        for(i=0;i<8;++i) {
            locate(flags[i],parts[i],blocks[i]);
            if(flags[i]) current=parts[i];
            observe(current,blocks[i],0);
        }
        memset(&writer,0,sizeof(writer)); CHECK(ot_dos_native_writer_open(&writer,"NRMT0")==0);
        CHECK(ot_dos_native_locate("NRMT0",0,0,0,&p,&error)==OT_NATIVE_BUSY);
        CHECK(ot_dos_native_writer_close(&writer)==OT_NATIVE_UNFINISHED);
        CHECK(ot_dos_native_locate("NRMT0",0,0,0,&p,&error)==OT_NATIVE_BUSY);
        observe(current,blocks[7],0x40);
        CHECK(ot_dos_native_execute("NRMT0",1,&c,&error)==0 && c.session_state==0);
        memset(&writer,0,sizeof(writer)); CHECK(ot_dos_native_writer_open(&writer,"NRMT0")==0);
        CHECK(ot_dos_native_writer_write(&writer,"RECORD01",8)==0);
        CHECK(ot_dos_native_writer_close(&writer)==OT_NATIVE_UNFINISHED);
        CHECK(ot_dos_native_locate("NRMT0",0,0,0,&p,&error)==OT_NATIVE_BUSY);
        observe(current,0,0x60);
        CHECK(ot_dos_native_execute("NRMT0",1,&c,&error)==0 && c.session_state==0);
        locate(0,0,0); observe(current,0,0);
    }
    puts("OpenTape installed native LOCATE tests passed");
    return 0;
}

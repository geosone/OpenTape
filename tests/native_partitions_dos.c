/* Installed MODE SENSE partition qualification. No physical media is attached.
 * Exact CDB/layout/length and no replay are checked by the mock manager. */
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/native_tape.h"
#define CHECK(x) ((x) ? (void)0 : (printf("Partitions assertion line %u: %s\n",(unsigned)__LINE__,#x),exit(1)))
static const char *kind;
static unsigned diagnostic;
static unsigned raw(unsigned h,unsigned fn,void *p)
{
    union REGS r; struct SREGS s;
    memset(&r,0,sizeof(r)); segread(&s);
    r.x.ax=0x440c; r.x.bx=h; r.x.cx=fn;
    r.x.dx=FP_OFF(p); s.ds=FP_SEG(p); intdosx(&r,&r,&s);
    return r.x.cflag;
}
static void status(unsigned char *data)
{
    unsigned h;
    memset(data,0,32);
    CHECK(_dos_open("NRMT0",O_RDONLY,&h)==0);
    CHECK(raw(h,0x0705,data)==0);
    CHECK(_dos_close(h)==0);
}
static void partitions(unsigned state)
{
    struct ot_tape_partitions_v1 p;
    unsigned error=0;
    unsigned char before[32],after[32];
    int rc,expected=0;
    if(!strcmp(kind,"legacy")) expected=OT_NATIVE_PARTIAL;
    if(!strcmp(kind,"fault") || !strcmp(kind,"malformed")) expected=OT_NATIVE_COMMAND;
    status(before);
    rc=ot_dos_native_partitions("NRMT0",&p,&error);
    CHECK(rc==expected && error==0 && p.session_state==state);
    CHECK(ot_tape_partitions_validate(&p,sizeof(p))==0 && p.scsi_opcode==0x1a);
    CHECK(p.completion_flags==(!strcmp(kind,"fault") ? 1 : 3));
    if(expected==0) {
        CHECK(p.result==0 && p.valid_fields==1);
        CHECK(p.maximum_additional==(!strcmp(kind,"pending") ? 0UL : 3UL));
        CHECK(p.defined_additional==(!strcmp(kind,"pending") ? 255UL : 2UL));
        CHECK(p.partition_flags==0x3c && p.format_recognition==3 && p.partition_units==0x18);
        CHECK(p.size_descriptors==(!strcmp(kind,"empty") ? 0UL : 4UL));
    } else {
        CHECK(p.valid_fields==0 && p.maximum_additional==0 && p.defined_additional==0);
        CHECK(p.partition_flags==0 && p.format_recognition==0 && p.partition_units==0 && p.size_descriptors==0);
        if(!strcmp(kind,"legacy")) CHECK(p.result==1 && p.diagnostic_class==5 && p.probe_flags==1);
        else CHECK(p.result==3 && p.diagnostic_class==(!strcmp(kind,"malformed") ? 3 : diagnostic));
    }
    status(after); CHECK(memcmp(before,after,32)==0); /* observation preserves legacy diagnostics */
}
static void invalid(void)
{
    struct ot_tape_partitions_v1 p,saved;
    unsigned h,other,i;
    CHECK(_dos_open("NRMT0",O_RDONLY,&h)==0);
    for(i=0;i<5;++i) {
        CHECK(ot_tape_partitions_init(&p)==0);
        if(i==0) p.format=2;
        if(i==1) p.flags=1;
        if(i==2) p.size=63;
        if(i==3) p.version=0x0200;
        if(i==4) p.magic[0]='X';
        saved=p; CHECK(raw(h,0x8006,&p)!=0 && memcmp(&saved,&p,sizeof(p))==0);
    }
    CHECK(_dos_open("NRMT0",O_RDONLY,&other)==0);
    CHECK(ot_tape_partitions_init(&p)==0);
    CHECK(raw(h,0x8006,&p)==0 && ot_tape_partitions_validate(&p,sizeof(p))==0 && p.result==2 && p.completion_flags==0);
    CHECK(_dos_close(other)==0 && _dos_close(h)==0);
}
/* Legacy partial-filemark state is separate from reported partition metadata. */
static int __far critical_fail(unsigned dev,unsigned err,unsigned __far *header)
{ (void)dev; (void)err; (void)header; return _HARDERR_FAIL; }
static void eof_test(void)
{
    unsigned h,actual;
    union REGS r;
    struct ot_tape_partitions_v1 p;
    char data[8];
    void (__interrupt __far *old)(void)=_dos_getvect(0x24);
    _harderr(critical_fail);
    CHECK(_dos_open("NRMT0",O_RDONLY,&h)==0);
    memset(&r,0,sizeof(r)); r.x.ax=0x4400; r.x.bx=h; intdos(&r,&r); CHECK(!r.x.cflag);
    r.x.dx=(r.x.dx&0xffu)|0x20u; r.x.ax=0x4401; intdos(&r,&r); CHECK(!r.x.cflag);
    CHECK(_dos_read(h,data,8,&actual)==0 && actual==4);
    CHECK(ot_tape_partitions_init(&p)==0 && raw(h,0x8006,&p)==0 && p.valid_fields==1);
    CHECK(_dos_read(h,data,8,&actual)==0 && actual==0);
    CHECK(_dos_close(h)==0); _dos_setvect(0x24,old);
}
int main(int argc,char **argv)
{
    struct ot_dos_native_writer writer;
    struct ot_tape_control_v1 c;
    struct ot_tape_partitions_v1 p;
    unsigned error=0;
    CHECK(argc>=2); kind=argv[1]; diagnostic=argc>2 ? (unsigned)atoi(argv[2]) : 0;
    invalid();
    if(!strcmp(kind,"retained")) {
        memset(&writer,0,sizeof(writer)); CHECK(ot_dos_native_writer_open(&writer,"NRMT0")==0);
        /* A competing open poisons the claim without issuing a CDB. */
        CHECK(ot_dos_native_partitions("NRMT0",&p,&error)==OT_NATIVE_BUSY);
        CHECK(ot_dos_native_writer_close(&writer)==OT_NATIVE_UNFINISHED);
    } else if(!strcmp(kind,"eof")) eof_test();
    else {
        partitions(0);
        memset(&writer,0,sizeof(writer)); CHECK(ot_dos_native_writer_open(&writer,"NRMT0")==0);
        CHECK(ot_dos_native_partitions("NRMT0",&p,&error)==OT_NATIVE_BUSY);
        CHECK(ot_dos_native_writer_close(&writer)==OT_NATIVE_UNFINISHED);
        partitions(0x40);
        CHECK(ot_dos_native_space("NRMT0",7,1,&c,&error)==OT_NATIVE_BUSY);
        CHECK(ot_dos_native_execute("NRMT0",1,&c,&error)==0 && c.session_state==0);
        memset(&writer,0,sizeof(writer)); CHECK(ot_dos_native_writer_open(&writer,"NRMT0")==0);
        CHECK(ot_dos_native_writer_write(&writer,"RECORD01",8)==0);
        CHECK(ot_dos_native_writer_close(&writer)==OT_NATIVE_UNFINISHED);
        partitions(0x60);
        CHECK(ot_dos_native_execute("NRMT0",1,&c,&error)==0 && c.session_state==0);
    }
    puts("OpenTape installed native partitions tests passed");
    return 0;
}

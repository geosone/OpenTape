/* Explicit mark writes against a memory-only ASPI manager. No host tape.
 * Check ownership/dirty state and exact submission without coupling setmarks
 * to the archive format's ordinary filemark finalization. */
#include <dos.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/native_tape.h"
#undef assert
#define assert(x) ((x) ? (void)0 : (printf("Setwrite assertion %u: %s\n",(unsigned)__LINE__,#x),exit(1)))
static struct ot_dos_native_writer w;
static unsigned raw(unsigned h,struct ot_tape_control_v1 *p)
{
    union REGS r; struct SREGS s;
    memset(&r,0,sizeof(r)); segread(&s); r.x.ax=0x440c; r.x.bx=h; r.x.cx=0x8001;
    r.x.dx=FP_OFF(p); s.ds=FP_SEG(p); intdosx(&r,&r,&s); return r.x.cflag;
}
static void open_writer(unsigned fixed)
{
    memset(&w,0,sizeof(w));
    assert((fixed ? ot_dos_native_writer_open_fixed(&w,"NRMT0",512) : ot_dos_native_writer_open(&w,"NRMT0"))==0);
}
#ifdef OT_NATIVE_TAPE_WRITE_SETMARK
static void invalid(void)
{
    struct ot_tape_control_v1 p,saved;
    unsigned h,other,i,error=0;
    assert(ot_dos_native_writer_setmark(0)==OT_NATIVE_ARGUMENT);
    assert(ot_dos_native_writer_setmark(&w)==OT_NATIVE_ARGUMENT);
    assert(ot_dos_native_execute("NRMT0",OT_TAPE_CONTROL_SETMARK,&p,&error)==OT_NATIVE_ARGUMENT);
    assert(ot_dos_native_space("NRMT0",OT_TAPE_CONTROL_SETMARK,0,&p,&error)==OT_NATIVE_ARGUMENT);
    assert(_dos_open("NRMT0",O_RDONLY,&h)==0);
    for(i=0;i<5;++i) {
        assert(ot_tape_control_init(&p,OT_TAPE_CONTROL_SETMARK)==0);
        if(i==0) p.count=1;
        if(i==1) p.count=-1;
        if(i==2) p.flags=1;
        if(i==3) p.block_size=512;
        if(i==4) p.operation=18;
        saved=p; assert(raw(h,&p)!=0 && memcmp(&saved,&p,sizeof(p))==0);
    }
    assert(ot_tape_control_init(&p,OT_TAPE_CONTROL_SETMARK)==0);
    assert(raw(h,&p)==0 && ot_tape_control_validate(&p,sizeof(p))==0 && p.result==2 && p.completion_flags==0);
    assert(_dos_open("NRMT0",O_RDONLY,&other)==0);
    assert(ot_tape_control_init(&p,OT_TAPE_CONTROL_SETMARK)==0);
    assert(raw(h,&p)==0 && p.result==2 && p.completion_flags==0);
    assert(_dos_close(other)==0 && _dos_close(h)==0);
}
static void success(void)
{
    assert(ot_dos_native_writer_setmark(&w)==0);
    assert(w.writer.claimed && w.writer.last_control.operation==OT_TAPE_CONTROL_SETMARK);
    assert(w.writer.last_control.scsi_opcode==0x10 && w.writer.last_control.completion_flags==3);
    assert(w.writer.last_control.session_state==0x30 && w.writer.last_control.count==0);
}
static void recover(void)
{
    struct ot_tape_info_v1 info;
    struct ot_tape_control_v1 c;
    unsigned error;
    assert(ot_dos_native_probe("NRMT0",0,&info,&error)==0 && info.session_state==0x60);
    assert(ot_dos_native_writer_open(&w,"NRMT0")==OT_NATIVE_BUSY);
    assert(ot_dos_native_execute("NRMT0",1,&c,&error)==0 && c.session_state==0 && c.completion_flags==0xb);
}
#endif
int main(int argc,char **argv)
{
    unsigned fixed,fault,abandon;
    assert(argc==4); fixed=(unsigned)atoi(argv[1]); fault=(unsigned)atoi(argv[2]); abandon=(unsigned)atoi(argv[3]);
#ifndef OT_NATIVE_TAPE_WRITE_SETMARK
    (void)fault; (void)abandon;
    open_writer(fixed);
    assert(ot_dos_native_writer_finish(&w)==0 && ot_dos_native_writer_close(&w)==0);
#else
    {
        struct ot_tape_control_v1 saved,p;
        static char block[512];
        oa_u32 capabilities;
        invalid(); open_writer(fixed);
        /* A missing provider bit is a preflight rejection, not a failed write. */
        capabilities=w.writer.operations; saved=w.writer.last_control;
        w.writer.operations &= ~OT_TAPE_OP_SETMARK;
        assert(ot_dos_native_writer_setmark(&w)==OT_NATIVE_UNSUPPORTED && !w.writer.failure && w.writer.claimed);
        assert(memcmp(&saved,&w.writer.last_control,sizeof(saved))==0);
        w.writer.operations=capabilities;
        if(fault) {
            unsigned expected=(fault==1 || fault==4) ? 1 : (fault==2 || fault==8) ? 4 : 2;
            assert(ot_dos_native_writer_setmark(&w)==OT_NATIVE_COMMAND);
            saved=w.writer.last_control;
            assert(saved.operation==OT_TAPE_CONTROL_SETMARK && saved.diagnostic_class==expected);
            assert(saved.scsi_opcode==0x10 && saved.completion_flags==5 && saved.session_state==0x70);
            assert(ot_tape_control_init(&p,OT_TAPE_CONTROL_SETMARK)==0);
            assert(raw(w.session.handle,&p)==0 && p.result==2 && p.completion_flags==0 && p.session_state==0x70);
            assert(ot_dos_native_writer_setmark(&w)==OT_NATIVE_COMMAND);
            assert(ot_dos_native_writer_write(&w,fixed ? block : "RECORD01",fixed ? 512 : 8)==OT_NATIVE_COMMAND);
            assert(ot_dos_native_writer_flush(&w)==OT_NATIVE_COMMAND);
            assert(ot_dos_native_writer_filemark(&w)==OT_NATIVE_COMMAND);
            assert(ot_dos_native_writer_finish(&w)==OT_NATIVE_COMMAND);
            assert(ot_dos_native_writer_close(&w)==OT_NATIVE_COMMAND);
            assert(memcmp(&saved,&w.writer.last_control,sizeof(saved))==0);
        } else {
            success();
            if(abandon) {
                if(abandon==2) assert(ot_dos_native_writer_abort(&w)==0);
                assert(ot_dos_native_writer_close(&w)==OT_NATIVE_UNFINISHED);
            } else {
                success();
                assert(ot_dos_native_writer_finish(&w)==0 && !w.writer.claimed);
                assert(ot_dos_native_writer_close(&w)==0);
            }
        }
        if(fault || abandon) {
            recover(); open_writer(fixed); success();
            assert(ot_dos_native_writer_finish(&w)==0 && ot_dos_native_writer_close(&w)==0);
        }
    }
#endif
    puts("Installed native setmark write/ownership/no-replay tests passed");
    return 0;
}

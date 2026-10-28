/* Installed provider qualification, exclusively with the memory-only mock.
 * A setmark is observable to a raw reader, but never completes an OTB1 archive. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/archive_native.h"
static struct ot_dos_native_reader n;
static unsigned char data[1026],scratch[1024];
#undef assert
#define assert(x) ((x) ? (void)0 : (printf("Setmark assertion %u: %s\n",(unsigned)__LINE__,#x),exit(1)))
static void open_reader(unsigned fixed)
{
    memset(&n,0,sizeof(n));
    assert((fixed ? ot_dos_native_reader_open_fixed(&n,"NRMT0",512) :
        ot_dos_native_reader_open(&n,"NRMT0"))==0);
}
int main(int argc,char **argv)
{
    unsigned fixed,combined,fault,unknown,capacity,actual,stream,completion,diag,i,j;
    int expected;
    assert(argc==5);
    fixed=(unsigned)atoi(argv[1]); combined=(unsigned)atoi(argv[2]);
    fault=(unsigned)atoi(argv[3]); unknown=(unsigned)atoi(argv[4]);
    capacity=fixed ? 1024 : 8;
#ifndef OT_TAPE_STREAM_SETMARK
    (void)combined; (void)fault; (void)unknown; (void)actual; (void)stream;
    (void)completion; (void)diag; (void)i; (void)j; (void)expected;
    open_reader(fixed);
    assert(ot_dos_native_reader_read(&n,data+1,capacity)==OT_NATIVE_RESPONSE);
    assert(n.reader.failed && n.reader.last.size==0);
    assert(ot_dos_native_reader_read(&n,data+1,capacity)==OT_NATIVE_COMMAND);
    assert(ot_dos_native_reader_close(&n)==0);
#else
    actual=combined ? (fixed ? 512 : 4) : 0;
    stream=OT_TAPE_STREAM_SETMARK|OT_TAPE_STREAM_SHORT;
    completion=0x23; diag=2; expected=OT_NATIVE_STREAM;
    if(fault || unknown) { expected=OT_NATIVE_COMMAND; completion=0x25; }
    if(fault<=3 && fault) { actual=0; stream=0; completion=5; diag=fault==2 ? 2 : 1; }
    if(fault==5 || fault==6) { actual=0; stream=0; completion=5; diag=3; }
    if(fault==7) { expected=OT_NATIVE_STREAM; completion=0x23; stream=9; }
    if(unknown) { actual=0; stream=OT_TAPE_STREAM_SETMARK; completion=5; }
    open_reader(fixed);
    for(i=0;i<2;++i) {
        memset(data,0xa5,sizeof(data));
        assert(ot_dos_native_reader_read(&n,data+1,capacity)==expected);
        assert(ot_tape_record_validate(&n.reader.last,sizeof(n.reader.last))==0);
        assert(n.reader.last.actual==actual && n.reader.last.stream_flags==stream);
        assert(n.reader.last.completion_flags==completion && n.reader.last.diagnostic_class==diag);
        assert(n.reader.last.session_state==0 && data[0]==0xa5 && data[capacity+1]==0xa5);
        for(j=0;j<actual;++j) assert(data[j+1]==0x5a);
        if(expected<0) {
            assert(n.reader.failed && ot_dos_native_reader_read(&n,data+1,capacity)==OT_NATIVE_COMMAND);
            break;
        }
    }
    assert(ot_dos_native_reader_close(&n)==0);
    if(!fault && !unknown) {
        struct ot_tape_reader archive;
        unsigned accepted=99;
        memset(&archive,0,sizeof(archive)); open_reader(fixed);
        assert((fixed ? ot_archive_native_reader_fixed(&archive,&n.reader,scratch,sizeof(scratch)) :
            ot_archive_native_reader(&archive,&n.reader))==0);
        assert(ot_reader_read(&archive,data+1,capacity,&accepted)==OT_READ_IO_ERROR);
        assert(accepted==0 && archive.failed && !archive.pending_filemark);
        assert(archive.backend_result==OT_NATIVE_STREAM);
        assert(ot_reader_read(&archive,data+1,capacity,&accepted)==OT_READ_FAILED_STATE);
        assert(ot_reader_close(&archive)==0);
    }
#endif
    puts("Installed native setmark distinction/archive gate passed");
    return 0;
}

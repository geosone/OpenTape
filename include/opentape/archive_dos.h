#ifndef OPENTAPE_ARCHIVE_DOS_H
#define OPENTAPE_ARCHIVE_DOS_H
#include "opentape/tape_output.h"
#include "opentape/verify_stream.h"
#ifdef OT_NATIVE_TAPE_API
void ot_dos_archive_select_native(void);
/* Bounded DOS scratch workspace; no MODE SELECT or automatic size guessing. */
#define OT_DOS_ARCHIVE_FIXED_MAX 4096u
int ot_dos_archive_select_native_fixed(unsigned);
unsigned ot_dos_archive_is_native(void);
unsigned ot_dos_archive_fixed_block(void);
int ot_dos_archive_reader_open(struct ot_tape_reader *, const char *);
int ot_dos_archive_output_open(struct ot_tape_output *, const char *, unsigned);
int ot_dos_archive_verify_observed(const char *, oa_u8 *, unsigned, ot_metadata_visitor, void *,
    ot_verify_observer, void *, struct ot_verify_session_result *);
int ot_dos_archive_verify(const char *, oa_u8 *, unsigned, ot_metadata_visitor, void *, struct ot_verify_session_result *);
#else
#define ot_dos_archive_is_native() 0u
#define ot_dos_archive_fixed_block() 0u
#define ot_dos_archive_reader_open(r,n) ot_reader_open((r),ot_dos_reader_ops(),(n))
#define ot_dos_archive_output_open(o,n,a) ot_output_open((o),ot_dos_output_ops(),(n),(a))
#define ot_dos_archive_verify_observed(n,b,c,v,x,o,y,r) ot_verify_session_observed(ot_dos_reader_ops(),(n),(b),(c),(v),(x),(o),(y),(r))
#define ot_dos_archive_verify(n,b,c,v,x,r) ot_verify_session(ot_dos_reader_ops(),(n),(b),(c),(v),(x),(r))
#endif
#endif

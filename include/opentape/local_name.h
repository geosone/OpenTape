#ifndef OPENTAPE_LOCAL_NAME_H
#define OPENTAPE_LOCAL_NAME_H
/* Detect the final DOS filename stem of tape aliases (MTx/RMTx/NMTx/NRMTx,
 * custom drive letters, optional '$' and extension). No filesystem access.
 * Regular-file callers must reject these BEFORE opening: closing a rewind
 * alias can move media even if a later handle check refuses device reads. */
int ot_local_tape_name(const char *);
/* Bounded regular 8.3 leaf; DOS backend additionally canonicalizes/checks parents. */
int ot_local_file_name_valid(const char *, unsigned maximum);
#endif

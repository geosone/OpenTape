#ifndef OPENTAPE_CATALOG_CLI_H
#define OPENTAPE_CATALOG_CLI_H
#include "opentape/record_payload.h"
/* One shared textual metadata row for live listing and offline catalogue files.
 * Formatting never establishes tape identity. Nonzero means stdout failed. */
int ot_dos_list_entry(oa_u32 session, oa_u32 object, const struct ot_file_metadata *);
/* Exactly one of input/output must be supplied. Input loads/checks/closes a
 * regular metadata file without tape I/O. Output preflights a new filename,
 * reads one fully verified tape session at current position, prints/flushes
 * the complete metadata, then publishes through the shared staging service.
 * No rewind/retry or restoration; all collected nodes are freed on every path.
 * Returns shell status 0 on success, 1 on invalid arguments, 2 on I/O/format
 * failure. Post-publication stdout failure may leave a valid exported file. */
int ot_dos_catalog_list(const char *input, const char *output, const char *device);
#endif

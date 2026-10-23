#ifndef OPENTAPE_ARCHIVE_NATIVE_H
#define OPENTAPE_ARCHIVE_NATIVE_H
#include "opentape/native_tape.h"
#include "opentape/tape_reader.h"
#include "opentape/tape_output.h"
/* Attach an already open native session. Its owner must outlive the archive
 * object. Generic close owns closing the attached session exactly once.
 */
int ot_archive_native_reader(struct ot_tape_reader *, struct ot_native_reader *);
int ot_archive_native_output(struct ot_tape_output *, struct ot_native_writer *);
/* Fixed framing: one logical OTB1 block spans whole physical blocks, with
 * zero padding only after its declared length. Workspace must be disjoint from
 * archive I/O buffers, at least one physical block and at most 65535 bytes;
 * its owner keeps it alive through close. No allocation or mode change occurs.
 * The ordinary attach functions still reject fixed sessions without workspace.
 */
int ot_archive_native_reader_fixed(struct ot_tape_reader *, struct ot_native_reader *, oa_u8 *, unsigned);
int ot_archive_native_output_fixed(struct ot_tape_output *, struct ot_native_writer *, oa_u8 *, unsigned);
#endif

#include <string.h>
#include "opentape/archive_dos.h"
#include "opentape/archive_native.h"

/* The DOS frontend runs one synchronous tape command at a time. Storage and
 * callback contexts remain alive through verification/restore and close.
 */
static unsigned selected_native, selected_block;
static oa_u8 fixed_workspace[OT_DOS_ARCHIVE_FIXED_MAX];
static struct ot_dos_native_reader native_reader;
static struct ot_dos_native_writer native_writer;
void ot_dos_archive_select_native(void)
{ selected_native = 1u; selected_block = 0u; }
int ot_dos_archive_select_native_fixed(unsigned block)
{
    if (block == 0u || block > OT_DOS_ARCHIVE_FIXED_MAX) return OT_NATIVE_ARGUMENT;
    selected_native = 1u; selected_block = block;
    return 0;
}
unsigned ot_dos_archive_is_native(void)
{ return selected_native; }
unsigned ot_dos_archive_fixed_block(void)
{ return selected_block; }

int ot_dos_archive_reader_open(struct ot_tape_reader *r, const char *name)
{
    int rc;
    if (!selected_native) return ot_reader_open(r, ot_dos_reader_ops(), name);
    if (r == 0 || r->opened) return OT_READ_BAD_ARGUMENT;
    rc = selected_block ? ot_dos_native_reader_open_fixed(&native_reader, name, selected_block) :
        ot_dos_native_reader_open(&native_reader, name);
    r->dos_error = native_reader.reader.platform_error;
    r->backend_result = rc;
    if (rc != 0) return rc;
    rc = selected_block ? ot_archive_native_reader_fixed(r, &native_reader.reader,
        fixed_workspace, sizeof(fixed_workspace)) : ot_archive_native_reader(r, &native_reader.reader);
    if (rc != 0) (void)ot_dos_native_reader_close(&native_reader);
    return rc;
}
int ot_dos_archive_output_open(struct ot_tape_output *o, const char *name, unsigned authorized)
{
    int rc;
    if (!selected_native) return ot_output_open(o, ot_dos_output_ops(), name, authorized);
    if (o == 0 || o->opened || !ot_probe_raw_name_valid(name)) return OT_OUTPUT_BAD_ARGUMENT;
    if (!authorized) return OT_OUTPUT_NOT_AUTHORIZED;
    rc = selected_block ? ot_dos_native_writer_open_fixed(&native_writer, name, selected_block) :
        ot_dos_native_writer_open(&native_writer, name);
    o->dos_error = native_writer.writer.platform_error;
    o->backend_result = rc;
    if (rc != 0) return rc;
    rc = selected_block ? ot_archive_native_output_fixed(o, &native_writer.writer,
        fixed_workspace, sizeof(fixed_workspace)) : ot_archive_native_output(o, &native_writer.writer);
    if (rc != 0) (void)ot_dos_native_writer_close(&native_writer);
    return rc;
}
int ot_dos_archive_verify_observed(const char *name, oa_u8 *buffer, unsigned capacity,
                           ot_metadata_visitor visitor, void *context, ot_verify_observer observer, void *observer_context, struct ot_verify_session_result *result)
{
    int rc;
    if (!selected_native) return ot_verify_session_observed(ot_dos_reader_ops(), name, buffer, capacity, visitor, context, observer, observer_context, result);
    if (result == 0) return OT_READ_BAD_ARGUMENT;
    memset(result, 0, sizeof(*result));
    result->transport_error = 1u;
    if (buffer == 0 || capacity < 2u || (oa_u32)capacity > 65535UL) return OT_READ_BAD_ARGUMENT;
    rc = ot_dos_archive_reader_open(&result->reader, name);
    if (rc != 0) return rc;
    return ot_verify_session_opened_observed(buffer, capacity, visitor, context, observer, observer_context, result);
}

int ot_dos_archive_verify(const char *name, oa_u8 *buffer, unsigned capacity,
                           ot_metadata_visitor visitor, void *context, struct ot_verify_session_result *result)
{
    return ot_dos_archive_verify_observed(name, buffer, capacity, visitor, context, 0, 0, result);
}

#include "opentape/source_file.h"

int ot_source_verify(const struct ot_source_ops *ops, oa_u32 size, oa_u32 expected_crc,
                      oa_u8 *buffer, unsigned capacity)
{
    oa_u32 remaining = size, crc = 0ul;
    unsigned got, want, error;
    int rc = 0;
    if (ops == 0 || ops->read == 0 || ops->recheck == 0 || ops->close == 0 ||
        buffer == 0 || capacity == 0u || (oa_u32)capacity > 65535ul) return OT_SOURCE_ARGUMENT;
    while (remaining != 0ul && rc == 0) {
        want = remaining > capacity ? capacity : (unsigned)remaining;
        got = 0u; error = ops->read(ops->context, buffer, want, &got);
        if (error != 0u || got > want) rc = OT_SOURCE_IO;
        else if (got == 0u) rc = OT_SOURCE_CONTENT;
        else { crc = ot_crc32_extend(crc, buffer, (oa_u16)got); remaining -= got; }
    }
    if (rc == 0) {
        got = 0u; error = ops->read(ops->context, buffer, 1u, &got);
        if (error != 0u || got > 1u) rc = OT_SOURCE_IO;
        else if (got != 0u || crc != expected_crc) rc = OT_SOURCE_CONTENT;
    }
    if (rc == 0 && ops->recheck(ops->context) != 0u) rc = OT_SOURCE_CHANGED;
    error = ops->close(ops->context);
    if (rc == 0 && error != 0u) rc = OT_SOURCE_IO;
    return rc;
}

int ot_backup_source(struct ot_archive_writer *w, oa_u32 object,
                      const struct ot_file_metadata *m, const struct ot_source_ops *ops,
                      oa_u8 *buffer, unsigned capacity)
{
    oa_u32 remaining;
    unsigned got, want, error;
    int rc;
    if (w == 0 || !w->initialized || m == 0 || m->kind != OT_FILE_REGULAR ||
        ops == 0 || ops->read == 0 || ops->recheck == 0 || ops->close == 0 ||
        buffer == 0 || capacity == 0u || (oa_u32)capacity > 65535ul ||
        capacity > w->capacity - OT_WRITER_RECORD_OVERHEAD)
        return OT_SOURCE_ARGUMENT;
    rc = ot_archive_writer_metadata(w, object, m);
    remaining = m->file_size;
    while (rc == 0 && remaining != 0ul) {
        want = remaining > capacity ? capacity : (unsigned)remaining;
        got = 0u;
        error = ops->read(ops->context, buffer, want, &got);
        if (error != 0u || got > want) rc = OT_SOURCE_IO;
        else if (got == 0u) rc = OT_SOURCE_CHANGED;
        else {
            rc = ot_archive_writer_data(w, buffer, (oa_u16)got);
            remaining -= got;
        }
    }
    if (rc == 0) {
        got = 0u;
        error = ops->read(ops->context, buffer, 1u, &got);
        if (error != 0u || got > 1u) rc = OT_SOURCE_IO;
        else if (got != 0u) rc = OT_SOURCE_CHANGED;
    }
    if (rc == 0 && ops->recheck(ops->context) != 0u) rc = OT_SOURCE_CHANGED;
    error = ops->close(ops->context);
    if (rc == 0 && error != 0u) rc = OT_SOURCE_IO;
    if (rc == 0) rc = ot_archive_writer_file_end(w);
    /* An incomplete or changed source must never be followed by SESSION_END,
     * even if a caller mistakenly continues using this writer. */
    if (rc != 0) w->failed = 1u;
    return rc;
}

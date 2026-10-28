/* Streaming offline metadata format. Header: OTCAT1 CR LF, u16 version/header/
 * count/reserved, u32 payload length/CRC/reserved/header CRC. Payload: existing
 * 88-byte volume codec, then (u16 metadata length/reserved,u32 session/object,
 * existing metadata codec). Bounded chunks/nodes work above DOS's 64 KiB limit. */
#include <string.h>
#include "opentape/catalog_file.h"
#include "wire.h"
static int identity(oa_u32 session, oa_u32 object, oa_u32 previous_session, oa_u32 previous_object)
{
    return session != 0ul && object != 0ul && session >= previous_session &&
        (session != previous_session || object > previous_object);
}
static int entry_bytes(const struct ot_catalog_entry *entry, oa_u8 *data, oa_u16 *size)
{
    oa_u16 metadata_size;
    if (ot_file_metadata_encode(data + 12u, OT_FILE_METADATA_PREFIX + OT_FILE_PATH_MAX,
        &entry->metadata, &metadata_size) != 0) return OT_CATALOG_ARGUMENT;
    ot_put_u16(data, metadata_size); ot_put_u16(data + 2u, 0u);
    ot_put_u32(data + 4u, entry->session); ot_put_u32(data + 8u, entry->object);
    *size = (oa_u16)(metadata_size + 12u);
    return 0;
}
int ot_catalog_file_measure(const struct ot_catalog *catalog, const struct ot_volume_label *label,
                             struct ot_catalog_file_info *info)
{
    const struct ot_catalog_entry *entry, *last = 0;
    struct ot_catalog_file_info value;
    oa_u8 data[12u + OT_FILE_METADATA_PREFIX + OT_FILE_PATH_MAX];
    oa_u32 session = 0ul, object = 0ul;
    oa_u16 size;
    unsigned i, marked = 0u, replacements = 0u;
    if (catalog == 0 || label == 0 || info == 0 || catalog->error || catalog->count > OT_CATALOG_MAX_ENTRIES ||
        ot_volume_label_encode(data, label) != 0) return OT_CATALOG_ARGUMENT;
    value.size = OT_CATALOG_FILE_HEADER + OT_VOLUME_LABEL_SIZE;
    value.payload_crc = ot_crc32(data, OT_VOLUME_LABEL_SIZE);
    entry = catalog->first;
    for (i = 0u; i < catalog->count; ++i) {
        if (entry == 0 || !identity(entry->session, entry->object, session, object) ||
            entry->marked > 1u || entry->replace > entry->marked ||
            (entry->metadata.kind == OT_FILE_DIRECTORY && entry->marked) || entry_bytes(entry, data, &size) != 0)
            return OT_CATALOG_ARGUMENT;
        value.size += size; value.payload_crc = ot_crc32_extend(value.payload_crc, data, size);
        marked += entry->marked; replacements += entry->replace;
        session = entry->session; object = entry->object; last = entry; entry = entry->next;
    }
    if (entry != 0 || last != catalog->last || marked != catalog->marked || replacements != catalog->replacements)
        return OT_CATALOG_ARGUMENT;
    *info = value;
    return 0;
}
static int write_bytes(const struct ot_selection_output *ops, const oa_u8 *data, unsigned size)
{
    unsigned accepted = 0u;
    return ops->write(ops->context, data, size, &accepted) != 0u || accepted != size ? OT_CATALOG_FILE_IO : 0;
}
int ot_catalog_file_save(const struct ot_catalog *catalog, const struct ot_volume_label *label,
                         const struct ot_selection_output *ops)
{
    struct ot_catalog_file_info info;
    const struct ot_catalog_entry *entry;
    oa_u8 data[12u + OT_FILE_METADATA_PREFIX + OT_FILE_PATH_MAX];
    oa_u16 size;
    unsigned error;
    int rc;
    if (ops == 0 || ops->write == 0 || ops->close == 0 || ops->publish == 0) return OT_CATALOG_ARGUMENT;
    rc = ot_catalog_file_measure(catalog, label, &info);
    if (rc == 0) {
        memset(data, 0, OT_CATALOG_FILE_HEADER); memcpy(data, "OTCAT1\r\n", 8u);
        ot_put_u16(data + 8u, 1u); ot_put_u16(data + 10u, OT_CATALOG_FILE_HEADER);
        ot_put_u16(data + 12u, (oa_u16)catalog->count);
        ot_put_u32(data + 16u, info.size - OT_CATALOG_FILE_HEADER); ot_put_u32(data + 20u, info.payload_crc);
        ot_put_u32(data + 28u, ot_crc32(data, 28u)); rc = write_bytes(ops, data, OT_CATALOG_FILE_HEADER);
        if (rc == 0) { ot_volume_label_encode(data, label); rc = write_bytes(ops, data, OT_VOLUME_LABEL_SIZE); }
        for (entry = catalog->first; entry != 0 && rc == 0; entry = entry->next) {
            rc = entry_bytes(entry, data, &size); if (rc == 0) rc = write_bytes(ops, data, size);
        }
    }
    error = ops->close(ops->context);
    if (rc == 0 && error) rc = OT_CATALOG_FILE_IO;
    if (rc == 0 && ops->publish(ops->context)) rc = OT_CATALOG_FILE_IO;
    return rc;
}
static int read_bytes(const struct ot_source_ops *ops, oa_u8 *data, unsigned size)
{
    unsigned got, used = 0u;
    while (used < size) {
        got = 0u;
        if (ops->read(ops->context, data + used, size - used, &got) || got > size - used) return OT_CATALOG_FILE_IO;
        if (got == 0u) return OT_CATALOG_FILE_FORMAT;
        used += got;
    }
    return 0;
}
int ot_catalog_file_load(struct ot_catalog *catalog, struct ot_volume_label *label, const struct ot_source_ops *ops)
{
    struct ot_catalog temporary;
    struct ot_volume_label volume;
    struct ot_file_metadata metadata;
    oa_u8 data[12u + OT_FILE_METADATA_PREFIX + OT_FILE_PATH_MAX];
    oa_u32 remaining = 0ul, expected_crc = 0ul, crc = 0ul, session = 0ul, object = 0ul, next_session, next_object;
    oa_u16 count = 0u, size;
    unsigned i, got, error;
    int rc;
    if (ops == 0 || ops->read == 0 || ops->recheck == 0 || ops->close == 0) return OT_CATALOG_ARGUMENT;
    memset(&temporary, 0, sizeof(temporary));
    rc = catalog == 0 || label == 0 ? OT_CATALOG_ARGUMENT : read_bytes(ops, data, OT_CATALOG_FILE_HEADER);
    if (rc == 0) {
        count = ot_get_u16(data + 12u); remaining = ot_get_u32(data + 16u); expected_crc = ot_get_u32(data + 20u);
        if (memcmp(data, "OTCAT1\r\n", 8u) || ot_get_u16(data + 8u) != 1u ||
            ot_get_u16(data + 10u) != OT_CATALOG_FILE_HEADER || count > OT_CATALOG_MAX_ENTRIES ||
            ot_get_u16(data + 14u) || ot_get_u32(data + 24u) ||
            ot_get_u32(data + 28u) != ot_crc32(data, 28u) || remaining < OT_VOLUME_LABEL_SIZE ||
            remaining > OT_CATALOG_FILE_MAX - OT_CATALOG_FILE_HEADER) rc = OT_CATALOG_FILE_FORMAT;
    }
    if (rc == 0) {
        rc = read_bytes(ops, data, OT_VOLUME_LABEL_SIZE);
        if (rc == 0) {
            remaining -= OT_VOLUME_LABEL_SIZE; crc = ot_crc32(data, OT_VOLUME_LABEL_SIZE);
            if (data[14] || data[15] || ot_volume_label_decode(&volume, data) != 0) rc = OT_CATALOG_FILE_FORMAT;
        }
    }
    for (i = 0u; i < count && rc == 0; ++i) {
        if (remaining < 12ul) { rc = OT_CATALOG_FILE_FORMAT; break; }
        rc = read_bytes(ops, data, 12u); if (rc != 0) break;
        remaining -= 12ul; crc = ot_crc32_extend(crc, data, 12u);
        size = ot_get_u16(data); next_session = ot_get_u32(data + 4u); next_object = ot_get_u32(data + 8u);
        if (ot_get_u16(data + 2u) || size <= OT_FILE_METADATA_PREFIX || size > OT_FILE_METADATA_PREFIX + OT_FILE_PATH_MAX ||
            (oa_u32)size > remaining || !identity(next_session, next_object, session, object)) { rc = OT_CATALOG_FILE_FORMAT; break; }
        rc = read_bytes(ops, data, size); if (rc != 0) break;
        remaining -= size; crc = ot_crc32_extend(crc, data, size);
        if (ot_file_metadata_decode(&metadata, data, size) != 0) { rc = OT_CATALOG_FILE_FORMAT; break; }
        rc = ot_catalog_capture(&temporary, next_session, next_object, &metadata);
        session = next_session; object = next_object;
    }
    if (rc == 0 && (remaining != 0ul || crc != expected_crc)) rc = OT_CATALOG_FILE_FORMAT;
    if (rc == 0) {
        got = 0u;
        if (ops->read(ops->context, data, 1u, &got)) rc = OT_CATALOG_FILE_IO;
        else if (got) rc = OT_CATALOG_FILE_FORMAT;
    }
    if (rc == 0 && ops->recheck(ops->context)) rc = OT_CATALOG_FILE_IO;
    error = ops->close(ops->context); if (rc == 0 && error) rc = OT_CATALOG_FILE_IO;
    if (rc == 0) { ot_catalog_free(catalog); *catalog = temporary; *label = volume; }
    else ot_catalog_free(&temporary);
    return rc;
}

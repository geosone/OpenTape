#include <string.h>
#include "opentape/record_payload.h"
#include "wire.h"

static char upper(char c)
{
    return c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c;
}

static int component_valid(const char *s, unsigned size)
{
    char base[9];
    unsigned i, n, ext;
    n = 0u;
    ext = 0u;
    memset(base, 0, sizeof(base));
    for (i = 0u; i < size; ++i) {
        char c = upper(s[i]);
        if (c == '.') {
            if (n == 0u || ext != 0u || i + 1u == size) return 0;
            ext = 1u;
        } else {
            if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                  c == '_' || c == '-' || c == '$' || c == '~')) return 0;
            if (ext != 0u) {
                if (++ext > 4u) return 0;
            } else {
                if (n == 8u) return 0;
                base[n++] = c;
            }
        }
    }
    if (n == 0u) return 0;
    if (strcmp(base, "CON") == 0 || strcmp(base, "PRN") == 0 ||
        strcmp(base, "AUX") == 0 || strcmp(base, "NUL") == 0 ||
        strcmp(base, "CLOCK$") == 0 || strcmp(base, "CONIN$") == 0 ||
        strcmp(base, "CONOUT$") == 0) return 0;
    if (n == 4u && base[3] >= '0' && base[3] <= '9' &&
        ((base[0] == 'C' && base[1] == 'O' && base[2] == 'M') ||
         (base[0] == 'L' && base[1] == 'P' && base[2] == 'T'))) return 0;
    return 1;
}

int ot_archive_path_valid(const char *path)
{
    unsigned i, start;
    if (path == 0) return 0;
    start = 0u;
    for (i = 0u; i <= OT_FILE_PATH_MAX; ++i) {
        if (path[i] == '/' || path[i] == 0) {
            if (i == start || !component_valid(path + start, i - start)) return 0;
            if (path[i] == 0) return 1;
            start = i + 1u;
        }
    }
    return 0;
}

static int metadata_valid(const struct ot_file_metadata *v)
{
    unsigned year, month, day, days;
    static const unsigned char month_days[12] = {31,28,31,30,31,30,31,31,30,31,30,31};
    if (!ot_archive_path_valid(v->path) ||
        (v->kind != OT_FILE_REGULAR && v->kind != OT_FILE_DIRECTORY) ||
        (v->dos_attributes & ~0x37u) != 0u ||
        ((v->dos_attributes & 0x10u) != 0u) != (v->kind == OT_FILE_DIRECTORY) ||
        (v->kind == OT_FILE_DIRECTORY && v->file_size != 0ul)) return 0;
    if (v->dos_date == 0u) return v->dos_time == 0u;
    if ((v->dos_time >> 11) > 23u || ((v->dos_time >> 5) & 63u) > 59u ||
        (v->dos_time & 31u) > 29u) return 0;
    year = 1980u + (v->dos_date >> 9);
    month = (v->dos_date >> 5) & 15u;
    day = v->dos_date & 31u;
    if (month < 1u || month > 12u || day < 1u) return 0;
    days = month_days[month - 1u];
    if (month == 2u && year % 4u == 0u && (year % 100u != 0u || year % 400u == 0u)) ++days;
    return day <= days;
}

static int prefix(const oa_u8 *data, oa_u16 size, oa_u16 expected)
{
    if (data == 0) return OT_FORMAT_BAD_ARGUMENT;
    if (size != expected) return OT_FORMAT_BAD_SIZE;
    return ot_get_u16(data) == OT_VOLUME_FORMAT_VERSION ? OT_FORMAT_OK : OT_FORMAT_BAD_VERSION;
}

int ot_session_start_encode(oa_u8 *out, oa_u16 capacity, const struct ot_session_start *v)
{
    unsigned i;
    if (out == 0 || v == 0) return OT_FORMAT_BAD_ARGUMENT;
    if (capacity < OT_SESSION_START_SIZE) return OT_FORMAT_BAD_SIZE;
    memset(out, 0, OT_SESSION_START_SIZE);
    ot_put_u16(out, OT_VOLUME_FORMAT_VERSION);
    ot_put_u32(out + 4, v->created_utc);
    memcpy(out + 8, v->uuid, 16u);
    for (i = 0u; i < 32u && v->name[i] != 0; ++i) out[24u + i] = (oa_u8)v->name[i];
    return OT_FORMAT_OK;
}

int ot_session_start_decode(struct ot_session_start *v, const oa_u8 *data, oa_u16 size)
{
    struct ot_session_start next;
    unsigned i, ended;
    int rc;
    if (v == 0) return OT_FORMAT_BAD_ARGUMENT;
    rc = prefix(data, size, OT_SESSION_START_SIZE);
    if (rc != OT_FORMAT_OK) return rc;
    if (ot_get_u16(data + 2) != 0u) return OT_PAYLOAD_BAD_DATA;
    ended = 0u;
    for (i = 24u; i < 56u; ++i) {
        if (ended && data[i] != 0u) return OT_PAYLOAD_BAD_DATA;
        if (data[i] == 0u) ended = 1u;
    }
    next.created_utc = ot_get_u32(data + 4);
    memcpy(next.uuid, data + 8, 16u);
    memcpy(next.name, data + 24, 32u);
    *v = next;
    return OT_FORMAT_OK;
}

int ot_file_metadata_encode(oa_u8 *out, oa_u16 capacity,
                            const struct ot_file_metadata *v, oa_u16 *written)
{
    oa_u16 length;
    if (written != 0) *written = 0u;
    if (out == 0 || v == 0 || written == 0) return OT_FORMAT_BAD_ARGUMENT;
    if (!metadata_valid(v)) return OT_PAYLOAD_BAD_DATA;
    length = (oa_u16)strlen(v->path);
    if ((oa_u32)length + OT_FILE_METADATA_PREFIX > capacity) return OT_FORMAT_BAD_SIZE;
    memset(out, 0, OT_FILE_METADATA_PREFIX);
    ot_put_u16(out, OT_VOLUME_FORMAT_VERSION);
    ot_put_u16(out + 2, v->kind);
    ot_put_u16(out + 4, v->dos_attributes);
    ot_put_u16(out + 6, length);
    ot_put_u32(out + 8, v->file_size);
    ot_put_u16(out + 12, v->dos_time);
    ot_put_u16(out + 14, v->dos_date);
    memcpy(out + OT_FILE_METADATA_PREFIX, v->path, length);
    *written = (oa_u16)(OT_FILE_METADATA_PREFIX + length);
    return OT_FORMAT_OK;
}

int ot_file_metadata_decode(struct ot_file_metadata *v, const oa_u8 *data, oa_u16 size)
{
    struct ot_file_metadata next;
    oa_u16 length;
    if (v == 0 || data == 0) return OT_FORMAT_BAD_ARGUMENT;
    if (size < OT_FILE_METADATA_PREFIX) return OT_FORMAT_BAD_SIZE;
    if (ot_get_u16(data) != OT_VOLUME_FORMAT_VERSION) return OT_FORMAT_BAD_VERSION;
    length = ot_get_u16(data + 6);
    if (length == 0u || length > OT_FILE_PATH_MAX ||
        (oa_u32)length + OT_FILE_METADATA_PREFIX != size) return OT_FORMAT_BAD_SIZE;
    if (ot_get_u32(data + 16) != 0ul || memchr(data + OT_FILE_METADATA_PREFIX, 0, length) != 0)
        return OT_PAYLOAD_BAD_DATA;
    memset(&next, 0, sizeof(next));
    next.kind = ot_get_u16(data + 2);
    next.dos_attributes = ot_get_u16(data + 4);
    next.file_size = ot_get_u32(data + 8);
    next.dos_time = ot_get_u16(data + 12);
    next.dos_date = ot_get_u16(data + 14);
    memcpy(next.path, data + OT_FILE_METADATA_PREFIX, length);
    if (!metadata_valid(&next)) return OT_PAYLOAD_BAD_DATA;
    *v = next;
    return OT_FORMAT_OK;
}

static int file_end_valid(const struct ot_file_end *v)
{
    return v->file_size == 0ul ? v->crc32 == 0ul && v->fragments == 0ul : v->fragments != 0ul;
}

int ot_file_end_encode(oa_u8 *out, oa_u16 capacity, const struct ot_file_end *v)
{
    if (out == 0 || v == 0) return OT_FORMAT_BAD_ARGUMENT;
    if (capacity < OT_FILE_END_SIZE) return OT_FORMAT_BAD_SIZE;
    if (!file_end_valid(v)) return OT_PAYLOAD_BAD_DATA;
    ot_put_u16(out, OT_VOLUME_FORMAT_VERSION);
    ot_put_u16(out + 2, 0u);
    ot_put_u32(out + 4, v->file_size);
    ot_put_u32(out + 8, v->crc32);
    ot_put_u32(out + 12, v->fragments);
    return OT_FORMAT_OK;
}
int ot_file_end_decode(struct ot_file_end *v, const oa_u8 *data, oa_u16 size)
{
    struct ot_file_end next;
    int rc;
    if (v == 0) return OT_FORMAT_BAD_ARGUMENT;
    rc = prefix(data, size, OT_FILE_END_SIZE);
    if (rc != OT_FORMAT_OK) return rc;
    if (ot_get_u16(data + 2) != 0u) return OT_PAYLOAD_BAD_DATA;
    next.file_size = ot_get_u32(data + 4);
    next.crc32 = ot_get_u32(data + 8);
    next.fragments = ot_get_u32(data + 12);
    if (!file_end_valid(&next)) return OT_PAYLOAD_BAD_DATA;
    *v = next;
    return OT_FORMAT_OK;
}
int ot_session_end_encode(oa_u8 *out, oa_u16 capacity, const struct ot_session_end *v)
{
    if (out == 0 || v == 0) return OT_FORMAT_BAD_ARGUMENT;
    if (capacity < OT_SESSION_END_SIZE) return OT_FORMAT_BAD_SIZE;
    if (v->result > OT_SESSION_INCOMPLETE) return OT_PAYLOAD_BAD_DATA;
    ot_put_u16(out, OT_VOLUME_FORMAT_VERSION);
    ot_put_u16(out + 2, v->result);
    ot_put_u32(out + 4, v->files);
    ot_put_u32(out + 8, v->directories);
    ot_put_u32(out + 12, v->bytes_low);
    ot_put_u32(out + 16, v->bytes_high);
    return OT_FORMAT_OK;
}
int ot_session_end_decode(struct ot_session_end *v, const oa_u8 *data, oa_u16 size)
{
    struct ot_session_end next;
    int rc;
    if (v == 0) return OT_FORMAT_BAD_ARGUMENT;
    rc = prefix(data, size, OT_SESSION_END_SIZE);
    if (rc != OT_FORMAT_OK) return rc;
    next.result = ot_get_u16(data + 2);
    if (next.result > OT_SESSION_INCOMPLETE) return OT_PAYLOAD_BAD_DATA;
    next.files = ot_get_u32(data + 4);
    next.directories = ot_get_u32(data + 8);
    next.bytes_low = ot_get_u32(data + 12);
    next.bytes_high = ot_get_u32(data + 16);
    *v = next;
    return OT_FORMAT_OK;
}

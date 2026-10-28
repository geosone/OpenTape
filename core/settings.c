/* Portable profile format, independent of C packing/pointers and DOS text EOF.
 * Header: OTCF, u16 version=1 (FILE/TREE) or 2 (SET), u16 size, u32 payload CRC, u32 header CRC.
 * Payload: configured/tree bytes, then source[41], archive path[41], volume[32]
 * and preview/restore/compare/import/export paths[41] in that fixed order.
 * CRC catches accidental damage; it is not an authentication mechanism. */
#include <string.h>
#include "opentape/settings.h"
#include "wire.h"
#include "opentape/local_name.h"
/* Changing an in-memory field bound requires a deliberate format revision. */
typedef char ot_settings_layout_check[(18u + sizeof(((struct ot_backup_form *)0)->source) +
    sizeof(((struct ot_backup_form *)0)->path) + sizeof(((struct ot_backup_form *)0)->volume) +
    sizeof(((struct ot_paths_form *)0)->names) == OT_SETTINGS_WIRE_SIZE) ? 1 : -1];

int ot_settings_filename_valid(const char *name)
{
    return ot_local_file_name_valid(name, OT_PATHS_FORM_MAX);
}

static int text_valid(const char *text, unsigned capacity)
{
    unsigned i;
    for (i = 0u; i < capacity; ++i) {
        unsigned char c = (unsigned char)text[i];
        if (c == 0u) return 1;
        if (c < 32u || c == 127u) return 0;
    }
    return 0;
}
int ot_settings_valid(const struct ot_settings *settings)
{
    struct ot_dos_backup_request request;
    struct ot_paths_form paths;
    if (settings == 0 || settings->backup.configured > 1u ||
        !text_valid(settings->backup.source, sizeof(settings->backup.source)) ||
        !text_valid(settings->backup.path, sizeof(settings->backup.path)) ||
        !text_valid(settings->backup.volume, sizeof(settings->backup.volume)) ||
        !ot_paths_form_commit(&paths, &settings->paths)) return 0;
    if (settings->backup.configured) return ot_backup_form_request(&settings->backup, &request);
    if (*settings->backup.source != 0) return 0;
    request.source = "UNSET"; request.path = settings->backup.path;
    request.volume = settings->backup.volume; request.tree = settings->backup.tree;
    return ot_dos_backup_request_valid(&request);
}
static void put_text(oa_u8 *data, unsigned *offset, const char *text, unsigned capacity)
{
    memcpy(data + *offset, text, strlen(text));
    *offset += capacity;
}
int ot_settings_encode(const struct ot_settings *settings, oa_u8 *data, unsigned capacity)
{
    unsigned i, offset = 18u;
    if (data == 0 || capacity < OT_SETTINGS_WIRE_SIZE || !ot_settings_valid(settings)) return OT_SETTINGS_ARGUMENT;
    memset(data, 0, OT_SETTINGS_WIRE_SIZE);
    memcpy(data, "OTCF", 4u); ot_put_u16(data + 4u, settings->backup.tree == OT_BACKUP_SET ? 2u : 1u); ot_put_u16(data + 6u, OT_SETTINGS_WIRE_SIZE);
    data[16] = (oa_u8)settings->backup.configured; data[17] = (oa_u8)settings->backup.tree;
    put_text(data, &offset, settings->backup.source, sizeof(settings->backup.source));
    put_text(data, &offset, settings->backup.path, sizeof(settings->backup.path));
    put_text(data, &offset, settings->backup.volume, sizeof(settings->backup.volume));
    for (i = 0u; i < OT_PATHS_FORM_COUNT; ++i)
        put_text(data, &offset, settings->paths.names[i], sizeof(settings->paths.names[i]));
    ot_put_u32(data + 8u, ot_crc32(data + 16u, OT_SETTINGS_WIRE_SIZE - 16u));
    ot_put_u32(data + 12u, ot_crc32(data, 12u));
    return 0;
}
static int get_text(char *text, const oa_u8 *data, unsigned *offset, unsigned capacity)
{
    unsigned i;
    int terminated = 0;
    for (i = 0u; i < capacity; ++i) {
        oa_u8 c = data[*offset + i];
        if (terminated && c != 0u) return 0;
        if (c == 0u) terminated = 1;
        text[i] = (char)c;
    }
    *offset += capacity;
    return terminated;
}
int ot_settings_decode(struct ot_settings *settings, const oa_u8 *data, unsigned size)
{
    struct ot_settings value;
    unsigned i, offset = 18u;
    if (settings == 0 || data == 0) return OT_SETTINGS_ARGUMENT;
    if (size != OT_SETTINGS_WIRE_SIZE || memcmp(data, "OTCF", 4u) != 0 ||
        (ot_get_u16(data + 4u) != 1u && ot_get_u16(data + 4u) != 2u) || ot_get_u16(data + 6u) != OT_SETTINGS_WIRE_SIZE ||
        ot_get_u32(data + 12u) != ot_crc32(data, 12u) ||
        ot_get_u32(data + 8u) != ot_crc32(data + 16u, OT_SETTINGS_WIRE_SIZE - 16u)) return OT_SETTINGS_FORMAT;
    memset(&value, 0, sizeof(value));
    value.backup.configured = data[16]; value.backup.tree = data[17];
    if (!get_text(value.backup.source, data, &offset, sizeof(value.backup.source)) ||
        !get_text(value.backup.path, data, &offset, sizeof(value.backup.path)) ||
        !get_text(value.backup.volume, data, &offset, sizeof(value.backup.volume))) return OT_SETTINGS_FORMAT;
    for (i = 0u; i < OT_PATHS_FORM_COUNT; ++i)
        if (!get_text(value.paths.names[i], data, &offset, sizeof(value.paths.names[i]))) return OT_SETTINGS_FORMAT;
    if ((ot_get_u16(data + 4u) == 1u && value.backup.tree > OT_BACKUP_TREE) ||
        (ot_get_u16(data + 4u) == 2u && value.backup.tree != OT_BACKUP_SET) ||
        !ot_settings_valid(&value)) return OT_SETTINGS_FORMAT;
    *settings = value;
    return 0;
}

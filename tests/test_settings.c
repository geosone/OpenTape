/* Profile encoding is a strict portable transaction, not a C struct dump.
 * Failed decode must not partially replace either settings group. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/settings.h"
#include "opentape/local_name.h"
static void checksums(oa_u8 *data)
{
    oa_u32 crc;
    unsigned i;
    crc = ot_crc32(data + 16u, OT_SETTINGS_WIRE_SIZE - 16u);
    for (i = 0u; i < 4u; ++i) { data[8u + i] = (oa_u8)crc; crc >>= 8; }
    crc = ot_crc32(data, 12u);
    for (i = 0u; i < 4u; ++i) { data[12u + i] = (oa_u8)crc; crc >>= 8; }
}
static void decode_failure(struct ot_settings *value, const oa_u8 *data, unsigned size)
{
    oa_u8 before[sizeof(*value)];
    memcpy(before, value, sizeof(before));
    assert(ot_settings_decode(value, data, size) != 0);
    assert(memcmp(before, value, sizeof(before)) == 0);
}
static void encode_failure(const struct ot_settings *value)
{
    oa_u8 data[OT_SETTINGS_WIRE_SIZE];
    unsigned i;
    memset(data, 0xA5, sizeof(data));
    assert(ot_settings_encode(value, data, sizeof(data)) == OT_SETTINGS_ARGUMENT);
    for (i = 0u; i < sizeof(data); ++i) assert(data[i] == 0xA5u);
}
int main(void)
{
    struct ot_settings settings, decoded, invalid;
    struct ot_dos_backup_request request;
    oa_u8 data[OT_SETTINGS_WIRE_SIZE], changed[OT_SETTINGS_WIRE_SIZE];
    unsigned i;
    memset(&settings, 0, sizeof(settings)); ot_backup_form_init(&settings.backup, 0);
    assert(ot_settings_valid(&settings));
    assert(ot_settings_encode(&settings, data, sizeof(data)) == 0);
    memset(&decoded, 0xA5, sizeof(decoded));
    assert(ot_settings_decode(&decoded, data, sizeof(data)) == 0 && !decoded.backup.configured);
    assert(!decoded.backup.source[0] && !decoded.paths.names[0][0]);
    request.source = "INPUT.BIN"; request.path = "DATA/INPUT.BIN"; request.volume = "TEST"; request.tree = 0u;
    ot_backup_form_init(&settings.backup, &request);
    assert(ot_paths_form_set(&settings.paths, OT_PATH_PREVIEW, "PREVIEW"));
    assert(ot_paths_form_set(&settings.paths, OT_PATH_RESTORE, "RECOVER"));
    assert(ot_paths_form_set(&settings.paths, OT_PATH_COMPARE, "CHECK"));
    assert(ot_paths_form_set(&settings.paths, OT_PATH_IMPORT, "INPUT.LST"));
    assert(ot_paths_form_set(&settings.paths, OT_PATH_EXPORT, "UI.LST"));
    assert(ot_settings_encode(&settings, data, sizeof(data)) == 0);
    assert(memcmp(data, "OTCF\1\0\121\1", 8u) == 0);
    assert(data[16] == 1u && data[17] == 0u && strcmp((const char *)data + 18u, "INPUT.BIN") == 0);
    assert(strcmp((const char *)data + 59u, "DATA/INPUT.BIN") == 0);
    assert(strcmp((const char *)data + 100u, "TEST") == 0);
    assert(strcmp((const char *)data + 132u, "PREVIEW") == 0);
    assert(ot_settings_decode(&decoded, data, sizeof(data)) == 0);
    assert(ot_settings_encode(&decoded, changed, sizeof(changed)) == 0);
    assert(memcmp(data, changed, sizeof(data)) == 0);
    for (i = 0u; i < sizeof(data); ++i) {
        memcpy(changed, data, sizeof(data)); changed[i] ^= 1u;
        decode_failure(&decoded, changed, sizeof(changed));
    }
    decode_failure(&decoded, data, sizeof(data) - 1u);
    decode_failure(&decoded, data, sizeof(data) + 1u);
    memcpy(changed, data, sizeof(data)); changed[4] = 2u; checksums(changed); decode_failure(&decoded, changed, sizeof(changed));
    memcpy(changed, data, sizeof(data)); changed[6] = 1u; checksums(changed); decode_failure(&decoded, changed, sizeof(changed));
    memcpy(changed, data, sizeof(data)); changed[16] = 2u; checksums(changed); decode_failure(&decoded, changed, sizeof(changed));
    memcpy(changed, data, sizeof(data)); changed[17] = 2u; checksums(changed); decode_failure(&decoded, changed, sizeof(changed));
    memcpy(changed, data, sizeof(data)); changed[28] = 'X'; checksums(changed); decode_failure(&decoded, changed, sizeof(changed));
    memcpy(changed, data, sizeof(data)); changed[18] = 26u; checksums(changed); decode_failure(&decoded, changed, sizeof(changed));
    memcpy(changed, data, sizeof(data)); changed[100] = '!'; checksums(changed); decode_failure(&decoded, changed, sizeof(changed));
    memcpy(changed, data, sizeof(data)); changed[132] = '*'; checksums(changed); decode_failure(&decoded, changed, sizeof(changed));
    invalid = settings; invalid.backup.configured = 2u; encode_failure(&invalid);
    invalid = settings; invalid.backup.configured = 0u; encode_failure(&invalid);
    invalid = settings; invalid.backup.tree = 3u; encode_failure(&invalid);
    invalid = settings; invalid.backup.source[0] = 0; encode_failure(&invalid);
    invalid = settings; strcpy(invalid.backup.path, "../BAD"); encode_failure(&invalid);
    invalid = settings; strcpy(invalid.backup.volume, "BAD!"); encode_failure(&invalid);
    invalid = settings; memset(invalid.backup.source, 'A', sizeof(invalid.backup.source)); encode_failure(&invalid);
    invalid = settings; memset(invalid.paths.names[4], 'A', sizeof(invalid.paths.names[4])); encode_failure(&invalid);
    invalid = settings; invalid.paths.names[0][0] = '\n'; encode_failure(&invalid);
    memset(changed, 0xA5, sizeof(changed));
    assert(ot_settings_encode(&settings, changed, sizeof(changed) - 1u) == OT_SETTINGS_ARGUMENT);
    for (i = 0u; i < sizeof(changed); ++i) assert(changed[i] == 0xA5u);
    assert(ot_settings_encode(&settings, 0, 999u) == OT_SETTINGS_ARGUMENT);
    assert(ot_settings_encode(0, changed, sizeof(changed)) == OT_SETTINGS_ARGUMENT);
    assert(ot_settings_decode(0, data, sizeof(data)) == OT_SETTINGS_ARGUMENT);
    decode_failure(&decoded, 0, sizeof(data));
    memset(settings.backup.source, 'S', 40u); settings.backup.source[40] = 0;
    strcpy(settings.backup.path, "ABCDEFGH/ABCDEFGH/ABCDEFGH/ABCDEFGH/ABCD");
    memset(settings.backup.volume, 'V', 31u); settings.backup.volume[31] = 0;
    for (i = 0u; i < OT_PATHS_FORM_COUNT; ++i) {
        memset(settings.paths.names[i], 'P', 40u); settings.paths.names[i][40] = 0;
    }
    assert(ot_settings_encode(&settings, data, sizeof(data)) == 0);
    assert(ot_settings_decode(&decoded, data, sizeof(data)) == 0);
    assert(ot_settings_encode(&decoded, changed, sizeof(changed)) == 0);
    assert(memcmp(data, changed, sizeof(data)) == 0);
    settings.backup.tree = OT_BACKUP_SET;
    assert(ot_settings_encode(&settings, data, sizeof(data)) == 0 && data[4] == 2u && data[17] == OT_BACKUP_SET);
    assert(ot_settings_decode(&decoded, data, sizeof(data)) == 0 && decoded.backup.tree == OT_BACKUP_SET);
    memcpy(changed, data, sizeof(data)); changed[4] = 1u; checksums(changed); decode_failure(&decoded, changed, sizeof(changed));
    memcpy(changed, data, sizeof(data)); changed[17] = 0u; checksums(changed); decode_failure(&decoded, changed, sizeof(changed));
    assert(ot_settings_filename_valid("OPENTAPE.CFG"));
    assert(ot_settings_filename_valid("C:\\PROFILES\\TAPE1.CFG"));
    assert(ot_settings_filename_valid("C:TAPE1.CFG"));
    assert(!ot_settings_filename_valid(0) && !ot_settings_filename_valid(""));
    assert(!ot_settings_filename_valid("CON.CFG") && !ot_settings_filename_valid("../"));
    assert(!ot_settings_filename_valid("*.CFG") && !ot_settings_filename_valid("TOOLONGNAME.CFG"));
    assert(!ot_settings_filename_valid("MT0") && !ot_settings_filename_valid("NT0"));
    assert(!ot_settings_filename_valid("NMT0.CFG") && !ot_settings_filename_valid("RMT0.CFG"));
    assert(!ot_settings_filename_valid("C:\\NRMT0$.CFG") && !ot_settings_filename_valid("NRXT9.CFG"));
    assert(ot_local_tape_name("RMT0:") && ot_local_tape_name("RMT0 : .CFG"));
    assert(ot_local_tape_name("C:\\D\\ NRMT0$ .CFG") && ot_local_tape_name("C:mt0.bin"));
    assert(!ot_local_tape_name("C:\\") && !ot_local_tape_name("."));
    assert(!ot_local_tape_name("DATA/RMT0ABC.BIN") && !ot_local_tape_name(0));
    puts("Settings profile codec tests passed");
    return 0;
}

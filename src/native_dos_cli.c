#include <stdio.h>
#include <string.h>
#include "opentape/native_tape.h"
#include "opentape/tape_probe.h"
#include "opentape/messages.h"

/* Decimal parsing is bounded before multiplication; even LONG_MIN-like input
 * is rejected without signed overflow or opening a device. */
static int space_count(const char *s, oa_s32 *out)
{
    oa_u32 value = 0UL, maximum;
    int negative = *s == '-';
    if (negative) ++s;
    if (*s == 0) return -1;
    maximum = negative ? 8388608UL : 8388607UL;
    while (*s) {
        unsigned digit;
        if (*s < '0' || *s > '9') return -1;
        digit = (unsigned)(*s++ - '0');
        if (value > (maximum - digit) / 10UL) return -1;
        value = value * 10UL + digit;
    }
    *out = negative ? -(oa_s32)value : (oa_s32)value;
    return 0;
}

int ot_dos_native_command(int argc, char **argv)
{
    const char *name = "NRMT0";
    struct ot_tape_info_v1 info;
    struct ot_tape_control_v1 control;
    unsigned error = 0u, refresh = 0u, named = 0u;
    int i, rc, is_query, feature, movement = 0;
    oa_s32 count = 0;
    oa_u16 operation;
    is_query = strcmp(argv[1], "--native-status") == 0;
    operation = strcmp(argv[1], "--native-rewind") == 0 ? OT_TAPE_CONTROL_REWIND :
        strcmp(argv[1], "--native-load") == 0 ? OT_TAPE_CONTROL_LOAD :
        strcmp(argv[1], "--native-unload") == 0 ? OT_TAPE_CONTROL_UNLOAD : OT_TAPE_CONTROL_READY;
    i = 2;
    if (strcmp(argv[1], "--native-space-records") == 0 || strcmp(argv[1], "--native-space-filemarks") == 0) {
        movement = 1;
        operation = strcmp(argv[1], "--native-space-records") == 0 ?
            OT_TAPE_CONTROL_SPACE_RECORDS : OT_TAPE_CONTROL_SPACE_FILEMARKS;
        if (argc < 3 || space_count(argv[2], &count) != 0) { puts(OT_MSG_NATIVE_SPACE_USAGE); return 1; }
        i = 3;
    } else if (strcmp(argv[1], "--native-eod") == 0) {
        movement = 1; operation = OT_TAPE_CONTROL_SPACE_EOD;
    }
    for (; i < argc; ++i) {
        if (is_query && !refresh && strcmp(argv[i], "--refresh") == 0) refresh = 1u;
        else if (!named && strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            name = argv[++i]; named = 1u;
        } else { puts(OT_MSG_NATIVE_USAGE); return 1; }
    }
    if (!ot_probe_raw_name_valid(name)) { puts(OT_MSG_NATIVE_USAGE); return 1; }
    if (is_query) {
        rc = ot_dos_native_probe(name, (oa_u16)refresh, &info, &error);
        printf(OT_MSG_NATIVE_RESULT, name, rc, error);
        if (ot_tape_info_validate(&info, sizeof(info)) == 0) {
            printf(OT_MSG_NATIVE_QUERY, info.result, (unsigned long)info.valid_fields, info.session_state);
            if ((info.valid_fields & OT_TAPE_VALID_IDENTITY) != 0UL)
                printf(OT_MSG_NATIVE_ID, (const char *)info.vendor, (const char *)info.product,
                    (const char *)info.revision);
            if ((info.valid_fields & OT_TAPE_VALID_LIMITS) != 0UL)
                printf(OT_MSG_NATIVE_LIMITS, (unsigned long)info.minimum_block,
                    (unsigned long)info.maximum_block, (unsigned long)info.block_granularity);
            if ((info.probe_flags & OT_TAPE_PROBE_UNVERIFIED_LENGTH) != 0UL)
                puts(OT_MSG_NATIVE_LENGTH);
            feature = ot_tape_info_feature(&info, OT_TAPE_FEATURE_COMPRESSION);
            printf(OT_MSG_NATIVE_COMPRESSION, feature < 0 ? OT_MSG_NATIVE_UNKNOWN :
                feature ? OT_MSG_NATIVE_SUPPORTED : OT_MSG_NATIVE_UNSUPPORTED);
            if (info.diagnostic_class != 0u)
                printf(OT_MSG_NATIVE_DIAG, info.diagnostic_class, info.host_status, info.target_status,
                    info.sense_key, info.asc, info.ascq);
            if ((info.session_state & OT_TAPE_SESSION_NATIVE_FAILED) != 0u)
                puts(OT_MSG_NATIVE_UNFINISHED);
            if ((info.session_state & OT_TAPE_SESSION_WRITE_ERROR) != 0u)
                puts(OT_MSG_NATIVE_SAVED);
        }
    } else {
        if (movement) printf(OT_MSG_NATIVE_SPACE, name, operation, (long)count);
        else if (operation == OT_TAPE_CONTROL_LOAD) printf(OT_MSG_NATIVE_LOAD, name);
        else if (operation == OT_TAPE_CONTROL_UNLOAD) printf(OT_MSG_NATIVE_UNLOAD, name);
        else printf(operation == OT_TAPE_CONTROL_REWIND ? OT_MSG_NATIVE_REWIND : OT_MSG_NATIVE_READY, name);
        if (fflush(stdout) != 0 || ferror(stdout)) return 2;
        rc = movement ? ot_dos_native_space(name, operation, count, &control, &error) :
            ot_dos_native_execute(name, operation, &control, &error);
        printf(OT_MSG_NATIVE_RESULT, name, rc, error);
        if (ot_tape_control_validate(&control, sizeof(control)) == 0) {
            printf(OT_MSG_NATIVE_CONTROL, control.result, control.completion_flags);
            if ((control.completion_flags & OT_TAPE_POSITION_BOT) != 0u) puts(OT_MSG_NATIVE_BOT);
            if ((control.completion_flags & OT_TAPE_POSITION_UNKNOWN) != 0u) puts(OT_MSG_NATIVE_POSITION_UNKNOWN);
            if ((control.session_state & OT_TAPE_SESSION_NATIVE_FAILED) != 0u)
                puts(OT_MSG_NATIVE_UNFINISHED);
            if (control.diagnostic_class != 0u)
                printf(OT_MSG_NATIVE_DIAG, control.diagnostic_class, control.host_status,
                    control.target_status, control.sense_key, control.asc, control.ascq);
        }
    }
    if (fflush(stdout) != 0 || ferror(stdout)) return 2;
    if (is_query && (info.session_state & (OT_TAPE_SESSION_WRITE_ERROR | OT_TAPE_SESSION_NATIVE_FAILED)) != 0u) return 2;
    return rc == OT_NATIVE_OK ? 0 : 2;
}

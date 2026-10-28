#include <stdio.h>
#include <string.h>
#include "opentape/native_tape.h"
#include "opentape/tape_probe.h"
#include "opentape/messages.h"

/* Decimal-only unsigned parsing checks before multiplication. This accepts the
 * full LOCATE(10) address range without LONG overflow or opening a device. */
static int unsigned_number(const char *s,oa_u32 maximum,oa_u32 *out)
{
    oa_u32 value=0;
    if(*s==0) return -1;
    while(*s) {
        unsigned digit;
        if(*s<'0' || *s>'9') return -1;
        digit=(unsigned)(*s++-'0');
        if(value>(maximum-digit)/10UL) return -1;
        value=value*10UL+digit;
    }
    *out=value; return 0;
}

int ot_dos_native_command(int argc, char **argv)
{
    const char *name = "NRMT0";
    struct ot_tape_info_v1 info;
    struct ot_tape_control_v1 control;
#ifdef OT_NATIVE_TAPE_POSITION
    struct ot_tape_position_v1 position;
#endif
#ifdef OT_NATIVE_TAPE_LOCATE
    struct ot_tape_locate_v1 locate;
#endif
#ifdef OT_NATIVE_TAPE_WIDE
    struct ot_tape_position64_v1 position64;
    struct ot_tape_locate64_v1 locate64;
    char block_text[21],file_text[21];
#endif
    oa_u32 block=0,partition=0;
#ifdef OT_NATIVE_TAPE_WIDE
    oa_u32 block_high=0;
#endif
    unsigned locate_flags=0;
    unsigned error = 0u, refresh = 0u, named = 0u, allow_clear=0u;
    int i, rc, is_alerts, extended_space, is_partitions, is_query, is_position, is_locate, wide_position, wide_locate, feature, movement = 0;
    oa_s32 count = 0;
    oa_u16 operation;
#ifdef OT_NATIVE_TAPE_PARTITIONS
    struct ot_tape_partitions_v1 layout;
#endif
#ifdef OT_NATIVE_TAPE_ALERTS
    struct ot_tape_alerts_v1 alerts;
    unsigned number,reported,active;
#endif
    is_alerts=strcmp(argv[1],"--native-alerts")==0;
#ifndef OT_NATIVE_TAPE_ALERTS
    if(is_alerts) { puts(OT_MSG_NATIVE_ALERTS_MISSING); return 2; }
#endif
    extended_space=strcmp(argv[1],"--native-space-seq-filemarks")==0 ||
        strcmp(argv[1],"--native-space-setmarks")==0 || strcmp(argv[1],"--native-space-seq-setmarks")==0;
#ifndef OT_NATIVE_TAPE_EXTENDED_SPACE
    if(extended_space) { puts(OT_MSG_NATIVE_EXT_SPACE_MISSING); return 2; }
#endif
    is_partitions=strcmp(argv[1],"--native-partitions")==0;
#ifndef OT_NATIVE_TAPE_PARTITIONS
    if(is_partitions) { puts(OT_MSG_NATIVE_LAYOUT_MISSING); return 2; }
#endif
    is_query = strcmp(argv[1], "--native-status") == 0;
    wide_position=strcmp(argv[1],"--native-position64")==0;
    wide_locate=strcmp(argv[1],"--native-locate64")==0;
#ifndef OT_NATIVE_TAPE_WIDE
    if(wide_position || wide_locate) { puts(OT_MSG_NATIVE_WIDE_MISSING); return 2; }
#endif
    is_position = strcmp(argv[1], "--native-position") == 0 || wide_position;
    is_locate = strcmp(argv[1], "--native-locate") == 0 || wide_locate;
#ifndef OT_NATIVE_TAPE_LOCATE
    if(is_locate) { puts(OT_MSG_NATIVE_LOCATE_MISSING); return 2; }
#endif
#ifndef OT_NATIVE_TAPE_POSITION
    if(is_position) { puts(OT_MSG_NATIVE_POSITION_MISSING); return 2; }
#endif
    operation = strcmp(argv[1], "--native-rewind") == 0 ? OT_TAPE_CONTROL_REWIND :
        strcmp(argv[1], "--native-load") == 0 ? OT_TAPE_CONTROL_LOAD :
        strcmp(argv[1], "--native-unload") == 0 ? OT_TAPE_CONTROL_UNLOAD : OT_TAPE_CONTROL_READY;
#ifdef OT_NATIVE_TAPE_REMOVAL
    if (strcmp(argv[1], "--native-lock") == 0) operation = OT_TAPE_CONTROL_LOCK;
    else if (strcmp(argv[1], "--native-unlock") == 0) operation = OT_TAPE_CONTROL_UNLOCK;
#else
    if (strcmp(argv[1], "--native-lock") == 0 || strcmp(argv[1], "--native-unlock") == 0) {
        puts(OT_MSG_NATIVE_REMOVAL_MISSING); return 2;
    }
#endif
    i = 2;
    if (strcmp(argv[1], "--native-space-records") == 0 || strcmp(argv[1], "--native-space-filemarks") == 0 || extended_space) {
        movement = 1;
        operation = strcmp(argv[1], "--native-space-records") == 0 ?
            OT_TAPE_CONTROL_SPACE_RECORDS : OT_TAPE_CONTROL_SPACE_FILEMARKS;
#ifdef OT_NATIVE_TAPE_EXTENDED_SPACE
        if(strcmp(argv[1],"--native-space-seq-filemarks")==0) operation=OT_TAPE_CONTROL_SPACE_SEQ_FILEMARKS;
        else if(strcmp(argv[1],"--native-space-setmarks")==0) operation=OT_TAPE_CONTROL_SPACE_SETMARKS;
        else if(strcmp(argv[1],"--native-space-seq-setmarks")==0) operation=OT_TAPE_CONTROL_SPACE_SEQ_SETMARKS;
#endif
        if (argc < 3 || ot_native_space_count_parse(argv[2], &count) != 0) { puts(OT_MSG_NATIVE_SPACE_USAGE); return 1; }
        i = 3;
    } else if (strcmp(argv[1], "--native-eod") == 0) {
        movement = 1; operation = OT_TAPE_CONTROL_SPACE_EOD;
    }
    if(is_locate) {
        if(argc<3) { puts(OT_MSG_NATIVE_LOCATE_USAGE); return 1; }
#ifdef OT_NATIVE_TAPE_WIDE
        if(wide_locate) {
            if(ot_tape_u64_parse(argv[2],&block,&block_high)!=0) { puts(OT_MSG_NATIVE_WIDE_USAGE); return 1; }
        } else
#endif
        if(unsigned_number(argv[2],0xffffffffUL,&block)!=0) { puts(OT_MSG_NATIVE_LOCATE_USAGE); return 1; }
        i=3;
    }
    for (; i < argc; ++i) {
        if(is_alerts && !allow_clear && strcmp(argv[i],"--allow-clear")==0) allow_clear=1u;
        else if (is_query && !refresh && strcmp(argv[i], "--refresh") == 0) refresh = 1u;
        else if(is_locate && !locate_flags && strcmp(argv[i],"--partition")==0 && i+1<argc) {
            if(unsigned_number(argv[++i],255UL,&partition)!=0) { puts(OT_MSG_NATIVE_LOCATE_USAGE); return 1; }
            locate_flags=1;
        }
        else if (!named && strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            name = argv[++i]; named = 1u;
        } else { puts(OT_MSG_NATIVE_USAGE); return 1; }
    }
    if(is_alerts && !allow_clear) { puts(OT_MSG_NATIVE_ALERTS_USAGE); return 1; }
    if (!ot_probe_raw_name_valid(name)) { puts(OT_MSG_NATIVE_USAGE); return 1; }
    if(is_alerts) {
#ifdef OT_NATIVE_TAPE_ALERTS
        puts(OT_MSG_NATIVE_ALERTS_ACK);
        if(fflush(stdout)!=0 || ferror(stdout)) return 2;
        rc=ot_dos_native_alerts(name,OT_TAPE_ALERTS_ACKNOWLEDGE,&alerts,&error);
        printf(OT_MSG_NATIVE_RESULT,name,rc,error);
        if(ot_tape_alerts_validate(&alerts,sizeof(alerts))==0) {
            if(alerts.valid_fields & OT_TAPE_ALERTS_VALID) {
                reported=active=0;
                for(number=1;number<=64;++number) {
                    int state=ot_tape_alert_state(&alerts,(oa_u16)number);
                    if(state>=0) ++reported;
                    if(state==1) ++active;
                }
                printf(OT_MSG_NATIVE_ALERTS_COUNTS,reported,active);
                if(!reported) puts(OT_MSG_NATIVE_ALERTS_UNKNOWN);
                else if(!active) puts(OT_MSG_NATIVE_ALERTS_NONE);
                for(number=1;number<=64;++number)
                    if(ot_tape_alert_state(&alerts,(oa_u16)number)==1) printf(OT_MSG_NATIVE_ALERTS_FLAG,number);
            }
            if(alerts.probe_flags & OT_TAPE_PROBE_UNVERIFIED_LENGTH) puts(OT_MSG_NATIVE_LENGTH);
            if(alerts.session_state & OT_TAPE_SESSION_NATIVE_FAILED) puts(OT_MSG_NATIVE_UNFINISHED);
            if(alerts.diagnostic_class) printf(OT_MSG_NATIVE_DIAG,alerts.diagnostic_class,alerts.host_status,
                alerts.target_status,alerts.sense_key,alerts.asc,alerts.ascq);
        }
#endif
    } else if(is_partitions) {
#ifdef OT_NATIVE_TAPE_PARTITIONS
        rc=ot_dos_native_partitions(name,&layout,&error);
        printf(OT_MSG_NATIVE_RESULT,name,rc,error);
        if(ot_tape_partitions_validate(&layout,sizeof(layout))==0) {
            if(layout.valid_fields & OT_TAPE_LAYOUT_VALID) {
                printf(OT_MSG_NATIVE_LAYOUT_COUNTS,(unsigned long)layout.maximum_additional,(unsigned long)layout.defined_additional);
                printf(OT_MSG_NATIVE_LAYOUT_RAW,(unsigned long)layout.partition_flags,(unsigned long)layout.format_recognition,
                    (unsigned long)layout.partition_units,(unsigned long)layout.size_descriptors);
                puts(OT_MSG_NATIVE_LAYOUT_PENDING);
            }
            if(layout.probe_flags & OT_TAPE_PROBE_UNVERIFIED_LENGTH) puts(OT_MSG_NATIVE_LENGTH);
            if(layout.session_state & OT_TAPE_SESSION_NATIVE_FAILED) puts(OT_MSG_NATIVE_UNFINISHED);
            if(layout.diagnostic_class) printf(OT_MSG_NATIVE_DIAG,layout.diagnostic_class,layout.host_status,
                layout.target_status,layout.sense_key,layout.asc,layout.ascq);
        }
#endif
    } else if (is_query) {
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
    } else if(wide_locate) {
#ifdef OT_NATIVE_TAPE_WIDE
        if(ot_tape_u64_format(block,block_high,block_text,sizeof(block_text))!=0) return 2;
        if(locate_flags) printf(OT_MSG_NATIVE_LOCATE64_PART,name,block_text,(unsigned long)partition);
        else printf(OT_MSG_NATIVE_LOCATE64,name,block_text);
        if(fflush(stdout)!=0 || ferror(stdout)) return 2;
        rc=ot_dos_native_locate64(name,(oa_u16)locate_flags,partition,block,block_high,&locate64,&error);
        printf(OT_MSG_NATIVE_RESULT,name,rc,error);
        if(ot_tape_locate64_validate(&locate64,sizeof(locate64))==0) {
            printf(OT_MSG_NATIVE_CONTROL,locate64.result,locate64.completion_flags);
            if(locate64.completion_flags & OT_TAPE_POSITION_UNKNOWN) puts(OT_MSG_NATIVE_POSITION_UNKNOWN);
            if(locate64.session_state & OT_TAPE_SESSION_NATIVE_FAILED) puts(OT_MSG_NATIVE_UNFINISHED);
            if(locate64.diagnostic_class) printf(OT_MSG_NATIVE_DIAG,locate64.diagnostic_class,locate64.host_status,
                locate64.target_status,locate64.sense_key,locate64.asc,locate64.ascq);
        }
#endif
    } else if(is_locate) {
#ifdef OT_NATIVE_TAPE_LOCATE
        if(locate_flags) printf(OT_MSG_NATIVE_LOCATE_PART,name,(unsigned long)block,(unsigned long)partition);
        else printf(OT_MSG_NATIVE_LOCATE_CURRENT,name,(unsigned long)block);
        if(fflush(stdout)!=0 || ferror(stdout)) return 2;
        rc=ot_dos_native_locate(name,(oa_u16)locate_flags,partition,block,&locate,&error);
        printf(OT_MSG_NATIVE_RESULT,name,rc,error);
        if(ot_tape_locate_validate(&locate,sizeof(locate))==0) {
            printf(OT_MSG_NATIVE_CONTROL,locate.result,locate.completion_flags);
            if(locate.completion_flags & OT_TAPE_POSITION_UNKNOWN) puts(OT_MSG_NATIVE_POSITION_UNKNOWN);
            if(locate.session_state & OT_TAPE_SESSION_NATIVE_FAILED) puts(OT_MSG_NATIVE_UNFINISHED);
            if(locate.diagnostic_class) printf(OT_MSG_NATIVE_DIAG,locate.diagnostic_class,locate.host_status,
                locate.target_status,locate.sense_key,locate.asc,locate.ascq);
        }
#endif
    } else if(wide_position) {
#ifdef OT_NATIVE_TAPE_WIDE
        printf(OT_MSG_NATIVE_POSITION_START,name);
        if(fflush(stdout)!=0 || ferror(stdout)) return 2;
        rc=ot_dos_native_position64(name,&position64,&error);
        printf(OT_MSG_NATIVE_RESULT,name,rc,error);
        if(ot_tape_position64_validate(&position64,sizeof(position64))==0) {
            if(position64.valid_fields & 1UL) {
                if(ot_tape_u64_format(position64.block,position64.block_high,block_text,sizeof(block_text))!=0) return 2;
                printf(OT_MSG_NATIVE_POSITION64,(unsigned long)position64.partition,block_text,(unsigned long)position64.device_flags);
            } else puts(OT_MSG_NATIVE_LOCATION_UNKNOWN);
            if(position64.valid_fields & 2UL) {
                if(ot_tape_u64_format(position64.file,position64.file_high,file_text,sizeof(file_text))!=0) return 2;
                printf(OT_MSG_NATIVE_FILE64,file_text);
            }
            if(position64.probe_flags & 1UL) puts(OT_MSG_NATIVE_LENGTH);
            if(position64.session_state & OT_TAPE_SESSION_NATIVE_FAILED) puts(OT_MSG_NATIVE_UNFINISHED);
            if(position64.diagnostic_class) printf(OT_MSG_NATIVE_DIAG,position64.diagnostic_class,position64.host_status,
                position64.target_status,position64.sense_key,position64.asc,position64.ascq);
        }
#endif
    } else if(is_position) {
#ifdef OT_NATIVE_TAPE_POSITION
        printf(OT_MSG_NATIVE_POSITION_START,name);
        if(fflush(stdout)!=0 || ferror(stdout)) return 2;
        rc=ot_dos_native_position(name,&position,&error);
        printf(OT_MSG_NATIVE_RESULT,name,rc,error);
        if(ot_tape_position_validate(&position,sizeof(position))==0) {
            if(position.valid_fields & OT_TAPE_POSITION_VALID)
                printf(OT_MSG_NATIVE_POSITION,(unsigned long)position.partition,
                    (unsigned long)position.first_block,(unsigned long)position.last_block,
                    (unsigned long)position.device_flags);
            else puts(OT_MSG_NATIVE_LOCATION_UNKNOWN);
            if(position.probe_flags & OT_TAPE_PROBE_UNVERIFIED_LENGTH) puts(OT_MSG_NATIVE_LENGTH);
            if(position.session_state & OT_TAPE_SESSION_NATIVE_FAILED) puts(OT_MSG_NATIVE_UNFINISHED);
            if(position.diagnostic_class)
                printf(OT_MSG_NATIVE_DIAG,position.diagnostic_class,position.host_status,
                    position.target_status,position.sense_key,position.asc,position.ascq);
        }
#endif
    } else {
        if (movement) printf(OT_MSG_NATIVE_SPACE, name, operation, (long)count);
        else if (operation == OT_TAPE_CONTROL_LOAD) printf(OT_MSG_NATIVE_LOAD, name);
        else if (operation == OT_TAPE_CONTROL_UNLOAD) printf(OT_MSG_NATIVE_UNLOAD, name);
#ifdef OT_NATIVE_TAPE_REMOVAL
        else if (operation == OT_TAPE_CONTROL_LOCK) printf(OT_MSG_NATIVE_LOCK, name);
        else if (operation == OT_TAPE_CONTROL_UNLOCK) printf(OT_MSG_NATIVE_UNLOCK, name);
#endif
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

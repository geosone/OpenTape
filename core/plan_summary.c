/* Caller supplies a successfully built plan; reject inconsistent list shape
 * before publishing counts. Keep the
 * arithmetic portable to DOS without requiring a compiler 64-bit integer. */
#include <string.h>
#include "opentape/backup_plan.h"
int ot_plan_summarize(const struct ot_backup_plan *plan, struct ot_plan_summary *summary)
{
    const struct ot_plan_entry *entry, *last = 0;
    struct ot_plan_summary value;
    oa_u8 encoded[OT_FILE_METADATA_PREFIX + OT_FILE_PATH_MAX];
    oa_u16 size;
    oa_u32 before;
    unsigned i;
    if (plan == 0 || summary == 0 || plan->count > OT_PLAN_MAX_ENTRIES) return OT_SOURCE_ARGUMENT;
    memset(&value, 0, sizeof(value)); entry = plan->first;
    for (i = 0u; i < plan->count; ++i) {
        if (entry == 0 || memchr(entry->source, 0, sizeof(entry->source)) == 0 || *entry->source == 0 ||
            ot_file_metadata_encode(encoded, sizeof(encoded), &entry->metadata, &size) != 0) return OT_SOURCE_ARGUMENT;
        if (entry->metadata.kind == OT_FILE_DIRECTORY) ++value.directories;
        else {
            ++value.files; before = value.bytes_low;
            value.bytes_low += entry->metadata.file_size;
            if (value.bytes_low < before) ++value.bytes_high;
        }
        last = entry; entry = entry->next;
    }
    if (entry != 0 || last != plan->last) return OT_SOURCE_ARGUMENT;
    *summary = value;
    return 0;
}

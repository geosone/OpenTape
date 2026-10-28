/* Per-node allocation also exercises DOS segmented memory at the 256-entry
 * limit. Wide payload accounting is independent of native tape capabilities. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/backup_plan.h"
static void fail(const struct ot_backup_plan *plan)
{
    struct ot_plan_summary result;
    oa_u8 before[sizeof(result)];
    memset(&result, 0xA5, sizeof(result)); memcpy(before, &result, sizeof(result));
    assert(ot_plan_summarize(plan, &result) == OT_SOURCE_ARGUMENT);
    assert(memcmp(before, &result, sizeof(result)) == 0);
}
int main(void)
{
    struct ot_plan_entry *nodes[OT_PLAN_MAX_ENTRIES];
    struct ot_backup_plan plan;
    struct ot_plan_summary result;
    unsigned i;
    memset(&plan, 0, sizeof(plan));
    assert(ot_plan_summarize(&plan, &result) == 0 && !result.files && !result.directories && !result.bytes_low && !result.bytes_high);
    for (i = 0u; i < OT_PLAN_MAX_ENTRIES; ++i) {
        nodes[i] = calloc(1u, sizeof(*nodes[i])); assert(nodes[i] != 0);
        sprintf(nodes[i]->source, "F%03u.BIN", i); sprintf(nodes[i]->metadata.path, "DATA/F%03u.BIN", i);
        nodes[i]->metadata.kind = OT_FILE_REGULAR; nodes[i]->metadata.file_size = 0xfffffffful;
    }
    for (i = 0u; i < OT_PLAN_MAX_ENTRIES - 1u; ++i) nodes[i]->next = nodes[i + 1u];
    plan.first = nodes[0]; plan.last = nodes[255]; plan.count = 256u;
    assert(ot_plan_summarize(&plan, &result) == 0);
    assert(result.files == 256ul && !result.directories && result.bytes_high == 255ul && result.bytes_low == 0xffffff00ul);
    nodes[0]->metadata.kind = OT_FILE_DIRECTORY; nodes[0]->metadata.dos_attributes = 0x10u; nodes[0]->metadata.file_size = 0ul;
    assert(ot_plan_summarize(&plan, &result) == 0);
    assert(result.files == 255ul && result.directories == 1ul && result.bytes_high == 254ul && result.bytes_low == 0xffffff01ul);
    plan.count = 3u; plan.last = nodes[2]; nodes[2]->next = 0; nodes[2]->metadata.file_size = 1ul;
    assert(ot_plan_summarize(&plan, &result) == 0 && result.bytes_high == 1ul && result.bytes_low == 0ul);
    plan.count = 1u; plan.last = nodes[0]; nodes[0]->next = 0;
    assert(ot_plan_summarize(&plan, &result) == 0 && !result.files && result.directories == 1ul && !result.bytes_low && !result.bytes_high);
    nodes[0]->metadata.file_size = 1ul; fail(&plan); nodes[0]->metadata.file_size = 0ul;
    nodes[0]->source[0] = 0; fail(&plan); strcpy(nodes[0]->source, "ROOT");
    memset(nodes[0]->source, 'A', sizeof(nodes[0]->source)); fail(&plan); strcpy(nodes[0]->source, "ROOT");
    strcpy(nodes[0]->metadata.path, "../BAD"); fail(&plan); strcpy(nodes[0]->metadata.path, "DATA");
    plan.last = nodes[1]; fail(&plan); plan.last = nodes[0];
    nodes[0]->next = nodes[1]; fail(&plan); nodes[0]->next = 0;
    plan.count = 2u; fail(&plan); plan.count = 257u; fail(&plan);
    fail(0); assert(ot_plan_summarize(&plan, 0) == OT_SOURCE_ARGUMENT);
    for (i = 0u; i < OT_PLAN_MAX_ENTRIES; ++i) free(nodes[i]);
    puts("Backup plan summary tests passed");
    return 0;
}

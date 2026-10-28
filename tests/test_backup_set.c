/* Parser transactions and bounded DOS-sized allocations. Read errors, changed
 * manifests and failed close cannot replace a previously checked source list. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/backup_set.h"
struct fixture { const char *data; unsigned size, offset, reads, closes, checks, read_fail, check_fail, close_fail, short_read; };
static struct fixture f;
static unsigned read_list(void *p, void *data, unsigned size, unsigned *got)
{
    struct fixture *v = p;
    ++v->reads; *got = 0u;
    if (v->read_fail && v->reads >= v->read_fail) return 5u;
    if (v->short_read && size > v->short_read) size = v->short_read;
    if (size > v->size - v->offset) size = v->size - v->offset;
    memcpy(data, v->data + v->offset, size); v->offset += size; *got = size; return 0u;
}
static unsigned recheck(void *p) { struct fixture *v = p; ++v->checks; return v->check_fail; }
static unsigned close_list(void *p) { struct fixture *v = p; ++v->closes; return v->close_fail; }
static struct ot_source_ops ops = { &f, read_list, recheck, close_list };
static void reset(const char *data, unsigned size)
{ memset(&f, 0, sizeof(f)); f.data = data; f.size = size; f.short_read = 7u; }
static void fails(struct ot_backup_set *set)
{
    struct ot_backup_set saved = *set;
    struct ot_backup_root node = *set->first;
    assert(ot_backup_set_load(set, &ops) != 0 && f.closes == 1u);
    assert(memcmp(set, &saved, sizeof(saved)) == 0 && memcmp(set->first, &node, sizeof(node)) == 0);
}
int main(void)
{
    static const char good[] = "  # two roots\r\n\t\nF INPUT.BIN SINGLE.BIN\r\nt C:\\HOME TREE";
    static const char *bad[] = { "", "# only comments\n", "F A.BIN", "F A.BIN X extra", "Q A.BIN X",
        "FF A.BIN X", "F *.BIN X", "F A?.BIN X", "F A.BIN ../X", "F A.BIN /X", "F A.BIN CON",
        "F RMT0.CFG X", "T NRMT0$ TREE", "F A.BIN X\r", "F A.BIN X\rF B.BIN Y", "F A.BIN X\032more", "F A.BIN X\177" };
    struct ot_backup_set set;
    const struct ot_backup_root *root;
    char *large, path[256], source[128];
    unsigned i, used;
    memset(&set, 0, sizeof(set)); reset(good, sizeof(good) - 1u);
    assert(ot_backup_set_load(&set, &ops) == 0 && f.closes == 1u && f.checks == 1u && set.count == 2u);
    assert(!set.first->tree && set.last->tree && !strcmp(set.first->path, "SINGLE.BIN"));
    for (i = 0u; i < sizeof(bad) / sizeof(bad[0]); ++i) { reset(bad[i], (unsigned)strlen(bad[i])); fails(&set); }
    reset("F A.BIN X\0T B Y", 16u); fails(&set);
    reset(good, sizeof(good) - 1u); f.read_fail = 4u; fails(&set);
    reset(good, sizeof(good) - 1u); f.check_fail = 1u; fails(&set);
    reset(good, sizeof(good) - 1u); f.close_fail = 1u; fails(&set);
    large = malloc((unsigned)OT_SET_MAX_BYTES + 2u); assert(large);
    used = 0u;
    for (i = 0u; i < 64u; ++i) used += (unsigned)sprintf(large + used, "F SOURCE F%03u.BIN\n", i);
    reset(large, used); assert(ot_backup_set_load(&set, &ops) == 0 && set.count == 64u);
    root = set.first;
    for (i = 0u; i < 64u; ++i) { assert(root && !root->tree); root = root->next; } assert(!root);
    used += (unsigned)sprintf(large + used, "F SOURCE LIMIT.BIN\n"); reset(large, used); fails(&set);
    /* Maximum source/path token and line accepted, each allocation below 64K. */
    memset(source, 'A', 127u); source[127] = 0; *path = 0;
    for (i = 0u; i < 27u; ++i) strcat(path, "ABCDEFGH/");
    strcat(path, "F1234567.BIN");
    assert(strlen(path) == 255u);
    used = (unsigned)sprintf(large, "F %s %s", source, path); reset(large, used);
    assert(ot_backup_set_load(&set, &ops) == 0 && strlen(set.first->path) == 255u);
    large[used++] = 'X'; reset(large, used); fails(&set);
    strcpy(large, "F SOURCE X\n"); used = (unsigned)strlen(large);
    while (used < (unsigned)OT_SET_MAX_BYTES) {
        large[used++] = '#';
        for (i = 0u; i < 300u && used < (unsigned)OT_SET_MAX_BYTES; ++i) large[used++] = ' ';
        if (used < (unsigned)OT_SET_MAX_BYTES) large[used++] = '\n';
    }
    reset(large, used); assert(ot_backup_set_load(&set, &ops) == 0 && set.count == 1u);
    large[used++] = '\n'; reset(large, used); fails(&set);
    ot_backup_set_free(&set); free(large);
    puts("backup sets: bounded parser, 64 roots, exact EOF and atomic failure handling OK");
    return 0;
}

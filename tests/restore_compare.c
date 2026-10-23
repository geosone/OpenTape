/* Compare the actual DOS extraction with the selected source tree. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "opentape/backup_plan.h"
int main(int argc, char **argv)
{
    struct ot_backup_plan a, b;
    struct ot_plan_entry *e, *other;
    struct ot_dos_source x, y;
    oa_u8 *left, *right;
    unsigned error, n, got, checked = 0u;
    int rc = 2;
    unsigned preserve_zero = argc == 2 && strcmp(argv[1], "--preserve-zero") == 0;
    memset(&a, 0, sizeof(a)); memset(&b, 0, sizeof(b));
    memset(&x, 0, sizeof(x)); memset(&y, 0, sizeof(y));
    left = malloc(4096u); right = malloc(4096u);
    if (left == 0 || right == 0) goto done;
    if (ot_dos_plan_tree(&a, "SRC", "ROOT", &error) != 0 ||
        ot_dos_plan_tree(&b, "RESTORED\\ROOT", "ROOT", &error) != 0 || a.count != b.count) goto done;
    for (e = a.first; e != 0; e = e->next) {
        for (other = b.first; other != 0; other = other->next)
            if (strcmp(e->metadata.path, other->metadata.path) == 0) break;
        if (other == 0 || other->metadata.kind != e->metadata.kind) goto done;
        if (e->metadata.kind == OT_FILE_REGULAR &&
            !(preserve_zero && strcmp(e->metadata.path, "ROOT/SUB/ZERO.BIN") == 0)) {
            if (other->metadata.file_size != e->metadata.file_size ||
                other->metadata.dos_date != e->metadata.dos_date || other->metadata.dos_time != e->metadata.dos_time ||
                other->metadata.dos_attributes != e->metadata.dos_attributes ||
                ot_dos_source_open(&x, e->source, e->metadata.path) != 0 ||
                ot_dos_source_open(&y, other->source, other->metadata.path) != 0) goto done;
            do {
                if (x.ops.read(&x, left, 4096u, &n) != 0u || y.ops.read(&y, right, 4096u, &got) != 0u ||
                    n != got || memcmp(left, right, n) != 0) goto done;
            } while (n != 0u);
            if (x.ops.close(&x) != 0u || y.ops.close(&y) != 0u) goto done;
        }
        ++checked;
    }
    rc = 0;
done:
    if (x.opened && x.ops.close(&x) != 0u) rc = 2;
    if (y.opened && y.ops.close(&y) != 0u) rc = 2;
    ot_plan_free(&a); ot_plan_free(&b); free(left); free(right);
    printf(rc == 0 ? "OpenTape extracted tree comparison passed: %u entries\n" :
                     "OpenTape extracted tree comparison FAILED after %u entries\n", checked);
    return rc;
}

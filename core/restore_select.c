#include <string.h>
#include <stdlib.h>
#include "opentape/restore.h"
#include "opentape/source_file.h"
static char upper(char c) { return c >= 'a' && c <= 'z' ? (char)(c - 'a' + 'A') : c; }
static int equal(const char *a, const char *b)
{
    while (*a != 0 && *b != 0) {
        if (upper(*a++) != upper(*b++)) return 0;
    }
    return *a == *b;
}
unsigned ot_restore_selected(const char *const *paths, unsigned count, const char *path)
{
    unsigned i;
    for (i = 0u; i < count; ++i) if (equal(paths[i], path)) return i + 1u;
    return 0u;
}
int ot_restore_selection_valid(const char *const *paths, unsigned count)
{
    unsigned i, j, n;
    char first[OT_FILE_PATH_MAX + 1u];
    if (count > OT_RESTORE_SELECTION_MAX || (count != 0u && paths == 0)) return 0;
    for (i = 0u; i < count; ++i) {
        if (!ot_archive_path_valid(paths[i])) return 0;
        for (n = 0u; paths[i][n] != 0 && paths[i][n] != '/'; ++n) first[n] = paths[i][n];
        first[n] = 0;
        if (equal(first, OT_RESTORE_TEMP) || equal(first, OT_RESTORE_OLD)) return 0;
        for (j = 0u; j < i; ++j) if (equal(paths[i], paths[j])) return 0;
    }
    return 1;
}

int ot_restore_selection_add(struct ot_restore_selection *list, const char *path)
{
    const char *one[1];
    char *copy;
    if (list == 0) return OT_RESTORE_PATH;
    one[0] = path;
    if (!ot_restore_selection_valid(one, 1u) ||
        ot_restore_selected(list->paths, list->count, path) != 0u) return OT_RESTORE_PATH;
    if (list->count >= OT_RESTORE_SELECTION_MAX) return OT_RESTORE_LIMIT;
    copy = malloc(strlen(path) + 1u);
    if (copy == 0) return OT_RESTORE_LIMIT;
    strcpy(copy, path); list->paths[list->count++] = copy;
    return 0;
}

static int add_line(struct ot_restore_selection *list, char *line, unsigned n)
{
    line[n] = 0;
    return n == 0u || line[0] == '#' ? 0 : ot_restore_selection_add(list, line);
}

int ot_restore_selection_load(struct ot_restore_selection *list, const struct ot_source_ops *ops)
{
    oa_u8 buffer[256];
    char line[OT_FILE_PATH_MAX + 1u];
    unsigned got, i, length = 0u, pending_cr = 0u, start, error;
    oa_u32 total = 0ul;
    int rc = 0;
    if (list == 0 || ops == 0 || ops->read == 0 || ops->recheck == 0 || ops->close == 0)
        return OT_RESTORE_PATH;
    start = list->count;
    for (;;) {
        got = 0u;
        error = ops->read(ops->context, buffer, sizeof(buffer), &got);
        if (error != 0u || got > sizeof(buffer)) { rc = OT_SOURCE_IO; break; }
        if (got == 0u) break;
        total += got;
        if (total > OT_RESTORE_SELECTION_BYTES) { rc = OT_RESTORE_LIMIT; break; }
        for (i = 0u; i < got; ++i) {
            unsigned c = buffer[i];
            if (pending_cr && c != '\n') { rc = OT_RESTORE_PATH; break; }
            if (c == '\r') { pending_cr = 1u; continue; }
            if (c == '\n') {
                pending_cr = 0u; rc = add_line(list, line, length); length = 0u;
                if (rc != 0) break;
            } else if (c < 32u || c > 126u || length == OT_FILE_PATH_MAX) {
                rc = OT_RESTORE_PATH; break;
            } else line[length++] = (char)c;
        }
        if (rc != 0) break;
    }
    if (rc == 0 && pending_cr) rc = OT_RESTORE_PATH;
    if (rc == 0 && length != 0u) rc = add_line(list, line, length);
    if (rc == 0 && ops->recheck(ops->context) != 0u) rc = OT_SOURCE_CHANGED;
    error = ops->close(ops->context);
    if (rc == 0 && error != 0u) rc = OT_SOURCE_IO;
    if (rc != 0) while (list->count > start) {
        --list->count; free((void *)list->paths[list->count]); list->paths[list->count] = 0;
    }
    return rc;
}

void ot_restore_selection_free(struct ot_restore_selection *list)
{
    if (list == 0) return;
    while (list->count != 0u) { --list->count; free((void *)list->paths[list->count]); list->paths[list->count] = 0; }
}

int ot_restore_selection_save(const struct ot_restore_selection *list,
                             const struct ot_selection_output *ops)
{
    unsigned i, accepted, size, error;
    int rc = 0;
    char line[OT_FILE_PATH_MAX + 2u];
    if (ops == 0 || ops->write == 0 || ops->close == 0 || ops->publish == 0)
        return OT_RESTORE_PATH;
    if (list == 0 || !ot_restore_selection_valid(list->paths, list->count))
        rc = OT_RESTORE_PATH;
    for (i = 0u; rc == 0 && i < list->count; ++i) {
        size = (unsigned)strlen(list->paths[i]);
        memcpy(line, list->paths[i], size); line[size++] = '\r'; line[size++] = '\n';
        accepted = 0u;
        error = ops->write(ops->context, (const oa_u8 *)line, size, &accepted);
        if (error != 0u || accepted != size) rc = OT_RESTORE_IO;
    }
    error = ops->close(ops->context);
    if (rc == 0 && error != 0u) rc = OT_RESTORE_IO;
    if (rc == 0 && ops->publish(ops->context) != 0u) rc = OT_RESTORE_IO;
    return rc;
}

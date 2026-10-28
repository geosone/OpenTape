/* Bounded streaming source-list parser. Do not treat DOS control-Z as EOF:
 * unseen trailing roots must never be silently omitted from a backup. */
#include <stdlib.h>
#include <string.h>
#include "opentape/backup_set.h"
#include "opentape/local_name.h"
#define SET_LINE_MAX (1u + 1u + 127u + 1u + OT_FILE_PATH_MAX)
void ot_backup_set_free(struct ot_backup_set *set)
{
    struct ot_backup_root *root, *next;
    if (set == 0) return;
    for (root = set->first; root != 0; root = next) { next = root->next; free(root); }
    memset(set, 0, sizeof(*set));
}
static int add_line(struct ot_backup_set *set, char *line)
{
    char *tokens[3], *p = line;
    struct ot_backup_root *root;
    unsigned count = 0u;
    while (*p == ' ' || *p == '\t') ++p;
    if (*p == 0 || *p == '#') return 0;
    while (*p != 0) {
        if (count == 3u) return OT_SET_FORMAT;
        tokens[count++] = p;
        while (*p != 0 && *p != ' ' && *p != '\t') ++p;
        if (*p != 0) *p++ = 0;
        while (*p == ' ' || *p == '\t') ++p;
    }
    if (count != 3u || strlen(tokens[0]) != 1u ||
        (tokens[0][0] != 'F' && tokens[0][0] != 'f' && tokens[0][0] != 'T' && tokens[0][0] != 't') ||
        strlen(tokens[1]) == 0u || strlen(tokens[1]) > 127u ||
        strpbrk(tokens[1], "*?\"<>|") != 0 || ot_local_tape_name(tokens[1]) ||
        !ot_archive_path_valid(tokens[2])) return OT_SET_FORMAT;
    if (set->count >= OT_SET_MAX_ROOTS) return OT_SET_LIMIT;
    root = malloc(sizeof(*root));
    if (root == 0) return OT_SET_LIMIT;
    root->next = 0; root->tree = tokens[0][0] == 'T' || tokens[0][0] == 't';
    strcpy(root->source, tokens[1]); strcpy(root->path, tokens[2]);
    if (set->last) set->last->next = root; else set->first = root;
    set->last = root; ++set->count; return 0;
}
int ot_backup_set_load(struct ot_backup_set *set, const struct ot_source_ops *ops)
{
    struct ot_backup_set temporary;
    oa_u8 chunk[128];
    char line[SET_LINE_MAX + 1u];
    oa_u32 bytes = 0ul;
    unsigned got, i, used = 0u, cr = 0u, error;
    int rc = 0;
    if (ops == 0 || ops->read == 0 || ops->recheck == 0 || ops->close == 0) return OT_SOURCE_ARGUMENT;
    memset(&temporary, 0, sizeof(temporary));
    if (set == 0) rc = OT_SOURCE_ARGUMENT;
    while (rc == 0) {
        got = 0u;
        if (ops->read(ops->context, chunk, sizeof(chunk), &got) || got > sizeof(chunk)) { rc = OT_SOURCE_IO; break; }
        if (got == 0u) break;
        for (i = 0u; i < got && rc == 0; ++i) {
            unsigned c = chunk[i];
            if (++bytes > OT_SET_MAX_BYTES) { rc = OT_SET_LIMIT; break; }
            if (cr && c != '\n') { rc = OT_SET_FORMAT; break; }
            if (c == '\r') { cr = 1u; continue; }
            if (c == '\n') { line[used] = 0; rc = add_line(&temporary, line); used = cr = 0u; }
            else if ((c < 32u && c != '\t') || c == 127u) rc = OT_SET_FORMAT;
            else if (used == SET_LINE_MAX) rc = OT_SET_LIMIT;
            else line[used++] = (char)c;
        }
    }
    if (rc == 0 && cr) rc = OT_SET_FORMAT;
    if (rc == 0 && used) { line[used] = 0; rc = add_line(&temporary, line); }
    if (rc == 0 && !temporary.count) rc = OT_SET_FORMAT;
    if (rc == 0 && ops->recheck(ops->context)) rc = OT_SOURCE_CHANGED;
    error = ops->close(ops->context); if (rc == 0 && error) rc = OT_SOURCE_IO;
    if (rc == 0) { ot_backup_set_free(set); *set = temporary; }
    else ot_backup_set_free(&temporary);
    return rc;
}

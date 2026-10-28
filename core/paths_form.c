/* Syntactic settings transaction only. The existing operation owns canonical
 * DOS path checks, regular-file/type/existence checks and all disk/tape access.
 * In particular saving a restore path does not authorize any replacement. */
#include <string.h>
#include "opentape/paths_form.h"

int ot_paths_form_value_valid(const char *name)
{
    unsigned i;
    if (name == 0) return 0;
    for (i = 0u; i <= OT_PATHS_FORM_MAX; ++i) {
        unsigned char c = (unsigned char)name[i];
        if (c == 0u) return 1;
        if (c < 32u || c == 127u || c == '*' || c == '?') return 0;
    }
    return 0;
}
int ot_paths_form_set(struct ot_paths_form *form, unsigned field, const char *name)
{
    if (form == 0 || field >= OT_PATHS_FORM_COUNT || !ot_paths_form_value_valid(name)) return 0;
    /* memmove also permits assigning the existing borrowed field to itself. */
    memmove(form->names[field], name, strlen(name) + 1u);
    return 1;
}
int ot_paths_form_commit(struct ot_paths_form *form, const struct ot_paths_form *draft)
{
    unsigned i;
    if (form == 0 || draft == 0) return 0;
    for (i = 0u; i < OT_PATHS_FORM_COUNT; ++i)
        if (!ot_paths_form_value_valid(draft->names[i])) return 0;
    *form = *draft;
    return 1;
}
const char *ot_paths_form_name(const struct ot_paths_form *form, unsigned field)
{
    if (form == 0 || field >= OT_PATHS_FORM_COUNT || !ot_paths_form_value_valid(form->names[field]) ||
        form->names[field][0] == 0) return 0;
    return form->names[field];
}

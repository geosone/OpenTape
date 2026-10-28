/* One device-stem rule for source opens and profile filenames. This does not
 * establish regular-file existence/type; the DOS handle checks still do that. */
#include <string.h>
#include "opentape/local_name.h"
#include "opentape/record_payload.h"
static int suffix(const char *stem)
{
    return strlen(stem) == 3u && stem[0] >= 'A' && stem[0] <= 'Z' &&
        stem[1] == 'T' && stem[2] >= '0' && stem[2] <= '9';
}
int ot_local_tape_name(const char *name)
{
    const char *leaf, *p;
    char stem[10];
    unsigned n = 0u;
    if (name == 0 || *name == 0) return 0;
    leaf = name;
    if (name[1] == ':') leaf += 2;
    for (p = leaf; *p != 0; ++p) if (*p == '/' || *p == '\\') leaf = p + 1;
    /* DOS accepts device colons and trims filename spaces. Be conservative
     * around these delimiters before any regular-file open can reach a driver. */
    while (*leaf == ' ') ++leaf;
    while (leaf[n] != 0 && leaf[n] != '.' && leaf[n] != ':' && leaf[n] != ' ') {
        if (n >= 9u) return 0;
        stem[n] = leaf[n] >= 'a' && leaf[n] <= 'z' ? leaf[n] - 'a' + 'A' : leaf[n];
        ++n;
    }
    if (n != 0u && stem[n - 1u] == '$') --n;
    stem[n] = 0;
    return suffix(stem) || (stem[0] == 'R' && suffix(stem + 1)) ||
        (stem[0] == 'N' && suffix(stem + 1)) ||
        (stem[0] == 'N' && stem[1] == 'R' && suffix(stem + 2));
}

int ot_local_file_name_valid(const char *name, unsigned maximum)
{
    const char *leaf, *p;
    unsigned n;
    if (name == 0 || *name == 0) return 0;
    for (n = 0u; name[n] != 0; ++n) {
        unsigned char c = (unsigned char)name[n];
        if (n >= maximum || c < 32u || c == 127u || c == '*' || c == '?') return 0;
    }
    leaf = name;
    if (name[1] == ':' && ((name[0] >= 'A' && name[0] <= 'Z') ||
        (name[0] >= 'a' && name[0] <= 'z'))) leaf += 2;
    for (p = leaf; *p != 0; ++p) if (*p == '/' || *p == '\\') leaf = p + 1;
    return ot_archive_path_valid(leaf) && !ot_local_tape_name(name);
}

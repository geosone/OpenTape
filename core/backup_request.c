/* Shared CLI/form syntax checks only. Validation never opens source or tape. */
#include <string.h>
#include "opentape/backup_dos.h"

int ot_dos_backup_request_valid(const struct ot_dos_backup_request *request)
{
    unsigned i;
    if (request == 0 || request->source == 0 || *request->source == 0 ||
        strlen(request->source) > 127u || strchr(request->source, '*') != 0 ||
        strchr(request->source, '?') != 0 || request->path == 0 ||
        !ot_archive_path_valid(request->path) || request->volume == 0 ||
        *request->volume == 0 || strlen(request->volume) > 31u || request->tree > OT_BACKUP_SET) return 0;
    for (i = 0u; request->volume[i] != 0; ++i)
        if (!((request->volume[i] >= 'A' && request->volume[i] <= 'Z') ||
              (request->volume[i] >= 'a' && request->volume[i] <= 'z') ||
              (request->volume[i] >= '0' && request->volume[i] <= '9') ||
              request->volume[i] == '-' || request->volume[i] == '_')) return 0;
    return 1;
}


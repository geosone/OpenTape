#include "opentape/backend.h"
#include "opentape/messages.h"

static int ot_backend_available(unsigned available,
                                enum ot_backend_kind kind)
{
    switch (kind) {
    case OT_BACKEND_LEGACY_TAPE:
        return (available & OT_BACKEND_HAVE_LEGACY_TAPE) != 0u;
    case OT_BACKEND_OPENASPI:
        return (available & OT_BACKEND_HAVE_OPENASPI) != 0u;
    case OT_BACKEND_DIRECT_ASPI:
        return (available & OT_BACKEND_HAVE_DIRECT_ASPI) != 0u;
    default:
        return 0;
    }
}

enum ot_backend_kind ot_backend_select(unsigned available,
                                       enum ot_backend_kind preferred)
{
    if (preferred != OT_BACKEND_AUTO &&
        ot_backend_available(available, preferred)) {
        return preferred;
    }

    if (ot_backend_available(available, OT_BACKEND_OPENASPI)) {
        return OT_BACKEND_OPENASPI;
    }
    if (ot_backend_available(available, OT_BACKEND_LEGACY_TAPE)) {
        return OT_BACKEND_LEGACY_TAPE;
    }
    if (ot_backend_available(available, OT_BACKEND_DIRECT_ASPI)) {
        return OT_BACKEND_DIRECT_ASPI;
    }
    return OT_BACKEND_NONE;
}

const char *ot_backend_name(enum ot_backend_kind kind)
{
    switch (kind) {
    case OT_BACKEND_AUTO:
        return OT_MSG_BACKEND_AUTO;
    case OT_BACKEND_LEGACY_TAPE:
        return OT_MSG_BACKEND_LEGACY;
    case OT_BACKEND_OPENASPI:
        return OT_MSG_BACKEND_OPENASPI;
    case OT_BACKEND_DIRECT_ASPI:
        return OT_MSG_BACKEND_DIRECT;
    default:
        return OT_MSG_BACKEND_NONE;
    }
}

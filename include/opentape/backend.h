#ifndef OPENTAPE_BACKEND_H
#define OPENTAPE_BACKEND_H

enum ot_backend_kind {
    OT_BACKEND_NONE = 0,
    OT_BACKEND_AUTO,
    OT_BACKEND_LEGACY_TAPE,
    OT_BACKEND_OPENASPI,
    OT_BACKEND_DIRECT_ASPI
};

#define OT_BACKEND_HAVE_LEGACY_TAPE 0x01u
#define OT_BACKEND_HAVE_OPENASPI    0x02u
#define OT_BACKEND_HAVE_DIRECT_ASPI 0x04u

enum ot_backend_kind ot_backend_select(unsigned available,
                                       enum ot_backend_kind preferred);
const char *ot_backend_name(enum ot_backend_kind kind);

#endif

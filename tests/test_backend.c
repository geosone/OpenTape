#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "openaspi/aspitape.h"
#include "opentape/backend.h"

int main(void)
{
    unsigned all;

    assert(sizeof(struct oa_aspitape_mtop) == 6u);
    assert(sizeof(struct oa_aspitape_mtget) == 14u);
    assert(sizeof(struct oa_aspitape_mtget16) == 16u);
    assert(OA_MT16_STATE_WRITE_ERROR == 0x0200u);

    all = OT_BACKEND_HAVE_LEGACY_TAPE |
          OT_BACKEND_HAVE_OPENASPI |
          OT_BACKEND_HAVE_DIRECT_ASPI;

    assert(ot_backend_select(all, OT_BACKEND_AUTO) == OT_BACKEND_OPENASPI);
    assert(ot_backend_select(all, OT_BACKEND_LEGACY_TAPE) ==
           OT_BACKEND_LEGACY_TAPE);
    assert(ot_backend_select(OT_BACKEND_HAVE_DIRECT_ASPI,
                             OT_BACKEND_LEGACY_TAPE) ==
           OT_BACKEND_DIRECT_ASPI);
    assert(ot_backend_select(0u, OT_BACKEND_AUTO) == OT_BACKEND_NONE);
    assert(ot_backend_name(OT_BACKEND_LEGACY_TAPE) != 0);
#ifdef LANG_GR
    assert(strcmp(ot_backend_name(OT_BACKEND_NONE), "keines") == 0);
    assert(strcmp(ot_backend_name(OT_BACKEND_DIRECT_ASPI),
                  "direktes ASPI") == 0);
#else
    assert(strcmp(ot_backend_name(OT_BACKEND_NONE), "none") == 0);
    assert(strcmp(ot_backend_name(OT_BACKEND_DIRECT_ASPI),
                  "direct ASPI") == 0);
#endif

    puts("OpenTape backend tests passed");
    return 0;
}

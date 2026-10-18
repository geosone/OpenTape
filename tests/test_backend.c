#include <assert.h>
#include <stdio.h>

#include "openaspi/aspitape.h"
#include "opentape/backend.h"

int main(void)
{
    unsigned all;

    assert(sizeof(struct oa_aspitape_mtop) == 6u);
    assert(sizeof(struct oa_aspitape_mtget) == 14u);

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

    puts("OpenTape backend tests passed");
    return 0;
}

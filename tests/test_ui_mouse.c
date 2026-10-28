#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "opentape/ui_mouse.h"
int main(void)
{
    struct ot_mouse_map map;
    unsigned i;
    memset(&map, 0, sizeof(map));
    assert(ot_mouse_bind(&map, 3, 21, 10u, 'y') == 0);
    assert(ot_mouse_bind(&map, 15, 21, 12u, 27u) == 0);
    assert(ot_mouse_bind(&map, 4, 21, 1u, 'x') != 0 && map.count == 2u);
    assert(ot_mouse_event(&map, OT_MOUSE_UP, 4, 21) == 0); /* No opener release reuse. */
    assert(ot_mouse_event(&map, OT_MOUSE_DOWN, 4, 21) == 0);
    assert(ot_mouse_event(&map, OT_MOUSE_UP, 4, 21) == 'y');
    assert(ot_mouse_event(&map, OT_MOUSE_UP, 4, 21) == 0); /* No repeat. */
    assert(ot_mouse_event(&map, OT_MOUSE_DOWN, 4, 21) == 0);
    assert(ot_mouse_event(&map, OT_MOUSE_UP, 16, 21) == 0); /* Different target. */
    assert(ot_mouse_event(&map, OT_MOUSE_DOWN, 4, 21) == 0);
    assert(ot_mouse_event(&map, OT_MOUSE_MOVE, 4, 20) == 0);
    assert(ot_mouse_event(&map, OT_MOUSE_MOVE, 4, 21) == 0);
    assert(ot_mouse_event(&map, OT_MOUSE_UP, 4, 21) == 0); /* Drag out cancels. */
    assert(ot_mouse_event(&map, OT_MOUSE_DOWN, 4, 21) == 0);
    assert(ot_mouse_event(&map, OT_MOUSE_CANCEL, 4, 21) == 0);
    assert(ot_mouse_event(&map, OT_MOUSE_UP, 4, 21) == 0); /* Keyboard cancels press. */
    assert(ot_mouse_event(&map, OT_MOUSE_DOWN, 4, 21) == 0);
    ot_mouse_clear(&map);
    assert(ot_mouse_bind(&map, 3, 21, 10u, 'w') == 0);
    assert(ot_mouse_event(&map, OT_MOUSE_UP, 4, 21) == 0); /* New view, new press. */
    assert(ot_mouse_event(&map, OT_MOUSE_DOWN, 4, 21) == 0);
    assert(ot_mouse_event(&map, OT_MOUSE_UP, 4, 21) == 'w');
    ot_mouse_clear(&map);
    assert(ot_mouse_bind(&map, -1, 0, 1u, 1u) != 0);
    assert(ot_mouse_bind(&map, 80, 0, 1u, 1u) != 0);
    assert(ot_mouse_bind(&map, 0, 25, 1u, 1u) != 0);
    assert(ot_mouse_bind(&map, 79, 24, 2u, 1u) != 0);
    assert(ot_mouse_bind(&map, 0, 0, 0u, 1u) != 0);
    assert(ot_mouse_bind(&map, 0, 0, 1u, 0u) != 0);
    for (i = 0u; i < OT_MOUSE_MAX_TARGETS; ++i) assert(ot_mouse_bind(&map, (int)i, 0, 1u, i + 1u) == 0);
    assert(ot_mouse_bind(&map, 64, 0, 1u, 99u) != 0);
    assert(ot_mouse_event(&map, OT_MOUSE_DOWN, 63, 0) == 0);
    assert(ot_mouse_event(&map, OT_MOUSE_UP, 63, 0) == 64u);
    assert(ot_mouse_event(0, OT_MOUSE_DOWN, 1, 1) == 0);
    ot_mouse_clear(0);
    puts("Mouse view/press/release boundary tests passed");
    return 0;
}

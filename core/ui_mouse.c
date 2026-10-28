#include "opentape/ui_mouse.h"
void ot_mouse_clear(struct ot_mouse_map *map)
{ if (map != 0) { map->count = 0u; map->pressed = 0u; } }
int ot_mouse_bind(struct ot_mouse_map *map, int x, int y, unsigned width, unsigned key)
{
    unsigned i;
    struct ot_mouse_target *target;
    if (map == 0 || x < 0 || x >= 80 || y < 0 || y >= 25 || !width || width > 80u - (unsigned)x ||
        !key || map->count >= OT_MOUSE_MAX_TARGETS) return -1;
    for (i = 0u; i < map->count; ++i) {
        target = map->targets + i;
        if (target->y == y && x < target->x + (int)target->width && target->x < x + (int)width) return -1;
    }
    target = map->targets + map->count++;
    target->x = x; target->y = y; target->width = width; target->key = key;
    return 0;
}
unsigned ot_mouse_event(struct ot_mouse_map *map, unsigned kind, int x, int y)
{
    unsigned i, hit = 0u, result = 0u;
    if (map == 0) return 0u;
    for (i = 0u; i < map->count; ++i)
        if (map->targets[i].y == y && x >= map->targets[i].x &&
            x < map->targets[i].x + (int)map->targets[i].width) { hit = i + 1u; break; }
    if (kind == OT_MOUSE_DOWN) map->pressed = hit;
    else if (kind == OT_MOUSE_MOVE) { if (map->pressed != hit) map->pressed = 0u; }
    else {
        if (kind == OT_MOUSE_UP && hit && hit == map->pressed) result = map->targets[hit - 1u].key;
        map->pressed = 0u;
    }
    return result;
}

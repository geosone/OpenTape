#ifndef OPENTAPE_UI_MOUSE_H
#define OPENTAPE_UI_MOUSE_H
#define OT_MOUSE_MAX_TARGETS 64u
#define OT_MOUSE_CANCEL 0u
#define OT_MOUSE_DOWN 1u
#define OT_MOUSE_UP 2u
#define OT_MOUSE_MOVE 3u
struct ot_mouse_target { int x, y; unsigned width, key; };
struct ot_mouse_map {
    struct ot_mouse_target targets[OT_MOUSE_MAX_TARGETS];
    unsigned count, pressed;
};
/* Text-cell hit targets. Reset on every redraw, including modal/busy views.
 * Activation requires press AND release inside the same current-view target;
 * drag-out, keyboard input, view replacement and stray releases cancel it. */
void ot_mouse_clear(struct ot_mouse_map *);
int ot_mouse_bind(struct ot_mouse_map *, int x, int y, unsigned width, unsigned key);
unsigned ot_mouse_event(struct ot_mouse_map *, unsigned kind, int x, int y);
#endif

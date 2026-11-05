// darling/test/event/event_kinds_test.c — mirrors darling c23/event_invoke
//
// Covers EVERY event kind, not just the mouse-down/scroll/document subset:
// mouse down/up/move/enter/leave, zoom, touch down/move/up, and key down AND
// key up. Also asserts the binding table is clearable before teardown, so a
// freed element leaves no dangling key.

#include <stdio.h>

#include "c23/event_invoke.h"
#include "ui/element.h"

static int g_fail = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

typedef struct Trace { int ids[32]; int count; float lastMag; } Trace;
// Appends an event-kind identifier while the fixed trace has room.
static void push(Trace *t, int id) { if ((*t).count < 32) (*t).ids[(*t).count++] = id; }

// Records mouse-down delivery.
static void m_down(Element *e, const Mouse *m, void *ud)  { (void)e; (void)m; push(ud,  1); }
// Records mouse-up delivery.
static void m_up(Element *e, const Mouse *m, void *ud)    { (void)e; (void)m; push(ud,  2); }
// Records mouse-move delivery.
static void m_move(Element *e, const Mouse *m, void *ud)  { (void)e; (void)m; push(ud,  3); }
// Records mouse-enter delivery.
static void m_enter(Element *e, const Mouse *m, void *ud) { (void)e; (void)m; push(ud,  4); }
// Records mouse-leave delivery.
static void m_leave(Element *e, const Mouse *m, void *ud) { (void)e; (void)m; push(ud,  5); }
// Records zoom delivery and saves its magnitude for the assertion.
static void z_zoom(Element *e, const Zoom *z, void *ud) {
    (void)e;
    Trace *t = ud;
    push(t, 6);
    (*t).lastMag = (*z).magnitude;
}
// Records touch-down delivery.
static void t_down(Element *e, const Touch *t, void *ud) { (void)e; (void)t; push(ud, 7); }
// Records touch-move delivery.
static void t_move(Element *e, const Touch *t, void *ud) { (void)e; (void)t; push(ud, 8); }
// Records touch-up delivery.
static void t_up(Element *e, const Touch *t, void *ud)   { (void)e; (void)t; push(ud, 9); }
// Records key-down delivery.
static void k_down(Element *e, const Key *k, void *ud)   { (void)e; (void)k; push(ud, 10); }
// Records key-up delivery.
static void k_up(Element *e, const Key *k, void *ud)     { (void)e; (void)k; push(ud, 11); }

// Dispatches a test event with shared coordinates and deterministic payloads.
static bool dispatch(Element *root, int kind, float x, float y) {
    Event ev = { .kind = kind, .x = x, .y = y, .magnitude = 2.5f, .key = 42 };
    return Element_dispatchEvent(root, &ev);
}

#include "test_application.h"
// Verifies callback delivery for every mouse, zoom, touch, and key event kind.
int main(void) {
    ElementDesc dd = {0};
    dd.width = 200; dd.height = 200;
    Element *root = Element(&dd);

    ElementDesc cd = {0};
    cd.width = 100; cd.height = 100;
    Element *child = Element(&cd);
    Element_add(root, child);

    Trace trace = {0};

    // mouse kinds on the child (hit at 10,10)
    Element_addMouseEvent(child, (MouseEvent){
        .onDown = m_down, .onUp = m_up, .onMove = m_move,
        .onEnter = m_enter, .onLeave = m_leave, .userdata = &trace });
    CHECK(dispatch(root, EV_MOUSE_DOWN, 10, 10));
    CHECK(dispatch(root, EV_MOUSE_UP, 10, 10));
    CHECK(dispatch(root, EV_MOUSE_MOVE, 10, 10));
    CHECK(dispatch(root, EV_MOUSE_ENTER, 10, 10));
    CHECK(dispatch(root, EV_MOUSE_LEAVE, 10, 10));
    CHECK(trace.count == 5);
    CHECK(trace.ids[0] == 1 && trace.ids[1] == 2 && trace.ids[2] == 3 &&
          trace.ids[3] == 4 && trace.ids[4] == 5);

    // zoom on the child
    trace.count = 0;
    Element_addZoomEvent(child, (ZoomEvent){ .onZoom = z_zoom, .userdata = &trace });
    CHECK(dispatch(root, EV_ZOOM, 10, 10));
    CHECK(trace.count == 1 && trace.ids[0] == 6);
    CHECK(trace.lastMag == 2.5f);

    // touch routes to the root (no pointer to hit-test)
    trace.count = 0;
    Element_addTouchEvent(root, (TouchEvent){
        .onDown = t_down, .onMove = t_move, .onUp = t_up, .userdata = &trace });
    CHECK(dispatch(root, EV_TOUCH_DOWN, 10, 10));
    CHECK(dispatch(root, EV_TOUCH_MOVE, 10, 10));
    CHECK(dispatch(root, EV_TOUCH_UP, 10, 10));
    CHECK(trace.count == 3 && trace.ids[0] == 7 && trace.ids[1] == 8 && trace.ids[2] == 9);

    // key DOWN and key UP both fire (the earlier suite only proved down)
    trace.count = 0;
    Element_addKeyEvent(root, (KeyEvent){ .onDown = k_down, .onUp = k_up, .userdata = &trace });
    CHECK(dispatch(root, EV_KEY_DOWN, 0, 0));
    CHECK(dispatch(root, EV_KEY_UP, 0, 0));
    CHECK(trace.count == 2 && trace.ids[0] == 10 && trace.ids[1] == 11);

    // lifecycle: clear before teardown leaves no dangling key
    CHECK(Element_eventBindingCount() == 4);   // child mouse, child zoom, root touch, root key
    Element_clearEvents(root);
    CHECK(Element_eventBindingCount() == 0);
    CHECK(!dispatch(root, EV_MOUSE_DOWN, 10, 10));

    Element_destroy(root);
    if (g_fail == 0) printf("event_kinds_test: ALL PASS\n");
    return g_fail == 0 ? 0 : 1;
}

// darling/test/event/event_invoke_test.c — mirrors darling c23/event_invoke
//
// Every element is an event target. Registration is per kind via the
// Element_add<Kind>Event base register; dispatch hit-tests the tree and bubbles
// deepest-first. This proves the registry (replace-not-leak), the hit + bubble
// order, non-pointer routing, and clear.

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

// A little sequence log the handlers append to, so we can assert WHO ran in
// WHAT order rather than just that something fired.
typedef struct Trace {
    int ids[16];
    int count;
    f32 last_dy;
} Trace;

// Appends a handler identifier while the fixed trace has room.
static void push(Trace *t, int id) {
    if ((*t).count < 16) (*t).ids[(*t).count++] = id;
}

// Records the first child scroll handler and its vertical delta.
static void on_scroll_child(Element *e, const Scroll *s, void *ud) {
    (void)e;
    Trace *t = ud;
    push(t, 1);
    (*t).last_dy = (*s).dy;   // stash the delta we were handed
}
// Records the replaceable child scroll handler in the trace.
static void on_scroll_child2(Element *e, const Scroll *s, void *ud) {
    (void)e; (void)s;
    push((Trace*) ud, 2);
}
// Records the root scroll callback for bubbling-order assertions.
static void on_scroll_root(Element *e, const Scroll *s, void *ud) {
    (void)e; (void)s;
    push((Trace*) ud, 3);
}
// Records delivery of a hit-tested mouse-down callback.
static void on_mouse_down(Element *e, const Mouse *m, void *ud) {
    (void)e; (void)m;
    push((Trace*) ud, 4);
}
// Records root-routed key-down delivery.
static void on_key_down(Element *e, const Key *k, void *ud) {
    (void)e; (void)k;
    push((Trace*) ud, 5);
}
// Records root-routed document-change delivery.
static void on_document_changed(Element *e, const Document *d, void *ud) {
    (void)e; (void)d;
    push((Trace*) ud, 6);
}

#include "test_application.h"
// Verifies event replacement, hit dispatch, bubbling order, and clearing.
int main(void) {
    ElementDesc dd = {0};
    dd.width = 200; dd.height = 200;
    Element *root = Element(&dd);

    ElementDesc cd = {0};
    cd.width = 100; cd.height = 100;
    Element *child = Element(&cd);
    Element_add(root, child);

    Trace trace = {0};

    // register on the child, then re-register: REPLACES, does not accumulate
    CHECK(Element_addScrollEvent(child, (ScrollEvent){ .onScroll = on_scroll_child2, .userdata = &trace }) == child);
    CHECK(Element_eventBindingCount() == 1);
    Element_addScrollEvent(child, (ScrollEvent){ .onScroll = on_scroll_child, .userdata = &trace });
    CHECK(Element_eventBindingCount() == 1);
    Element_addScrollEvent(root,  (ScrollEvent){ .onScroll = on_scroll_root,  .userdata = &trace });
    CHECK(Element_eventBindingCount() == 2);

    // scroll over the child: deepest first (child=1), then the ancestor (root=3)
    Event sc = { .kind = EV_SCROLL, .x = 10, .y = 10, .dy = -7.5f };
    CHECK(Element_dispatchEvent(root, &sc));
    CHECK(trace.count == 2);
    CHECK(trace.ids[0] == 1);
    CHECK(trace.ids[1] == 3);
    CHECK((f32)trace.last_dy == -7.5f);   // the delta reached the handler

    // scroll outside the child: only the root
    trace.count = 0;
    Event sc2 = { .kind = EV_SCROLL, .x = 150, .y = 150, .dy = 3 };
    CHECK(Element_dispatchEvent(root, &sc2));
    CHECK(trace.count == 1 && trace.ids[0] == 3);

    // mouse-down on the child (only the child registered a mouse handler)
    trace.count = 0;
    Element_addMouseEvent(child, (MouseEvent){ .onDown = on_mouse_down, .userdata = &trace });
    Event md = { .kind = EV_MOUSE_DOWN, .x = 10, .y = 10, .key = 0 };
    CHECK(Element_dispatchEvent(root, &md));
    CHECK(trace.count == 1 && trace.ids[0] == 4);

    // a kind with no handler anywhere is not "handled"
    Event mv = { .kind = EV_MOUSE_MOVE, .x = 10, .y = 10 };
    CHECK(!Element_dispatchEvent(root, &mv));

    // key + document route to the root (no pointer to hit-test)
    trace.count = 0;
    Element_addKeyEvent(root, (KeyEvent){ .onDown = on_key_down, .userdata = &trace });
    Element_addDocumentEvent(root, (DocumentEvent){ .onChanged = on_document_changed, .userdata = &trace });
    Event kd = { .kind = EV_KEY_DOWN, .key = 42 };
    CHECK(Element_dispatchEvent(root, &kd));
    Event dc = { .kind = EV_DOCUMENT_CHANGED };
    CHECK(Element_dispatchEvent(root, &dc));
    CHECK(trace.count == 2 && trace.ids[0] == 5 && trace.ids[1] == 6);

    CHECK(Element_eventBindingCount() == 5);   // child+root scroll, child mouse, root key, root doc

    // clear under the root drops every binding (the tree owns nothing)
    Element_clearEvents(root);
    CHECK(Element_eventBindingCount() == 0);
    CHECK(!Element_dispatchEvent(root, &sc2));

    Element_destroy(root);
    if (g_fail == 0) printf("event_invoke_test: ALL PASS\n");
    return g_fail == 0 ? 0 : 1;
}

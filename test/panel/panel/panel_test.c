// darling/test/panel/panel_test.c — mirrors darling-framework/src/panel
//
// The Panel wrapper: it owns a graphvex GraphicsPanel (an Element) and is the
// handle the app/input talk to.

#include <stdio.h>

#include "panel/panel.h"
#include "ui/element.h"

static int g_fail = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

#include "test_application.h"
// Verify Panel construction, placement, painting, tree queries, and cleanup.
int main(void) {
    ElementDesc d = {0};
    d.width = 200; d.height = 80;
    d.anchor = PART_CENTER; d.pivot = PART_CENTER;
    d.radius = 16;
    d.background = COLOR_RGBA(120, 190, 220, 255);

    // the wrapper owns an attached GraphicsPanel
    Panel *p = Panel(&d);
    CHECK(p != nullptr);
    Element *g = Panel_graphics(p);
    CHECK(g != nullptr);
    CHECK(Element_width(g) == 200.0f && Element_height(g) == 80.0f);

    // placement comes from the GraphicsPanel
    Rect parent = {0, 0, 400, 200};
    Rect r = Element_resolve(g, parent);
    CHECK(r.x == 100.0f && r.y == 60.0f);
    CHECK(r.w == 200.0f && r.h == 80.0f);

    // paint reaches the display list
    DisplayList *dl = DisplayList_0();
    Element_paint(g, r, dl);
    CHECK(DisplayList_count(dl) >= 1);
    DisplayList_free(dl);

    // find + hit over a small tree of fresh panels
    Element *root = Element();
    Element_setSize(root, 400, 200);   // the hit-test root rect
    Element *a = Element(&d); Element_setTag(a, "a");
    Element *b = Element(&d); Element_setTag(b, "b.sh");
    Element_add(root, a);
    Element_add(root, b);
    CHECK(Element_find(root, "b.sh") == b);
    CHECK(Element_find(root, "nope") == nullptr);
    CHECK(Element_hit(root, 200, 100) != nullptr);   // centre of the tree

    Element_destroy(root);    // frees a + b.sh
    Panel_destroy(p);         // frees the attached GraphicsPanel
    CHECK(Panel_graphics(nullptr) == nullptr);

    if (g_fail == 0) printf("panel_test: ALL PASS (Panel wraps a GraphicsPanel)\n");
    return g_fail == 0 ? 0 : 1;
}

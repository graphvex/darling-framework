// darling/test/panel/scroll_panel_test.c — mirrors darling panel/scroll_panel
//
// A viewport over oversized content: the offset is end-clamped per axis and
// reflected into the tree as the content's (-x,-y) placement.

#include <stdio.h>

#include "panel/scroll_panel.h"
#include "ui/element.h"

static int g_fail = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

// Compare scroll geometry with tolerance for floating-point calculations.
static bool near(float a, float b) { return (a > b ? a - b : b - a) < 0.01f; }

#include "test_application.h"
// Check clamping, content placement, resizing, painting, and ownership.
int main(void) {
    ScrollPanel *sp = ScrollPanel(200, 150);
    CHECK(sp != nullptr);
    CHECK(ScrollPanel_graphics(sp) != nullptr);

    ElementDesc d = {0};
    d.width = 200; d.height = 1000;
    Element *doc = Element(&d);
    ScrollPanel_setContent(sp, doc);
    CHECK(ScrollPanel_content(sp) == doc);

    // extents + what can scroll
    CHECK(near(ScrollPanel_maxY(sp), 850.0f));       // 1000 - 150
    CHECK(near(ScrollPanel_maxX(sp), 0.0f));         // 200 - 200
    CHECK(ScrollPanel_canScrollY(sp));
    CHECK(!ScrollPanel_canScrollX(sp));

    // the offset clamps at both ends
    ScrollPanel_setOffset(sp, 0, 900);
    CHECK(near(ScrollPanel_getOffsetY(sp), 850.0f));
    ScrollPanel_scrollBy(sp, 0, -1000);
    CHECK(near(ScrollPanel_getOffsetY(sp), 0.0f));
    ScrollPanel_scrollBy(sp, 0, 120);
    CHECK(near(ScrollPanel_getOffsetY(sp), 120.0f));

    // the offset IS the content placement: resolve shows it scrolled up
    Rect r = Element_resolve(doc, (Rect){0, 0, 200, 150});
    CHECK(near(r.y, -120.0f));

    // shrinking the content re-clamps the offset (never marooned)
    ScrollPanel_setContentSize(sp, 200, 160);
    CHECK(near(ScrollPanel_getOffsetY(sp), 10.0f));   // 160 - 150
    ScrollPanel_setContentSize(sp, 200, 100);
    CHECK(near(ScrollPanel_getOffsetY(sp), 0.0f));   // no scroll range -> home

    // it paints: the viewport clips, then the content
    DisplayList *dl = DisplayList_0();
    Element_paint(ScrollPanel_graphics(sp), (Rect){0, 0, 200, 150}, dl);
    CHECK(DisplayList_count(dl) >= 1);
    DisplayList_free(dl);

    ScrollPanel_destroy(sp);   // frees the viewport and the content child
    if (g_fail == 0) printf("scroll_panel_test: ALL PASS\n");
    return g_fail == 0 ? 0 : 1;
}

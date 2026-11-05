// darling/test/panel/panel_shadow_test.c — mirrors darling panel.
//
// Headless: a Panel shadow is offset + blur. It grows the PAINT bounds
// (Element_bounds) without ever moving the layout/hit rect, and paint emits a
// shadow quad before the body quad.

#include <stdio.h>

#include "graphics/graphics.h"
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
// Check that shadow paint bounds expand without changing layout geometry.
int main(void) {
    Panel *p = Panel();
    Panel_setSize(p, 100, 50);
    Panel_setBackground(p, COLOR_RGBA(200, 100, 100, 255));
    Panel_setShadow(p, 0, 0, 12);
    Panel_setShadowColor(p, COLOR_RGBA(0, 0, 0, 150));

    Rect parent = {0, 0, 400, 200};
    Rect layout = Element_resolve(Panel_graphics(p), parent);
    Rect bounds = Element_bounds(Panel_graphics(p), parent);

    CHECK(layout.w == 100.0f && layout.h == 50.0f);   // the hit/layout rect is untouched
    CHECK(bounds.w > layout.w);                       // the PAINT bounds grow...
    CHECK(bounds.h > layout.h);                       // ...on both axes
    CHECK(bounds.x <= layout.x && bounds.y <= layout.y);

    DisplayList *dl = DisplayList_0();
    Element_paint(Panel_graphics(p), layout, dl);
    CHECK(DisplayList_count(dl) >= 2);                // shadow quad + body quad
    DisplayList_free(dl);

    // no shadow -> the paint bounds collapse back to the layout rect
    Panel *flat = Panel();
    Panel_setSize(flat, 100, 50);
    Rect fb = Element_bounds(Panel_graphics(flat), parent);
    CHECK(fb.w == 100.0f && fb.h == 50.0f);
    Panel_destroy(flat);

    Panel_destroy(p);
    if (g_fail == 0) printf("panel_shadow_test: ALL PASS\n");
    return g_fail == 0 ? 0 : 1;
}

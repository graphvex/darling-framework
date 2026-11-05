// darling/test/panel/panel_corner_radius_test.c — mirrors darling panel.
//
// Headless (CPU raster): a Panel whose radius is half its side renders as a
// CIRCLE, and a Panel nested inside another keeps its own radius. Pixels prove
// it — a corner outside the circle is transparent, the centre is filled.

#include <stdio.h>

#include "graphics/graphics.h"
#include "graphvex/lang/image.h"
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

// Read one RGBA pixel from the image using its row stride.
static void px(const Image *img, int x, int y, int *r, int *g, int *b, int *a) {
    const uint8_t *p = Image_pixels(img) + (size_t)y * Image_stride(img) + (size_t)x * 4;
    *r = p[0]; *g = p[1]; *b = p[2]; *a = p[3];
}

#include "test_application.h"
// Assert circle rasterization at pixel samples and preserve nested radii.
int main(void) {
    CHECK(Graphics_use(BACKEND_RASTER));
    CHECK(Graphics_resize(64, 64));

    // radius == half the side -> the Panel is a circle
    Panel *circle = Panel();
    Panel_setSize(circle, 64, 64);
    Panel_setAnchor(circle, PART_TOP_LEFT);
    Panel_setPivot(circle, PART_TOP_LEFT);
    Panel_setRadius(circle, 32);
    Panel_setBackground(circle, COLOR_RGBA(220, 80, 80, 255));
    CHECK(Panel_radius(circle) == 32.0f);

    DisplayList *dl = DisplayList_0();
    CHECK(dl != nullptr);
    Element_paint(Panel_graphics(circle), (Rect){0, 0, 64, 64}, dl);
    CHECK(DisplayList_count(dl) >= 1);

    CHECK(Graphics_begin());
    CHECK(Graphics_clear(COLOR_CLEAR));
    CHECK(Graphics_submit(dl));
    CHECK(Graphics_end());
    Image *img = Image_0();
    CHECK(Graphics_capture(img));

    int r, g, b, a;
    px(img, 2, 2, &r, &g, &b, &a);
    CHECK(a == 0);                       // corner is outside the circle
    px(img, 61, 61, &r, &g, &b, &a);
    CHECK(a == 0);                       // ... and so is the far corner
    px(img, 32, 32, &r, &g, &b, &a);
    CHECK(a == 255 && r == 220);         // centre is filled
    px(img, 32, 1, &r, &g, &b, &a);
    CHECK(a == 255);                     // the circle touches the top midpoint
    px(img, 1, 32, &r, &g, &b, &a);
    CHECK(a == 255);                     // ... and the left midpoint
    Image_destroy(img);

    // a rounded Panel inside a rounded Panel: both radii survive
    Panel *outer = Panel();
    Panel_setSize(outer, 200, 200);
    Panel_setBackground(outer, COLOR_RGBA(40, 40, 60, 255));
    Panel_setRadius(outer, 20);
    Panel *inner = Panel();
    Panel_setSize(inner, 96, 96);
    Panel_setAnchor(inner, PART_CENTER);
    Panel_setPivot(inner, PART_CENTER);
    Panel_setRadius(inner, 48);          // another circle
    Panel_setBackground(inner, COLOR_RGBA(120, 190, 220, 255));
    CHECK(Panel_add(outer, inner) == inner);
    CHECK(Panel_childCount(outer) == 1);
    CHECK(Panel_radius(outer) == 20.0f && Panel_radius(inner) == 48.0f);

    DisplayList_clear(dl);
    Element_paint(Panel_graphics(outer), (Rect){0, 0, 200, 200}, dl);
    CHECK(DisplayList_count(dl) >= 2);   // outer quad + nested inner quad

    DisplayList_free(dl);
    Panel_destroy(outer);                // frees the inner wrapper + both Elements
    Panel_destroy(circle);

    if (g_fail == 0) printf("panel_corner_radius_test: ALL PASS\n");
    return g_fail == 0 ? 0 : 1;
}

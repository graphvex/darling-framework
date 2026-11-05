// darling/test/panel/panel_anchor_corner_radius_test.c — mirrors panel/frame.
//
// VISUAL: nine Panels, one per PARENT anchor (row-major PART_*), each a fat
// rounded rect with a shadow, added to a Frame through the Panel API. Saves a
// PNG, then returns to Application lifetime until the user closes the window.
//
// (Split from the old "gallery" so this test proves exactly two things at once:
// anchoring and corner radius.)

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "frame/frame.h"
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

#define DARLING_TEST_HAS_FRAMES
#define DARLING_TEST_WITH_ARGS
#include "test_application.h"
// Build and capture Panels at each parent anchor with rounded corners and shadows.
int main(int argc, char **argv) {
    bool interactive = argc == 2 && strcmp(argv[1], "--interactive") == 0;
    if (argc > 1 && !interactive) { fprintf(stderr, "usage: %s [--interactive]\n", argv[0]); return 1; }
    Frame *f = Frame("panel — anchor + corner radius", 1200, 820);
    CHECK(f != nullptr);
    Frame_setBackgroundColor(f, COLOR_CLEAR);   // clear paint (alpha is paint only)
    Frame_setTransparent(f, false);              // explicit OS see-through
    Frame_setBlur(f, 30.0f);                    // frosted backdrop

    Color cols[PART_COUNT] = {
        COLOR_RGBA(233, 128, 128, 255), COLOR_RGBA(233, 186, 110, 255), COLOR_RGBA(210, 220, 120, 255),
        COLOR_RGBA(120, 200, 150, 255), COLOR_RGBA(120, 190, 220, 255), COLOR_RGBA(140, 160, 230, 255),
        COLOR_RGBA(170, 140, 220, 255), COLOR_RGBA(220, 130, 200, 255), COLOR_RGBA(230, 140, 160, 255),
    };
    for (int a = 0; a < PART_COUNT; a++) {
        Panel *p = Panel();                     // the arity chooser -> Panel_0()
        Panel_setSize(p, 320, 220);             // big: the corners are the point
        Panel_setAnchor(p, a);                  // a PARENT anchor (row-major)
        Panel_setPivot(p, PART_CENTER);
        Panel_setRadius(p, 56);                 // fat curve
        Panel_setBackground(p, cols[a]);
        Panel_setShadow(p, 0, 0, 30);
        Panel_setShadowColor(p, COLOR_RGBA(0, 0, 0, 150));
        CHECK(Frame_add(f, p) == p);            // the Frame owns the Panel now
    }
    CHECK(Frame_count(f) == PART_COUNT);

    CHECK(Frame_savePNG(f, "/tmp/panel_anchor_corner_radius.png"));   // CAPTURE

    Frame_show(f);
    if (interactive) Darling_testKeepOpen(); // wait for actual closure, no timed hold
    else Frame_destroy(f);

    if (g_fail == 0) printf("panel_anchor_corner_radius_test: ALL PASS\n");
    return g_fail == 0 ? 0 : 1;
}

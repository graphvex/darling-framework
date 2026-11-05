// darling/test/panel/panel_matryoshka_test.c — mirrors darling panel.
//
// Headless: twenty Panels stacked inside one another, each centred and slightly
// smaller, each a rounded rect (the innermost is a circle). Proves deep nesting
// lays out, paints every layer, and tears down as one owned chain.

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

#define LAYERS 20

// Alternate the nested-panel palette by depth.
static Color layerColor(int i) {
    return (i % 2) ? COLOR_RGBA(120, 190, 220, 255) : COLOR_RGBA(230, 140, 160, 255);
}

#include "test_application.h"
// Exercise layout, paint, and teardown for a deeply nested Panel chain.
int main(void) {
    Panel *layers[LAYERS];
    float sizes[LAYERS];

    sizes[0] = 400.0f;
    layers[0] = Panel();
    Panel_setSize(layers[0], sizes[0], sizes[0]);
    Panel_setBackground(layers[0], COLOR_RGBA(20, 20, 30, 255));
    Panel_setRadius(layers[0], sizes[0] / 2);

    for (int i = 1; i < LAYERS; i++) {
        sizes[i] = sizes[i - 1] - 18.0f;
        Panel *child = Panel();
        Panel_setSize(child, sizes[i], sizes[i]);
        Panel_setAnchor(child, PART_CENTER);
        Panel_setPivot(child, PART_CENTER);
        Panel_setRadius(child, sizes[i] / 2);   // every layer is a circle
        Panel_setBackground(child, layerColor(i));
        CHECK(Panel_add(layers[i - 1], child) == child);
        layers[i] = child;
    }

    // the chain is exactly LAYERS deep, one child each
    for (int i = 0; i < LAYERS - 1; i++) {
        CHECK(Panel_childCount(layers[i]) == 1);
        CHECK(Panel_childElement(layers[i], 0) == Panel_graphics(layers[i + 1]));
    }
    CHECK(Panel_childCount(layers[LAYERS - 1]) == 0);
    CHECK(Panel_radius(layers[LAYERS - 1]) == sizes[LAYERS - 1] / 2);   // a circle

    // every layer paints: one quad each, nested
    DisplayList *dl = DisplayList_0();
    Element_paint(Panel_graphics(layers[0]), (Rect){0, 0, 400, 400}, dl);
    CHECK(DisplayList_count(dl) >= LAYERS);
    DisplayList_free(dl);

    Panel_destroy(layers[0]);   // one call frees the entire matryoshka
    if (g_fail == 0) printf("panel_matryoshka_test: ALL PASS (%d layers)\n", LAYERS);
    return g_fail == 0 ? 0 : 1;
}

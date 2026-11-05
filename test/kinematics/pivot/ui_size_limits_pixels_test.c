// The concurrently added min/max Element APIs must clamp BOTH geometry
// and captured pixels, including a second Element sharing the same Property.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frame/frame.h"
#include "nio/property_pool.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

// Writes a failing capture to the optional battle-artifact directory.
static void captureEvidence(const Image *image) {
    const char *dir = getenv("UI_BATTLE_ARTIFACT_DIR");
    if (!dir || !image || !Image_pixels(image)) return;
    const char *name = strrchr(__FILE__, '/');
    name = name ? name + 1 : __FILE__;
    char path[1024];
    int n = snprintf(path, sizeof path, "%s/%s.png", dir, name);
    if (n > 0 && (size_t) n < sizeof path &&
        Window_writePNG(Image_pixels(image), Image_stride(image), (int) Image_width(image), (int) Image_height(image), path))
        fprintf(stderr, "capture: %s\n", path);
}

// Asserts one captured RGBA pixel within the test's channel tolerance.
static inline void pixel(const Image *image, int x, int y, Color expected) {
    CHECK(image && Image_pixels(image));
    CHECK(x >= 0 && y >= 0 && (uint32_t) x < Image_width(image) && (uint32_t) y < Image_height(image));
    const uint8_t *p = Image_pixels(image) + (size_t) y * Image_stride(image) + (size_t) x * 4;
    int want[4] = {Color_red(expected), Color_green(expected), Color_blue(expected), Color_alpha(expected)};
    for (int i = 0; i < 4; ++i) {
        if (abs((int) p[i] - want[i]) > 2) {
            fprintf(stderr, "FAIL pixel (%d,%d) channel %d: got %d expected %d\n", x, y, i, p[i], want[i]);
            captureEvidence(image);
            exit(1);
        }
    }
}

// Paints an element tree with Raster and returns its captured pixels.
static inline Image *rasterCapture(Element *root, int width, int height) {
    CHECK(Graphics_use(BACKEND_RASTER));
    CHECK(Graphics_resize((uint32_t) width, (uint32_t) height));
    DisplayList *dl = DisplayList_0();
    CHECK(dl);
    Element_paint(root, (Rect){0, 0, (float) width, (float) height}, dl);
    CHECK(Graphics_begin());
    CHECK(Graphics_clear(COLOR_BLACK));
    CHECK(Graphics_submit(dl));
    CHECK(Graphics_end());
    Image *image = Image_0();
    CHECK(image && Graphics_capture(image));
    DisplayList_free(dl);
    return image;
}


#include "test_application.h"
// Checks shared-property min/max clamping in dimensions, pixels, hit bounds, and teardown.
int main(void) {
    uint32_t baseline = PropertyPool_live(PropertyPool_default());
    ElementDesc rd = {.width = 192, .height = 128};
    ElementDesc ad = {.width = 80, .height = 90, .offsetX = 16, .offsetY = 16, .background = COLOR_WHITE};
    ElementDesc bd = {.offsetX = 112, .offsetY = 16};
    Element *root = Element(&rd), *a = Element(&ad), *b = Element(&bd);
    CHECK(root && a && b);
    CHECK(Element_setProperty(b, Element_property(a)) == b);
    Element_add(root, a); Element_add(root, b);
    const int expected[][2] = {{40, 32}, {24, 20}, {64, 48}};
    for (int state = 0; state < 3; ++state) {
        if (state == 0) {
            CHECK(Element_setMinimumSize(a, 24, 20) == a);
            CHECK(Element_setMaximumSize(a, 40, 32) == a);
        } else if (state == 1) {
            Element_setSize(a, 4, 6);
        } else {
            Element_setMaximumSize(b, 64, 48); Element_setSize(b, 80, 90);
        }
        int w = expected[state][0], h = expected[state][1];
        CHECK(Element_width(a) == w && Element_width(b) == w);
        CHECK(Element_height(a) == h && Element_height(b) == h);
        CHECK(Element_resolve(a, (Rect){0, 0, 192, 128}).w == w);
        Image *shot = rasterCapture(root, 192, 128);
        for (int y = 0; y < 128; ++y) {
            for (int x = 0; x < 192; ++x) {
                bool inA = x >= 16 && x < 16 + w && y >= 16 && y < 16 + h;
                bool inB = x >= 112 && x < 112 + w && y >= 16 && y < 16 + h;
                pixel(shot, x, y, inA || inB ? COLOR_WHITE : COLOR_BLACK);
            }
        }
        CHECK(Element_hit(root, 16 + w - 1, 16 + h - 1) == a);
        CHECK(Element_hit(root, 16 + w, 16 + h) == root);
        Image_destroy(shot);
    }
    CHECK(!Element_setMinimumSize(nullptr, 1, 1) && !Element_setMaximumSize(nullptr, 1, 1));
    Element_setMinimumSize(a, -1, -1); Element_setMaximumSize(a, 0, 0);
    CHECK(Element_width(a) == 80 && Element_height(a) == 90);
    Element_ownProperty(b); // detach borrow before owner's pooled record is released
    Element_destroy(root);
    CHECK(PropertyPool_live(PropertyPool_default()) == baseline);
    puts("ui_size_limits_pixels_test: PASS (shared min/max clamps, pixels and hit bounds)");
    return 0;
}

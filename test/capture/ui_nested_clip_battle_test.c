// Clip push/pop must intersect ancestors and restore the enclosing mask.
// Two distinct adversaries: an oversized grandchild and a later sibling.
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

#define DARLING_TEST_HAS_FRAMES
#include "test_application.h"
// Verifies nested clips intersect and do not leak to an oversized sibling.
int main(void) {
    ElementDesc desc1 = {.width = 128, .height = 128};
    Element *root = Element(&desc1);
    ElementDesc desc2 = {.width = 64, .height = 64, .offsetX = 32, .offsetY = 32, .radius = 16};
    Element *outer = Element(&desc2);
    ElementDesc desc3 = {.width = 128, .height = 128, .offsetX = -32, .offsetY = -32};
    Element *inner = Element(&desc3);
    ElementDesc desc4 = {.width = 128, .height = 128, .background = COLOR_WHITE};
    Element *leaf = Element(&desc4);
    CHECK(root && outer && inner && leaf);
    Element_setClip(inner, true);
    Element_add(root, outer);
    Element_add(outer, inner);
    Element_add(inner, leaf);
    Frame *frame = Frame("nested masks and border GPU battle", 128, 128);
    CHECK(frame); Frame_setBackground(frame, COLOR_BLACK);
    Element_add(Frame_element(frame), root); Frame_show(frame);
    Image *gpuShot = Frame_capture(frame);
    pixel(gpuShot, 64, 64, COLOR_WHITE);
    pixel(gpuShot, 8, 8, COLOR_BLACK); // inner mask may not replace outer bounds

    CHECK(Element_remove(root)); Frame_destroy(frame);
    Image *shot = rasterCapture(root, 128, 128);
    pixel(shot, 64, 64, COLOR_WHITE);
    pixel(shot, 8, 8, COLOR_BLACK); // inner mask may not replace outer bounds
    Image_destroy(shot);
    Element_destroy(root);
    puts("ui_nested_clip_battle_test: PASS (ancestor masks intersect)");
    return 0;
}

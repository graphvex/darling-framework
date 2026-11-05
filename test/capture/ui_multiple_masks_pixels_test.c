// Differently sized/offset rounded ancestors: image must equal the
// intersection of BOTH original shapes, not a rounded intersection bounding box.
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


// Returns the signed distance from a point to a rounded rectangle.
static float distance(float x, float y, float left, float top, float width, float height, float radius) {
    float qx = fabsf(x - left - width / 2) - (width / 2 - radius);
    float qy = fabsf(y - top - height / 2) - (height / 2 - radius);
    return fminf(fmaxf(qx, qy), 0) + hypotf(fmaxf(qx, 0), fmaxf(qy, 0)) - radius;
}
// Compares interior pixels against the intersection of both rounded shapes.
static void verify(Image *shot) {
    for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
            float outer = distance(x + 0.5f, y + 0.5f, 16, 16, 96, 96, 30);
            float inner = distance(x + 0.5f, y + 0.5f, 32, 8, 88, 88, 28);
            if (fabsf(outer) < 2 || fabsf(inner) < 2) continue;
            pixel(shot, x, y, outer < 0 && inner < 0 ? COLOR_WHITE : COLOR_BLACK);
        }
    }
}
#define DARLING_TEST_HAS_FRAMES
#include "test_application.h"
// Checks independently-oracled rounded-mask intersection on GPU and Raster.
int main(void) {
    ElementDesc od = {.width = 96, .height = 96, .offsetX = 16, .offsetY = 16, .radius = 30};
    ElementDesc id = {.width = 88, .height = 88, .offsetX = 16, .offsetY = -8, .radius = 28};
    ElementDesc ld = {.width = 256, .height = 256, .offsetX = -64, .offsetY = -64, .background = COLOR_WHITE};
    Frame *frame = Frame("intersecting rounded masks", 128, 128); CHECK(frame);
    Frame_setBackground(frame, COLOR_BLACK);
    Element *outer = Element(&od), *inner = Element(&id), *leaf = Element(&ld);
    CHECK(outer && inner && leaf);
    Element_add(Frame_element(frame), outer); Element_add(outer, inner); Element_add(inner, leaf);
    Frame_show(frame); verify(Frame_capture(frame));
    Image *cpu = rasterCapture(Frame_element(frame), 128, 128); verify(cpu); Image_destroy(cpu);
    Frame_destroy(frame);
    puts("ui_multiple_masks_pixels_test: PASS (independent CPU/GPU intersection oracle)");
    return 0;
}

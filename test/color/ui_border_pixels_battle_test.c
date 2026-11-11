// Regression: an opaque 4px border must appear in the captured image.
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
    if (n > 0 && (bytes) n < sizeof path &&
        Window_writePNG(Image_pixels(image), Image_stride(image), (int) Image_width(image), (int) Image_height(image), path))
        fprintf(stderr, "capture: %s\n", path);
}

// Asserts one captured RGBA pixel within the test's channel tolerance.
static inline void pixel(const Image *image, int x, int y, Color expected) {
    CHECK(image && Image_pixels(image));
    CHECK(x >= 0 && y >= 0 && (u32) x < Image_width(image) && (u32) y < Image_height(image));
    const u8 *p = Image_pixels(image) + (bytes) y * Image_stride(image) + (bytes) x * 4;
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
    CHECK(Graphics_resize((u32) width, (u32) height));
    DisplayList *dl = DisplayList_0();
    CHECK(dl);
    Element_paint(root, (Rect){0, 0, (f32) width, (f32) height}, dl);
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
// Verifies that the configured four-pixel border appears in GPU and Raster captures.
int main(void) {
    ElementDesc rd = {.width = 96, .height = 96};
    ElementDesc cd = {.width = 40, .height = 40, .offsetX = 24, .offsetY = 24, .background = COLOR_WHITE};
    Element *root = Element(&rd), *child = Element(&cd);
    CHECK(root && child); Element_add(root, child);
    Element_setBorder(child, COLOR_RGBA(20, 180, 220, 255), 4);
    Frame *frame = Frame("nested masks and border GPU battle", 96, 96);
    CHECK(frame); Frame_setBackground(frame, COLOR_BLACK);
    Element_add(Frame_element(frame), root); Frame_show(frame);
    Image *gpuShot = Frame_capture(frame);
    pixel(gpuShot, 40, 40, COLOR_WHITE);
    pixel(gpuShot, 25, 40, COLOR_RGBA(20, 180, 220, 255));
    pixel(gpuShot, 40, 25, COLOR_RGBA(20, 180, 220, 255));

    CHECK(Element_remove(root)); Frame_destroy(frame);
    Image *shot = rasterCapture(root, 96, 96);
    pixel(shot, 40, 40, COLOR_WHITE);
    pixel(shot, 25, 40, COLOR_RGBA(20, 180, 220, 255));
    pixel(shot, 40, 25, COLOR_RGBA(20, 180, 220, 255));
    Image_destroy(shot); Element_destroy(root);
    puts("ui_border_pixels_battle_test: PASS (border actually rendered)");
    return 0;
}

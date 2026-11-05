// Pixel oracles for RGBA order, alpha blending, pressed overlay,
// visibility, rectangular clipping, shadow and blur versus layout bounds.
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
// Checks color, pressed/visible state, shadows, blur bounds, and clipping by pixels.
int main(void) {
    ElementDesc desc1 = {.width = 96, .height = 96};
    Element *root = Element(&desc1);
    ElementDesc desc2 = {.width = 40, .height = 40, .offsetX = 24, .offsetY = 24, .background = COLOR_RGBA(200, 100, 50, 255)};
    Element *child = Element(&desc2);
    CHECK(root && child); Element_add(root, child);
    Image *shot = rasterCapture(root, 96, 96);
    pixel(shot, 40, 40, COLOR_RGBA(200, 100, 50, 255)); pixel(shot, 8, 8, COLOR_BLACK); Image_destroy(shot);
    Element_setPressed(child, true);
    shot = rasterCapture(root, 96, 96);
    pixel(shot, 40, 40, COLOR_RGBA(162, 81, 41, 255)); Image_destroy(shot);
    Element_setPressed(child, false); Element_setBorder(child, COLOR_CLEAR, 0);
    Element_setBackground(child, COLOR_RGBA(200, 100, 50, 128));
    shot = rasterCapture(root, 96, 96);
    pixel(shot, 40, 40, COLOR_RGBA(100, 50, 25, 255)); Image_destroy(shot);
    Element_setVisible(child, false);
    shot = rasterCapture(root, 96, 96); pixel(shot, 40, 40, COLOR_BLACK); Image_destroy(shot);
    Element_setVisible(child, true); Element_setBackground(child, COLOR_WHITE);
    Element_setShadow(child, 12, 0, 0); Element_setShadowColor(child, COLOR_RGBA(180, 20, 60, 255));
    shot = rasterCapture(root, 96, 96); pixel(shot, 70, 40, COLOR_RGBA(180, 20, 60, 255)); Image_destroy(shot);
    Rect layout = Element_resolve(child, (Rect){0, 0, 96, 96});
    Element_setBlur(child, 4);
    CHECK(Element_resolve(child, (Rect){0, 0, 96, 96}).x == layout.x);
    CHECK(Element_bounds(child, (Rect){0, 0, 96, 96}).w > layout.w);
    shot = rasterCapture(root, 96, 96);
    const uint8_t *edge = Image_pixels(shot) + 40 * Image_stride(shot) + 23 * 4;
    CHECK(edge[0] > 0 && edge[0] < 255); Image_destroy(shot);
    Element_setBlur(child, 0); Element_setShadowColor(child, COLOR_CLEAR);
    ElementDesc desc3 = {.width = 80, .height = 80, .offsetX = -20, .offsetY = -20, .background = COLOR_WHITE};
    Element *overflow = Element(&desc3);
    CHECK(overflow); Element_add(child, overflow); Element_setClip(child, true);
    shot = rasterCapture(root, 96, 96); pixel(shot, 8, 8, COLOR_BLACK); pixel(shot, 40, 40, COLOR_WHITE); Image_destroy(shot);
    Element_setClip(child, false);
    shot = rasterCapture(root, 96, 96); pixel(shot, 8, 8, COLOR_WHITE); Image_destroy(shot);
    Element_destroy(root);
    puts("ui_visual_state_pixels_test: PASS (paint state verified in captured pixels)");
    return 0;
}

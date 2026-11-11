// All 81 placements, at two parent sizes, with nonzero offsets. Independent
// row/column arithmetic predicts every pixel (not Part_point vs itself).
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

#include "test_application.h"
// Checks every anchor/pivot pair at two parent sizes against pixel and geometry oracles.
int main(void) {
    ElementDesc desc1 = {.width = 320, .height = 240};
    Element *root = Element(&desc1);
    ElementDesc desc2 = {.width = 120, .height = 80, .offsetX = 80, .offsetY = 60};
    Element *parent = Element(&desc2);
    ElementDesc desc3 = {.width = 20, .height = 12, .background = COLOR_WHITE};
    Element *child = Element(&desc3);
    CHECK(root && parent && child);
    Element_add(root, parent);
    Element_add(parent, child);
    Element_setOffset(child, 4, -2);
    for (int size = 0; size < 2; ++size) {
        int w = 120 + size * 40, h = 80 + size * 20;
        Element_setSize(parent, (f32) w, (f32) h);
        for (int anchor = 0; anchor < PART_COUNT; ++anchor) {
            for (int pivot = 0; pivot < PART_COUNT; ++pivot) {
                Element_setAnchor(child, anchor);
                Element_setPivot(child, pivot);
                int left = 80 + (anchor % 3) * w / 2 - (pivot % 3) * 10 + 4;
                int top = 60 + (anchor / 3) * h / 2 - (pivot / 3) * 6 - 2;
                Rect r = Element_resolve(child, (Rect){80, 60, (f32) w, (f32) h});
                CHECK(r.x == left && r.y == top && r.w == 20 && r.h == 12);
                Image *shot = rasterCapture(root, 320, 240);
                for (int y = 0; y < 240; ++y)
                    for (int x = 0; x < 320; ++x)
                        pixel(shot, x, y, x >= left && x < left + 20 && y >= top && y < top + 12 ? COLOR_WHITE : COLOR_BLACK);
                CHECK(Element_hit(root, (f32) left + 10, (f32) top + 6) == child);
                Image_destroy(shot);
            }
        }
    }
    Element_destroy(root);
    puts("ui_anchor_pivot_pixels_test: PASS (162 complete-image placement oracles)");
    return 0;
}

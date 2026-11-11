// Real window: colored document bands must scroll into the viewport without
// bleeding outside it; shrinking content reclamps and updates captured pixels.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frame/frame.h"
#include "nio/property_pool.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

// Save a failed pixel capture when the caller configured an artifact directory.
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

// Assert that one captured pixel matches all expected RGBA channels.
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

// Render an element tree through the CPU raster backend and return its capture.
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

#include "panel/scroll_panel.h"

#define DARLING_TEST_HAS_FRAMES
#include "test_application.h"
// Verify scroll clipping and color bands in frame captures and CPU raster output.
int main(void) {
    u32 baseline = PropertyPool_live(PropertyPool_default());
    Frame *frame = Frame("scroll capture battle", 96, 64);
    CHECK(frame); Frame_setBackground(frame, COLOR_BLACK);
    ScrollPanel *scroll = ScrollPanel(64, 32); CHECK(scroll);
    Element *view = ScrollPanel_graphics(scroll);
    Element_setOffset(view, 16, 16);
    ElementDesc dd = {.width = 64, .height = 96};
    Element *doc = Element(&dd); CHECK(doc);
    Color colors[] = {COLOR_RGBA(200, 40, 60, 255), COLOR_RGBA(40, 200, 60, 255), COLOR_RGBA(40, 60, 200, 255)};
    for (int i = 0; i < 3; ++i) {
        ElementDesc band = {.width = 64, .height = 32, .offsetY = (f32) (i * 32), .background = colors[i]};
        Element *row = Element(&band); CHECK(row); Element_add(doc, row);
    }
    ScrollPanel_setContent(scroll, doc);
    CHECK(Element_add(Frame_element(frame), view) == view);
    Frame_show(frame);
    for (int i = 0; i < 3; ++i) {
        ScrollPanel_setOffset(scroll, 0, (f32) (i * 32));
        Image *shot = Frame_capture(frame);
        pixel(shot, 32, 24, colors[i]); pixel(shot, 8, 24, COLOR_BLACK);
        pixel(shot, 32, 8, COLOR_BLACK); pixel(shot, 32, 56, COLOR_BLACK);
    }
    ScrollPanel_scrollBy(scroll, 1000, 1000);
    f32 x = ScrollPanel_getOffsetX(scroll), y = ScrollPanel_getOffsetY(scroll);
    CHECK(x == 0 && y == 64);
    ScrollPanel_setContentSize(scroll, 64, 32);
    x = ScrollPanel_getOffsetX(scroll); y = ScrollPanel_getOffsetY(scroll); CHECK(x == 0 && y == 0);
    pixel(Frame_capture(frame), 32, 24, colors[0]);
    Image *cpu = rasterCapture(Frame_element(frame), 96, 64);
    pixel(cpu, 32, 24, colors[0]); pixel(cpu, 32, 56, COLOR_BLACK); Image_destroy(cpu);
    CHECK(Element_remove(view)); // wrapper owns the viewport; detach before freeing
    ScrollPanel_destroy(scroll); Frame_destroy(frame);
    CHECK(PropertyPool_live(PropertyPool_default()) == baseline);
    puts("ui_scroll_capture_battle_test: PASS (GPU + CPU viewport, scroll and shrink)");
    return 0;
}

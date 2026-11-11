// Oversized child must not escape a rounded parent. CPU and real window/GPU
// captures are judged against an independent analytic rounded-rectangle oracle.
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

#define EXTENT 128
// Checks captured pixels against the rounded-parent interior oracle.
static void verify(Image *shot) {
    for (int y = 0; y < EXTENT; ++y) {
        for (int x = 0; x < EXTENT; ++x) {
            f32 dx = fmaxf(fabsf((f32) x + 0.5f - 64) - 24, 0);
            f32 dy = fmaxf(fabsf((f32) y + 0.5f - 64) - 24, 0);
            f32 distance = sqrtf(dx * dx + dy * dy) - 24;
            if (fabsf(distance) < 2) continue; // omit the antialias fringe only
            pixel(shot, x, y, distance < 0 ? COLOR_RGBA(220, 40, 60, 255) : COLOR_BLACK);
        }
    }
}
#define DARLING_TEST_HAS_FRAMES
#include "test_application.h"
// Verifies an oversized child stays within its rounded parent on both backends.
int main(void) {
    Frame *frame = Frame("rounded clipping battle", EXTENT, EXTENT);
    CHECK(frame);
    Frame_setBackground(frame, COLOR_BLACK);
    Panel *parent = Panel(96, 96), *child = Panel(160, 160);
    CHECK(parent && child);
    Panel_setOffset(parent, 16, 16);
    Panel_setRadius(parent, 24);
    Panel_setBackground(parent, COLOR_CLEAR);
    Panel_setOffset(child, -32, -32);
    Panel_setBackground(child, COLOR_RGBA(220, 40, 60, 255));
    CHECK(Panel_add(parent, child) == child);
    CHECK(Frame_add(frame, parent) == parent);
    Frame_show(frame);
    verify(Frame_capture(frame));
    // Re-run the identical tree through the CPU backend, independent of GPU.
    Image *cpu = rasterCapture(Frame_element(frame), EXTENT, EXTENT);
    verify(cpu);
    Image_destroy(cpu);
    Frame_destroy(frame);
    puts("ui_rounded_clip_battle_test: PASS (whole-image rounded bounds, CPU + GPU)");
    return 0;
}

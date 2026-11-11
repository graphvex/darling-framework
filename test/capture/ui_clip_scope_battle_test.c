// An explicitly installed clip surrounds submissions, survives nested pops
// and malformed lists, and does not leak stack state into the next frame.
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



#include "test_application.h"
// Exercises caller clip scope across nested lists, malformed lists, and reuse.
int main(void) {
    CHECK(Graphics_use(BACKEND_RASTER) && Graphics_resize(64, 64));
    Rect outer = {16, 16, 32, 32};
    Rect huge = {0, 0, 64, 64};
    Brush white = {COLOR_WHITE, 0, 0, 0, 0};
    DisplayList *dl = DisplayList_0(); Image *shot = Image_0();
    CHECK(dl && shot);
    CHECK(Graphics_clip(&outer));
    for (int iteration = 0; iteration < 20; ++iteration) {
        DisplayList_clear(dl);
        DisplayList_clipRounded(dl, huge, 8);
        DisplayList_rect(dl, huge, &white);
        DisplayList_unclip(dl);
        DisplayList_rect(dl, huge, &white);
        CHECK(Graphics_begin() && Graphics_clear(COLOR_BLACK));
        CHECK(Graphics_submit(dl) && Graphics_end() && Graphics_capture(shot));
        pixel(shot, 24, 24, COLOR_WHITE); pixel(shot, 8, 8, COLOR_BLACK);
        CHECK(Graphics_clear(COLOR_BLACK) && Graphics_fillRect(&huge, &white) && Graphics_capture(shot));
        pixel(shot, 24, 24, COLOR_WHITE); pixel(shot, 8, 8, COLOR_BLACK);
    }
    DisplayList_clear(dl); DisplayList_unclip(dl);
    CHECK(!Graphics_submit(dl)); // underflow rejected; caller clip preserved
    CHECK(Graphics_clear(COLOR_BLACK) && Graphics_fillRect(&huge, &white) && Graphics_capture(shot));
    pixel(shot, 8, 8, COLOR_BLACK);
    DisplayList_clear(dl); DisplayList_clip(dl, huge);
    CHECK(!Graphics_submit(dl)); // unbalanced push rejected
    CHECK(Graphics_clear(COLOR_BLACK) && Graphics_fillRect(&huge, &white) && Graphics_capture(shot));
    pixel(shot, 8, 8, COLOR_BLACK);
    CHECK(Graphics_clip(nullptr));
    DisplayList_clear(dl); DisplayList_rect(dl, huge, &white);
    CHECK(Graphics_submit(dl) && Graphics_capture(shot)); pixel(shot, 8, 8, COLOR_WHITE);
    Image_destroy(shot); DisplayList_free(dl);
    puts("ui_clip_scope_battle_test: PASS (caller scope, nested reuse and malformed recovery)");
    return 0;
}

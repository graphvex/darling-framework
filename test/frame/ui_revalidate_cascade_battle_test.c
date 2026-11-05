// Prove the new Frame->Surface->Board->tree cascade without letting capture
// repaint the scene and conceal a missing revalidation step.
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



// Reads the currently published native surface or retained Surface image without repaint.
static Image *snapshot(Frame *frame) {
    Surface *surface = Frame_surface(frame);
    Image *image = Image_2(Surface_width(surface), Surface_height(surface));
    CHECK(image && Image_ensureShadow(image, Surface_width(surface), Surface_height(surface)));
    void *present = Window_presentSurfaceContents(Frame_window(frame));
    if (present) {
        Window_readPresentSurface(Frame_window(frame), present, Image_pixels(image), Image_stride(image));
    } else {
        Image *retained = Surface_presentImage(surface);
        CHECK(Image_upload(Image_pixels(retained), Image_width(retained), Image_height(retained), image));
    }
    return image;
}
// Asserts the Frame element was cleared before the observer Board revalidates.
static void observe(Board *board, void *userdata) {
    (void) board;
    Frame *frame = userdata;
    CHECK(!Element_isDirty(Frame_element(frame))); // content board must run first
}
#define DARLING_TEST_HAS_FRAMES
#include "test_application.h"
// Verifies revalidation order, published pixels, hit bounds, and resize generations.
int main(void) {
    Frame *frame = Frame("revalidate cascade battle", 240, 160); CHECK(frame);
    // This oracle proves synchronous cascade ordering, not paced publication.
    // Explicitly use the supported uncapped policy; keep all assertions intact.
    Frame_setFPSCap(frame, -1);
    Frame_setFPSCapWhenFocusGain(frame, 0);
    Frame_setFPSCapWhenFocusLost(frame, 0);
    Frame_setBackground(frame, COLOR_BLACK);
    Panel *parent = Panel(240, 160), *child = Panel(40, 30);
    CHECK(parent && child); Panel_setBackground(parent, COLOR_CLEAR);
    Panel_setOffset(child, 10, 20); Panel_setBackground(child, COLOR_WHITE);
    CHECK(Panel_add(parent, child) == child && Frame_add(frame, parent) == parent);
    Frame_show(frame);
    Board *content = Frame_contentBoard(frame); CHECK(content);
    Board *observer = Board_0();
    CHECK(observer);
    Board_addRevalidator(observer, observe, frame);
    Surface_addBoard(Frame_surface(frame), observer);
    uint64_t before = Board_generation(content);
    uint64_t observed = Board_generation(observer);
    Panel_setOffset(child, 120, 80); Panel_setBackground(child, COLOR_RGBA(20, 180, 60, 255));
    CHECK(Element_isDirty(Frame_element(frame)));
    Frame_revalidate(frame);
    CHECK(Board_generation(content) == before + 1 && Board_generation(observer) == observed + 1);
    CHECK(!Element_isDirty(Panel_graphics(child)) && !Element_isDirty(Frame_element(frame)));
    Image *shot = snapshot(frame);
    pixel(shot, 20, 25, COLOR_BLACK); pixel(shot, 130, 90, COLOR_RGBA(20, 180, 60, 255));
    CHECK(Element_hit(Frame_element(frame), 130, 90) == Panel_graphics(child));
    CHECK(Element_hit(Frame_element(frame), 20, 25) != Panel_graphics(child));
    Image_destroy(shot);
    before = Board_generation(content);
    Frame_setSize(frame, 257, 173);
    CHECK(Board_generation(content) == before + 1);
    CHECK(Surface_width(Frame_surface(frame)) == 257 && Surface_height(Frame_surface(frame)) == 173);
    shot = snapshot(frame); CHECK(Image_width(shot) == 257 && Image_height(shot) == 173);
    pixel(shot, 130, 90, COLOR_RGBA(20, 180, 60, 255)); Image_destroy(shot);
    before = Board_generation(content);
    Frame_setSize(frame, 257, 173); CHECK(Board_generation(content) == before);
    Frame_render(frame); CHECK(Board_generation(content) == before + 1);
    Surface_removeBoard(Frame_surface(frame), observer); Board_destroy(observer);
    Frame_revalidate(nullptr); CHECK(Frame_contentBoard(nullptr) == nullptr);
    Frame_destroy(frame);
    puts("ui_revalidate_cascade_battle_test: PASS (ordered board/tree/present without repainting capture)");
    return 0;
}

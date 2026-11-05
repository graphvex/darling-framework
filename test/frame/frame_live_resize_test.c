// Deterministic native geometry-step regression, not a human drag smoothness
// oracle. A frozen R3 clock proves modal resize never waits on content/focus FPS
// deadlines; native IOSurface snapshots do not call Frame_capture/repaint.
// Window_setSize exercises the same native geometry callback as tracking;
// actual mouse drag, CA display timing and live-flag branches remain unproved.
#include <stdio.h>
#include "frame/frame.h"

#define DARLING_TEST_HAS_FRAMES
#include "test_application.h"

// Supplies a fixed Surface clock for deterministic native resize publication.
static uint64_t frozenNow(void *context) {
    (void) context;
    return 1000000000ULL;
}

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "FAIL %d: %s\n", __LINE__, #condition); return 1; } } while (0)

// Verifies each native size change publishes immediately without extra repaint.
int main(void) {
    Frame *frame = Frame("live resize publication", 240, 160);
    CHECK(frame);
    Window *window = Frame_window(frame);
    Surface *surface = Frame_surface(frame);
    CHECK(surface);
    Surface_setClock(surface, frozenNow, nullptr);
    Frame_setBackground(frame, COLOR_RGBA(20, 80, 140, 255));
    Frame_setFPSCap(frame, 30);
    Frame_setFPSCapWhenFocusLost(frame, 0);
    Frame_render(frame);
    uint64_t count = Surface_getPresentCount(surface);
    CHECK(count == 1);
    Frame_render(frame);
    CHECK(Surface_getPresentCount(surface) == count); // ordinary demand still capped
    for (unsigned i = 0; i < 6; ++i) {
        // Even native extents avoid AppKit's fractional-point size rounding;
        // fractional backing geometry is not this publication regression's scope.
        int width = 242 + (int) i * 8, height = 162 + (int) i * 4;
        Window_setSize(window, width, height);
        CHECK(Window_width(window) == width && Window_height(window) == height);
        CHECK(Surface_width(surface) == (unsigned) width && Surface_height(surface) == (unsigned) height);
        CHECK(Surface_getPresentCount(surface) == ++count);
        CHECK(Surface_getFPSCap(surface) == 30);
        void *published = Window_presentSurfaceContents(window);
        if (!published) {
            Frame_destroy(frame);
            fprintf(stderr, "SKIP: native IOSurface seam unavailable\n");
            return B_TEST_SKIP;
        }
        Image *snapshot = Image_2((unsigned) width, (unsigned) height);
        CHECK(snapshot && Image_ensureShadow(snapshot, (unsigned) width, (unsigned) height));
        Image_fill(snapshot, COLOR_CLEAR);
        CHECK(Window_readPresentSurface(window, published,
                                        Image_pixels(snapshot), Image_stride(snapshot)));
        const uint8_t *pixel = Image_pixels(snapshot) + (size_t) (height - 2) * Image_stride(snapshot) +
                               (size_t) (width - 2) * 4;
        CHECK(pixel[0] == 20 && pixel[1] == 80 && pixel[2] == 140 && pixel[3] == 255);
        Image_destroy(snapshot);
        Frame_render(frame);
        CHECK(Surface_getPresentCount(surface) == count); // cap restored at same time
    }
    Window_setSize(window, Window_width(window), Window_height(window));
    CHECK(Surface_getPresentCount(surface) == count); // no redundant settle repaint
    Frame_setSize(frame, 0, 0);
    CHECK(Surface_getPresentCount(surface) == count);
    Frame_destroy(frame);
    puts("frame_live_resize_test: PASS (frozen-clock native geometry publications)");
    return 0;
}

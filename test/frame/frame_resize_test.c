// darling/test/frame/frame_resize_test.c — mirrors darling-framework/src/frame
//
// The GPU seam rebuilds an IOSurface per resize (an IOSurface cannot grow), so
// walk a spread of sizes — including widths whose row Bytes are NOT already
// aligned — and render each. A Metal stride-validation abort here means the
// IOSurface stride is wrong; this is the zoom/live-resize crash in the shell.

#include <stdio.h>
#include "lifetime/platform.h"

#include "frame/frame.h"
#include "window/window.h"

static int g_fail = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

#define DARLING_TEST_HAS_FRAMES
#include "test_application.h"
// Exercises a spread of resize dimensions and repeated no-op resizing.
int main(void) {
    Frame *f = Frame("resize", 800, 600);
    CHECK(f != nullptr);
    if (!f) return 1;

    static const int sizes[][2] = {
        {900, 600}, {1204, 753}, {2408, 1506}, {1000, 700},
        {801, 601}, {640, 480}, {1920, 1080}, {450, 300},
    };
    int n = (int)(sizeof sizes / sizeof sizes[0]);
    for (int i = 0; i < n; i++) {
        Frame_setSize(f, sizes[i][0], sizes[i][1]);
        Frame_render(f);
        CHECK(Frame_root(f).w == (f32)sizes[i][0]);
#if defined(VEX_PLATFORM_APPLE)
        CHECK(Window_presentSurfaceContents(Frame_window(f)) != nullptr);
#endif
    }

    // and the same size twice (a no-op resize) must not tear anything down
    Frame_setSize(f, 1204, 753);
    Frame_setSize(f, 1204, 753);
    Frame_render(f);

    Frame_destroy(f);
    if (g_fail == 0) printf("frame_resize_test: ALL PASS\n");
    return g_fail == 0 ? 0 : 1;
}

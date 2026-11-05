// darling/test/frame/frame_test.c — mirrors darling-framework/src/frame
//
// The Frame: a real window. Lifecycle, the ownership tree, the one resize
// surface, background/transparency/blur, panels, and capture.

#include <stdio.h>
#include "lifetime/platform.h"

#include "frame/frame.h"
#include "panel/panel.h"
#include "ui/element.h"

static int g_fail = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

// Counts invocation of a child Frame's close callback.
static void onClosed(Frame *frame, void *ud) {
    (void)frame;
    *(int*) ud += 1;
}

#define DARLING_TEST_HAS_FRAMES
#include "test_application.h"
// Checks Frame lifecycle, surface sizing, appearance state, panels, and ownership.
int main(void) {
    Frame *f = Frame("frame test", 400, 300);
    CHECK(f != nullptr);
    CHECK(Frame_window(f) != nullptr);
    CHECK(!Window_isTransparent(Frame_window(f)));
    CHECK(!Frame_isClosed(f));
    CHECK(Frame_active() == f);

    // the content element IS the layout root
    Element *root = Frame_element(f);
    CHECK(root != nullptr);

    // the present seam is owned and live (its present Image is the target)
    Surface *surf = Frame_surface(f);
    CHECK(surf != nullptr);
    CHECK(Surface_presentImage(surf) != nullptr);

    // one resize surface: setSize -> root tracks the new size
    Frame_setSize(f, 400, 300);
    Rect r = Frame_root(f);
    CHECK(r.w == 400.0f && r.h == 300.0f);
    Frame_setSize(f, 640, 360);
    r = Frame_root(f);
    CHECK(r.w == 640.0f && r.h == 360.0f);
    // ...and the Surface was revalidated to the same size (render target)
    CHECK(Surface_width(surf) == 640 && Surface_height(surf) == 360);

    // the zero-copy seam: on Apple a render publishes an IOSurface to the
    // window's CALayer; elsewhere the RGBA fallback leaves the layer untouched
    Frame_render(f);
#if defined(VEX_PLATFORM_APPLE)
    CHECK(Window_presentSurfaceContents(Frame_window(f)) != nullptr);
#else
    CHECK(Window_presentSurfaceContents(Frame_window(f)) == nullptr);
#endif

    // title / background
    Frame_setTitle(f, "renamed");
    Frame_setBackgroundColor(f, COLOR_RGBA(10, 20, 30, 255));
    CHECK(Frame_background(f) == COLOR_RGBA(10, 20, 30, 255));
    Frame_setBackground(f, COLOR_CLEAR);       // paint alpha does not enable see-through
    CHECK(Frame_background(f) == COLOR_CLEAR);
    CHECK(!Window_isTransparent(Frame_window(f)));
    Frame_setBackgroundColor(f, COLOR_RGBA(10, 20, 30, 128));
    CHECK(Frame_background(f) == COLOR_RGBA(10, 20, 30, 128));
    CHECK(!Window_isTransparent(Frame_window(f)));

    // Only explicit transparency changes the OS policy; colors preserve it.
    Frame_setTransparent(f, true);
    CHECK(Window_isTransparent(Frame_window(f)));
    Frame_setBackgroundColor(f, COLOR_RGBA(10, 20, 30, 255));
    CHECK(Window_isTransparent(Frame_window(f)));
    Frame_setBackground(f, COLOR_CLEAR);
    CHECK(Window_isTransparent(Frame_window(f)));
    Frame_setTransparent(f, false);
    CHECK(!Window_isTransparent(Frame_window(f)));
    Frame_setBackgroundColor(f, COLOR_CLEAR);
    CHECK(!Window_isTransparent(Frame_window(f)));

    // Leave the ordinary frame test with an opaque painted background.
    Frame_setBackground(f, COLOR_RGBA(10, 20, 30, 255));
    Frame_setBlur(f, 12.0f);
    Frame_setBlur(f, 0.0f);

    // panels: add / count / index / remove
    ElementDesc d = {0};
    d.width = 100; d.height = 40;
    d.anchor = PART_CENTER; d.pivot = PART_CENTER;
    d.background = COLOR_RGBA(200, 100, 100, 255);
    Panel *p0 = Frame_addPanel(f, &d);
    Panel *p1 = Frame_addPanel(f, &d);
    CHECK(p0 != nullptr && p1 != nullptr);
    CHECK(Frame_count(f) == 2);
    CHECK(Frame_panel(f, 0) == p0 && Frame_panel(f, 1) == p1);
    CHECK(Frame_panel(f, 5) == nullptr);
    Frame_removePanels(f);
    CHECK(Frame_count(f) == 0);

    // capture re-renders and hands back pixels (the GPU path reads the front
    // IOSurface back into a CPU Image; the fallback path returns the Surface's)
    Frame_addPanel(f, &d);
    Frame_render(f);
    Image *shot = Frame_capture(f);
    CHECK(shot != nullptr);
    CHECK(Image_width(shot) == 640 && Image_height(shot) == 360);

    // hide / show don't crash and keep the frame alive
    Frame_hide(f);
    Frame_show(f);
    CHECK(!Frame_isClosed(f));

    // ownership tree: closing the OWNER closes the children
    Frame *child = Frame("child", 200, 150);
    Frame_setOwner(child, f);
    CHECK(Frame_owner(child) == f);
    int closed = 0;
    Frame_onClose(child, onClosed, &closed);
    Frame_close(f);                    // frees f AND child
    CHECK(closed == 1);                // the child's close hook fired

    printf("frame_test: ALL PASS\n");
    return g_fail == 0 ? 0 : 1;
}

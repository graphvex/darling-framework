#ifndef DARLING_TEST_FRAME_CHROME_LAB_H
#define DARLING_TEST_FRAME_CHROME_LAB_H

#include "lifetime/platform.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frame/frame.h"
#include "c23/event_invoke.h"
#include "test_support.h"
#define DARLING_TEST_HAS_FRAMES
#define DARLING_TEST_WITH_ARGS
#include "test_application.h"

#define CHROME_CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); Frame_destroy(frame); return 1; } } while (0)

// Requests native closure when the interactive chrome sample's exit panel is clicked.
static void chromeClose(Element *element, const Mouse *mouse, void *userdata) {
    (void)element; (void)mouse;
    Window_setShouldClose(Frame_window(userdata), true);
}

// Captures are content-only: native titlebars/traffic lights/window-server
// effects are NOT present in Frame_capture and cannot earn pixel proof here.
static bool chromeContent(Frame *frame, Color color) {
    Image *image = Frame_capture(frame);
    if (!image || !Image_pixels(image)) return false;
    if (Image_width(image) != (u32)Frame_root(frame).w ||
        Image_height(image) != (u32)Frame_root(frame).h) return false;
    const u8 *p = Image_pixels(image) + (Image_height(image) / 2) * Image_stride(image)
                       + (Image_width(image) / 2) * 4;
    int want[] = {Color_red(color), Color_green(color), Color_blue(color), Color_alpha(color)};
    for (int i = 0; i < 4; ++i) if (abs((int)p[i] - want[i]) > 2) return false;
    return true;
}

// Runs shared Frame decoration, content, transparency, resize, and chrome checks.
static int frameChromeLab(int argc, char **argv, const char *title, int mode, bool lights) {
    bool interactive = argc == 2 && strcmp(argv[1], "--interactive") == 0;
    if (argc > 1 && !interactive) {
        fprintf(stderr, "usage: %s [--interactive]\n", argv[0]);
        return 1;
    }
#if !defined(VEX_PLATFORM_APPLE)
    if (lights) {
        puts("SKIP: native traffic-light assertions require macOS");
        return B_TEST_SKIP;
    }
#endif
    Frame *frame = Frame(title, 640, 420);
    if (!frame) { puts("SKIP: no native Frame available"); return B_TEST_SKIP; }
    Window *window = Frame_window(frame);
    CHROME_CHECK(window);
    CHROME_CHECK(Frame_count(frame) == 0);
    Window_setUndecorated(window, mode);
    CHROME_CHECK(Window_isNaked(window) == (mode == WINDOW_UNDECORATED_NAKED));
    CHROME_CHECK(Window_isBorderless(window) == (mode == WINDOW_UNDECORATED_BORDERLESS));
    CHROME_CHECK(Window_isDecorated(window) == (mode == WINDOW_DECORATED));
    // Naked and borderless modes reject the decorated-only flush policy.
    if (mode != WINDOW_DECORATED) {
        Window_setViewportFlushToTop(window, true);
        CHROME_CHECK(!Window_isViewportFlushToTop(window));
    }
    Color background = COLOR_RGBA(30, 45, 65, 255);
    Frame_setBackground(frame, background);
    Frame_show(frame);
    Frame_setSize(frame, 640, 420);
    CHROME_CHECK(chromeContent(frame, background));
    Frame_setSize(frame, 720, 480);
    CHROME_CHECK(chromeContent(frame, background));
    CHROME_CHECK(Frame_count(frame) == 0);
    CHROME_CHECK(!Window_isTransparent(window));
    Frame_setTransparent(frame, true);
    CHROME_CHECK(Window_isTransparent(window));
    Frame_setBackground(frame, background);
    CHROME_CHECK(Window_isTransparent(window));
    Frame_setTransparent(frame, false);
    CHROME_CHECK(!Window_isTransparent(window));
#if defined(VEX_PLATFORM_APPLE)
    if (mode == WINDOW_UNDECORATED_NAKED) {
        for (int i = 0; i < 3; ++i)
            CHROME_CHECK(Window_macOS_isTrafficLightButtonVisible(window, (WindowTrafficLight)i));
    }
    if (lights) {
        for (int i = 0; i < 3; ++i) {
            WindowTrafficLight light = (WindowTrafficLight)i;
            CHROME_CHECK(Window_macOS_isTrafficLightButtonVisible(window, light));
            Window_macOS_setTrafficLightButtonVisible(window, light, false);
            CHROME_CHECK(!Window_macOS_isTrafficLightButtonVisible(window, light));
            Window_macOS_setTrafficLightButtonVisible(window, light, true);
            CHROME_CHECK(Window_macOS_isTrafficLightButtonVisible(window, light));
        }
        Window_setFloatingTrafficLights(window, true);
        CHROME_CHECK(Window_isViewportFlushToTop(window));
        Window_macOS_setTrafficLightHeaderPosition(window, 18, 12);
        f32 x = -1, y = -1;
        Window_macOS_getTrafficLightHeaderPosition(window, &x, &y);
        CHROME_CHECK(fabsf(x - 18) < 0.01f && fabsf(y - 12) < 0.01f);
        Window_setFloatingTrafficLights(window, false);
        CHROME_CHECK(!Window_isViewportFlushToTop(window));
    }
#endif
    // Reversible chrome transitions keep this Frame's content surface alive.
    Surface *surface = Frame_surface(frame);
    Window_setDecorated(window, true);
    CHROME_CHECK(Window_isDecorated(window));
    Window_setUndecorated(window, mode);
    Frame_setSize(frame, 640, 420);
    CHROME_CHECK(Frame_surface(frame) == surface);
    CHROME_CHECK(chromeContent(frame, background));
    if (interactive) {
        // A borderless window has no native close button. Provide an explicit
        // red square exit, and make the empty background drag the OS window.
        Window_setMovableByBackground(window, true);
        ElementDesc desc = { .width = 32, .height = 32,
            .offsetX = 590, .offsetY = 18, .background = COLOR_RGBA(230, 60, 65, 255) };
        Panel *close = Frame_addPanel(frame, &desc);
        CHROME_CHECK(close);
        Panel_addMouseEvent(close, (MouseEvent){ .onUp = chromeClose, .userdata = frame });
        printf("%s: drag the empty background to move; click red square to close.\n", title);
        Frame_invalidate(frame);
        Darling_testKeepOpen();
    }
    if (!interactive) Frame_destroy(frame);
    puts("PASS: Frame chrome state, empty content pixels, resize and reversible mode changes; native appearance is user-checked.");
    return 0;
}

#endif

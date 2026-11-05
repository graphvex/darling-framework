// darling/test/frame/liquid_glass_frame_test.c — mirrors darling-framework/src/frame
//
// Visual Liquid Glass harness. Opens an empty, naked, see-through window, applies
// the macOS-exclusive capability-gated Liquid Glass chrome (Frame_macOS_*), prints
// the round-tripped properties, and holds the window so the glass is visible.
//
// It is a MANUAL visual test: on macOS < 26 (or a non-Mac build) the probe is
// false, the chrome falls back to Frame_setBlur, and the test still passes.

#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "frame/frame.h"
#include "panel/panel.h"
#include "ui/element.h"
#include "window/window.h"

static int g_fail = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

#define GLASS_WINDOW_WIDTH  900
#define GLASS_WINDOW_HEIGHT 600

#define DARLING_TEST_HAS_FRAMES
#define DARLING_TEST_WITH_ARGS
#include "test_application.h"
// Probes and round-trips Liquid Glass settings; interactive mode holds the window open.
int main(int argc, char **argv) {
    bool interactive = argc == 2 && strcmp(argv[1], "--interactive") == 0;
    if (argc > 1 && !interactive) { fprintf(stderr, "usage: %s [--interactive]\n", argv[0]); return 1; }
    printf("=== LiquidGlassFrame (visual) ===\n");

    // 1. An empty, naked, see-through window: transparent content, no opaques.
    Frame *f = Frame("liquid glass", GLASS_WINDOW_WIDTH, GLASS_WINDOW_HEIGHT);
    CHECK(f != nullptr);
    if (f == nullptr)
        return 1;

    Window *w = Frame_window(f);
    CHECK(w != nullptr);
    Window_setUndecorated(w, WINDOW_UNDECORATED_NAKED);  // transparent top bar, traffic lights kept
    Frame_setBackground(f, COLOR_CLEAR);                 // clear paint; explicit opt-in below
    Frame_setTransparent(f, true);
    Frame_setTitle(f, "liquid glass");

    // 2. Capability probe + apply + read back the properties.
    bool available = Frame_macOS_hasLiquidGlass();
    printf("Frame_macOS_hasLiquidGlass: %s\n",
           available ? "true (macOS 26+)" : "false (fallback: Frame_setBlur / NSVisualEffectView)");

    FrameLiquidGlassDesc want = {
        .enabled = true,
        .style = FRAME_LIQUID_GLASS_STYLE_CLEAR,   // see-through glass (vs REGULAR = frosted)
        .cornerRadius = 24.0f,
        .tintColor = 0x00000000u,                  // fully transparent (0,0,0,0): pure distortion, no colour
    };
    Frame_macOS_setLiquidGlass(f, &want);

    FrameLiquidGlassDesc got = { 0 };
    bool read = Frame_macOS_getLiquidGlass(f, &got);
    printf("liquid glass (read=%s): enabled=%d style=%d cornerRadius=%.1f tint=0x%08X\n",
           read ? "yes" : "no", got.enabled, got.style, got.cornerRadius, got.tintColor);

    CHECK(read);
    CHECK(got.enabled == true);
    CHECK(got.style == FRAME_LIQUID_GLASS_STYLE_CLEAR);
    CHECK(got.cornerRadius == 24.0f);
    CHECK(got.tintColor == 0x00000000u);

    // 3. Show, then return to Application lifetime until actual window closure.
    Frame_show(f);
    // The glass is composited by the window server, independently of us; the
    // content here is empty and static, so there is nothing to repaint per step.
    // Application services events — spamming Frame_render (software raster) every
    // 16ms was pure busywork and made the window feel laggy.
    if (interactive) Darling_testKeepOpen(); // lives until the user CLOSES it
    else Frame_destroy(f); // automated descriptor checks explicitly end their window
    printf("liquid_glass_frame_test: %s\n", g_fail == 0 ? "ALL PASS" : "FAILURES");
    return g_fail == 0 ? 0 : 1;
}

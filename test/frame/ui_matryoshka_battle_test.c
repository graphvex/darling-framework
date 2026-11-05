// 96 nested panels: verify every visible ring, deepest hit, visibility,
// repaint after mutation, resize retention, and recursive ownership teardown.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "frame/frame.h"
#include "nio/property_pool.h"
#include "frame_drag_lab.h"

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


#define DEPTH 96
#define EXTENT (DEPTH * 4 + 16)
// Alternates the two colors used to identify adjacent nested panel rings.
static Color layerColor(int i) { return i % 2 ? COLOR_RGBA(40, 180, 80, 255) : COLOR_RGBA(180, 40, 80, 255); }
#define DARLING_TEST_HAS_FRAMES
#define DARLING_TEST_WITH_ARGS
#include "test_application.h"
// Exercises a 96-panel nested tree, mutation, resize, synthetic drag, and cleanup.
int main(int argc, char **argv) {
    bool interactive = argc == 2 && strcmp(argv[1], "--interactive") == 0;
    if (argc > 1 && !interactive) {
        fprintf(stderr, "usage: %s [--interactive]\n", argv[0]);
        return 1;
    }
    uint32_t baseline = PropertyPool_live(PropertyPool_default());
    Frame *frame = Frame("matryoshka battle", EXTENT, EXTENT);
    CHECK(frame);
    Panel *layers[DEPTH];
    for (int i = 0; i < DEPTH; ++i) {
        layers[i] = Panel(EXTENT - i * 4, EXTENT - i * 4);
        CHECK(layers[i]);
        Panel_setBackground(layers[i], layerColor(i));
        if (i == 0) CHECK(Frame_add(frame, layers[i]) == layers[i]);
        else {
            Panel_setOffset(layers[i], 2, 2);
            CHECK(Panel_add(layers[i - 1], layers[i]) == layers[i]);
        }
    }
    Frame_show(frame);
    Image *shot = Frame_capture(frame);
    for (int i = 0; i < DEPTH; ++i) {
        pixel(shot, i * 2 + 1, EXTENT / 2, layerColor(i));
        CHECK(Panel_childCount(layers[i]) == (i == DEPTH - 1 ? 0 : 1));
    }
    CHECK(Element_hit(Frame_element(frame), EXTENT / 2, EXTENT / 2) == Panel_graphics(layers[DEPTH - 1]));
    Panel_setVisible(layers[DEPTH - 1], false);
    pixel(Frame_capture(frame), EXTENT / 2, EXTENT / 2, layerColor(DEPTH - 2));
    CHECK(Element_hit(Frame_element(frame), EXTENT / 2, EXTENT / 2) == Panel_graphics(layers[DEPTH - 2]));
    Panel_setVisible(layers[DEPTH - 1], true);
    Panel_setBackground(layers[DEPTH - 1], COLOR_WHITE);
    pixel(Frame_capture(frame), EXTENT / 2, EXTENT / 2, COLOR_WHITE);
    Frame_setSize(frame, EXTENT + 17, EXTENT + 11);
    shot = Frame_capture(frame);
    CHECK(Image_width(shot) == EXTENT + 17 && Image_height(shot) == EXTENT + 11);
    pixel(shot, EXTENT / 2, EXTENT / 2, COLOR_WHITE);
    FrameDragLab drag = { .frame = frame, .panels = layers, .count = DEPTH };
    for (int i = 1; i < DEPTH; ++i) drag.x[i] = drag.y[i] = 2;
    frameDragAttach(&drag);
    // Deterministic press/drag/release through the SAME dispatcher as OS input.
    // Move layer 40 and its entire subtree; release outside that selected layer.
    const int selected = 40;
    Event event = { .kind = EV_MOUSE_DOWN, .x = selected * 2 + 1, .y = EXTENT / 2 };
    CHECK(Element_dispatchEvent(Frame_element(frame), &event));
    CHECK(drag.selected == selected);
    event.kind = EV_MOUSE_DRAG; event.x += 12;
    CHECK(Element_dispatchEvent(Frame_element(frame), &event));
    CHECK(drag.x[selected] == 14 && drag.y[selected] == 2);
    pixel(Frame_capture(frame), selected * 2 + 1, EXTENT / 2, layerColor(selected - 1));
    pixel(Frame_capture(frame), selected * 2 + 13, EXTENT / 2, layerColor(selected));
    event.kind = EV_MOUSE_UP; event.x = 1;
    CHECK(Element_dispatchEvent(Frame_element(frame), &event));
    CHECK(drag.selected == -1);
    // Restore the symmetric nested stack before offering it to the user.
    drag.x[selected] = 2;
    Panel_setOffset(layers[selected], 2, 2);
    for (int i = 0; i < DEPTH; ++i) Panel_setBackground(layers[i], layerColor(i));
    Frame_setTitle(frame, "matryoshka: drag a ring (moves its subtree)");
    if (interactive) {
        puts("Drag any ring or the center; its nested children move with it. Close the native window to exit.");
        puts("Dragging is supported inside this frame; outside-window pointer capture is not implemented here.");
        Frame_invalidate(frame);
        Darling_testKeepOpen();
    }
    if (!interactive) Frame_destroy(frame);
    CHECK(PropertyPool_live(PropertyPool_default()) == baseline);
    puts("ui_matryoshka_battle_test: PASS (96 rings, mutation, hit, resize, drag events/pixels, reclamation)");
    return 0;
}

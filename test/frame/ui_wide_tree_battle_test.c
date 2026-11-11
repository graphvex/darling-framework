// 50,000 visible children: real window/GPU capture, ordering, hit targets,
// mutation, detach/reinsert, and complete pooled-property reclamation.
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


#define CHILDREN 50000
#define COLUMNS 250
#define TILE 2
#define DARLING_TEST_HAS_FRAMES
#include "test_application.h"
// Exercises all 50,000 child tiles, targeted hit/mutation, reordering, and reclamation.
int main(void) {
    u32 baseline = PropertyPool_live(PropertyPool_default());
    Frame *frame = Frame("50k children battle", COLUMNS * TILE, CHILDREN / COLUMNS * TILE);
    CHECK(frame);
    Frame_setBackground(frame, COLOR_BLACK);
    Panel *parent = Panel(COLUMNS * TILE, CHILDREN / COLUMNS * TILE);
    CHECK(parent && Frame_add(frame, parent) == parent);
    Panel **children = calloc(CHILDREN, sizeof *children);
    CHECK(children);
    for (int i = 0; i < CHILDREN; ++i) {
        children[i] = Panel(TILE, TILE);
        CHECK(children[i]);
        Panel_setOffset(children[i], (f32) (i % COLUMNS * TILE), (f32) (i / COLUMNS * TILE));
        Panel_setBackground(children[i], i % 2 ? COLOR_RGBA(20, 180, 60, 255) : COLOR_RGBA(200, 40, 80, 255));
        CHECK(Panel_add(parent, children[i]) == children[i]);
        CHECK(Panel_parent(children[i]) == parent);
    }
    CHECK(Panel_childCount(parent) == CHILDREN);
    Frame_show(frame);
    Image *shot = Frame_capture(frame);
    CHECK(shot && Image_width(shot) == COLUMNS * TILE);
    CHECK(Image_height(shot) == CHILDREN / COLUMNS * TILE);
    for (int i = 0; i < CHILDREN; ++i) {
        int x = i % COLUMNS * TILE + 1, y = i / COLUMNS * TILE + 1;
        pixel(shot, x, y, i % 2 ? COLOR_RGBA(20, 180, 60, 255) : COLOR_RGBA(200, 40, 80, 255));
        CHECK(Panel_childElement(parent, i) == Panel_graphics(children[i]));
    }
    const int probes[] = {0, CHILDREN / 2, CHILDREN - 1};
    for (bytes j = 0; j < sizeof probes / sizeof probes[0]; ++j) {
        int i = probes[j];
        CHECK(Element_hit(Frame_element(frame), (f32) (i % COLUMNS * TILE + 1),
                          (f32) (i / COLUMNS * TILE + 1)) == Panel_graphics(children[i]));
        Panel_setBackground(children[i], COLOR_WHITE);
    }
    shot = Frame_capture(frame);
    for (bytes j = 0; j < sizeof probes / sizeof probes[0]; ++j) {
        int i = probes[j];
        pixel(shot, i % COLUMNS * TILE + 1, i / COLUMNS * TILE + 1, COLOR_WHITE);
    }
    Panel *last = children[CHILDREN - 1];
    CHECK(Panel_remove(last) == last && Panel_parent(last) == nullptr);
    CHECK(Panel_childCount(parent) == CHILDREN - 1);
    CHECK(Panel_add(parent, last, 0) == last);
    CHECK(Panel_childElement(parent, 0) == Panel_graphics(last));
    pixel(Frame_capture(frame), COLUMNS * TILE - 1, CHILDREN / COLUMNS * TILE - 1, COLOR_WHITE);
    Frame_destroy(frame);
    free(children);
    CHECK(PropertyPool_live(PropertyPool_default()) == baseline);
    puts("ui_wide_tree_battle_test: PASS (50,000 visible children, every tile checked)");
    return 0;
}

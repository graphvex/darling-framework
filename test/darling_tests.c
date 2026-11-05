// _main/darling_tests.c — the test harness.
//
//   ./tools/b.sh run darling_tests
//
// A main window full of test buttons. Clicking one opens that test in its own
// window, OWNED by the main window — so closing the main window closes them all.
// Everything is an Element; the pointer is registered in input/ (accessibility).

#include "frame/frame.h"
#include "ui/element.h"
#include "input/pointer.h"

typedef struct {
    const char *names;
    Color color;
    Frame *owner;
} TestCase;

typedef struct {
    const char *name;
    Color color;
} TestInfo;

// Every test opens the same shape of window for now; the point is the WINDOW
// TREE + the pointer, not the test content yet.
static void open_test(Panel *panel, void *ud) {
    (void)panel;
    TestCase *t = ud;
    Frame *w = Frame((*t).names, 560, 400);
    Frame_setOwner(w, (*t).owner);

    ElementDesc d = {0};
    d.width = 360; d.height = 220;
    d.anchor = PART_CENTER; d.pivot = PART_CENTER;
    d.radius = 56;
    d.background = (*t).color;
    d.shadow = COLOR_RGBA(0, 0, 0, 150);
    d.shadowX = 0; d.shadowY = 0; d.shadowBlur = 30;
    Frame_addPanel(w, &d);
    Frame_show(w);
}

static const TestInfo kInfos[] = {
    {"panel test", COLOR_RGBA(233, 128, 128, 255)},
    {"corner test", COLOR_RGBA(233, 186, 110, 255)},
    {"shadow test", COLOR_RGBA(210, 220, 120, 255)},
    {"color test", COLOR_RGBA(120, 200, 150, 255)},
    {"text test", COLOR_RGBA(120, 190, 220, 255)},
    {"label test", COLOR_RGBA(140, 160, 230, 255)},
    {"scrollpanel test", COLOR_RGBA(170, 140, 220, 255)},
    {"button test", COLOR_RGBA(220, 130, 200, 255)},
};
static const int kCount = (int)(sizeof kInfos / sizeof kInfos[0]);
static TestCase kCases[sizeof kInfos / sizeof kInfos[0]];

#define DARLING_TEST_HAS_FRAMES
#include "test_application.h"
// Builds the launcher window and attaches a button for each test entry.
int main(void) {
    Frame *m = Frame("darling — tests", 460, 700);

    for (int i = 0; i < kCount; i++) {
        kCases[i] = (TestCase){kInfos[i].name, kInfos[i].color, m};
        ElementDesc d = {0};
        d.width = 380; d.height = 62;
        d.anchor = PART_TOP_CENTER; d.pivot = PART_TOP_CENTER;
        d.offsetX = 0; d.offsetY = 24 + i * 78;
        d.radius = 16;
        d.background = kInfos[i].color;
        d.shadow = COLOR_RGBA(0, 0, 0, 120);
        d.shadowX = 0; d.shadowY = 0; d.shadowBlur = 12;
        Pointer_addButton(m, &d, open_test, &kCases[i]);
    }

    Pointer_track(m);   // optional: route the real mouse into the pointer actions

    Frame_savePNG(m, "/tmp/darling_tests.png");
    Darling_testKeepOpen(); // Application_start waits for ALL attached windows.
    return 0;
}

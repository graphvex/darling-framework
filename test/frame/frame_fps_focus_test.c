#include <stdio.h>
#include "frame/frame.h"
#include "input/focus.h"
#define DARLING_TEST_HAS_FRAMES
#include "test_application.h"

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)
static uint64_t now;
// Returns the test-controlled time used by the Surface cadence checks.
static uint64_t clockNow(void *userdata) { (void)userdata; return now; }
static int presentations;
static _Atomic unsigned sceneSteps;
// Advances a separate scene counter to prove render demand does not own scene work.
static void *sceneWorker(void *userdata) {
    (void)userdata;
    for (unsigned i = 0; i < 1000; i++) atomic_fetch_add(&sceneSteps, 1);
    return nullptr;
}
// Counts actual Surface presentation callbacks for cadence assertions.
static bool presented(Surface *surface, void *userdata) {
    (void)surface; (void)userdata; presentations++; return true;
}

// Checks focus-dependent frame caps and demand-driven presentation with a fake clock.
int main(void) {
    Frame *frame = Frame("FPS focus policy", 120, 80);
    Frame *other = Frame("other focus target", 120, 80);
    if (!frame || !other) return B_TEST_SKIP;
    Application *app = Application_current();
    CHECK(app && Frame_application(frame) == app);
    CHECK(Frame_getFPSCap(frame) == 60);
    CHECK(Frame_getFPSCapWhenFocusLost(frame) == 1);
    Frame_setFPSCap(frame, 120);
    Frame_setFPSCapWhenFocusGain(frame, -1);
    Frame_setFPSCapWhenFocusLost(frame, 1);
    Frame_setFPSCap(frame, 0); // invalid base leaves policy intact
    Frame_setFPSCapWhenFocusLost(frame, -2);
    CHECK(Frame_getFPSCap(frame) == 120);
    CHECK(Frame_getFPSCapWhenFocusLost(frame) == 1);
    Surface *surface = Frame_surface(frame);
    Surface_setClock(surface, clockNow, nullptr);
    Surface_onPresent(surface, presented, nullptr); // instrument actual R3 submission seam
    now = 1000000000ULL;

    // Native focus request + mirrored focus ID: no sleeps or display-Hz assumptions.
    Window_focus(Frame_window(frame));
    CHECK(Window_isFocused(Frame_window(frame)));
    CHECK(Frame_getEffectiveFPSCap(frame) == -1);
    Application_poll(app);
    int before = presentations;
    Frame_render(frame); Frame_render(frame); Frame_render(frame);
    CHECK(presentations == before + 3); // uncapped, even at the same clock time

    Window_focus(Frame_window(other));
    CHECK(!Window_isFocused(Frame_window(frame)));
    CHECK(Frame_getEffectiveFPSCap(frame) == 1);
    Application_poll(app);
    before = presentations;
    pthread_t scene;
    int sceneStarted = pthread_create(&scene, nullptr, sceneWorker, nullptr);
    CHECK(sceneStarted == 0);
    for (int i = 0; i < 100; i++) Frame_render(frame); // demand storm, not keep-alive
    if (sceneStarted == 0) { pthread_join(scene, nullptr); CHECK(atomic_load(&sceneSteps) == 1000); }
    CHECK(presentations == before);
    now += 999999999ULL; Surface_poll(surface);
    CHECK(presentations == before);
    now++; Surface_poll(surface);
    CHECK(presentations == before + 1);
    Surface_poll(surface); CHECK(presentations == before + 1); // no demand = no repaint

    Frame_setFPSCapWhenFocusGain(frame, 0); // inherit 120
    Window_focus(Frame_window(frame)); Application_poll(app);
    CHECK(Frame_getEffectiveFPSCap(frame) == 120);
    before = presentations;
    Frame_render(frame);
    now += 8333333ULL; Surface_poll(surface); CHECK(presentations == before);
    now++; Surface_poll(surface); CHECK(presentations == before + 1);

    // Focus policy changes do not execute/re-time scene work; publication count
    // is independent of the number of invalidations submitted above.
    CHECK(Surface_getPresentCount(surface) == (uint64_t)presentations);
    Frame_destroy(other); Frame_destroy(frame);
    puts("frame_fps_focus_test: focus selection, uncapped/1/120 FPS ceilings and demand coalescing");
    return failures ? 1 : 0;
}

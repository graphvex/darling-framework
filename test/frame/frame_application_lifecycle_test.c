#include <stdio.h>
#include <stdatomic.h>
#include <pthread.h>
#include "frame/frame.h"
#include "test_support.h"

static _Atomic int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)
typedef struct State {
    Frame *first, *second;
    pthread_t owner;
    _Atomic bool returned;
    int stage;
    uint32_t polls;
} State;

// Marks that the worker start callback returned without closing the app.
static void startup(Application *app, void *userdata) {
    State *state = userdata;
    (void)app;
    CHECK(!pthread_equal((*state).owner, pthread_self()));
    atomic_store(&(*state).returned, true);
    // Returning from startup MUST NOT close either window or stop the app.
}

// Drives owner-thread assertions for closed, hidden, and remaining windows.
static void service(Application *app, void *userdata) {
    State *state = userdata;
    CHECK(pthread_equal((*state).owner, pthread_self()));
    if (++(*state).polls > 1000) { failures++; Application_close(app); return; }
    if (!atomic_load(&(*state).returned)) return;
    if ((*state).stage == 0) {
        CHECK(Application_isRunning(app));
        CHECK(!Application_begin(app)); // concurrent/reentrant starts refused
        CHECK(!Application_addStartEvent(app, startup, state));
        CHECK(!Application_isFinished(app));
        Window_close(Frame_window((*state).first));
        Window_hide(Frame_window((*state).second));
        CHECK(!Window_shouldClose(Frame_window((*state).second)));
        CHECK(!Application_isFinished(app)); // hidden is STILL alive
        (*state).stage++;
    } else {
        CHECK(Application_isRunning(app)); // one CLOSED + one HIDDEN is not done
        Window_close(Frame_window((*state).second));
        CHECK(Application_isFinished(app));
        (*state).stage++;
    }
}

// Requests idempotent application closure from the start worker.
static void closeFromWorker(Application *app, void *userdata) {
    State *state = userdata;
    CHECK(!pthread_equal((*state).owner, pthread_self()));
    Application_close(app); Application_close(app); // idempotent, owner closes natives
}

// Closes the app on its owner thread and marks the callback stage complete.
static void closeOnOwner(Application *app, void *userdata) {
    State *state = userdata;
    CHECK(pthread_equal((*state).owner, pthread_self()));
    Application_close(app);
    (*state).stage++;
}
// Tests that an admitted owner callback completes before later calls are refused.
static void invokeThenClose(Application *app, void *userdata) {
    State *state = userdata;
    CHECK(Application_invoke(app, closeOnOwner, state));
    CHECK((*state).stage == 1); // admitted callback completed despite close
    CHECK(!Application_invoke(app, closeOnOwner, state)); // no post-close admissions
}

// Exercises worker startup, window-count lifetime, close-all, and owner invocation.
int main(void) {
    Application *app = Application("multiwindow lifetime test");
    State state = { .owner = pthread_self() };
    state.first = Frame("first window", 120, 80);
    state.second = Frame("second window", 120, 80);
    if (!app || !state.first || !state.second) {
        Frame_destroy(state.first); Frame_destroy(state.second); Application_free(app); return B_TEST_SKIP;
    }
    CHECK(Frame_attachApplication(state.first, app));
    CHECK(Frame_attachApplication(state.second, app));
    CHECK(Application_addStartEvent(app, startup, &state));
    CHECK(!Application_addStartEvent(app, nullptr, &state));
    CHECK(Application_addPollEvent(app, service, &state));
    Application_start(app); // no test keep-alive loop
    CHECK(state.stage == 2);
    CHECK(!Application_isRunning(app));
    CHECK(Window_shouldClose(Frame_window(state.first)) && Window_shouldClose(Frame_window(state.second)));
    Frame_destroyApplicationFrames(app);
    CHECK(Application_getWindowCount(app) == 0);
    Application_free(app);

    app = Application("worker close-all test");
    state.first = Frame("close-all first", 120, 80);
    state.second = Frame("close-all second", 120, 80);
    CHECK(Frame_attachApplication(state.first, app)); CHECK(Frame_attachApplication(state.second, app));
    CHECK(Application_addStartEvent(app, closeFromWorker, &state));
    Application_start(app);
    CHECK(Window_shouldClose(Frame_window(state.first)) && Window_shouldClose(Frame_window(state.second)));
    CHECK(!Application_isRunning(app));
    Frame_destroyApplicationFrames(app); Application_free(app);

    app = Application("close while invoking test");
    state.stage = 0;
    state.first = Frame("invoke owner window", 120, 80);
    CHECK(Frame_attachApplication(state.first, app));
    CHECK(Application_addStartEvent(app, invokeThenClose, &state));
    Application_start(app);
    CHECK(state.stage == 1 && !Application_isRunning(app));
    Frame_destroyApplicationFrames(app); Application_free(app);
    puts("frame_application_lifecycle_test: worker startup, callback-return, all-window closure, hidden-window keepalive and worker close-all");
    return failures ? 1 : 0;
}

#include "vex.h"
#include <stdio.h>
#include <pthread.h>
#include "frame/frame.h"
#include "test_support.h"

static _Atomic i32 failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL %d: %s\n", __LINE__, #c); failures++; } } while (0)
typedef struct State {
    Frame *first, *second;
    pthread_t owner;
    _Atomic bool returned;
    i32 stage;
    u32 polls;
} State;

;;GETTER
static Frame *State_first(const State *s) { return (*s).first; }
;;GETTER
static Frame *State_second(const State *s) { return (*s).second; }
;;GETTER
static pthread_t State_owner(const State *s) { return (*s).owner; }
;;GETTER
static _Atomic bool *State_returned(State *s) { return &(*s).returned; }
;;GETTER
static i32 State_stage(const State *s) { return (*s).stage; }
;;SETTER
static fn State_setStage(State *s, i32 stage) { (*s).stage = stage; }
;;GETTER
static u32 State_polls(const State *s) { return (*s).polls; }
;;SETTER
static fn State_setPolls(State *s, u32 polls) { (*s).polls = polls; }

// Marks that the worker start callback returned without closing the app.
static fn startup(Application *app, any userdata) {
    State *state = userdata;
    (discard)app;
    CHECK(!pthread_equal(State_owner(state), pthread_self()));
    atomic_store(State_returned(state), true);
    // Returning from startup MUST NOT close either window or stop the app.
}

// Drives owner-thread assertions for closed, hidden, and remaining windows.
static fn service(Application *app, any userdata) {
    State *state = userdata;
    CHECK(pthread_equal(State_owner(state), pthread_self()));
    State_setPolls(state, State_polls(state) + 1);
    if (State_polls(state) > 1000) { failures++; Application_close(app); return; }
    if (!atomic_load(State_returned(state))) return;
    if (State_stage(state) == 0) {
        CHECK(Application_isRunning(app));
        CHECK(!Application_begin(app)); // concurrent/reentrant starts refused
        CHECK(!Application_addStartEvent(app, startup, state));
        CHECK(!Application_isFinished(app));
        Window_close(Frame_window(State_first(state)));
        Window_hide(Frame_window(State_second(state)));
        CHECK(!Window_shouldClose(Frame_window(State_second(state))));
        CHECK(!Application_isFinished(app)); // hidden is STILL alive
        State_setStage(state, State_stage(state) + 1);
    } else {
        CHECK(Application_isRunning(app)); // one CLOSED + one HIDDEN is not done
        Window_close(Frame_window(State_second(state)));
        CHECK(Application_isFinished(app));
        State_setStage(state, State_stage(state) + 1);
    }
}

// Requests idempotent application closure from the start worker.
static fn closeFromWorker(Application *app, any userdata) {
    State *state = userdata;
    CHECK(!pthread_equal(State_owner(state), pthread_self()));
    Application_close(app); Application_close(app); // idempotent, owner closes natives
}

// Closes the app on its owner thread and marks the callback stage complete.
static fn closeOnOwner(Application *app, any userdata) {
    State *state = userdata;
    CHECK(pthread_equal(State_owner(state), pthread_self()));
    Application_close(app);
    State_setStage(state, State_stage(state) + 1);
}
// Tests that an admitted owner callback completes before later calls are refused.
static fn invokeThenClose(Application *app, any userdata) {
    State *state = userdata;
    CHECK(Application_invoke(app, closeOnOwner, state));
    CHECK(State_stage(state) == 1); // admitted callback completed despite close
    CHECK(!Application_invoke(app, closeOnOwner, state)); // no post-close admissions
}

// Exercises worker startup, window-count lifetime, close-all, and owner invocation.
i32 main() {
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

#ifndef DARLING_TEST_APPLICATION_H
#define DARLING_TEST_APPLICATION_H

// Every Darling executable enters Application_start. Assertions/native graphics
// stay on the owner thread; the startup worker marshals them with invoke.
// Primitive tests receive a real starter window; Frame tests auto-attach theirs.

#include "vex.h"
#include <stdio.h>
#include "frame/frame.h"

#ifdef DARLING_TEST_WITH_ARGS
static i32 darling_test_body(i32 argc, char **argv);
#else
static i32 darling_test_body();
#endif

typedef struct DarlingTestState {
    i32 argc;
    char **argv;
    i32 result;
    bool keepOpen;
} DarlingTestState;
static DarlingTestState darling_test_state;

;;GETTER
static i32 DarlingTestState_argc(const DarlingTestState *s) { return (*s).argc; }
;;GETTER
static char **DarlingTestState_argv(const DarlingTestState *s) { return (*s).argv; }
;;GETTER
static i32 DarlingTestState_result(const DarlingTestState *s) { return (*s).result; }
;;SETTER
static fn DarlingTestState_setResult(DarlingTestState *s, i32 res) { (*s).result = res; }
;;GETTER
static bool DarlingTestState_keepOpen(const DarlingTestState *s) { return (*s).keepOpen; }

// Interactive bodies return to Application_start rather than running a loop.
static inline fn Darling_testKeepOpen() { darling_test_state.keepOpen = true; }

static fn darling_test_owner(Application *application, any userdata) {
    (discard)application;
    DarlingTestState *state = userdata;
#ifdef DARLING_TEST_WITH_ARGS
    DarlingTestState_setResult(state, darling_test_body(DarlingTestState_argc(state), DarlingTestState_argv(state)));
#else
    DarlingTestState_setResult(state, darling_test_body());
#endif
}

static fn darling_test_start(Application *application, any userdata) {
    DarlingTestState *state = userdata;
    if (!Application_invoke(application, darling_test_owner, state)) DarlingTestState_setResult(state, 1);
    if (!DarlingTestState_keepOpen(state) || DarlingTestState_result(state) != 0) Application_close(application);
}

i32 main(i32 argc, char **argv) {
    Application *application = Application(argv[0]);
    if (!application) return 1;
    Window *starter = nullptr;
#ifndef DARLING_TEST_HAS_FRAMES
    starter = Window_create("Darling test starter", 240, 120);
    if (!starter) { Application_free(application); return B_TEST_SKIP; }
    if (!Application_addWindow(application, starter)) {
        Window_destroy(starter); Application_free(application); return 1;
    }
#endif
    darling_test_state = (DarlingTestState){ .argc = argc, .argv = argv, .result = 1 };
    if (!Application_addStartEvent(application, darling_test_start, &darling_test_state)) {
        if (starter) Window_destroy(starter);
        Application_free(application); return 1;
    }
    Application_start(application);
    Frame_destroyApplicationFrames(application);
    if (starter) { Application_removeWindow(application, starter); Window_destroy(starter); }
    Application_free(application);
    return darling_test_state.result;
}

// Preserve ordinary main signatures in owner tests and b.sh's source discovery.
#define main darling_test_body
#endif

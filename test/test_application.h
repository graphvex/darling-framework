#ifndef DARLING_TEST_APPLICATION_H
#define DARLING_TEST_APPLICATION_H

// Every Darling executable enters Application_start. Assertions/native graphics
// stay on the owner thread; the startup worker marshals them with invoke.
// Primitive tests receive a real starter window; Frame tests auto-attach theirs.
#include <stdio.h>
#include "frame/frame.h"
#include "test_support.h"

#ifdef DARLING_TEST_WITH_ARGS
static int darling_test_body(int argc, char **argv);
#else
static int darling_test_body(void);
#endif

typedef struct DarlingTestState {
    int argc;
    char **argv;
    int result;
    bool keepOpen;
} DarlingTestState;
static DarlingTestState darling_test_state;

// Interactive bodies return to Application_start rather than running a loop.
static inline void Darling_testKeepOpen(void) { darling_test_state.keepOpen = true; }

static void darling_test_owner(Application *application, void *userdata) {
    (void)application;
    DarlingTestState *state = userdata;
#ifdef DARLING_TEST_WITH_ARGS
    (*state).result = darling_test_body((*state).argc, (*state).argv);
#else
    (*state).result = darling_test_body();
#endif
}

static void darling_test_start(Application *application, void *userdata) {
    DarlingTestState *state = userdata;
    if (!Application_invoke(application, darling_test_owner, state)) (*state).result = 1;
    if (!(*state).keepOpen || (*state).result != 0) Application_close(application);
}

int main(int argc, char **argv) {
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

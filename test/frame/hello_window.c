// _main/hello_window.c — a MINIMAL sample app proving that `b.sh run` discovers it,
// builds it, wraps it in a macOS .app, ad-hoc codesigns it, and launches it.
//
// Drop any _main/<name>.c with a main() and it becomes an app target the same
// way. This file is just a demo — delete it freely.
//
//   ./tools/b.sh run hello_window

#include <stdio.h>

#include "frame/frame.h"
#define DARLING_TEST_HAS_FRAMES
#include "test_application.h"

// Opens the sample Frame and keeps its Application alive until window closure.
int main(void) {
    Frame *frame = Frame("hello from b.sh", 900, 600);
    if (!frame) return B_TEST_SKIP;
    Darling_testKeepOpen();
    return 0;
}

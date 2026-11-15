// Naked Frame: transparent titlebar, no title, native traffic lights retained.
// Automated state/content/resize checks; --interactive allows native dragging.

#include "vex.h"
#include "frame_chrome_lab.h"

// Runs the shared chrome lab for a naked Frame with native traffic lights.
i32 main(i32 argc, char **argv) {
    return frameChromeLab(argc, argv, "naked frame", WINDOW_UNDECORATED_NAKED, false);
}

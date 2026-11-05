// Naked Frame: transparent titlebar, no title, native traffic lights retained.
// Automated state/content/resize checks; --interactive allows native dragging.
#include "frame_chrome_lab.h"

// Runs the shared chrome lab for a naked Frame with native traffic lights.
int main(int argc, char **argv) {
    return frameChromeLab(argc, argv, "naked frame", WINDOW_UNDECORATED_NAKED, false);
}

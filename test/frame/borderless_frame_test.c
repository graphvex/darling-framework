// Borderless Frame: neither titlebar nor native traffic lights.
// --interactive: move by background; the red square is an explicit exit.
#include "frame_chrome_lab.h"

// Runs the shared chrome lab for an undecorated, borderless Frame.
int main(int argc, char **argv) {
    return frameChromeLab(argc, argv, "borderless frame", WINDOW_UNDECORATED_BORDERLESS, false);
}

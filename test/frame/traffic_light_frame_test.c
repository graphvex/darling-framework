// Decorated Frame: red/yellow/green visibility, custom header and floating mode.
// Content capture cannot verify native chrome pixels; --interactive is user viewing.
#include "frame_chrome_lab.h"

// Runs the decorated Frame chrome lab, including traffic-light controls.
int main(int argc, char **argv) {
    return frameChromeLab(argc, argv, "traffic light frame", WINDOW_DECORATED, true);
}

// Decorated Frame: red/yellow/green visibility, custom header and f32ing mode.
// Content capture cannot verify native chrome pixels; --interactive is user viewing.

#include "vex.h"
#include "frame_chrome_lab.h"

// Runs the decorated Frame chrome lab, including traffic-light controls.
i32 main(i32 argc, char **argv) {
    return frameChromeLab(argc, argv, "traffic light frame", WINDOW_DECORATED, true);
}

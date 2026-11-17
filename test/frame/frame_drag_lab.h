#ifndef DARLING_TEST_FRAME_DRAG_LAB_H
#define DARLING_TEST_FRAME_DRAG_LAB_H

#include "vex.h"
#include "frame/frame.h"
#include "c23/event_invoke.h"

// Demo-local drag controller. The root receives bubbled events so moving off
// the selected panel (but still inside the frame) does not lose the drag.
// This is not toolkit pointer capture outside the native window.
typedef struct FrameDragLab {
    Frame *frame;
    Panel **panels;
    i32 count, selected;
    f32 x[96], y[96], startX, startY, baseX, baseY;
} FrameDragLab;

;;GETTER
static Frame *FrameDragLab_frame(const FrameDragLab *d) { return (*d).frame; }
;;GETTER
static Panel **FrameDragLab_panels(const FrameDragLab *d) { return (*d).panels; }
;;GETTER
static i32 FrameDragLab_count(const FrameDragLab *d) { return (*d).count; }
;;GETTER
static i32 FrameDragLab_selected(const FrameDragLab *d) { return (*d).selected; }
;;SETTER
static fn FrameDragLab_setSelected(FrameDragLab *d, i32 sel) { (*d).selected = sel; }
;;GETTER
static f32 FrameDragLab_xAt(const FrameDragLab *d, i32 idx) { return (*d).x[idx]; }
;;SETTER
static fn FrameDragLab_setXAt(FrameDragLab *d, i32 idx, f32 x) { (*d).x[idx] = x; }
;;GETTER
static f32 FrameDragLab_yAt(const FrameDragLab *d, i32 idx) { return (*d).y[idx]; }
;;SETTER
static fn FrameDragLab_setYAt(FrameDragLab *d, i32 idx, f32 y) { (*d).y[idx] = y; }
;;GETTER
static f32 FrameDragLab_startX(const FrameDragLab *d) { return (*d).startX; }
;;SETTER
static fn FrameDragLab_setStartX(FrameDragLab *d, f32 x) { (*d).startX = x; }
;;GETTER
static f32 FrameDragLab_startY(const FrameDragLab *d) { return (*d).startY; }
;;SETTER
static fn FrameDragLab_setStartY(FrameDragLab *d, f32 y) { (*d).startY = y; }
;;GETTER
static f32 FrameDragLab_baseX(const FrameDragLab *d) { return (*d).baseX; }
;;SETTER
static fn FrameDragLab_setBaseX(FrameDragLab *d, f32 x) { (*d).baseX = x; }
;;GETTER
static f32 FrameDragLab_baseY(const FrameDragLab *d) { return (*d).baseY; }
;;SETTER
static fn FrameDragLab_setBaseY(FrameDragLab *d, f32 y) { (*d).baseY = y; }

;;GETTER
static f32 MousePayload_x(const Mouse *m) { return (*m).x; }
;;GETTER
static f32 MousePayload_y(const Mouse *m) { return (*m).y; }

// Selects the hit panel and stores its starting pointer and offset coordinates.
static fn frameDragDown(Element *element, const Mouse *mouse, any userdata) {
    FrameDragLab *drag = userdata;
    FrameDragLab_setSelected(drag, -1);
    Element *hit = Element_hit(element, MousePayload_x(mouse), MousePayload_y(mouse));
    for (i32 i = 0; i < FrameDragLab_count(drag); ++i) {
        if (Panel_graphics(FrameDragLab_panels(drag)[i]) != hit) continue;
        FrameDragLab_setSelected(drag, i);
        FrameDragLab_setStartX(drag, MousePayload_x(mouse));
        FrameDragLab_setStartY(drag, MousePayload_y(mouse));
        FrameDragLab_setBaseX(drag, FrameDragLab_xAt(drag, i));
        FrameDragLab_setBaseY(drag, FrameDragLab_yAt(drag, i));
        break;
    }
}

// Moves the selected panel subtree by the pointer delta and invalidates its Frame.
static fn frameDragMove(Element *element, const Mouse *mouse, any userdata) {
    (discard)element;
    FrameDragLab *drag = userdata;
    i32 i = FrameDragLab_selected(drag);
    if (i < 0) return;
    FrameDragLab_setXAt(drag, i, FrameDragLab_baseX(drag) + MousePayload_x(mouse) - FrameDragLab_startX(drag));
    FrameDragLab_setYAt(drag, i, FrameDragLab_baseY(drag) + MousePayload_y(mouse) - FrameDragLab_startY(drag));
    Panel_setOffset(FrameDragLab_panels(drag)[i], FrameDragLab_xAt(drag, i), FrameDragLab_yAt(drag, i));
    Frame_invalidate(FrameDragLab_frame(drag));
}

// Clears the active panel selection when the drag ends.
static fn frameDragUp(Element *element, const Mouse *mouse, any userdata) {
    (discard)element; (discard)mouse;
    FrameDragLab *drag = userdata;
    FrameDragLab_setSelected(drag, -1);
}

// Registers the demo drag callbacks on the Frame's root element.
static fn frameDragAttach(FrameDragLab *drag) {
    FrameDragLab_setSelected(drag, -1);
    Element_addMouseEvent(Frame_element(FrameDragLab_frame(drag)), (MouseEvent){
        .onDown = frameDragDown, .onDrag = frameDragMove, .onUp = frameDragUp,
        .userdata = drag,
    });
}

#endif

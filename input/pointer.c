#include "vex.h"
#include "input/pointer.h"

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "input/mouse.h"   // vexspoke R2: window-scoped events + Mouse_x/y
#include "c23/event_invoke.h"   // Element_dispatchEvent + the Event carrier
#include "properties/set_cursor.h"


;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Pointer (input/pointer.c)
 * ============================================================================
 * The pointer bridge between the OS and the element tree. Handlers bind to
 * Panels (the wrappers are the interactables); a Cursor holds the per-root
 * press state so a press/release pair resolves against the same tree.
 *
 * Two paths feed the SAME tree: the synthetic actions (Pointer_press/release/
 * click/hold/drag) and the optional real-mouse bridge (Pointer_track). The OS
 * bridge installs a vexspoke MouseHandler on the Frame's window; every event is
 * BOTH the legacy click binding and a per-element event dispatched through
 * c23/event_invoke (down/up/move/drag/scroll/zoom). Registrations are owned by
 * this file and dropped on Frame close via the close hook, so frame.c never
 * learns about input.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Pointer (input/pointer.c)
 * ============================================================================
 * Pointer actions + optional OS mouse bridge. File-static registries, no owned
 * class instance.
 *
 * FILE-STATIC STATE:
 * ----------------------------------------------------------------------------
 *   Cursor  **s_cursors;   // one per root Element (press state + tracked bridge)
 *   i32       s_curCount, s_curCap;
 *   Binding  *s_bindings;  // Panel click bindings keyed by the panel's Element
 *   i32       s_bindCount, s_bindCap;
 *
 * PRIVATE TYPES:
 * ----------------------------------------------------------------------------
 *   Binding : { Element *graphics; Panel *panel; PointerFn fn; any userdata; }
 *   Cursor  : { Element *root; Element *pressed; Frame *frame;
 *               MouseHandler handler; bool tracked; }
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   find_binding / cursor_for / cursor_open : registry lookups
 *   bound_at(root,x,y) : nearest bound element under the cursor
 *   track_dispatch / track_down|up|move|mod|dragf|scroll|zoom : OS bridge
 *   track_cursor : apply the hit node's inherited hover preference
 *   cleanup_on_close   : drop registrations when the owning Frame closes
 *
 * FUNCTION REGISTRY (exported by input/pointer.h):
 * ----------------------------------------------------------------------------
 * Actions:
 *   - Pointer_hover, Pointer_press, Pointer_release, Pointer_move,
 *     Pointer_click, Pointer_hold, Pointer_drag
 * Handlers:
 *   - Pointer_onClick, Pointer_addButton
 * OS bridge:
 *   - Pointer_track
 * ============================================================================
 */

typedef struct {
    Element *graphics;   // the panel's GraphicsPanel (the hit target)
    Panel *panel;        // the interactable wrapper (handed back on click)
    PointerFn fn;
    any userdata;
} Binding;

;;GETTER
static inline Element *Binding_graphics(const Binding *self) { return (*self).graphics; }
;;GETTER
static inline Panel *Binding_panel(const Binding *self) { return (*self).panel; }
;;SETTER
static inline fn Binding_setPanel(Binding *self, Panel *v) { (*self).panel = v; }
;;GETTER
static inline PointerFn Binding_fn(const Binding *self) { return (*self).fn; }
;;SETTER
static inline fn Binding_setFn(Binding *self, PointerFn v) { (*self).fn = v; }
;;GETTER
static inline any Binding_userdata(const Binding *self) { return (*self).userdata; }
;;SETTER
static inline fn Binding_setUserdata(Binding *self, any v) { (*self).userdata = v; }

typedef struct {
    Element *root;       // the top element (keyed here)
    Element *pressed;    // the bound element under the button
    Frame *frame;        // only when tracked (OS bridge)
    MouseHandler handler;
    bool tracked;
} Cursor;

;;GETTER
static inline Element *Cursor_root(const Cursor *self) { return (*self).root; }
;;SETTER
static inline fn Cursor_setRoot(Cursor *self, Element *v) { (*self).root = v; }
;;GETTER
static inline Element *Cursor_pressed(const Cursor *self) { return (*self).pressed; }
;;SETTER
static inline fn Cursor_setPressed(Cursor *self, Element *v) { (*self).pressed = v; }
;;GETTER
static inline Frame *Cursor_frame(const Cursor *self) { return (*self).frame; }
;;SETTER
static inline fn Cursor_setFrame(Cursor *self, Frame *v) { (*self).frame = v; }
;;GETTER
static inline MouseHandler *Cursor_handlerMut(Cursor *self) { return &(*self).handler; }
;;SETTER
static inline fn Cursor_setHandler(Cursor *self, MouseHandler v) { (*self).handler = v; }
;;GETTER
static inline bool Cursor_tracked(const Cursor *self) { return (*self).tracked; }
;;SETTER
static inline fn Cursor_setTracked(Cursor *self, bool v) { (*self).tracked = v; }

static Cursor **s_cursors = nullptr;
static i32 s_curCount = 0, s_curCap = 0;

static Binding *s_bindings = nullptr;
static i32 s_bindCount = 0, s_bindCap = 0;

// Finds the click binding associated with an Element, if registered.
static Binding *find_binding(Element *graphics) {
    for (i32 i = 0; i < s_bindCount; i++)
        if (s_bindings[i].graphics == graphics) return &s_bindings[i];
    return nullptr;
}

// Finds the cursor state record for an Element root.
static Cursor *cursor_for(Element *root) {
    for (i32 i = 0; i < s_curCount; i++)
        if (Cursor_root(s_cursors[i]) == root) return s_cursors[i];
    return nullptr;
}

// Returns existing cursor state for root or allocates and registers a record.
static Cursor *cursor_open(Element *root) {
    Cursor *c = cursor_for(root);
    if (c) return c;
    c = calloc(1, sizeof *c);
    if (!c) return nullptr;
    Cursor_setRoot(c, root);
    if (s_curCount == s_curCap) {
        s_curCap = s_curCap ? s_curCap * 2 : 4;
        s_cursors = realloc(s_cursors, (bytes)s_curCap * sizeof *s_cursors);
    }
    s_cursors[s_curCount++] = c;
    return c;
}

// the nearest BOUND element under the cursor (so a child of a button hits it)
static Binding *bound_at(Element *root, f32 x, f32 y) {
    Element *e = Element_hit(root, x, y);
    while (e && !find_binding(e)) e = Element_parent(e);
    return e ? find_binding(e) : nullptr;
}

// Pauses for a requested duration, resuming the remainder after an interrupt.
static fn sleep_seconds(d64 s) {
    if (s <= 0.0) return;
    struct timespec ts;
    ts.tv_sec = (time_t)s;
    ts.tv_nsec = (i64)((s - (d64)ts.tv_sec) * 1e9);
    while (nanosleep(&ts, &ts) == -1) { /* resume on signal */ }
}

// ── actions ─────────────────────────────────────────────────────────────────
// Returns the nearest bound Panel beneath the supplied root-space point.
Panel *Pointer_hover(Element *root, f32 x, f32 y) {
    if (!root) return nullptr;
    Binding *b = bound_at(Element_root(root), x, y);
    return b ? Binding_panel(b) : nullptr;
}

// Marks the bound panel under the point pressed and invalidates its root.
fn Pointer_press(Element *root, f32 x, f32 y) {
    if (!root) return;
    root = Element_root(root);
    Cursor *c = cursor_open(root);
    if (!c) return;
    Binding *b = bound_at(root, x, y);
    Cursor_setPressed(c, b ? Binding_graphics(b) : nullptr);
    if (Cursor_pressed(c)) Element_setPressed(Cursor_pressed(c), true);
    Frame_invalidateElement(root);
}

// Clears pressed state and invokes the click callback when released on the same binding.
fn Pointer_release(Element *root, f32 x, f32 y) {
    if (!root) return;
    root = Element_root(root);
    Cursor *c = cursor_open(root);
    if (!c) return;
    Binding *b = bound_at(root, x, y);
    if (b && Binding_graphics(b) == Cursor_pressed(c) && Binding_fn(b))
        Binding_fn(b)(Binding_panel(b), Binding_userdata(b));
    if (Cursor_pressed(c)) Element_setPressed(Cursor_pressed(c), false);
    Cursor_setPressed(c, nullptr);
    Frame_invalidateElement(root);
}

// Invalidates the root after pointer movement so hover state can be repainted.
fn Pointer_move(Element *root, f32 x, f32 y) {
    (discard)x; (discard)y;
    if (root) Frame_invalidateElement(Element_root(root));
}

// Synthesizes a press followed by a release at one point.
fn Pointer_click(Element *root, f32 x, f32 y) {
    Pointer_press(root, x, y);
    Pointer_release(root, x, y);
}

// Holds the pointer pressed for the requested duration, then releases it.
fn Pointer_hold(Element *root, f32 x, f32 y, d64 seconds) {
    Pointer_press(root, x, y);
    sleep_seconds(seconds);
    Pointer_release(root, x, y);
}

// Synthesizes a press, interpolated movement, and release along a drag path.
fn Pointer_drag(Element *root, f32 x, f32 y, f32 toX, f32 toY, d64 seconds) {
    if (!root) return;
    const i32 steps = 30;
    Pointer_press(root, x, y);
    for (i32 i = 1; i <= steps; i++) {
        f32 t = (f32)i / (f32)steps;
        sleep_seconds(seconds / steps);
        Pointer_move(root, x + (toX - x) * t, y + (toY - y) * t);
    }
    Pointer_release(root, toX, toY);
}

// ── handlers ────────────────────────────────────────────────────────────────
// Registers or replaces the click callback associated with a Panel's Element.
fn Pointer_onClick(Panel *panel, PointerFn fn, any userdata) {
    Element *graphics = Panel_graphics(panel);
    if (!panel || !graphics) return;
    Binding *b = find_binding(graphics);
    if (b) { Binding_setPanel(b, panel); Binding_setFn(b, fn); Binding_setUserdata(b, userdata); return; }
    if (s_bindCount == s_bindCap) {
        s_bindCap = s_bindCap ? s_bindCap * 2 : 16;
        s_bindings = realloc(s_bindings, (bytes)s_bindCap * sizeof *s_bindings);
    }
    s_bindings[s_bindCount++] = (Binding){graphics, panel, fn, userdata};
}

// Adds a Panel to the frame and registers its click callback, returning the Panel.
Panel *Pointer_addButton(Frame *frame, const ElementDesc *desc, PointerFn fn, any userdata) {
    Panel *p = Frame_addPanel(frame, desc);
    if (p) Pointer_onClick(p, fn, userdata);
    return p;
}

// ── optional OS bridge (feeds the actions above AND the event dispatcher) ───
// Every OS event is BOTH the legacy pointer action (click bindings) and a
// per-element event (Element_add<Kind>Event handlers), dispatched against the
// cursor's root and bubbled.
static fn track_dispatch(Cursor *c, i32 kind, f32 x, f32 y,
                           f32 dx, f32 dy, f32 magnitude, i32 key) {
    Event event = { .kind = kind, .x = x, .y = y, .dx = dx, .dy = dy,
                    .magnitude = magnitude, .key = key };
    Element_dispatchEvent(Cursor_root(c), &event);
}

// Updates the native window cursor to match the element under the pointer.
static fn track_cursor(Cursor *c, f32 x, f32 y) {
    Window *window = Frame_window(Cursor_frame(c));
    if (!window) return;
    WindowCursorType type = (WindowCursorType) Element_cursorAt(Cursor_root(c), x, y);
    if (Window_getCursorType(window) != type) Window_setCursorType(window, type);
}

// Bridges a native mouse-down into element events and the legacy press action.
static fn track_down(any self, i32 mouseEvent, u64 nanos) {
    (discard)nanos;
    Cursor *c = self;
    f32 x = (f32)Mouse_x(), y = (f32)Mouse_y();
    track_dispatch(c, EV_MOUSE_DOWN, x, y, 0, 0, 1.0f, Mouse_button(mouseEvent));
    Pointer_press(Cursor_root(c), x, y);
}
// Bridges a native mouse-up into element events and the legacy release action.
static fn track_up(any self, i32 mouseEvent, u64 nanos) {
    (discard)nanos;
    Cursor *c = self;
    f32 x = (f32)Mouse_x(), y = (f32)Mouse_y();
    track_dispatch(c, EV_MOUSE_UP, x, y, 0, 0, 1.0f, Mouse_button(mouseEvent));
    Pointer_release(Cursor_root(c), x, y);
}
// Bridges native pointer motion to event dispatch, repaint, and cursor tracking.
static fn track_move(any self, d64 x, d64 y) {
    Cursor *c = self;
    track_dispatch(c, EV_MOUSE_MOVE, (f32)x, (f32)y, 0, 0, 1.0f, 0);
    Pointer_move(Cursor_root(c), (f32)x, (f32)y);
    track_cursor(c, (f32)x, (f32)y);
}
// Accepts repeat callbacks for the adapter interface; repeat events need no action.
static fn track_none0(any self, i32 ev, u64 t) { (discard)self; (discard)ev; (discard)t; }
// Bridges native movement deltas to a mouse-move event at the current pointer.
static fn track_mod(any self, d64 dx, d64 dy) {
    Cursor *c = self;
    track_dispatch(c, EV_MOUSE_MOVE, (f32)Mouse_x(), (f32)Mouse_y(),
                   (f32)dx, (f32)dy, 1.0f, 0);
}
// Bridges native drag motion to event dispatch and cursor tracking.
static fn track_dragf(any self, i32 ev, d64 x, d64 y) {
    Cursor *c = self;
    track_dispatch(c, EV_MOUSE_DRAG, (f32)x, (f32)y, 0, 0, 1.0f, Mouse_button(ev));
    track_cursor(c, (f32)x, (f32)y);
}
// Bridges native scroll deltas to the element event dispatcher.
static fn track_scroll(any self, d64 dx, d64 dy) {
    Cursor *c = self;
    track_dispatch(c, EV_SCROLL, (f32)Mouse_x(), (f32)Mouse_y(),
                   (f32)dx, (f32)dy, 1.0f, 0);
}
// Bridges native magnification gestures to the element event dispatcher.
static fn track_zoom(any self, d64 magnification) {
    Cursor *c = self;
    track_dispatch(c, EV_ZOOM, (f32)Mouse_x(), (f32)Mouse_y(),
                   0, 0, (f32)magnification, 0);
}

// Releases cursor and click-binding records belonging to a closing frame root.
static fn cleanup_on_close(Frame *frame, any userdata) {
    (discard)userdata;
    Element *root = Frame_element(frame);
    for (i32 i = 0; i < s_curCount; i++) {
        if (Cursor_root(s_cursors[i]) != root) continue;
        free(s_cursors[i]);
        memmove(&s_cursors[i], &s_cursors[i + 1],
                (bytes)(s_curCount - i - 1) * sizeof *s_cursors);
        s_curCount--;
        break;
    }
    for (i32 i = 0; i < s_bindCount; ) {
        Element *g = s_bindings[i].graphics;
        Element *r = g ? Element_root(g) : nullptr;
        if (r != root) { i++; continue; }
        memmove(&s_bindings[i], &s_bindings[i + 1],
                (bytes)(s_bindCount - i - 1) * sizeof *s_bindings);
        s_bindCount--;
    }
}

// Installs the native mouse adapter and close cleanup once for this frame.
fn Pointer_track(Frame *frame) {
    if (!frame) return;
    Element *root = Frame_element(frame);
    Cursor *c = cursor_open(root);
    if (!c || Cursor_tracked(c)) return;
    Cursor_setTracked(c, true);
    Cursor_setFrame(c, frame);
    MouseHandler handler = {
        .self = c,
        .onMouseDown = track_down,
        .onMouseUp = track_up,
        .onMouseMove = track_move,
        .onMouseMoveDelta = track_mod,
        .onMouseDrag = track_dragf,
        .onMouseScroll = track_scroll,
        .onMouseZoom = track_zoom,
        .onMouseRepeat = track_none0,
    };
    Cursor_setHandler(c, handler);
    Window_addMouseAdapter(Frame_window(frame), Cursor_handlerMut(c));
    Frame_onClose(frame, cleanup_on_close, nullptr);
}

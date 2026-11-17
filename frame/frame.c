#include "vex.h"
#include "frame/frame.h"
#include "frame/frame_internal.h"   // the seam properties/ calls back into

#include <stdlib.h>
#include <string.h>

#include "lang/image.h"                   // graphvex R3
#include "vulkan/surface.h"          // graphvex R3: the present seam
#include "vulkan/vulkan_backend.h"   // graphvex R3: the GPU backend
#include "input/pointer.h"           // automatic per-window hover bridge


;;INTENTION("Application owns the keep-alive lifetime. Frame focus selects a "
            "Surface presentation ceiling, never a scene update rate. Default "
            "60 is conservative across WindowServer displays; opt into 120 or "
            "higher when supported, or -1 to leave pacing to the compositor.")

;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Frame (frame/frame.c)
 * ============================================================================
 * The JFrame: a hotcwap window plus a tree of owned child Panels, GPU-rendered.
 * Frames form an ownership tree — closing an owner closes its children.
 *
 * frame.c is PURE WINDOW + LAYOUT + PAINT. It knows nothing about the mouse;
 * input lives in darling/input and uses the close hook for teardown.
 * Construction attaches that bridge once; cursor preferences work without
 * requiring applications to install mouse handlers themselves.
 * Revalidation follows Frame -> Surface -> content Board -> Element tree/paint: the Board
 * owns the content generation/callback, and GPU pixels stay on the borrowed
 * d64-buffered IOSurfaces, not a redundant CPU render target.
 *
 * Attach/detach and the revalidate entry live in properties/ (add, remove,
 * revalidate); this file exposes the small ownership seam they call
 * (frame_internal.h) and stays a widget: construction, teardown, window chrome,
 * the present path, and the runner.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: Frame (frame/frame.c)
 * ============================================================================
 * An OS window, its content Element, and owned child Panels.
 *
 * STRUCT FIELDS:
 * ----------------------------------------------------------------------------
 *   Window   *window;        // owned R1 window
 *   Color     background;    // paint clear color (not OS transparency)
 *   Element  *root;          // the content Element (owned)
 *   Panel   **panels;        // owned Panel wrappers, aligned with root children
 *   i32       count, cap;    // panels array
 *   Surface  *surface;       // owned present seam
 *   Board    *contentBoard;  // content revalidate step + generation (owned)
 *   bool      paintReady;    // content produced a frame; present may publish
 *   any surfaces[2];   // borrowed IOSurfaces (Apple zero-copy, d64 buf)
 *   i32       targets[2];    // graphvex surface slots for those IOSurfaces
 *   i32       front;         // which surface the layer shows
 *   bool      gpu;           // zero-copy path is live
 *   Image    *shot;          // capture readback buffer (GPU path, lazy)
 *   DisplayList *dl;         // reused paint list
 *   i32       lastW, lastH;  // the single size authority
 *   struct Frame *owner;     // owning frame (nullable)
 *   struct Frame **children; // owned child frames
 *   i32       childCount, childCap;
 *   bool      closed, dirty;
 *   FrameCloseFn *closeFns; any *closeUd; i32 closeCount, closeCap;
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   live_add / live_remove / child_add / child_remove / fire_close_hooks
 *   frame_on_resize / frame_gpu_open / frame_gpu_close / frame_gpu_reopen
 *   frame_on_present (host blit + GPU publish) / frame_on_revalidate
 *   frame_renderContent (paint the tree; set paintReady)
 *
 * FUNCTION REGISTRY (exported by frame/frame.h):
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - Frame_0 / Frame_1 / Frame_3, and the Frame(...) arity chooser;
 *     Frame_destroy, Frame_close
 * Window basics:
 *   - Frame_window, Frame_surface, Frame_contentBoard, Frame_setTitle,
 *     Frame_setBackground(+Color), Frame_background, Frame_setTransparent,
 *     Frame_setBlur
 * Liquid Glass (macOS): Frame_macOS_hasLiquidGlass / setLiquidGlass / getLiquidGlass
 * Children (read): Frame_element, Frame_count, Frame_panel
 * Layout:
 *   - Frame_root
 * Windows / ownership:
 *   - Frame_show, Frame_hide, Frame_isClosed, Frame_setOwner, Frame_owner
 * Lifecycle:
 *   - Frame_onClose, Frame_runAll, Frame_run
 * Paint / present:
 *   - Frame_paint, Frame_render, Frame_invalidate, Frame_invalidateElement
 * Screenshots:
 *   - Frame_active, Frame_capture, Frame_savePNG
 * Ownership / revalidate seam (frame/frame_internal.h, called by properties/):
 *   - Frame_ownPanel, Frame_clearPanels, Frame_prepareRevalidate,
 *     Frame_resizeTargets
 *
 * Operations that live OUTSIDE this widget (properties/):
 *   - Frame_add(...), Frame_addPanel(...), Frame_removePanels(...),
 *     Frame_revalidate(...), Frame_setSize(...)
 * ============================================================================
 */

struct Frame {
    Window *window;
    Color background;
    Element *root;       // the content element (the Frame's GraphicsPanel)
    Panel **panels;      // the Panel wrappers (interface), aligned with root's children
    i32 count, cap;
    Surface *surface;    // the present seam: capture target + host blit (fallback)
    Board *contentBoard; // content revalidator + generation; pixels stay on the present seam
    bool paintReady;     // only publish after the content callback produced a frame
    any surfaces[2];   // borrowed IOSurfaces (Apple zero-copy seam, d64 buffer)
    i32 targets[2];      // graphvex surface slots for those IOSurfaces
    i32 front;           // which surface the layer currently shows
    bool gpu;            // the zero-copy present path is live
    Image *shot;         // capture readback buffer (GPU path only, lazy)
    DisplayList *dl;     // reused paint list
    i32 lastW, lastH;    // the single size authority

    struct Frame *owner;
    struct Frame **children;
    i32 childCount, childCap;

    bool closed;
    bool dirty;          // needs a repaint

    FrameCloseFn *closeFns;
    any *closeUd;
    i32 closeCount, closeCap;
    Application *application; // borrowed; detach before either owner is freed
    i32 fpsCap, fpsFocusGain, fpsFocusLost;
};

;;SETTER
static inline fn Frame_setWindow(Frame *f, Window *v) { (*f).window = v; }
;;SETTER
static inline fn Frame_setBackgroundField(Frame *f, Color v) { (*f).background = v; }
;;GETTER
static inline Element *Frame_rootEl(const Frame *f) { return (*f).root; }
;;SETTER
static inline fn Frame_setRootEl(Frame *f, Element *v) { (*f).root = v; }
;;GETTER
static inline Panel **Frame_panels(const Frame *f) { return (*f).panels; }
;;SETTER
static inline fn Frame_setPanels(Frame *f, Panel **v) { (*f).panels = v; }
;;SETTER
static inline fn Frame_setCount(Frame *f, i32 v) { (*f).count = v; }
;;SETTER
static inline fn Frame_setSurface(Frame *f, Surface *v) { (*f).surface = v; }
;;SETTER
static inline fn Frame_setContentBoard(Frame *f, Board *v) { (*f).contentBoard = v; }
;;GETTER
static inline bool Frame_paintReady(const Frame *f) { return (*f).paintReady; }
;;SETTER
static inline fn Frame_setPaintReady(Frame *f, bool v) { (*f).paintReady = v; }
;;GETTER
static inline any Frame_surfaceSlot(const Frame *f, i32 i) { return (*f).surfaces[i]; }
;;SETTER
static inline fn Frame_setSurfaceSlot(Frame *f, i32 i, any v) { (*f).surfaces[i] = v; }
;;GETTER
static inline i32 Frame_target(const Frame *f, i32 i) { return (*f).targets[i]; }
;;SETTER
static inline fn Frame_setTarget(Frame *f, i32 i, i32 v) { (*f).targets[i] = v; }
;;GETTER
static inline i32 Frame_front(const Frame *f) { return (*f).front; }
;;SETTER
static inline fn Frame_setFront(Frame *f, i32 v) { (*f).front = v; }
;;GETTER
static inline bool Frame_gpu(const Frame *f) { return (*f).gpu; }
;;SETTER
static inline fn Frame_setGpu(Frame *f, bool v) { (*f).gpu = v; }
;;GETTER
static inline Image *Frame_shot(const Frame *f) { return (*f).shot; }
;;SETTER
static inline fn Frame_setShot(Frame *f, Image *v) { (*f).shot = v; }
;;GETTER
static inline DisplayList *Frame_dl(const Frame *f) { return (*f).dl; }
;;SETTER
static inline fn Frame_setDl(Frame *f, DisplayList *v) { (*f).dl = v; }
;;GETTER
static inline i32 Frame_lastW(const Frame *f) { return (*f).lastW; }
;;SETTER
static inline fn Frame_setLastW(Frame *f, i32 v) { (*f).lastW = v; }
;;GETTER
static inline i32 Frame_lastH(const Frame *f) { return (*f).lastH; }
;;SETTER
static inline fn Frame_setLastH(Frame *f, i32 v) { (*f).lastH = v; }
;;SETTER
static inline fn Frame_setOwnerField(Frame *f, struct Frame *v) { (*f).owner = v; }
;;GETTER
static inline struct Frame **Frame_children(const Frame *f) { return (*f).children; }
;;SETTER
static inline fn Frame_setChildren(Frame *f, struct Frame **v) { (*f).children = v; }
;;GETTER
static inline i32 Frame_childCount(const Frame *f) { return (*f).childCount; }
;;SETTER
static inline fn Frame_setChildCount(Frame *f, i32 v) { (*f).childCount = v; }
;;GETTER
static inline i32 Frame_childCap(const Frame *f) { return (*f).childCap; }
;;SETTER
static inline fn Frame_setChildCap(Frame *f, i32 v) { (*f).childCap = v; }
;;GETTER
static inline bool Frame_closed(const Frame *f) { return (*f).closed; }
;;SETTER
static inline fn Frame_setClosed(Frame *f, bool v) { (*f).closed = v; }
;;GETTER
static inline bool Frame_dirty(const Frame *f) { return (*f).dirty; }
;;SETTER
static inline fn Frame_setDirty(Frame *f, bool v) { (*f).dirty = v; }
;;GETTER
static inline FrameCloseFn *Frame_closeFns(const Frame *f) { return (*f).closeFns; }
;;SETTER
static inline fn Frame_setCloseFns(Frame *f, FrameCloseFn *v) { (*f).closeFns = v; }
;;GETTER
static inline any *Frame_closeUd(const Frame *f) { return (*f).closeUd; }
;;SETTER
static inline fn Frame_setCloseUd(Frame *f, any *v) { (*f).closeUd = v; }
;;GETTER
static inline i32 Frame_closeCount(const Frame *f) { return (*f).closeCount; }
;;SETTER
static inline fn Frame_setCloseCount(Frame *f, i32 v) { (*f).closeCount = v; }
;;GETTER
static inline i32 Frame_closeCap(const Frame *f) { return (*f).closeCap; }
;;SETTER
static inline fn Frame_setCloseCap(Frame *f, i32 v) { (*f).closeCap = v; }
;;SETTER
static inline fn Frame_setApplication(Frame *f, Application *v) { (*f).application = v; }
;;GETTER
static inline i32 Frame_fpsCap(const Frame *f) { return (*f).fpsCap; }
;;SETTER
static inline fn Frame_setFpsCapField(Frame *f, i32 v) { (*f).fpsCap = v; }
;;GETTER
static inline i32 Frame_fpsFocusGain(const Frame *f) { return (*f).fpsFocusGain; }
;;SETTER
static inline fn Frame_setFpsFocusGainField(Frame *f, i32 v) { (*f).fpsFocusGain = v; }
;;GETTER
static inline i32 Frame_fpsFocusLost(const Frame *f) { return (*f).fpsFocusLost; }
;;SETTER
static inline fn Frame_setFpsFocusLostField(Frame *f, i32 v) { (*f).fpsFocusLost = v; }

// ── the live-frame set (the runner + ownership) ─────────────────────────────
static Frame **s_live = nullptr;
static i32 s_liveCount = 0, s_liveCap = 0;
static Frame *s_active = nullptr;   // most recently created (CAPTURE)
static fn frame_pollApplication(Application *application, any userdata);

// Adds a live Frame to the process-local runner registry.
static fn live_add(Frame *f) {
    if (s_liveCount == s_liveCap) {
        s_liveCap = s_liveCap ? s_liveCap * 2 : 4;
        s_live = realloc(s_live, (bytes)s_liveCap * sizeof *s_live);
    }
    s_live[s_liveCount++] = f;
}

// Removes a Frame from the live runner registry if present.
static fn live_remove(Frame *f) {
    for (i32 i = 0; i < s_liveCount; i++) {
        if (s_live[i] != f) continue;
        memmove(&s_live[i], &s_live[i + 1], (bytes)(s_liveCount - i - 1) * sizeof *s_live);
        s_liveCount--;
        return;
    }
}

// Records a child Frame in its owner's close-order list.
static fn child_add(Frame *owner, Frame *child) {
    if (Frame_childCount(owner) == Frame_childCap(owner)) {
        i32 ncap = Frame_childCap(owner) ? Frame_childCap(owner) * 2 : 4;
        Frame_setChildCap(owner, ncap);
        Frame_setChildren(owner, realloc(Frame_children(owner), (bytes)ncap * sizeof *Frame_children(owner)));
    }
    Frame_children(owner)[Frame_childCount(owner)] = child;
    Frame_setChildCount(owner, Frame_childCount(owner) + 1);
}

// Removes a child Frame from its owner's close-order list.
static fn child_remove(Frame *owner, Frame *child) {
    for (i32 i = 0; i < Frame_childCount(owner); i++) {
        if (Frame_children(owner)[i] != child) continue;
        memmove(&Frame_children(owner)[i], &Frame_children(owner)[i + 1],
                (bytes)(Frame_childCount(owner) - i - 1) * sizeof *Frame_children(owner));
        Frame_setChildCount(owner, Frame_childCount(owner) - 1);
        return;
    }
}

// Runs registered close callbacks once, then clears the callback count.
static fn fire_close_hooks(Frame *f) {
    for (i32 i = 0; i < Frame_closeCount(f); i++) Frame_closeFns(f)[i](f, Frame_closeUd(f)[i]);
    Frame_setCloseCount(f, 0);
}

// ── construction ────────────────────────────────────────────────────────────
static fn frame_on_resize(any userdata) {
    Frame *f = (Frame*) userdata;
    Frame_setSize(f, Window_width(Frame_window(f)), Window_height(Frame_window(f)));
}

// The host blit (fallback path): hand the Surface's present Image to the R1
// window as an RGBA buffer. Used off-Apple or when the GPU seam is unavailable.
static fn frame_renderContent(Frame *frame);

// Revalidates the frame's element tree and renders after its Board changes.
static fn frame_on_revalidate(Board *board, any userdata) {
    (discard) board;
    Frame *frame = userdata;
    Element_revalidate(Frame_rootEl(frame));
    frame_renderContent(frame);
}

// Publishes the completed GPU surface or captured RGBA image to the host window.
static bool frame_on_present(Surface *surface, any userdata) {
    Frame *f = userdata;
    if (!f || !Frame_window(f) || !Frame_paintReady(f)) return false;
    Frame_setPaintReady(f, false);
    if (Frame_gpu(f)) {
        Window_presentSurface(Frame_window(f), Frame_surfaceSlot(f, Frame_front(f)));
        return true;
    }
    Image *img = Surface_presentImage(surface);
    if (!img) return false;
    Window_presentRGBA(Frame_window(f), Image_pixels(img), Image_stride(img),
                       (i32) Image_width(img), (i32) Image_height(img));
    return true;
}

// The zero-copy seam: two host IOSurfaces imported as GPU targets. The back one
// is rendered into, published to the layer, then swapped. Fails (returns false)
// off-Apple, when VK_EXT_metal_objects is absent, or on OOM — the Frame then
// falls back to the RGBA path.
static bool frame_gpu_open(Frame *f, i32 wpx, i32 hpx) {
    for (i32 i = 0; i < 2; i++)
        Frame_setSurfaceSlot(f, i, Window_createPresentSurface(Frame_window(f), wpx, hpx));
    if (!Frame_surfaceSlot(f, 0) || !Frame_surfaceSlot(f, 1)) return false;
    for (i32 i = 0; i < 2; i++) {
        Frame_setTarget(f, i, VulkanBackend_addSurface(Frame_surfaceSlot(f, i), (u32)wpx, (u32)hpx));
        if (Frame_target(f, i) < 0) return false;
    }
    Frame_setFront(f, 0);
    Frame_setGpu(f, true);
    return true;
}

// Tear the GPU seam down (idempotent; safe to call after a partial open).
static fn frame_gpu_close(Frame *f) {
    for (i32 i = 0; i < 2; i++) {
        if (Frame_target(f, i) >= 0) VulkanBackend_removeSurface(Frame_target(f, i));
        Frame_setTarget(f, i, -1);
        if (Frame_surfaceSlot(f, i)) Window_destroyPresentSurface(Frame_window(f), Frame_surfaceSlot(f, i));
        Frame_setSurfaceSlot(f, i, nullptr);
    }
    Frame_setGpu(f, false);
}

// Resize = an IOSurface cannot grow, so rebuild both at the new extent.
static fn frame_gpu_reopen(Frame *f, i32 wpx, i32 hpx) {
    frame_gpu_close(f);
    if (!frame_gpu_open(f, wpx, hpx))
        frame_gpu_close(f);   // leave gpu false; the RGBA fallback carries on
}

// Creates a default titled frame at the default pixel dimensions.
Frame *Frame_0() { return Frame_3("darling", 800, 600); }
// Creates a frame with the supplied title and default pixel dimensions.
Frame *Frame_1(const char *title) { return Frame_3(title, 800, 600); }

// Creates a window-backed Frame, preferring the GPU surface seam with RGBA fallback.
Frame *Frame_3(const char *title, i32 widthPx, i32 heightPx) {
    Graphics_register(VulkanBackend_row());
    Graphics_use(BACKEND_VULKAN);

    i32 wpx = widthPx > 0 ? widthPx : 800;
    i32 hpx = heightPx > 0 ? heightPx : 600;
    Frame *f = calloc(1, sizeof *f);
    if (!f) return nullptr;
    Frame_setTarget(f, 0, -1);
    Frame_setTarget(f, 1, -1);
    Frame_setFpsCapField(f, 60);
    Frame_setFpsFocusLostField(f, 1);
    Frame_setWindow(f, Window_create(title ? title : "darling", wpx, hpx));
    if (!Frame_window(f)) { free(f); return nullptr; }
    // The present seam: its retained Image is the capture target, and its host
    // blit publishes to the window. The borrowed native is the content view.
    Frame_setSurface(f, Surface(Window_contentView(Frame_window(f)), (u32)wpx, (u32)hpx));
    if (!Frame_surface(f)) { Window_destroy(Frame_window(f)); free(f); return nullptr; }
    Surface_onPresent(Frame_surface(f), frame_on_present, f);
    // Prefer the zero-copy GPU seam; it falls back silently when unavailable.
    if (!frame_gpu_open(f, wpx, hpx)) frame_gpu_close(f);
    // OPAQUE by default. Background alpha controls paint only; OS window
    // transparency is independent and requires Frame_setTransparent(true).
    Frame_setBackgroundField(f, COLOR_RGBA(18, 20, 28, 255));
    Frame_setRootEl(f, Element());   // the window's content element (transparent)
    Element_setBackground(Frame_rootEl(f), COLOR_CLEAR);
    Element_setSize(Frame_rootEl(f), (f32)Window_width(Frame_window(f)), (f32)Window_height(Frame_window(f)));
    Frame_setDl(f, DisplayList_0());
    Frame_setContentBoard(f, Board_0());
    if (!Frame_dl(f) || !Frame_contentBoard(f)) { Frame_destroy(f); return nullptr; }
    Board_addRevalidator(Frame_contentBoard(f), frame_on_revalidate, f);
    Surface_addBoard(Frame_surface(f), Frame_contentBoard(f));
    Window_setResizeRenderHook(Frame_window(f), frame_on_resize, f);
    live_add(f);
    s_active = f;
    Pointer_track(f);
    Application *application = Application_current();
    if (application && !Frame_attachApplication(f, application)) { Frame_destroy(f); return nullptr; }
    Surface_setFPSCap(Frame_surface(f), Frame_getEffectiveFPSCap(f));
    return f;
}

// the single teardown: closes children (deepest-first), drops registrations,
// frees the element tree, the window and the frame.
fn Frame_destroy(Frame *frame) {
    if (!frame) return;
    Frame_setClosed(frame, true);
    if (Frame_application(frame)) {
        Application_removePollEvent(Frame_application(frame), frame_pollApplication, frame);
        Application_removeWindow(Frame_application(frame), Frame_window(frame));
        Frame_setApplication(frame, nullptr);
    }
    while (Frame_childCount(frame) > 0) Frame_close(Frame_children(frame)[Frame_childCount(frame) - 1]);
    if (Frame_owner(frame)) child_remove(Frame_owner(frame), frame);
    live_remove(frame);
    if (s_active == frame) s_active = nullptr;
    fire_close_hooks(frame);   // let input drop its registrations before we free
    Frame_removePanels(frame);
    free(Frame_panels(frame));
    Element_destroy(Frame_rootEl(frame));
    free(Frame_children(frame));
    free(Frame_closeFns(frame));
    free(Frame_closeUd(frame));
    frame_gpu_close(frame);
    if (Frame_shot(frame)) Image_destroy(Frame_shot(frame));
    Surface_removeBoard(Frame_surface(frame), Frame_contentBoard(frame));
    Board_destroy(Frame_contentBoard(frame));
    Surface_destroy(Frame_surface(frame));
    DisplayList_free(Frame_dl(frame));
    if (Frame_window(frame)) Window_destroy(Frame_window(frame));
    free(frame);
}

// close this frame and every child (deepest-first), then free it
fn Frame_close(Frame *f) { Frame_destroy(f); }

// ── basics ──────────────────────────────────────────────────────────────────
// Returns the borrowed host Window, or nullptr for a null Frame.
;;GETTER
Window *Frame_window(const Frame *frame) { return frame ? (*frame).window : nullptr; }
// Returns the owned presentation Surface, or nullptr for a null Frame.
;;GETTER
Surface *Frame_surface(const Frame *frame) { return frame ? (*frame).surface : nullptr; }
// Returns the frame's content Board, or nullptr for a null Frame.
;;GETTER
Board *Frame_contentBoard(const Frame *frame) { return frame ? (*frame).contentBoard : nullptr; }
// Forwards the title to the frame's host window when both arguments are usable.
fn Frame_setTitle(Frame *frame, const char *title) {
    if (frame && Frame_window(frame)) Window_setTitle(Frame_window(frame), title);
}
// Sets the color used to clear the frame before painting its element tree.
fn  Frame_setBackground(Frame *frame, Color color) {
    if (!frame) return;
    Frame_setBackgroundField(frame, color);
    // Paint alpha must not change the explicit OS window transparency policy.
}
// Compatibility spelling for Frame_setBackground.
fn  Frame_setBackgroundColor(Frame *frame, Color color) { Frame_setBackground(frame, color); }
// Returns the paint background, or clear for a null frame.
;;GETTER
Color Frame_background(const Frame *frame) { return frame ? (*frame).background : COLOR_CLEAR; }
// Sets whether the host window background is transparent.
fn  Frame_setTransparent(Frame *frame, bool transparent) {   // true = see-through
    if (frame && Frame_window(frame)) Window_setTransparentBackground(Frame_window(frame), transparent);
}
// Sets the host window's backdrop blur radius.
fn  Frame_setBlur(Frame *frame, f32 radius) {   // frost the WINDOW backdrop
    if (frame && Frame_window(frame)) Window_setBackdropBlur(Frame_window(frame), radius);
}

// ── Liquid Glass (macOS-exclusive, capability-gated) ─────────────────────────
// The Frame forwards to the borrowed R1 window; the AppKit chrome lives there.
bool Frame_macOS_hasLiquidGlass() { return Window_macOS_hasLiquidGlass(); }

// Applies the macOS Liquid Glass descriptor through the borrowed host window.
fn Frame_macOS_setLiquidGlass(Frame *frame, const FrameLiquidGlassDesc *desc) {
    if (frame && Frame_window(frame)) Window_macOS_setLiquidGlass(Frame_window(frame), desc);
}

// Reads the host window's Liquid Glass descriptor when supported.
bool Frame_macOS_getLiquidGlass(const Frame *frame, FrameLiquidGlassDesc *out) {
    return (frame && Frame_window(frame)) ? Window_macOS_getLiquidGlass(Frame_window(frame), out) : false;
}

// Registers a callback to run during Frame teardown.
fn Frame_onClose(Frame *frame, FrameCloseFn fn, any userdata) {
    if (!frame || !fn) return;
    if (Frame_closeCount(frame) == Frame_closeCap(frame)) {
        i32 ncap = Frame_closeCap(frame) ? Frame_closeCap(frame) * 2 : 4;
        FrameCloseFn *gf = realloc(Frame_closeFns(frame), (bytes)ncap * sizeof *gf);
        any *gu = realloc(Frame_closeUd(frame), (bytes)ncap * sizeof *gu);
        if (!gf || !gu) return;
        Frame_setCloseFns(frame, gf);
        Frame_setCloseUd(frame, gu);
        Frame_setCloseCap(frame, ncap);
    }
    Frame_closeFns(frame)[Frame_closeCount(frame)] = fn;
    Frame_closeUd(frame)[Frame_closeCount(frame)] = userdata;
    Frame_setCloseCount(frame, Frame_closeCount(frame) + 1);
}

// ── windows ─────────────────────────────────────────────────────────────────
// Shows the host window and renders its current contents once.
fn Frame_show(Frame *f) {
    if (!f || Frame_closed(f)) return;
    Window_show(Frame_window(f));
    Frame_setDirty(f, true);
    Frame_render(f);
    Frame_setDirty(f, false);
}

// Hides the host window without destroying the Frame.
fn Frame_hide(Frame *f) {
    if (f && Frame_window(f)) Window_hide(Frame_window(f));
}

// Reports whether the frame is absent, destroyed, or its window requested close.
bool Frame_isClosed(const Frame *f) { return !f || Frame_closed(f) || Window_shouldClose(Frame_window(f)); }
// Returns the owning Frame, if one was assigned.
;;GETTER
Frame *Frame_owner(const Frame *f) { return f ? (*f).owner : nullptr; }

// Reassigns the frame's owner and updates the owner's child registry.
fn Frame_setOwner(Frame *f, Frame *owner) {
    if (!f || f == owner) return;
    if (Frame_owner(f)) child_remove(Frame_owner(f), f);
    Frame_setOwnerField(f, owner);
    if (owner) child_add(owner, f);
}

// close this frame and every child (deepest-first), then free it

// ── children (panels) ───────────────────────────────────────────────────────
;;GETTER
Element *Frame_element(const Frame *frame) { return frame ? (*frame).root : nullptr; }

// The ownership seam properties/add.c and properties/remove.c call. Frames own
// their panels directly (there is no Frame-as-Panel), so this is the only place
// the private panels array is touched.
bool Frame_ownPanel(Frame *frame, Panel *panel, i32 index) {
    if (!frame || !Frame_rootEl(frame) || !panel) return false;
    i32 at = index < 0 ? Frame_count(frame) : index;
    if (at > Frame_count(frame)) at = Frame_count(frame);
    Panel **grown = realloc(Frame_panels(frame), (bytes)(Frame_count(frame) + 1) * sizeof *grown);
    if (!grown) return false;
    Frame_setPanels(frame, grown);
    memmove(&Frame_panels(frame)[at + 1], &Frame_panels(frame)[at],
            (bytes)(Frame_count(frame) - at) * sizeof *Frame_panels(frame));
    Frame_panels(frame)[at] = panel;
    Frame_setCount(frame, Frame_count(frame) + 1);
    Element_addAt(Frame_rootEl(frame), Panel_graphics(panel), at);
    return true;
}

// Detaches and destroys all panels currently owned by the frame.
fn Frame_clearPanels(Frame *frame) {
    if (!frame) return;
    for (i32 i = 0; i < Frame_count(frame); i++) {
        Element *g = Panel_graphics(Frame_panels(frame)[i]);
        if (g) Element_remove(g);        // unlink from the tree before freeing it
        Panel_destroy(Frame_panels(frame)[i]);
    }
    Frame_setCount(frame, 0);
}

// Returns the number of panels owned by the frame.
;;GETTER
i32 Frame_count(const Frame *frame) { return frame ? (*frame).count : 0; }

// Returns the panel at index, or nullptr when the frame/index is invalid.
Panel *Frame_panel(const Frame *frame, i32 index) {
    if (!frame || index < 0 || index >= Frame_count(frame)) return nullptr;
    return Frame_panels(frame)[index];
}

// Returns the frame's native-pixel layout bounds, using current window size before first render.
Rect Frame_root(const Frame *frame) {
    if (!frame) return (Rect){0, 0, 0, 0};
    // the layout root IS the window (native px); before the first render, fall
    // back to the live window size so layout/hit-testing works immediately.
    i32 w = Frame_lastW(frame), h = Frame_lastH(frame);
    if ((w <= 0 || h <= 0) && Frame_window(frame)) {
        w = Window_width(Frame_window(frame));
        h = Window_height(Frame_window(frame));
    }
    return (Rect){0, 0, (f32)w, (f32)h};
}

// Appends the root element tree's paint commands to the supplied display list.
fn Frame_paint(const Frame *frame, DisplayList *dl) {
    if (!frame || !dl || !Frame_rootEl(frame)) return;
    Element_paint(Frame_rootEl(frame), Frame_root(frame), dl);
}

// ── the one resize surface ──────────────────────────────────────────────────
bool Frame_resizeTargets(Frame *frame, i32 widthPx, i32 heightPx) {
    if (!frame || Frame_closed(frame) || widthPx <= 0 || heightPx <= 0) return false;
    if (widthPx == Frame_lastW(frame) && heightPx == Frame_lastH(frame)) return false;
    Frame_setLastW(frame, widthPx);
    Frame_setLastH(frame, heightPx);
    if (Frame_rootEl(frame)) Element_setSize(Frame_rootEl(frame), (f32)widthPx, (f32)heightPx);
    Graphics_resize((u32)widthPx, (u32)heightPx);
    // revalidate the present seam: an IOSurface cannot grow, so rebuild the GPU
    // d64 buffer; the fallback capture Image is resized in place.
    if (Frame_gpu(frame)) frame_gpu_reopen(frame, widthPx, heightPx);
    Surface_resize(Frame_surface(frame), (u32)widthPx, (u32)heightPx);
    return true;
}

// The cascade preamble that properties/revalidate.c calls. A frame is only
// ready to revalidate once it has a size; the first sizing comes from the
// window and itself repaints, so report "not ready" and let the caller stop —
// there must be no recursive Board_revalidate.
bool Frame_prepareRevalidate(Frame *frame) {
    if (!frame || !Frame_window(frame) || Frame_closed(frame)) return false;
    if (Frame_lastW(frame) <= 0 || Frame_lastH(frame) <= 0) {
        Frame_setSize(frame, Window_width(Frame_window(frame)), Window_height(Frame_window(frame)));
        return false;
    }
    return true;
}

// Compatibility entry: the cascade itself lives in properties/revalidate.c.
fn Frame_render(Frame *frame) { Frame_revalidate(frame); }

// Returns the configured fallback FPS cap.
;;GETTER
i32 Frame_getFPSCap(const Frame *f) { return f ? (*f).fpsCap : 0; }
// Returns the optional FPS cap used while the window is focused.
;;GETTER
i32 Frame_getFPSCapWhenFocusGain(const Frame *f) { return f ? (*f).fpsFocusGain : 0; }
// Returns the optional FPS cap used while the window is unfocused.
;;GETTER
i32 Frame_getFPSCapWhenFocusLost(const Frame *f) { return f ? (*f).fpsFocusLost : 0; }
// Selects the focus-specific cap when set, otherwise the configured fallback.
i32 Frame_getEffectiveFPSCap(const Frame *f) {
    if (!f) return 0;
    i32 override = Window_isFocused(Frame_window(f)) ? Frame_fpsFocusGain(f) : Frame_fpsFocusLost(f);
    return override ? override : Frame_fpsCap(f);
}
// Sets the fallback cap for positive rates, or -1 for uncapped operation.
fn Frame_setFPSCap(Frame *f, i32 fps) {
    if (f && (fps == -1 || fps > 0)) { Frame_setFpsCapField(f, fps); Surface_setFPSCap(Frame_surface(f), Frame_getEffectiveFPSCap(f)); }
}
// Sets the focused cap override; zero uses the fallback cap.
fn Frame_setFPSCapWhenFocusGain(Frame *f, i32 fps) {
    if (f && fps >= -1) { Frame_setFpsFocusGainField(f, fps); Surface_setFPSCap(Frame_surface(f), Frame_getEffectiveFPSCap(f)); }
}
// Sets the unfocused cap override; zero uses the fallback cap.
fn Frame_setFPSCapWhenFocusLost(Frame *f, i32 fps) {
    if (f && fps >= -1) { Frame_setFpsFocusLostField(f, fps); Surface_setFPSCap(Frame_surface(f), Frame_getEffectiveFPSCap(f)); }
}

// Polls one attached frame, rendering dirty content and servicing its Surface.
static fn frame_pollApplication(Application *application, any userdata) {
    (discard)application;
    Frame *f = userdata;
    if (Window_shouldClose(Frame_window(f))) {
        for (i32 i = 0; i < Frame_childCount(f); i++)
            Window_close(Frame_window(Frame_children(f)[i]));
        return;
    }
    Surface_setFPSCap(Frame_surface(f), Frame_getEffectiveFPSCap(f));
    if (Frame_dirty(f)) { Frame_render(f); Frame_setDirty(f, false); }
    Surface_poll(Frame_surface(f));
}

// Registers this frame's window and poll callback with an Application.
bool Frame_attachApplication(Frame *f, Application *application) {
    if (!f || !application || (Frame_application(f) && Frame_application(f) != application)) return false;
    if (Frame_application(f) == application) return true;
    if (!Application_addWindow(application, Frame_window(f))) return false;
    if (!Application_addPollEvent(application, frame_pollApplication, f)) {
        Application_removeWindow(application, Frame_window(f)); return false;
    }
    Frame_setApplication(f, application);
    Surface_setFPSCap(Frame_surface(f), Frame_getEffectiveFPSCap(f));
    if (Application_isRunning(application)) Frame_show(f);
    return true;
}
// Returns the borrowed Application attached to this Frame, if any.
;;GETTER
Application *Frame_application(const Frame *f) { return f ? (*f).application : nullptr; }
// Destroys every live frame attached to the specified Application.
fn Frame_destroyApplicationFrames(Application *application) {
    for (i32 i = 0; i < s_liveCount; ) {
        if (Frame_application(s_live[i]) == application) Frame_destroy(s_live[i]);
        else i++;
    }
}

// Paints and submits the frame into its GPU seam or retained RGBA fallback.
static fn frame_renderContent(Frame *frame) {
    if (!frame || !Frame_window(frame) || Frame_closed(frame)) return;
    Frame_setPaintReady(frame, false);
    if (Frame_gpu(frame)) {
        // zero-copy: render into the back IOSurface, publish it, swap front/back
        i32 back = 1 - Frame_front(frame);
        VulkanBackend_useSurface(Frame_target(frame, back));
        if (!Graphics_begin()) return;
        Graphics_clear(Frame_background(frame));
        DisplayList_clear(Frame_dl(frame));
        Frame_paint(frame, Frame_dl(frame));
        Graphics_submit(Frame_dl(frame));
        Graphics_end();
        if (Graphics_present()) {
            Frame_setFront(frame, back);
            Frame_setPaintReady(frame, true);
        }
        return;
    }
    if (!Frame_surface(frame)) return;
    if (!Graphics_begin()) return;
    Graphics_clear(Frame_background(frame));
    DisplayList_clear(Frame_dl(frame));
    Frame_paint(frame, Frame_dl(frame));
    Graphics_submit(Frame_dl(frame));
    Graphics_end();
    // fallback: render into the Surface's retained Image, publish via the blit
    Frame_setPaintReady(frame, Graphics_capture(Surface_presentImage(Frame_surface(frame))));
}

// Marks frame content dirty for the next Application poll.
fn Frame_invalidate(Frame *frame) { if (frame) Frame_setDirty(frame, true); }

// the input layer works in ELEMENTS; this is how it reaches the owning Frame
fn Frame_invalidateElement(Element *root) {
    if (!root) return;
    for (i32 i = 0; i < s_liveCount; i++)
        if (Frame_rootEl(s_live[i]) == root) { Frame_setDirty(s_live[i], true); return; }
}

// ── the runner ──────────────────────────────────────────────────────────────
fn Frame_runAll(Frame *root) {
    if (!root) return;
    // Compatibility starter; no second R4 keep-alive/render loop. Applications
    // already running must return to their lifecycle instead of nesting runs.
    if (Frame_application(root)) return;
    Application *app = Application("darling");
    if (!app) return;
    bool attached = true;
    for (i32 i = 0; i < s_liveCount; i++)
        if (!Frame_application(s_live[i]) && !Frame_attachApplication(s_live[i], app)) {
            attached = false; break;
        }
    if (attached) Application_start(app);
    // The caller retains Frames, just as it retains root. Detach the temporary
    // Application before freeing its borrowed callback/window registries.
    for (i32 i = 0; i < s_liveCount; i++) {
        Frame *f = s_live[i];
        if (Frame_application(f) != app) continue;
        Application_removePollEvent(app, frame_pollApplication, f);
        Application_removeWindow(app, Frame_window(f));
        Frame_setApplication(f, nullptr);
    }
    Application_free(app);
}

// Compatibility entry that runs the application lifecycle for live frames.
fn Frame_run(Frame *frame) { Frame_runAll(frame); }

// ── screenshots ─────────────────────────────────────────────────────────────
// Returns the most recently created live Frame, if one is active.
Frame *Frame_active() { return s_active; }

// Captures the current frame into its retained Image; GPU readback is on demand.
Image *Frame_capture(Frame *frame) {
    if (!frame) return nullptr;
    if (!Frame_prepareRevalidate(frame)) return nullptr;
    Surface_revalidateNow(Frame_surface(frame));
    if (!Frame_gpu(frame)) return Surface_presentImage(Frame_surface(frame));
    // GPU seam: read the FRONT IOSurface back into a CPU Image (on demand only)
    if (Frame_lastW(frame) <= 0 || Frame_lastH(frame) <= 0) return nullptr;
    if (!Frame_shot(frame)) Frame_setShot(frame, Image_0());
    if (!Frame_shot(frame)) return nullptr;
    if (!Image_ensureShadow(Frame_shot(frame), (u32)Frame_lastW(frame), (u32)Frame_lastH(frame)))
        return nullptr;
    Window_readPresentSurface(Frame_window(frame), Frame_surfaceSlot(frame, Frame_front(frame)),
                              Image_pixels(Frame_shot(frame)), Image_stride(Frame_shot(frame)));
    return Frame_shot(frame);
}

// Captures the current frame and writes its pixels as a PNG at path.
bool Frame_savePNG(Frame *frame, const char *path) {
    Image *img = Frame_capture(frame);
    if (!img || !path) return false;
    return Window_writePNG(Image_pixels(img), Image_stride(img),
                           (i32)Image_width(img), (i32)Image_height(img), path);
}

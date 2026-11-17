#include "vex.h"
#include "panel/scroll_panel.h"

#include <stdlib.h>


;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: ScrollPanel (panel/scroll_panel.c)
 * ============================================================================
 * A viewport over oversized content: one clipped Element (the viewport) holding
 * one owned child Element (the content) offset by (-offsetX, -offsetY). The
 * offset is the single source of truth and is end-clamped per axis on every
 * write, so a shrinking content or viewport can never maroon the view.
 *
 * The viewport Element clips its children (Property.clip), so the content is
 * cut at the viewport edge for free. Revalidation is explicit: every setter
 * funnels through setOffset, which re-clamps and reflects the offset into the
 * tree as the content's placement, then revalidates the owning root. Scrolling
 * from a wheel is just a scroll handler wired to ScrollPanel_scrollBy; the
 * panel itself never touches the OS.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * CLASS: ScrollPanel (panel/scroll_panel.c)
 * ============================================================================
 * Clipped viewport + owned content child, with an end-clamped scroll offset.
 *
 * STRUCT FIELDS:
 * ----------------------------------------------------------------------------
 *   Element *viewport;  // owned, clipped Element (the scroll window)
 *   Element *content;   // owned; a TOP_LEFT child of the viewport
 *   f32 viewW, viewH; // viewport extent (native px)
 *   f32 contentW, contentH; // content extent (native px)
 *   f32 offsetX, offsetY;   // scroll offset; [0, max(0, content-viewport)]
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   (none — every path funnels through ScrollPanel_setOffset)
 *
 * FUNCTION REGISTRY (exported by panel/scroll_panel.h):
 * ----------------------------------------------------------------------------
 * Constructors:
 *   - ScrollPanel_0 / ScrollPanel_1 / ScrollPanel_2, and ScrollPanel(...)
 * Core:
 *   - ScrollPanel_destroy, ScrollPanel_graphics, ScrollPanel_content,
 *     ScrollPanel_setContent, ScrollPanel_revalidate
 * Extents:
 *   - ScrollPanel_setViewportSize, setContentSize, viewportWidth/Height,
 *     contentWidth/Height
 * Offset:
 *   - ScrollPanel_setOffset, getOffset, scrollBy, maxX, maxY,
 *     canScrollX, canScrollY
 * Events:
 *   - ScrollPanel_add<Kind>Event (IMPLEMENT_EVENTS; see event_invoke.h)
 * ============================================================================
 */

struct ScrollPanel {
    Element *viewport;      // owned, clipped
    Element *content;       // owned; a child of the viewport
    f32 viewW, viewH;
    f32 contentW, contentH;
    f32 offsetX, offsetY;
};

;;GETTER
static inline Element *ScrollPanel_viewport(const ScrollPanel *sp) { return (*sp).viewport; }
;;SETTER
static inline fn ScrollPanel_setViewport(ScrollPanel *sp, Element *v) { (*sp).viewport = v; }
;;SETTER
static inline fn ScrollPanel_setContentPtr(ScrollPanel *sp, Element *v) { (*sp).content = v; }
;;SETTER
static inline fn ScrollPanel_setViewW(ScrollPanel *sp, f32 v) { (*sp).viewW = v; }
;;SETTER
static inline fn ScrollPanel_setViewH(ScrollPanel *sp, f32 v) { (*sp).viewH = v; }
;;SETTER
static inline fn ScrollPanel_setContentW(ScrollPanel *sp, f32 v) { (*sp).contentW = v; }
;;SETTER
static inline fn ScrollPanel_setContentH(ScrollPanel *sp, f32 v) { (*sp).contentH = v; }
;;SETTER
static inline fn ScrollPanel_setOffsetX(ScrollPanel *sp, f32 v) { (*sp).offsetX = v; }
;;SETTER
static inline fn ScrollPanel_setOffsetY(ScrollPanel *sp, f32 v) { (*sp).offsetY = v; }

// Creates a zero-sized clipped viewport.
ScrollPanel *ScrollPanel_0() { return ScrollPanel_2(0.0f, 0.0f); }
// Creates a square clipped viewport with the given side length.
ScrollPanel *ScrollPanel_1(f32 viewSize) { return ScrollPanel_2(viewSize, viewSize); }

// Creates a clipped viewport with the requested width and height.
ScrollPanel *ScrollPanel_2(f32 viewW, f32 viewH) {
    ScrollPanel *sp = calloc(1, sizeof *sp);
    if (!sp) return nullptr;
    ScrollPanel_setViewport(sp, Element());
    if (!ScrollPanel_viewport(sp)) { free(sp); return nullptr; }
    Element_setSize(ScrollPanel_viewport(sp), viewW, viewH);
    Element_setClip(ScrollPanel_viewport(sp), true);
    ScrollPanel_setViewW(sp, viewW);
    ScrollPanel_setViewH(sp, viewH);
    return sp;
}

// Destroys the viewport, its owned content subtree, and the wrapper.
fn ScrollPanel_destroy(ScrollPanel *sp) {
    if (!sp) return;
    Element_destroy(ScrollPanel_viewport(sp));   // frees the viewport and its content child
    free(sp);
}

// Returns the clipped viewport Element, or nullptr for a null wrapper.
;;GETTER
Element *ScrollPanel_graphics(const ScrollPanel *sp) { return sp ? (*sp).viewport : nullptr; }
// Returns the content Element currently attached to the viewport.
;;GETTER
Element *ScrollPanel_content(const ScrollPanel *sp) { return sp ? (*sp).content : nullptr; }

// Replaces the owned content child and re-clamps the current scroll offset.
fn ScrollPanel_setContent(ScrollPanel *sp, Element *content) {
    if (!sp) return;
    if (ScrollPanel_content(sp)) Element_remove(ScrollPanel_content(sp));   // detach the old (owned) child
    ScrollPanel_setContentPtr(sp, content);
    if (content) {
        Element_add(ScrollPanel_viewport(sp), content);
        ScrollPanel_setContentW(sp, Element_width(content));
        ScrollPanel_setContentH(sp, Element_height(content));
    } else {
        ScrollPanel_setContentW(sp, 0.0f);
        ScrollPanel_setContentH(sp, 0.0f);
    }
    ScrollPanel_setOffset(sp, ScrollPanel_getOffsetX(sp), ScrollPanel_getOffsetY(sp));   // re-clamp
}

// Updates the clipped viewport extent and re-clamps the current offset.
fn ScrollPanel_setViewportSize(ScrollPanel *sp, f32 w, f32 h) {
    if (!sp) return;
    ScrollPanel_setViewW(sp, w);
    ScrollPanel_setViewH(sp, h);
    Element_setSize(ScrollPanel_viewport(sp), w, h);
    ScrollPanel_setOffset(sp, ScrollPanel_getOffsetX(sp), ScrollPanel_getOffsetY(sp));   // re-clamp
}

// Updates the content extent used to calculate the scroll range.
fn ScrollPanel_setContentSize(ScrollPanel *sp, f32 w, f32 h) {
    if (!sp) return;
    ScrollPanel_setContentW(sp, w);
    ScrollPanel_setContentH(sp, h);
    ScrollPanel_setOffset(sp, ScrollPanel_getOffsetX(sp), ScrollPanel_getOffsetY(sp));   // re-clamp
}

// Returns the viewport width, or zero for a null wrapper.
;;GETTER
f32 ScrollPanel_viewportWidth(const ScrollPanel *sp) { return sp ? (*sp).viewW : 0.0f; }
// Returns the viewport height, or zero for a null wrapper.
;;GETTER
f32 ScrollPanel_viewportHeight(const ScrollPanel *sp) { return sp ? (*sp).viewH : 0.0f; }
// Returns the content width, or zero for a null wrapper.
;;GETTER
f32 ScrollPanel_contentWidth(const ScrollPanel *sp) { return sp ? (*sp).contentW : 0.0f; }
// Returns the content height, or zero for a null wrapper.
;;GETTER
f32 ScrollPanel_contentHeight(const ScrollPanel *sp) { return sp ? (*sp).contentH : 0.0f; }

// Returns the maximum horizontal offset, never less than zero.
f32 ScrollPanel_maxX(const ScrollPanel *sp) {
    if (!sp) return 0.0f;
    f32 m = ScrollPanel_contentWidth(sp) - ScrollPanel_viewportWidth(sp);
    return m > 0.0f ? m : 0.0f;
}
// Returns the maximum vertical offset, never less than zero.
f32 ScrollPanel_maxY(const ScrollPanel *sp) {
    if (!sp) return 0.0f;
    f32 m = ScrollPanel_contentHeight(sp) - ScrollPanel_viewportHeight(sp);
    return m > 0.0f ? m : 0.0f;
}
// Reports whether content extends beyond the viewport horizontally.
bool ScrollPanel_canScrollX(const ScrollPanel *sp) { return ScrollPanel_maxX(sp) > 0.0f; }
// Reports whether content extends beyond the viewport vertically.
bool ScrollPanel_canScrollY(const ScrollPanel *sp) { return ScrollPanel_maxY(sp) > 0.0f; }

// Sets both offsets, clamped independently to the current scroll ranges.
fn ScrollPanel_setOffset(ScrollPanel *sp, f32 x, f32 y) {
    if (!sp) return;
    f32 mx = ScrollPanel_maxX(sp), my = ScrollPanel_maxY(sp);
    if (x < 0.0f) x = 0.0f; if (x > mx) x = mx;   // THE clamp: one funnel
    if (y < 0.0f) y = 0.0f; if (y > my) y = my;
    ScrollPanel_setOffsetX(sp, x);
    ScrollPanel_setOffsetY(sp, y);
    ScrollPanel_revalidate(sp);
}

// Returns the current horizontal offset.
;;GETTER
f32 ScrollPanel_getOffsetX(const ScrollPanel *sp) {
    return sp ? (*sp).offsetX : 0.0f;
}

// Returns the current vertical offset.
;;GETTER
f32 ScrollPanel_getOffsetY(const ScrollPanel *sp) {
    return sp ? (*sp).offsetY : 0.0f;
}

// Applies a relative scroll delta through the clamping setter.
fn ScrollPanel_scrollBy(ScrollPanel *sp, f32 dx, f32 dy) {
    if (!sp) return;
    ScrollPanel_setOffset(sp, ScrollPanel_getOffsetX(sp) + dx, ScrollPanel_getOffsetY(sp) + dy);
}

// Applies the negative offset to content and revalidates the viewport tree.
fn ScrollPanel_revalidate(ScrollPanel *sp) {
    if (!sp || !ScrollPanel_viewport(sp)) return;
    if (ScrollPanel_content(sp)) {
        // content is a TOP_LEFT child; a negative offset scrolls it up/left
        Element_setOffset(ScrollPanel_content(sp), -ScrollPanel_getOffsetX(sp), -ScrollPanel_getOffsetY(sp));
    }
    Element_markDirty(ScrollPanel_viewport(sp));
    Element_revalidate(Element_root(ScrollPanel_viewport(sp)));
}

// ── events ──────────────────────────────────────────────────────────────────
// The `##` IS the class: these become ScrollPanel_addMouseEvent,
// ScrollPanel_addScrollEvent, ScrollPanel_addZoomEvent, ScrollPanel_addKeyEvent,
// ScrollPanel_addTouchEvent, ScrollPanel_addDocumentEvent.
IMPLEMENT_EVENTS(ScrollPanel)

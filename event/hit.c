#include "vex.h"
#include "event/hit.h"


;;DEFINITION
/**
 * ============================================================================
 * DEFINITION: Hit (event/hit.c)
 * ============================================================================
 * The coordinate resolver: pure math over a flat array of HitNodes. It answers
 * "which node is under this window point, and where is that point in the node's
 * own space and its parent's space?" No scene ownership, no allocation, no
 * callbacks — the caller owns the nodes and reads the Hit back.
 *
 * A node wins when it contains the point, its whole visible chain is visible,
 * and the point survives every clipping ancestor. Ties break by depth, then by
 * z; the walk is guarded against a malformed parent cycle, so a corrupt link
 * degrades to "no hit" rather than hanging.
 * ============================================================================
 */

;;OVERVIEW
/**
 * ============================================================================
 * MODULE: Hit (event/hit.c)
 * ============================================================================
 * Coordinate resolution over a caller-owned HitNode array. No owned state.
 *
 * STRUCT FIELDS: none — no owned state (HitNode/Hit live in event/hit.h).
 *
 * PRIVATE HELPERS:
 * ----------------------------------------------------------------------------
 *   node_origin(n, count, i, &ox, &oy)         : accumulated ancestor origin
 *   node_depth(n, count, i)                    : ancestor count (tie-break)
 *   node_visible_chain(n, count, i)            : self + ancestors all visible
 *   node_clip_chain(n, count, i, px, py)       : point inside every clip ancestor
 *
 * FUNCTION REGISTRY (exported by event/hit.h):
 * ----------------------------------------------------------------------------
 * Core Functions:
 *   - Hit_resolve(nodes, count, winX, winY, out)
 * ============================================================================
 */

// Sums ancestor offsets for node i, stopping at an invalid link or bounded walk.
static fn node_origin(const HitNode *n, i32 count, i32 i, f32 *ox, f32 *oy) {
    f32 x = 0.0f, y = 0.0f;
    i32 cur = i, guard = 0;
    while (cur >= 0 && cur < count && guard++ <= count) {
        x += n[cur].rect.x;
        y += n[cur].rect.y;
        cur = n[cur].parent;
    }
    *ox = x;
    *oy = y;
}

// Counts valid parent links above a node for hit-priority tie breaking.
static i32 node_depth(const HitNode *n, i32 count, i32 i) {
    i32 d = 0, cur = n[i].parent, guard = 0;
    while (cur >= 0 && cur < count && guard++ <= count) {
        d++;
        cur = n[cur].parent;
    }
    return d;
}

// Checks visibility from the candidate through its valid ancestors.
static bool node_visible_chain(const HitNode *n, i32 count, i32 i) {
    i32 cur = i, guard = 0;
    while (cur >= 0 && cur < count && guard++ <= count) {
        if (!n[cur].visible) return false;
        cur = n[cur].parent;
    }
    return true;
}

// every clipping ancestor (and self) must contain the point
static bool node_clip_chain(const HitNode *n, i32 count, i32 i, f32 px, f32 py) {
    i32 cur = i, guard = 0;
    while (cur >= 0 && cur < count && guard++ <= count) {
        if (n[cur].clip) {
            f32 ox, oy;
            node_origin(n, count, cur, &ox, &oy);
            const Rect r = n[cur].rect;
            if (!(px >= ox && px < ox + r.w && py >= oy && py < oy + r.h)) return false;
        }
        cur = n[cur].parent;
    }
    return true;
}

;;SETTER
static fn Hit_setId(Hit *h, i32 id) { (*h).id = id; }
;;SETTER
static fn Hit_setIndex(Hit *h, i32 idx) { (*h).index = idx; }
;;SETTER
static fn Hit_setWinX(Hit *h, f32 x) { (*h).winX = x; }
;;SETTER
static fn Hit_setWinY(Hit *h, f32 y) { (*h).winY = y; }
;;SETTER
static fn Hit_setParentX(Hit *h, f32 x) { (*h).parentX = x; }
;;SETTER
static fn Hit_setParentY(Hit *h, f32 y) { (*h).parentY = y; }
;;SETTER
static fn Hit_setLocalX(Hit *h, f32 x) { (*h).localX = x; }
;;SETTER
static fn Hit_setLocalY(Hit *h, f32 y) { (*h).localY = y; }

// Resolves the topmost eligible node and returns its window, parent, and local coordinates.
bool Hit_resolve(const HitNode *nodes, i32 count, f32 winX, f32 winY, Hit *out) {
    if (!nodes || count <= 0 || !out) return false;
    Hit_setId(out, -1);
    Hit_setIndex(out, -1);
    Hit_setWinX(out, winX);
    Hit_setWinY(out, winY);
    Hit_setParentX(out, winX);
    Hit_setParentY(out, winY);
    Hit_setLocalX(out, 0.0f);
    Hit_setLocalY(out, 0.0f);

    i32 best = -1, bestDepth = -1, bestZ = 0;
    for (i32 i = 0; i < count; i++) {
        if (!nodes[i].visible) continue;
        f32 ox, oy;
        node_origin(nodes, count, i, &ox, &oy);
        const Rect r = nodes[i].rect;
        if (!(winX >= ox && winX < ox + r.w && winY >= oy && winY < oy + r.h)) continue;
        if (!node_visible_chain(nodes, count, i)) continue;
        if (!node_clip_chain(nodes, count, i, winX, winY)) continue;
        i32 depth = node_depth(nodes, count, i);
        i32 z = nodes[i].z;
        if (depth > bestDepth || (depth == bestDepth && z >= bestZ)) {
            best = i;
            bestDepth = depth;
            bestZ = z;
        }
    }
    if (best < 0) return false;

    f32 sox, soy, pox = 0.0f, poy = 0.0f;
    node_origin(nodes, count, best, &sox, &soy);
    i32 parent = nodes[best].parent;
    if (parent >= 0 && parent < count) {
        node_origin(nodes, count, parent, &pox, &poy);
    } else {
        pox = winX;
        poy = winY;
    }
    i32 root = best, guard = 0;
    while (nodes[root].parent >= 0 && nodes[root].parent < count && guard++ <= count)
        root = nodes[root].parent;

    Hit_setId(out, nodes[best].id);
    Hit_setIndex(out, best);
    Hit_setParentX(out, best == root ? winX : winX - pox);
    Hit_setParentY(out, best == root ? winY : winY - poy);
    Hit_setLocalX(out, winX - sox);
    Hit_setLocalY(out, winY - soy);
    return true;
}

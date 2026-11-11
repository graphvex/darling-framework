#ifndef DARLING_POINTER_H
#define DARLING_POINTER_H

#include "frame/frame.h"   // Frame (the OS bridge) + the content Element
#include "panel/panel.h"   // Panel (the interactable wrapper)

// darling R4 — input/pointer.h
//
// ACCESSIBILITY: the pointer. Handlers attach to PANELS (the wrappers are the
// interface); the actions point at coordinates and hit-test the element tree.
//
//   Pointer_addButton(frame, &desc, on_click, ud);
//   Pointer_click(Frame_element(frame), 120, 40);
//
// Coordinates are native pixels, top-left origin. Real OS input is optional:
// Pointer_track routes it through these same actions.

typedef void (*PointerFn)(Panel *panel, void *userdata);

// ── actions (target the content element; hit-test runs from its root) ────────
void Pointer_move(Element *root, f32 x, f32 y);
void Pointer_press(Element *root, f32 x, f32 y);
void Pointer_release(Element *root, f32 x, f32 y);
void Pointer_click(Element *root, f32 x, f32 y);
void Pointer_hold(Element *root, f32 x, f32 y, d64 seconds);
void Pointer_drag(Element *root, f32 x, f32 y, f32 toX, f32 toY, d64 seconds);
Panel *Pointer_hover(Element *root, f32 x, f32 y);   // the bound Panel under (x,y)

// ── handlers ────────────────────────────────────────────────────────────────
void   Pointer_onClick(Panel *panel, PointerFn fn, void *userdata);
Panel *Pointer_addButton(Frame *frame, const ElementDesc *desc, PointerFn fn, void *userdata);

// Optional: feed the REAL mouse in through the same actions.
void Pointer_track(Frame *frame);

#endif // DARLING_POINTER_H

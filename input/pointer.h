#ifndef DARLING_POINTER_H
#define DARLING_POINTER_H

#include "vex.h"
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

typedef fn (*PointerFn)(Panel *panel, any userdata);

// ── actions (target the content element; hit-test runs from its root) ────────
fn Pointer_move(Element *root, f32 x, f32 y);
fn Pointer_press(Element *root, f32 x, f32 y);
fn Pointer_release(Element *root, f32 x, f32 y);
fn Pointer_click(Element *root, f32 x, f32 y);
fn Pointer_hold(Element *root, f32 x, f32 y, d64 seconds);
fn Pointer_drag(Element *root, f32 x, f32 y, f32 toX, f32 toY, d64 seconds);
Panel *Pointer_hover(Element *root, f32 x, f32 y);   // the bound Panel under (x,y)

// ── handlers ────────────────────────────────────────────────────────────────
fn   Pointer_onClick(Panel *panel, PointerFn fn, any userdata);
Panel *Pointer_addButton(Frame *frame, const ElementDesc *desc, PointerFn fn, any userdata);

// Optional: feed the REAL mouse in through the same actions.
fn Pointer_track(Frame *frame);

#endif // DARLING_POINTER_H

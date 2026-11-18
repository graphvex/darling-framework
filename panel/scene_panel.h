#ifndef DARLING_SCENE_PANEL_H
#define DARLING_SCENE_PANEL_H

// darling/panel/scene_panel.h — 2D/3D scene panel bridging a meld MeldView
// into a collage SceneSource rect at the panel's event bound.

#include "vex.h"
#include "meld.h"
#include "collage.h"

;;CLASSNAME("ScenePanel")

typedef struct ScenePanel {
    ElementNode element;
    MeldView view;
    const SceneImage *heldImage;
    u64 _pad[3];
} ScenePanel;

_Static_assert(sizeof(ScenePanel) % 32 == 0, "ScenePanel obeys 32-byte floor");

ScenePanel ScenePanel_4(f32 x, f32 y, f32 width, f32 height);
#define ScenePanel(...) CONSTRUCTOR_DISPATCH(ScenePanel, __VA_ARGS__)

// Acquires the newest completed SceneImage from the MeldView's ring (never
// blocking on in-flight producer work) and returns a collage SceneSource rect.
bool ScenePanel_acquireSceneSource(ScenePanel *self, Rect parentWorld, SceneSource *outSource);
fn   ScenePanel_releaseSceneSource(ScenePanel *self);

#endif

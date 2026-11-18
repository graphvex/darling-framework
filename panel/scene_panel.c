// darling/panel/scene_panel.c — bridges meld MeldView into collage SceneSource.

#include "vex.h"
#include "panel/scene_panel.h"

;;OVERVIEW
;;DEFINITION

ScenePanel ScenePanel_4(f32 x, f32 y, f32 width, f32 height) {
    ScenePanel panel = { 0 };
    panel.element = ElementNode_1(Rect_4(x, y, width, height));
    u32 pxW = (width  > 1.0f) ? (u32) width  : 1u;
    u32 pxH = (height > 1.0f) ? (u32) height : 1u;
    panel.view = MeldView_2(pxW, pxH);
    panel.heldImage = nullptr;
    return panel;
}

;;GETTER
static const SceneImage *ScenePanel_heldImage(const ScenePanel *self) {
    return self ? (*self).heldImage : nullptr;
}
;;SETTER
static fn ScenePanel_setHeldImage(ScenePanel *self, const SceneImage *held) {
    if (self) (*self).heldImage = held;
}
;;GETTER
static MeldView *ScenePanel_viewPtr(ScenePanel *self) {
    return self ? &(*self).view : nullptr;
}
;;GETTER
static const ElementNode *ScenePanel_elementPtr(const ScenePanel *self) {
    return self ? &(*self).element : nullptr;
}

bool ScenePanel_acquireSceneSource(ScenePanel *self, Rect parentWorld, SceneSource *outSource) {
    if (!self || !outSource) return false;
    MeldView *view = ScenePanel_viewPtr(self);
    SceneImageRing *ring = MeldView_ring(view);
    const SceneImage *held = ScenePanel_heldImage(self);
    if (held) {
        SceneImageRing_release(ring, held);
        ScenePanel_setHeldImage(self, nullptr);
    }
    const SceneImage *newest = SceneImageRing_acquireNewest(ring);
    if (!newest) return false;
    ScenePanel_setHeldImage(self, newest);
    Rect ev = ElementNode_eventBound(ScenePanel_elementPtr(self), parentWorld);
    *outSource = SceneSource_3(SceneImage_image(newest), SceneImage_version(newest), ev);
    return true;
}

fn ScenePanel_releaseSceneSource(ScenePanel *self) {
    const SceneImage *held = ScenePanel_heldImage(self);
    if (!self || !held) return;
    SceneImageRing_release(MeldView_ring(ScenePanel_viewPtr(self)), held);
    ScenePanel_setHeldImage(self, nullptr);
}

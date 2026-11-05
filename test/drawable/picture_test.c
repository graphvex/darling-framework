// Owner for drawable/picture.{h,c}: native-size/stretch borrowed-image widget.
// Public inventory: both constructor arities/chooser, destroy, graphics,
// image/setImage, size/location setters/queries and bounded string projections.
// Headless pixel/Element-parent teardown proof, NOT real Frame/GPU/visual proof.
// Coordinator gallery supplies one-Frame integration. Gaps: allocation-failure
// injection, concurrent mutation and unsupported fit/crop/automatic filters.
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "drawable/picture.h"

// Exercises Picture construction, painting, accessors, projection, and borrowing.
static void widget(void) {
    Image *image = Image_2(2, 1);
    Image_fill(image, COLOR_RGBA(10, 20, 30, 255));
    assert(image && Image_pixels(image));
    Picture *picture = Picture(image), *empty = Picture();
    assert(picture && empty);
    assert(Picture_width(picture) == 2 && Picture_height(picture) == 1);
    assert(Picture_image(picture) == image && Picture_image(empty) == nullptr);
    assert(Picture_width(empty) == 0 && Picture_height(empty) == 0);
    Picture_setSize(picture, 8, 4);
    Picture_setLocation(picture, -1, 2);
    Point location = Picture_location(picture);
    assert(location.x == -1 && location.y == 2);
    ElementDesc desc = {.width = 16, .height = 12};
    Element *parent = Element(&desc);
    Element *graphics = Picture_graphics(picture);
    assert(parent && graphics);
    Element_add(parent, graphics);
    assert(Element_parent(graphics) == parent && Element_count(parent) == 1);
    DisplayList *dl = DisplayList_0();
    assert(dl);
    Element_paint(parent, (Rect){0, 0, 16, 12}, dl);
    assert(DisplayList_count(dl) == 1 && DisplayList_cmds(dl)[0].kind == CMD_IMAGE);
    assert(Graphics_register(RasterGraphics_row()) && Graphics_use(BACKEND_RASTER));
    assert(Raster_configure(16, 12) && Graphics_clear(COLOR_CLEAR));
    assert(Graphics_submit(dl));
    assert(Raster_pixelAt(1, 3) == COLOR_RGBA(10, 20, 30, 255));
    char text[128];
    bool truncated = true;
    Picture_toString(picture, text, sizeof text, &truncated);
    assert(!truncated && strcmp(text, "Picture(8x4,image=2x1)") == 0);
    Picture_toStringStruct(picture, text, sizeof text, &truncated);
    assert(!truncated && strstr(text, "graphics=Element(width=8,height=4)"));
    Picture_toString(nullptr, text, sizeof text, &truncated);
    assert(!truncated && strcmp(text, "nullptr") == 0);
    Picture_toStringStruct(picture, text, 1, &truncated);
    assert(truncated && text[0] == '\0');
    Picture_toString(picture, nullptr, 0, &truncated);
    assert(truncated);
    Picture_toString(picture, text, sizeof text, nullptr);
    Image *replacement = Image_2(1, 2);
    Image_fill(replacement, COLOR_WHITE);
    Picture_setImage(picture, replacement);
    assert(Picture_image(picture) == replacement && Picture_width(picture) == 8);
    Picture_setImage(picture, nullptr);
    assert(Picture_image(picture) == nullptr && Image_pixels(replacement)[0] == 255);
    DisplayList_free(dl); // release old recorded image borrow before asset teardown
    Picture_destroy(picture); // BEFORE parent: removes owned node from borrowed tree
    assert(Element_count(parent) == 0);
    Element_destroy(parent);
    Picture_destroy(empty);
    assert(Image_pixels(image)[0] == 10);
    Image_destroy(image);
    Image_destroy(replacement);
    Picture_destroy(nullptr);
    Picture_setImage(nullptr, nullptr);
    Picture_setSize(nullptr, 0, 0);
    Picture_setLocation(nullptr, 0, 0);
    assert(Picture_graphics(nullptr) == nullptr && Picture_image(nullptr) == nullptr);
    assert(Picture_width(nullptr) == 0 && Picture_height(nullptr) == 0);
    assert(Picture_location(nullptr).x == 0 && Picture_location(nullptr).y == 0);
}

// Checks rejected dimensions/formats, diagnostics, and preservation of prior state.
static void invalid(void) {
    Picture *picture = Picture();
    assert(picture);
    Picture_setSize(picture, 5, 7);
    Picture_setLocation(picture, 2, 3);
    FILE *capture = tmpfile();
    assert(capture);
    fflush(stderr);
    int saved = dup(STDERR_FILENO);
    assert(saved >= 0 && dup2(fileno(capture), STDERR_FILENO) >= 0);
    Picture_setSize(picture, -1, 4);
    Picture_setSize(picture, NAN, 4);
    Picture_setSize(picture, 4, INFINITY);
    Picture_setLocation(picture, NAN, 1);
    Picture_setLocation(picture, 1, INFINITY);
    Image *missing = Image_2(1, 1);
    assert(missing && Picture(missing) == nullptr);
    Picture_setImage(picture, missing);
    Image *bgra = Image_4(1, 1, IMAGE_FORMAT_BGRA8, 0);
    Image_fill(bgra, COLOR_WHITE);
    Picture_setImage(picture, bgra);
    bool truncated = false;
    Picture_toString(picture, nullptr, 1, &truncated);
    assert(truncated);
    fflush(stderr);
    assert(dup2(saved, STDERR_FILENO) >= 0);
    close(saved);
    rewind(capture);
    char line[256];
    unsigned count = 0;
    while (fgets(line, sizeof line, capture)) {
        assert(strstr(line, "[vex] ") && strstr(line, "Picture "));
        ++count;
    }
    assert(count == 9);
    fclose(capture);
    assert(Picture_width(picture) == 5 && Picture_height(picture) == 7);
    assert(Picture_location(picture).x == 2 && Picture_location(picture).y == 3);
    assert(Picture_image(picture) == nullptr);
    Picture_setSize(picture, -0.0f, 0);
    assert(Picture_width(picture) == 0 && Picture_height(picture) == 0);
    Picture_destroy(picture);
    Image_destroy(missing);
    Image_destroy(bgra);
}

static unsigned textureReferences;
// Records a successful retained reference to the fake GPU texture.
static bool retainTexture(void *texture) {
    assert(texture == &textureReferences);
    ++textureReferences;
    return true;
}
// Releases the fake texture reference acquired during Image binding.
static bool releaseTexture(void *texture) {
    assert(texture == &textureReferences && textureReferences);
    --textureReferences;
    return true;
}
// Proves Picture admits a GPU-only borrowed Image without CPU pixel storage.
static void gpuOnlyAdmission(void) {
    // Headless widget admission only; actual texture pixels are proven by R3.
    Image *image = Image(8, 4);
    assert(Image_bindGpu(image, &textureReferences, image, image, retainTexture, releaseTexture));
    Picture *picture = Picture(image), *empty = Picture();
    assert(picture && empty && !Image_pixels(image));
    assert(Picture_width(picture) == 8 && Picture_height(picture) == 4);
    Picture_setImage(empty, image);
    assert(Picture_image(empty) == image);
    Picture_destroy(picture);
    Picture_destroy(empty);
    assert(textureReferences == 1); // both widgets borrow the Image
    Image_destroy(image);
    assert(!textureReferences);
}

// Runs the normal widget, invalid-input, and GPU-only admission scenarios.
int main(void) {
    widget();
    invalid();
    gpuOnlyAdmission();
    puts("picture_test: PASS (borrowed-image widget, normal paint, detach lifetime)");
    return 0;
}

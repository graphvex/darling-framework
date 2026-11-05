#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "drawable/picture.h"
#include "frame/frame.h"
#include "panel/panel.h"
#include "vulkan/vulkan_backend.h"
#include "compositor/filter_gallery_fixture.h"


// Interactive gallery, intentionally not an automatically registered _test.x
// --smoke performs native captured-pixel checks then closes; it is lab proof,
// never user appearance approval. Filter outputs remain sampled GPU textures;
// only explicitly requested smoke-test oracles/capture perform readback.
typedef struct FilterGalleryState {
    Picture *pictures[6];
    Image *images[6];
    Image *oracles[3]; // smoke-only CPU references; absent in interactive gallery
    Panel *cards[3]; // borrowed from Frame; anchors resolved by the real tree
    Device *device;
    GpuScope *gpu;
} FilterGalleryState;
static FilterGalleryState gallery;

// Releases gallery-owned widgets and images after outstanding GPU work retires.
static void galleryClosed(Frame *frame, void *userdata) {
    (void) frame;
    FilterGalleryState *state = userdata;
    if (GpuScope_destroy((*state).gpu)) {
        (*state).gpu=nullptr;
        (*state).device=nullptr;
    } // unsignaled GPU job/device remain alive; never free under outstanding work
    for (unsigned i = 0; i < 6; ++i) {
        Picture_destroy((*state).pictures[i]);
        (*state).pictures[i] = nullptr;
        Image_destroy((*state).images[i]);
        (*state).images[i] = nullptr;
    }
    for (unsigned i = 0; i < 3; ++i)
        Image_destroy((*state).oracles[i]);
}

// Compares one captured RGBA sample with its reference sample.
static bool sampleMatches(const Image *capture, const Image *source,
                          unsigned x, unsigned y, unsigned sx, unsigned sy) {
    if (!capture || x >= Image_width(capture) || y >= Image_height(capture))
        return false;
    const uint8_t *a = Image_pixels(capture) + (size_t) y * Image_stride(capture) + (size_t) x * 4;
    const uint8_t *b = Image_pixels(source) + (size_t) sy * Image_stride(source) + (size_t) sx * 4;
    for (unsigned c = 0; c < 4; ++c)
        if (abs((int) a[c] - b[c]) > 1) {
            fprintf(stderr, "sample %u,%u channel %u got %u expected %u\n", x, y, c, a[c], b[c]);
            return false;
        }
    return true;
}

// Supplies a fixed time source so the smoke checks have deterministic pacing.
static uint64_t galleryFrozenClock(void *userdata) {
    (void) userdata;
    return 1000000000ULL;
}

// Read the already-published native surface. Unlike Frame_capture, this cannot
// repaint and conceal a deferred resize or a stale anchor position.
static Image *galleryPublished(Frame *frame) {
    void *surface = Window_presentSurfaceContents(Frame_window(frame));
    if (!surface)
        return nullptr;
    unsigned width = Surface_width(Frame_surface(frame));
    unsigned height = Surface_height(Frame_surface(frame));
    Image *image = Image_2(width, height);
    if (!image || !Image_ensureShadow(image, width, height)) {
        Image_destroy(image);
        return nullptr;
    }
    Image_fill(image, COLOR_CLEAR);
    if (!Window_readPresentSurface(Frame_window(frame), surface, Image_pixels(image), Image_stride(image))) {
        Image_destroy(image);
        return nullptr;
    }
    return image;
}

#define DARLING_TEST_HAS_FRAMES
#define DARLING_TEST_WITH_ARGS
#include "test_application.h"

// Builds the filter gallery; --smoke checks published pixels and resized anchors.

int main(int argc, char **argv) {
    bool smoke = argc > 1 && strcmp(argv[1], "--smoke") == 0;
    // Boot bottom-up: the device is R3 and the frame is R4, so the GPU exists
    // before any window does. MoltenVK brings itself up in ~60 ms, and a frame
    // constructed against a live device never pays that cost itself.
    (void) VulkanBackend_device();
    Frame *frame = Frame("Graphvex filters | Backdrop / Foreground / Element", 1160, 430);
    if (!frame)
        return 1;
    Frame_setBackground(frame, COLOR_RGBA(11, 15, 24, 255));
    // The gallery rests when clean. Do not impose a 30 Hz content ceiling on
    // native resize; changed geometry is published immediately by Frame_setSize.
    Frame_onClose(frame, galleryClosed, &gallery);
    // Show before the GPU work. The window is what macOS waits on: a launch that
    // creates its pipelines and renders three filter passes first sits in the
    // Dock's launch bounce for the whole bring-up. The backdrop is already set,
    // so the window opens on its own background and fills in when the passes
    // land. A failure below still closes it through Frame_destroy.
    Frame_show(frame);
    Device *device=VulkanBackend_device(); // borrowed same-device texture sampling
    char shaderDirectory[2048];
    GpuScope *gpu=nullptr;
    if (Device_isValid(device) && FilterGallery_shaderDirectory(shaderDirectory,sizeof shaderDirectory))
        gpu=GpuScope(device,shaderDirectory,FILTER_GALLERY_GPU_PIXEL_BUDGET);
    gallery.device=device;
    gallery.gpu=gpu;
    if (!gpu) {
        Frame_destroy(frame); return 1;
    }
    for (unsigned i = 0; i < 3; ++i) {
        if (!FilterGallery_renderSampled(gpu,i,&gallery.images[i * 2]) ||
            (smoke && !FilterGallery_render(gpu,i,&gallery.oracles[i]))) {
            Frame_destroy(frame);
            return 1;
        }
        gallery.images[i * 2 + 1] = FilterGallery_caption(i);
        if (!gallery.images[i * 2 + 1]) {
            Frame_destroy(frame);
            return 1;
        }
        if (!VulkanBackend_prepareImage(gallery.images[i * 2]) ||
            !VulkanBackend_prepareImage(gallery.images[i * 2 + 1])) {
            Frame_destroy(frame);
            return 1;
        }
        Panel *card = Panel(372, 394);
        if (!card) {
            Frame_destroy(frame);
            return 1;
        }
        static const int anchors[] = {PART_TOP_LEFT, PART_TOP_CENTER, PART_TOP_RIGHT};
        Panel_setAnchor(card, anchors[i]);
        Panel_setPivot(card, anchors[i]);
        Panel_setOffset(card, i == 0 ? 10 : i == 1 ? 0 : -10, 14);
        Panel_setBackground(card, COLOR_RGBA(18, 23, 35, 255));
        Panel_setRadius(card, 10);
        if (!Frame_add_2(frame, card)) {
            Frame_destroy(frame);
            return 1;
        }
        gallery.cards[i] = card;
        for (unsigned j = 0; j < 2; ++j) {
            unsigned index = i * 2 + j;
            Picture *picture = Picture(gallery.images[index]);
            if (!picture) {
                Frame_destroy(frame);
                return 1;
            }
            gallery.pictures[index] = picture;
            Picture_setLocation(picture, 6, j ? 6 : 82);
            Element_add(Panel_graphics(card), Picture_graphics(picture));
        }
    }
    if (!GpuScope_destroy(gpu)) {
        Frame_destroy(frame); return 1; // never free borrowed device under live GPU work
    }
    gallery.gpu=nullptr;
    gallery.device=nullptr;
    if (smoke)
        Surface_setClock(Frame_surface(frame), galleryFrozenClock, nullptr);
    Frame_render(frame);
    if (smoke) {
        static const int widths[] = {1160, 1400, 1240, 1632};
        for (unsigned step = 0; step < sizeof widths / sizeof widths[0]; ++step) {
            uint64_t before = Surface_getPresentCount(Frame_surface(frame));
            if (step)
                Window_setSize(Frame_window(frame), widths[step], 430 + (int) step * 20);
            if (step && Surface_getPresentCount(Frame_surface(frame)) != before + 1) {
                fprintf(stderr, "filter_gallery: geometry publication was deferred\n");
                Frame_destroy(frame);
                return 1;
            }
            Image *capture = galleryPublished(frame);
            if (!capture) {
                fprintf(stderr, "filter_gallery: no published capture at step %u\n", step);
                Frame_destroy(frame);
                return 1;
            }
            // The three cards sit top-left, top-center and top-right, so their
            // expected column follows the resized width.
            for (unsigned i = 0; i < 3; ++i) {
                unsigned expected = i == 0 ? 10u
                                  : i == 1 ? (unsigned) ((widths[step] - 372) / 2)
                                  : (unsigned) (widths[step] - 372 - 10);
                if (!sampleMatches(capture, gallery.oracles[i], expected + 6, 116, 0, 20) ||
                    !sampleMatches(capture, gallery.oracles[i], (unsigned) expected + 16, 116, 10, 20) ||
                    !sampleMatches(capture, gallery.oracles[i], (unsigned) expected + 66, 226, 60, 130)) {
                    fprintf(stderr, "filter_gallery: resized anchor/Picture mismatch in case %u step %u\n", i + 1, step);
                    Image_destroy(capture);
                    Frame_destroy(frame);
                    return 1;
                }
            }
            Image_destroy(capture);
        }
        Frame_destroy(frame);
        puts("filter_gallery: PASS (left/center/right anchors, four extents, frozen-clock native publication)");
    } else {
        puts("GPU-filtered gallery: backdrop LEFT; child blur CENTER; element blur RIGHT. Drag window edges to compare anchors.");
        Darling_testKeepOpen();
    }
    return 0;
}

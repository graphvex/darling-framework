#ifndef DARLING_FILTER_GALLERY_FIXTURE_H
#define DARLING_FILTER_GALLERY_FIXTURE_H

// Bundled photograph and deterministic captions, not a production image loader.
// All three views execute Graphvex's real Vulkan scoped scatter pass.
#include "compositor/gpu_scope.h"
#include "graphvex/lang/image.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "compositor/gallery_photo.h"
#include "exception/throw.h"

enum { FILTER_GALLERY_WIDTH = 360, FILTER_GALLERY_HEIGHT = 300,
       FILTER_GALLERY_GPU_PIXEL_BUDGET = GALLERY_PHOTO_OUTPUT_PIXEL_LIMIT }; // cold allocation/work safety budget

// Writes one in-bounds RGBA pixel into the gallery's CPU fixture image.
static inline void FilterGallery_pixel(Image *image, unsigned x, unsigned y, Color color) {
    if (x >= Image_width(image) || y >= Image_height(image))
        return;
    uint8_t *pixel = Image_pixels(image) + (size_t) y * Image_stride(image) + (size_t) x * 4;
    pixel[0] = Color_red(color);
    pixel[1] = Color_green(color);
    pixel[2] = Color_blue(color);
    pixel[3] = Color_alpha(color);
}

// Small uppercase fixture font; readable labels do not claim Label/text support.
static inline void FilterGallery_text(Image *image, unsigned x, unsigned y,
                                      const char *text, unsigned scale, Color color) {
    static const uint8_t letters[26][7] = {
        {14,17,17,31,17,17,17}, {30,17,17,30,17,17,30}, {14,17,16,16,16,17,14},
        {30,17,17,17,17,17,30}, {31,16,16,30,16,16,31}, {31,16,16,30,16,16,16},
        {14,17,16,23,17,17,15}, {17,17,17,31,17,17,17}, {14,4,4,4,4,4,14},
        {7,2,2,2,18,18,12}, {17,18,20,24,20,18,17}, {16,16,16,16,16,16,31},
        {17,27,21,21,17,17,17}, {17,25,21,19,17,17,17}, {14,17,17,17,17,17,14},
        {30,17,17,30,16,16,16}, {14,17,17,17,21,18,13}, {30,17,17,30,20,18,17},
        {15,16,16,14,1,1,30}, {31,4,4,4,4,4,4}, {17,17,17,17,17,17,14},
        {17,17,17,17,17,10,4}, {17,17,17,21,21,21,10}, {17,17,10,4,10,17,17},
        {17,17,10,4,4,4,4}, {31,1,2,4,8,16,31}
    };
    for (size_t i = 0; text[i]; ++i) {
        if (text[i] < 'A' || text[i] > 'Z')
            continue;
        const uint8_t *glyph = letters[(unsigned) (text[i] - 'A')];
        for (unsigned row = 0; row < 7; ++row)
            for (unsigned column = 0; column < 5; ++column)
                if (glyph[row] & (1u << (4 - column)))
                    for (unsigned iy = 0; iy < scale; ++iy)
                        for (unsigned ix = 0; ix < scale; ++ix)
                            FilterGallery_pixel(image, x + (unsigned) i * 6 * scale + column * scale + ix,
                                                y + row * scale + iy, color);
    }
}

// Cold Apple fixture decoding only. Bound source/output dimensions before
// allocating; return nullptr on failure, never synthesize replacement artwork.
// Draw into a top-left RGBA shadow, then convert premultiplied to straight alpha.
static inline Image *FilterGallery_photoFromPath(const char *path, unsigned width, unsigned height) {
    if (!width || !height || (uint64_t) width * height > FILTER_GALLERY_GPU_PIXEL_BUDGET)
        return nullptr;
    Image *image = Image_2(width, height);
    if (!image || !Image_ensureShadow(image, width, height)) {
        Image_destroy(image);
        return nullptr;
    }
    Image_fill(image, COLOR_CLEAR);
    if (!GalleryPhoto_decode(path, width, height, Image_pixels(image), Image_stride(image))) {
        Image_destroy(image);
        return nullptr;
    }
    return image;
}

// The gallery photo is the shared sunflower in _resources/ at the workspace
// root. Resolve it from this header's own compiled path, so the picture is
// found whether the binary is launched from the repository or from a .app.
static inline const char *FilterGallery_photoPath(void) {
    static char resolved[4096];
    if (resolved[0] != '\0')
        return resolved;
#ifdef FILTER_GALLERY_SOURCE_RESOURCE
    snprintf(resolved, sizeof resolved, "%s", FILTER_GALLERY_SOURCE_RESOURCE);
    if (resolved[0] != '\0')
        return resolved;
#endif
    const char *marker = strstr(__FILE__, "/darling/");
    if (marker == nullptr)
        return nullptr;
    snprintf(resolved, sizeof resolved, "%.*s/_resources/sunflower.png",
             (int) (marker - __FILE__), __FILE__);
    return resolved;
}

// Copies an already-decoded photo. The gallery asks for the same two sizes
// repeatedly, and decoding a 9.9 MB photograph five times per launch dominated
// startup, so each size is decoded once and handed out as a copy.
static inline Image *FilterGallery_photoCopy(Image *source, unsigned width, unsigned height) {
    Image *copy = Image_2(width, height);
    if (!copy || !Image_ensureShadow(copy, width, height)) {
        Image_destroy(copy);
        return nullptr;
    }
    memcpy(Image_pixels(copy), Image_pixels(source), (size_t) width * height * 4u);
    return copy;
}

// Loads the gallery photo from the tracked resource, decoding each size once.
static inline Image *FilterGallery_photo(unsigned width, unsigned height) {
    enum { SLOTS = 4 };
    static Image *cached[SLOTS];
    static unsigned cachedWidth[SLOTS], cachedHeight[SLOTS];
    static unsigned next;

    for (unsigned i = 0; i < SLOTS; ++i)
        if (cached[i] != nullptr && cachedWidth[i] == width && cachedHeight[i] == height)
            return FilterGallery_photoCopy(cached[i], width, height);

    const char *path = FilterGallery_photoPath();
    Image *decoded = FilterGallery_photoFromPath(path, width, height);
    if (!decoded) {
        THROW("filter gallery sunflower resource missing or invalid");
        return nullptr;
    }
    unsigned slot = next++ % SLOTS;
    Image_destroy(cached[slot]);
    cached[slot] = FilterGallery_photoCopy(decoded, width, height);
    cachedWidth[slot] = width;
    cachedHeight[slot] = height;
    return decoded;
}

// The compositor's SPIR-V modules are tracked beside their sources. Resolve
// that directory from this header's own compiled path, so the gallery finds
// them from the repository or from inside a .app bundle alike.
static inline bool FilterGallery_shaderDirectory(char *dest,size_t cap) {
    const char *marker = strstr(__FILE__, "/darling/");
    if (marker == nullptr)
        return false;
    int length = snprintf(dest, cap, "%.*s/graphvex/shaders/spv",
                          (int) (marker - __FILE__), __FILE__);
    return length >= 0 && (size_t) length < cap;
}

// Builds one backdrop/foreground/element fixture and renders the chosen output form.
static inline bool FilterGallery_renderForm(GpuScope *gpu,unsigned which,bool sampled,Image **out) {
    if (which > 2 || !out || !gpu)
        return false;
    Image *baseline = FilterGallery_photo(FILTER_GALLERY_WIDTH, FILTER_GALLERY_HEIGHT);
    Image *photo = nullptr, *decorationImage = nullptr;
    if (!baseline)
        return false;
    if (which) {
        for (unsigned y = 0; y < FILTER_GALLERY_HEIGHT; ++y)
            for (unsigned x = 0; x < FILTER_GALLERY_WIDTH; ++x)
                FilterGallery_pixel(baseline, x, y, (x / 12 + y / 12) % 2 ?
                    COLOR_RGBA(39, 45, 61, 255) : COLOR_RGBA(27, 33, 47, 255));
    }
    bool status=false;
    decorationImage = Image_2(296, 180);
    if (!decorationImage || !Image_ensureShadow(decorationImage, 296, 180)) {
        goto cleanup;
    }
    Image_fill(decorationImage, which == 0 ? COLOR_RGBA(225, 239, 255, 80) :
               which == 1 ? COLOR_RGBA(69, 93, 141, 255) : COLOR_RGBA(138, 76, 119, 255));
    if (which) {
        photo = FilterGallery_photo(296, 132);
        if (!photo) {
            goto cleanup;
        }
    } else {
        photo = Image_2(154, 24);
        if (!photo || !Image_ensureShadow(photo, 154, 24)) {
            goto cleanup;
        }
        Image_fill(photo, COLOR_CLEAR);
        FilterGallery_text(photo, 2, 2, "GLASS", 3, COLOR_RGBA(12, 37, 65, 255));
    }
    if (sampled)
        status=GpuScope_renderSampled(gpu,which,baseline,decorationImage,32,78,
            photo,which ? 32 : 110,which ? 100 : 154,8,out);
    else
        status=GpuScope_render(gpu,which,baseline,decorationImage,32,78,
            photo,which ? 32 : 110,which ? 100 : 154,8,out);
cleanup:
    Image_destroy(photo);
    Image_destroy(decorationImage);
    Image_destroy(baseline);
    return status;
}
// Renders one fixture output as a CPU-readable reference image.
static inline bool FilterGallery_render(GpuScope *gpu,unsigned which,Image **out) {
    return FilterGallery_renderForm(gpu,which,false,out);
}
// Renders one fixture output as a sampled GPU image.
static inline bool FilterGallery_renderSampled(GpuScope *gpu,unsigned which,Image **out) {
    return FilterGallery_renderForm(gpu,which,true,out);
}

// Creates the deterministic title/detail image for one filter-scope example.
static inline Image *FilterGallery_caption(unsigned which) {
    static const char *titles[] = {"GPU BACKDROP", "GPU FOREGROUND", "GPU ELEMENT"};
    static const char *details[] = {"BEHIND PANEL", "CHILD BLUR CLIPPED", "WHOLE PANEL BLUR"};
    if (which > 2)
        return nullptr;
    Image *image = Image_2(FILTER_GALLERY_WIDTH, 70);
    if (!image || !Image_ensureShadow(image, FILTER_GALLERY_WIDTH, 70)) {
        Image_destroy(image);
        return nullptr;
    }
    Image_fill(image, COLOR_RGBA(18, 23, 35, 255));
    FilterGallery_text(image, 12, 6, titles[which], 4, COLOR_RGBA(224, 235, 255, 255));
    FilterGallery_text(image, 12, 44, details[which], 2, COLOR_RGBA(145, 181, 220, 255));
    return image;
}

#endif

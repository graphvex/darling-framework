#ifndef GALLERY_PHOTO_H
#define GALLERY_PHOTO_H
#include <stddef.h>
#include <stdint.h>

// Explicit fixture decoder safety limits, not dynamic entity capacities.
enum { GALLERY_PHOTO_OUTPUT_PIXEL_LIMIT = 262144,
       GALLERY_PHOTO_SOURCE_PIXEL_LIMIT = 16777216 };

// Apple-only test fixture bridge. nullptr path resolves the app bundle resource;
// explicit paths are for owner tests. Cold, synchronous, owner-thread use.
// Dest must hold height rows at stride Bytes; false leaves it unchanged until
// successful context construction. Success writes straight sRGB RGBA, top-left,
// center-cropped without stretching. No production decoder support is claimed.
bool GalleryPhoto_decode(const char *path, unsigned width, unsigned height,
                         uint8_t *dest, size_t stride);
#endif

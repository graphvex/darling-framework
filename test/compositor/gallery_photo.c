#include "gallery_photo.h"
#include "lifetime/platform.h"
#include <string.h>

#if defined(VEX_PLATFORM_APPLE)
#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <ImageIO/ImageIO.h>
#endif

// MODULE: gallery fixture photo decoding. No owned persistent state.
// Native headers stay in this TU: Apple's Point/Rect collide with Graphvex's.
// Decodes and center-crops a photo into the caller-provided top-left RGBA buffer.
// GalleryPhoto_decode owns temporary URL/source/image/context references,
// releases them on every path, and writes only the caller's bounded shadow.
// Source pixel safety bound: 16 Mi pixels; output bound matches GpuScope budget.
bool GalleryPhoto_decode(const char *path, unsigned width, unsigned height,
                         uint8_t *dest, size_t stride) {
#if !defined(VEX_PLATFORM_APPLE)
    (void) path;
    (void) width;
    (void) height;
    (void) dest;
    (void) stride;
    return false;
#else
    if (!dest || !width || !height || (uint64_t) width * height > GALLERY_PHOTO_OUTPUT_PIXEL_LIMIT ||
        stride < (size_t) width * 4u || stride > SIZE_MAX / height)
        return false;
    CFURLRef url = path ? CFURLCreateFromFileSystemRepresentation(nullptr,
        (const UInt8*) path, (CFIndex) strlen(path), false) :
        CFBundleCopyResourceURL(CFBundleGetMainBundle(), CFSTR("other-sunflower"), CFSTR("png"), nullptr);
    if (!url)
        return false;
    CGImageSourceRef source = CGImageSourceCreateWithURL(url, nullptr);
    CFRelease(url);
    if (!source)
        return false;
    CFDictionaryRef properties = CGImageSourceCopyPropertiesAtIndex(source, 0, nullptr);
    int sourceWidth = 0, sourceHeight = 0;
    if (properties) {
        CFNumberRef w = CFDictionaryGetValue(properties, kCGImagePropertyPixelWidth);
        CFNumberRef h = CFDictionaryGetValue(properties, kCGImagePropertyPixelHeight);
        if (w && CFGetTypeID(w) == CFNumberGetTypeID())
            CFNumberGetValue(w, kCFNumberIntType, &sourceWidth);
        if (h && CFGetTypeID(h) == CFNumberGetTypeID())
            CFNumberGetValue(h, kCFNumberIntType, &sourceHeight);
        CFRelease(properties);
    }
    if (sourceWidth <= 0 || sourceHeight <= 0 ||
        (uint64_t) sourceWidth * (uint64_t) sourceHeight > GALLERY_PHOTO_SOURCE_PIXEL_LIMIT) {
        CFRelease(source);
        return false;
    }
    CGImageRef photograph = CGImageSourceCreateImageAtIndex(source, 0, nullptr);
    CFRelease(source);
    if (!photograph)
        return false;
    CGColorSpaceRef colorSpace = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
    CGContextRef context = colorSpace ? CGBitmapContextCreate(dest, width, height,
        8, stride, colorSpace, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big) : nullptr;
    if (colorSpace)
        CGColorSpaceRelease(colorSpace);
    if (!context) {
        CGImageRelease(photograph);
        return false;
    }
    double scaleX = (double) width / sourceWidth, scaleY = (double) height / sourceHeight;
    double scale = scaleX > scaleY ? scaleX : scaleY;
    double drawWidth = sourceWidth * scale, drawHeight = sourceHeight * scale;
    // Bitmap-context row order already matches CGImage's decoded top row;
    // a UI-style vertical transform would invert the fixture (owner-tested).
    CGContextSetBlendMode(context, kCGBlendModeCopy);
    CGContextDrawImage(context, CGRectMake((width - drawWidth) * 0.5,
        (height - drawHeight) * 0.5, drawWidth, drawHeight), photograph);
    CGContextRelease(context);
    CGImageRelease(photograph);
    for (unsigned y = 0; y < height; ++y)
        for (unsigned x = 0; x < width; ++x) {
            uint8_t *pixel = dest + (size_t) y * stride + (size_t) x * 4;
            for (unsigned c = 0; c < 3; ++c)
                pixel[c] = pixel[3] ? (uint8_t) ((pixel[c] * 255u + pixel[3] / 2u) / pixel[3]) : 0;
        }
    return true;
#endif
}

// darling/test/panel/properties_test.c — mirrors darling properties/
//
// The operation-by-property layer: add (by arity), remove (hands the child
// back), set_location (by arity), and set_corner_radius. These prove the ops
// act on the widget's Element through the ownership seam, and that detaching is
// non-destructive.

#include <stdio.h>

#include "panel/panel.h"
#include "properties/add.h"
#include "properties/remove.h"
#include "properties/set_location.h"
#include "properties/set_corner_radius.h"
#include "ui/element.h"

static int g_fail = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

// Compare resolved geometry with tolerance for floating-point calculations.
static bool near(float a, float b) { return (a > b ? a - b : b - a) < 0.01f; }

#include "test_application.h"
// Verify property operations, detachment ownership, location arities, and bounds.
int main(void) {
    Panel *root = Panel(200, 200);
    Panel *a = Panel(50, 50);
    Panel *b = Panel(50, 50);
    Panel *c = Panel(50, 50);
    CHECK(root && a && b && c);

    // Panel_add(...) is arity-overloaded: append (2) or insert (3)
    CHECK(Panel_add(root, a) == a);
    CHECK(Panel_add(root, b) == b);
    CHECK(Panel_add(root, c, 1) == c);        // insert between a and b.sh
    CHECK(Panel_childCount(root) == 3);
    CHECK(Panel_parent(a) == root);
    CHECK(Panel_childElement(root, 0) == Panel_graphics(a));
    CHECK(Panel_childElement(root, 1) == Panel_graphics(c));
    CHECK(Panel_childElement(root, 2) == Panel_graphics(b));

    // Panel_remove(child) unlinks WITHOUT freeing: caller keeps the child
    Panel *detached = Panel_remove(c);
    CHECK(detached == c);
    CHECK(Panel_parent(c) == nullptr);
    CHECK(Panel_childCount(root) == 2);
    CHECK(Panel_childElement(root, 0) == Panel_graphics(a));
    CHECK(Panel_childElement(root, 1) == Panel_graphics(b));
    Panel_destroy(detached);                  // caller now owns it

    // Panel_setLocation grows with arity: point -> +anchor -> +pivot
    Panel_setLocation(a, 5, 6);
    Rect r = Element_resolve(Panel_graphics(a), (Rect){0, 0, 200, 200});
    CHECK(near(r.x, 5.0f) && near(r.y, 6.0f));

    Panel_setLocation(a, 10, 20, PART_CENTER);
    CHECK(Element_anchor(Panel_graphics(a)) == PART_CENTER);
    Panel_setLocation(a, 0, 0, PART_TOP_LEFT, PART_CENTER);
    CHECK(Element_pivot(Panel_graphics(a)) == PART_CENTER);

    // Panel_setCornerRadius writes the shared bound
    Panel_setCornerRadius(a, 12.0f);
    CHECK(near(Element_radius(Panel_graphics(a)), 12.0f));

    // min/max size modulate the EFFECTIVE size (clamped on the bound)
    Panel_setSize(a, 300, 40);
    Panel_setMinimumSize(a, 80, 90);
    Panel_setMaximumSize(a, 200, 0);        // width ceiling; height unbounded
    CHECK(near(Element_width(Panel_graphics(a)), 200.0f));
    CHECK(near(Element_height(Panel_graphics(a)), 90.0f));

    // destroying the root frees a/b.sh and their Elements in one pass
    Panel_destroy(root);

    if (g_fail == 0) printf("properties_test: ALL PASS\n");
    return g_fail == 0 ? 0 : 1;
}

// darling/test/panel/panel_add_test.c — mirrors darling panel.
//
// Headless: Panels nest. Panel_add attaches a child and the parent owns the
// wrapper, so one Panel_destroy frees the whole wrapper chain plus the Element
// tree (no leaks, no d64 free) — including a grandchild added after the
// child was adopted.

#include <stdio.h>

#include "graphics/graphics.h"
#include "panel/panel.h"
#include "ui/element.h"

static int g_fail = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #cond);          \
            g_fail++;                                                      \
        }                                                                  \
    } while (0)

#include "test_application.h"
// Verify child ordering, lookup, late nesting, and recursive wrapper ownership.
int main(void) {
    Panel *root = Panel();
    Panel_setSize(root, 400, 300);
    CHECK(Panel_childCount(root) == 0);

    Panel *a = Panel();
    Panel_setSize(a, 100, 50);
    Panel_setTag(a, "a");
    Panel *b = Panel();
    Panel_setSize(b, 100, 50);
    Panel_setTag(b, "b.sh");
    CHECK(Panel_add(root, a) == a);
    CHECK(Panel_add(root, b) == b);
    CHECK(Panel_childCount(root) == 2);
    CHECK(Panel_childElement(root, 0) == Panel_graphics(a));
    CHECK(Panel_childElement(root, 1) == Panel_graphics(b));
    CHECK(Panel_childElement(root, 9) == nullptr);
    CHECK(Element_find(Panel_graphics(root), "b.sh") == Panel_graphics(b));

    // a grandchild added AFTER the child was adopted is still owned downward
    Panel *c = Panel();
    Panel_setSize(c, 20, 20);
    CHECK(Panel_add(a, c) == c);
    CHECK(Panel_childCount(a) == 1);
    CHECK(Panel_childCount(root) == 2);   // direct children only, as the tree reads

    CHECK(Panel_graphics(nullptr) == nullptr);
    CHECK(Panel_childCount(nullptr) == 0);
    CHECK(Panel_add(nullptr, a) == nullptr);
    CHECK(Panel_add(root, nullptr) == nullptr);

    Panel_destroy(root);   // frees a, b.sh, c wrappers + the whole Element tree
    if (g_fail == 0) printf("panel_add_test: ALL PASS\n");
    return g_fail == 0 ? 0 : 1;
}

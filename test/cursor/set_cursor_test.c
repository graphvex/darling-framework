#include <stdio.h>

#include "properties/set_cursor.h"
#include "panel/panel.h"
#include "panel/scroll_panel.h"

// Headless lab: class-named setters, inheritance/overrides, masked hover hits,
// shared-bound independence, null/invalid inputs and destruction. No OS window.
static int failures;
#define CHECK(condition) do { if (!(condition)) { \
    printf("FAIL line %d: %s\n", __LINE__, #condition); failures++; \
} } while (0)

// Prove that any implemented widget gets its own Class##_setCursor API.
typedef struct ExampleWidget { Element *element; } ExampleWidget;
// Exposes the example widget's backing Element to the cursor adapter macro.
static Element *ExampleWidget_graphics(const ExampleWidget *self) {
    return self ? (*self).element : nullptr;
}
DECLARE_CURSOR(ExampleWidget);
IMPLEMENT_CURSOR(ExampleWidget)

#include "test_application.h"
// Tests cursor inheritance, overrides, masked hits, shared bounds, and teardown.
int main(void) {
    Panel *parent = Panel(200, 100);
    Panel *child = Panel(50, 50);
    CHECK(parent && child);
    Panel_add(parent, child);
    Element *root = Panel_graphics(parent);
    CHECK(Element_cursorAt(root, 10, 10) == CURSOR_ARROW);
    CHECK(Panel_setCursor(parent, CURSOR_POINTER) == parent);
    CHECK(Element_cursorAt(root, 10, 10) == CURSOR_POINTER); // child inherits
    CHECK(Panel_setCursor(child, CURSOR_TEXT) == child);
    CHECK(Element_cursorAt(root, 10, 10) == CURSOR_TEXT);
    CHECK(Element_cursorAt(root, 70, 10) == CURSOR_POINTER); // leave child
    CHECK(Element_cursorAt(root, 250, 10) == CURSOR_ARROW); // leave root
    Panel_setCursor(child, CURSOR_ARROW);
    CHECK(Element_cursorAt(root, 10, 10) == CURSOR_ARROW); // explicit override
    Panel_setCursor(child, CURSOR_INHERIT);
    CHECK(Element_cursorAt(root, 10, 10) == CURSOR_POINTER);
    Panel_setCursor(child, (CursorType) 999);
    CHECK(Element_cursorPreference(Panel_graphics(child)) == CURSOR_INHERIT);

    Panel_setCursor(child, CURSOR_CROSSHAIR);
    Element_setVisible(Panel_graphics(child), false);
    CHECK(Element_cursorAt(root, 10, 10) == CURSOR_POINTER);
    Element_setVisible(Panel_graphics(child), true);
    Element_setRadius(Panel_graphics(child), 25);
    CHECK(Element_cursorAt(root, 1, 1) == CURSOR_POINTER); // rounded corner masked
    CHECK(Element_cursorAt(root, 25, 25) == CURSOR_CROSSHAIR);

    // Cursor identity must not leak through a shared pooled paint bound.
    Element *alias = Element();
    Element_setProperty(alias, Element_property(Panel_graphics(child)));
    CHECK(Element_cursorPreference(alias) == CURSOR_INHERIT);
    ExampleWidget example = {alias};
    CHECK(ExampleWidget_setCursor(&example, CURSOR_TEXT) == &example);
    CHECK(Element_cursorPreference(alias) == CURSOR_TEXT);
    CHECK(Element_cursorPreference(Panel_graphics(child)) == CURSOR_CROSSHAIR);
    Element_destroy(alias); // borrow goes away before the owner

    ScrollPanel *scroll = ScrollPanel(80, 60);
    Element *content = Element();
    Element_setSize(content, 80, 200);
    ScrollPanel_setContent(scroll, content);
    CHECK(ScrollPanel_setCursor(scroll, CURSOR_POINTER) == scroll);
    CHECK(Element_cursorAt(ScrollPanel_graphics(scroll), 10, 10) == CURSOR_POINTER);
    Element_setCursor(content, CURSOR_TEXT);
    CHECK(Element_cursorAt(ScrollPanel_graphics(scroll), 10, 10) == CURSOR_TEXT);
    CHECK(Element_cursorAt(ScrollPanel_graphics(scroll), 10, 70) == CURSOR_ARROW);
    ScrollPanel_destroy(scroll);

    // Repeated lifetimes do not retain registrations or stale cursor preferences.
    Panel_destroy(parent);
    for (int i = 0; i < 100; i++) {
        Panel *fresh = Panel(10, 10);
        CHECK(Element_cursorPreference(Panel_graphics(fresh)) == CURSOR_INHERIT);
        Panel_setCursor(fresh, CURSOR_HIDDEN);
        Panel_destroy(fresh);
    }
    CHECK(Panel_setCursor(nullptr, CURSOR_TEXT) == nullptr);
    CHECK(ScrollPanel_setCursor(nullptr, CURSOR_TEXT) == nullptr);
    CHECK(Frame_setCursor(nullptr, CURSOR_TEXT) == nullptr);
    CHECK(Element_cursorAt(nullptr, 1, 1) == CURSOR_ARROW);
    puts(failures ? "set_cursor_test: FAIL" : "set_cursor_test: PASS");
    return failures != 0;
}

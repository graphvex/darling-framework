# darling-framework

## Definition

The remastered C23 retained UI framework: widget interfaces, tree construction,
layout policy, input/focus and the native-window/application bridge. It is the
**core tier** — it arranges what the lifetime tier owns (the OS window,
residency, platform spelling) and what graphvex owns (element composition,
graphics types, the present seam), and it rebuilds neither.

## Disclaimers

- **125 of 185 `.c` files are `;;DRAFT`/`;;INCOMPLETE` scaffolds with zero API.**
  Compiling them is structure proof, not behavior proof.
- **Text, Label and typography are not implemented** in this generation; the
  declarations are drafts.
- **Visual approval is the user's.** No appearance or interaction is certified
  by a passing test, and no visual acceptance is recorded here.
- The `darling-editor` studio below was a vision for a separate application
  built *on* this framework, not a capability of this repository.

<details>
<summary>Historical darling-editor vision — not current framework capabilities</summary>

### darling-editor, by Vex.

The Figma + Miro Inspired Spatial Canvas & Interface Builder.

`darling-editor` is a bare-metal, high-performance spatial whiteboard and UI design studio built directly on `darling-framework` and Vulkan. It unites the collaborative freedom of Miro with the interface construction power of Figma—running locally with zero Electron or webview overhead.

---

### Core Capabilities

1. **Infinite Spatial Canvas**: Smooth 120Hz pan/zoom board for brainstorming, architecture diagrams, sticky notes, and visual mindmaps.
2. **Interactive UI Prototyping**: Drag-and-drop construction of `darling-framework` widgets (`Button`, `CardPanel`, `Switch`, `CodeField`, containers).
3. **Multi-Format Export Engine**:
   - **C23 Code**: Emits idiomatic `darling` construction code (`Class(...)` with 9-grid anchors).
   - **HTML / CSS**: Transpiles canvas interfaces into clean, standalone semantic HTML.
   - **Vector SVG**: Exports vector shapes, connectors, and diagrams as standards-compliant SVGs.
   - **Raster Images**: High-resolution PNG and JPEG rendering via `graphvex` FrameImporter.
4. **Icon Subsystem & SVG Baking**:
   - Comprehensive icon catalog (Lucide, Tabler, Material Icons, Feather).
   - Crisp, infinite-resolution vector rendering baked into Signed Distance Fields (SDFs) using GPU Jump Flooding Algorithms (`sdf_jfa`).
5. **Real-Time Multiplayer Sync**: Pairs with `../sesh` for live multi-user cursors, collaborative note taking, and remote canvas sharing.

</details>

## Current State

**Implemented and proven (macOS arm64, window tests opt-in):** `frame.c` (the
retained UI host: window + panel/element tree, Surface/Board revalidation,
d64-buffered IOSurface GPU present with RGBA fallback, capture/PNG, Application
lifecycle, FPS policy), `panel.c` ownership, `scroll_panel.c` (partial viewport:
clamp/clip/revalidate/events), the nine `properties/*` operations,
`event_invoke.c` dispatch, `hit.c` resolver, `pointer.c` synthetic+OS input, and
`picture.c`.

**Draft / not implemented (source-verified):** all buttons, most inputs, every
layout panel, overlays, dialogs, kit/game/scene/spatial widgets, theme, bridges,
and Text/Label/typography. `event/focus.c` and `layout/container.c` are drafts
(no focus traversal, no layout-policy module). The real CPU image-group
compositor is graphvex's, not here.

**Platforms proven:** macOS arm64 only; Linux and Windows native execution
unproven.

**Evidence:** `test/` in this repository (window-based, opt-in — run
`./tools/b test ui_` from the workspace root) and the workspace ledger
`metadata/gists/test-checklist.md`. Most rows remain untested; a test file
existing is not per-file proof.

## What It Does

- `Frame` — the retained UI host: a window plus a panel/element tree, with
  Surface/Board revalidation, IOSurface presentation (RGBA fallback), capture and
  PNG, application lifecycle and FPS policy.
- `Panel` — element wrapper, child ownership, geometry, paint properties and
  event adders. A primitive, not a library of finished controls.
- `ScrollPanel` — a partial but real viewport: content, explicit offsets,
  clamp/reclamp and revalidation.
- **Shared properties** — nine operations: add, remove, location, size, min/max
  bounds, corner radius, cursor and revalidation.
- **Events and pointer** — typed handler registration and bubbling, hit
  resolution, and synthetic plus native pointer forwarding.
- **Cursor selection** — per-node cursor preference and inheritance, masked hit
  selection.
- **Picture** — a borrowed RGBA8 image through ordinary Element painting.
- **Drafts** — buttons, inputs, layout panels, overlays, dialogs, kit/game/
  scene/spatial widgets, theme and bridges exist as scaffold pairs that describe
  intent. `SCAFFOLDS.md` inventories them; `STATUS.md` tracks the implementation
  checklist and next steps.

## Dependencies

| Repository | Consumed | Link |
| :--- | :--- | :--- |
| **lifetime** | `window/`, `core/` and `lifetime/` headers — the OS window, residency and platform spelling | [vex-lib/lifetime](https://github.com/vex-lib/lifetime) |
| **graphvex** | `ui/`, `graphics/`, `vulkan/` and `nio/property_pool.h` — element composition, graphics types, the Vulkan present seam and property pools | [vex-lib/graphvex](https://github.com/vex-lib/graphvex) |

Build-only:

| Tool | Role | Link |
| :--- | :--- | :--- |
| **b** | the workspace build runner (`./tools/b test ui_`) | [vex-graph/b](https://github.com/vex-graph/b) |
| **vex-c** (`lib`) | the include floor `b` injects as `B_INCLUDE` | [vex-graph/vex-c](https://github.com/vex-graph/vex-c) |

Rendering, graphical element composition, filters and render bounds are
graphvex's; darling consumes that compositor and does not implement a competing
one.

## Scope and Limitations

**Scope:** widget interfaces, tree construction, layout policy, input/focus and
the native-window/application bridge.

**Deliberately not covered:**

- No competing renderer or compositor — the real CPU image-group compositor is
  graphvex's, and `compositor/compositor.c` here is a draft.
- No text/label engine in this generation (Text, Label and typography are
  drafts).
- No application, session or lifetime ownership: residency and the OS window
  belong to the lifetime tier, and app policy belongs to the app that composes
  darling.

**Known limits and gaps:**

- 125 of 185 `.c` files are draft scaffolds with zero API; the implemented
  surface is exactly the list under Current State.
- No focus traversal (`event/focus.c`) and no layout-policy module
  (`layout/container.c`).
- ScrollPanel has clamped offsets and clipping but lacks default wheel handling,
  a scrollbar widget and typed attachment ergonomics.
- macOS arm64 only; all visual appearance and interaction is user-owned.

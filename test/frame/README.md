# Frame tests and interactive viewing

Run from the workspace root. `b test` runs bounded automated assertions and exits;
`b run … --interactive` keeps the window open for you to use. No fixed viewing
timeout is imposed in interactive mode.

Every C starter now enters `Application_start`: native events and pending Frame
presentation are serviced by that lifetime, not a test-written keep-alive loop.
The shared starter uses a worker start event and `Application_invoke` to keep
native graphics/assertions on their owner thread. Completion requires all
attached windows to **close**, not merely hide; worker close requests close all.

| Test | Automated scope | Interactive behavior |
| :--- | :--- | :--- |
| `naked_frame_test` | Empty content, naked mode, retained traffic-light visibility preferences on macOS, resize, paint/transparency independence and reversible chrome | Transparent titlebar; drag the empty background to move the window |
| `borderless_frame_test` | Empty content, borderless mode, resize, paint/transparency independence and reversible chrome | No native titlebar/buttons; drag the background; click the red square to exit |
| `traffic_light_frame_test` | macOS per-button visibility preferences, exact header position, floating/decorated transitions, empty content and resize | Native red/yellow/green buttons; drag the background; red square also exits |
| `ui_matryoshka_battle_test` | 96 captured rings, mutation, hit testing, resize, scripted press/drag/release with pixel assertions, pool reclamation | Drag a ring or the center to move it and its nested children; native close button exits |
| `liquid_glass_frame_test` | Descriptor round-trip assertions only; no appearance claim | `--interactive` stays open until actual window closure; no 12-second timeout |
| `frame_application_lifecycle_test` | Worker startup/return, owner invocation, multiple windows, hide-versus-close and close-all/join | Automated only |
| `frame_fps_focus_test` | Focus selection, deterministic R3 -1/1/120 FPS ceilings, coalesced demand and independent scene worker | Automated only; no physical display-Hz claim |

## Try them yourself

```sh
./tools/b.sh run naked_frame_test --interactive
./tools/b.sh run borderless_frame_test --interactive
./tools/b.sh run traffic_light_frame_test --interactive
./tools/b.sh run ui_matryoshka_battle_test --interactive
./tools/b.sh run liquid_glass_frame_test --interactive
```

The three chrome viewers include a red square exit control. Matryoshka uses
the real mouse bridge and root-level bubbled callbacks, not just a pre-blitted
image. Grab a colored ring to move that panel and its subtree. Drag delivery is
supported inside the Frame; this test does not implement outside-window pointer
capture, keyboard focus, or a generic toolkit drag-and-drop API.

## Automated checks

```sh
./tools/b.sh test naked_frame_test
./tools/b.sh test borderless_frame_test
./tools/b.sh test traffic_light_frame_test
./tools/b.sh test ui_matryoshka_battle_test
./tools/b.sh test frame_application_lifecycle_test
./tools/b.sh test frame_fps_focus_test
```

These are automated state/content-pixel checks over live Frames, not visual
acceptance. `Frame_capture` contains content only: native titlebar/traffic-light
pixels, backdrop distortion and Window Server chrome are not captured. Traffic
light getters expose visibility preferences, not a screenshot of native buttons.
Liquid Glass appearance and fallback appearance remain user-checked. Automated
descriptor assertions do not launch its persistent interactive viewing mode.

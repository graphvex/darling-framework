# Darling UI battle tests

Run `./tools/b test ui_` from the workspace root. These are actual tree/paint/
capture tests, not gallery buttons that all instantiate the same placeholder.
Detailed API coverage, confirmed defects and next tests are recorded in
`_notes/darling/ui-battle-testing.md` in the umbrella workspace.

Seven passing targets exercise every public Element operation, 162 full-image
anchor/pivot combinations, oversized children clipped by rounded parents on
CPU/GPU, visual paint-state pixels, actual ScrollPanel captured bands, a real
Frame with 50,000 visible children, and a 96-level nested Panel tree.

The original four red regressions now pass after fixes to nested clip intersection,
clip restoration, clipped-child hit testing and CPU/GPU borders. Four further
oracles cover distinct overlapping rounded masks, caller clip scope/malformed
list recovery, the Frame -> Surface -> Board -> tree/present cascade, and shared min/max
size bounds. All 15 `ui_` targets pass; none of their expected pixels were weakened.

Full-suite verification: 207 passed, zero failed, one existing visual test
(`panel_anchor_corner_radius_test`) timed out at 30 seconds. Its focused rerun
passed. Do not describe that full run as 208/208 green.

For PNG evidence, set `UI_BATTLE_ARTIFACT_DIR` to an existing writable directory.
Pixel failures emit capture paths; clipped-hit tests save their image before
checking input targeting. Pixel tolerance is two channel levels; the analytic
rounded-boundary oracle omits only the antialias fringe.

Tests are compiled/discovered by the umbrella build with C23 and warnings as
errors. Added through shell fallback because the authoring session lacked direct
write/edit tools. Follow-through fixed the demonstrated library defects and wired the Frame cascade;
the sibling agent owns the subsequent Frame/properties migration.

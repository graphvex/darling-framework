#!/usr/bin/env python3
"""Lab-only scaffold integrity/header compilation; not widget runtime readiness."""

import json
import os
from pathlib import Path
import re
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[3]
REPO = ROOT / "interfaces/darling-framework"


# Raises the supplied diagnostic when a scaffold invariant is false.
def require(condition, message):
    if not condition:
        raise AssertionError(message)


# Validates the manifest/source map and compiles the aggregate C23 headers.
def main():
    manifest = json.loads((REPO / "scaffolds.json").read_text())
    entries = manifest["entries"]
    require(manifest["schema"] == 1 and entries, "missing scaffold inventory")
    require(manifest["intent"] == "gist/readiness/darling.md", "wrong intent source")
    paths, types = set(), set()
    sources = []
    includes = ['#include "frame/frame.h"', '#include "panel/panel.h"',
                '#include "panel/scroll_panel.h"', '#include "input/pointer.h"']
    documentation = (REPO / "SCAFFOLDS.md").read_text()
    intent = (ROOT / manifest["intent"]).read_text()
    require("## Current Remastered Scaffold Map" in intent, "missing current-system readiness map")
    current_map = intent.split("## Current Remastered Scaffold Map", 1)[1].split("## 🎯 Tier-0", 1)[0]
    for entry in entries:
        stem, kind = entry["path"], entry["kind"]
        require(stem not in paths, f"duplicate source home: {stem}")
        require(not Path(stem).is_absolute() and ".." not in Path(stem).parts,
                f"noncanonical source path: {stem}")
        paths.add(stem)
        sources.append(str(REPO / "src" / (stem + ".c")))
        require(entry["status"] == "draft" and entry["scope"], f"false readiness: {stem}")
        source = (REPO / "src" / (stem + ".c")).read_text()
        header = (REPO / "src" / (stem + ".h")).read_text()
        require(source.startswith(f'#include "{stem}.h"'), f"unpaired include: {stem}")
        for marker in (";;DRAFT", ";;INCOMPLETE", ";;DEFINITION", ";;OVERVIEW"):
            require(marker in source, f"missing {marker}: {stem}")
        require(entry["scope"] in source and entry["scope"] in header,
                f"stale blueprint: {stem}")
        require(f"src/{stem}.c" in documentation, f"undocumented source: {stem}")
        require(f"src/{stem}.c" in current_map and entry["scope"] in current_map,
                f"missing current-system draft row: {stem}")
        clean_header = re.sub(r"/\*.*?\*/|//[^\n]*", "", header, flags=re.S)
        clean_source = re.sub(r"/\*.*?\*/|//[^\n]*", "", source, flags=re.S)
        require("{" not in clean_source and "{" not in clean_header,
                f"draft introduced storage/function body: {stem}")
        require(not re.search(r"\w+\s*\([^;]*\)\s*;", clean_header),
                f"draft promised callable API: {stem}")
        require("IMPLEMENT_EVENTS" not in clean_source and "DECLARE_EVENTS" not in clean_header,
                f"blanket event opt-in: {stem}")
        if kind == "class":
            name = entry["name"]
            require(name not in types, f"duplicate class: {name}")
            types.add(name)
            require(f"typedef struct {name} {name};" in header, f"opaque type missing: {stem}")
        else:
            require(kind == "module" and "typedef" not in clean_header,
                    f"module duplicates a type: {stem}")
        guard = "DARLING_" + stem.upper().replace("/", "_").replace("-", "_") + "_H"
        require(f"#ifndef {guard}" in header and f"#define {guard}" in header,
                f"missing include guard: {stem}")
        includes += [f'#include "{stem}.h"', f'#include "{stem}.h"']
    require("Color" not in types and "Window" not in types and "Clipboard" not in types,
            "duplicated lower-level public type")
    for legacy, current in manifest["aliases"].items():
        require(current in types or current == "ScrollPanel", f"unmapped alias: {legacy}")
    # One real compiler invocation includes every draft header twice alongside
    # the live API. This checks coexistence, guards and cross-header conflicts.
    command = shlex.split(os.environ.get("CC", "cc"))
    command += ["-std=gnu23", "-Wall", "-Wextra", "-Werror", "-fsyntax-only", "-x", "c", "-"]
    for relative in ("interfaces/darling-framework/src", "host-behavior/vexspoke/src",
                     "vex/re/src", "drivers/graphvex/src", "host-behavior/hotcwap"):
        command += ["-I", str(ROOT / relative)]
    subprocess.run(command, input="\n".join(includes) + "\n", text=True,
                   cwd=ROOT, check=True, timeout=120)
    # Compile every draft implementation freshly, not only an incremental
    # build-cache lookup. No object files, executable or window is produced.
    source_command = [argument for argument in command if argument not in ("-x", "c", "-")]
    subprocess.run(source_command + sources, cwd=ROOT, check=True, timeout=120)
    print(f"PASS: {len(entries)} draft pairs, {len(types)} opaque classes; blueprints and aggregate headers")
    print("Scope: structure and C23 compilation only; no runtime/API behavior or visual approval.")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Builds setup/mods.json for Setup.ps1.

Usage:
  python tools/build-setup-manifest.py <folder with the catalog mod sources>

The catalog mod sources are the .wh.cpp files Windhawk keeps in
C:\\ProgramData\\Windhawk\\ModsSource. The custom mods are read from Files\\.
Run this again whenever a mod or a setting below changes.
"""
import json
import re
import sys
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent

# Catalog mods: id -> settings that differ from the mod defaults.
CATALOG_MODS = {
    "icon-resource-redirect": {
        "iconTheme": "macOS Blue|themes/icons/niivu/macOS%20blue.zip",
        "disableThumbnails": 1,
        "allResourceRedirect": 1,
    },
    "custom-corner-radius": {"radius": 12, "smallRadius": 6},
    "windows-11-file-explorer-styler": {"theme": "Minimal Explorer11"},
    "center-new-windows": {},
    "invisible-borders": {},
    "monitor-rounded-edges": {},
    "slick-window-arrangement": {},
    "dark-menus": {},
}

# Custom mods from Files\: file -> (settings overrides, optional, question)
LOCAL_MODS = {
    "macos-dock-taskbar.wh.cpp": ({}, False, None),
    "top-menubar.wh.cpp": ({}, False, None),
    # Fork of the catalog mod "MacOS Minimize Animation" that also supports a
    # dock on the left/right. Replaces the original (see CONFLICTING_MODS).
    "macos-minimize-animation-dock.wh.cpp": ({}, False, None),
    "stage-manager-taskbar-aware.wh.cpp": ({}, True, "Install Stage Manager (recent windows as cards next to the active window, Win+Alt+S)?"),
    "three-finger-drag.wh.cpp": ({}, True, "Install Three Finger Drag (laptops with a Precision Touchpad)?"),
    "turbo-boost-tray-toggle.wh.cpp": ({}, True, "Install Turbo Boost Tray Toggle (tray icon to turn CPU Turbo Boost on/off)?"),
}

# Mods that conflict with macOS Dock and get disabled if installed.
# macOS Dock has its own magnification and Recycle Bin divider since 2.2.
CONFLICTING_MODS = ["windows-11-taskbar-styler", "taskbar-icon-size", "taskbar-dock-animation", "macos-minimize-animation", "taskbar-dock-animation-plus", "taskbar-icon-separators"]


class JsonLikeLoader(yaml.SafeLoader):
    """Only true/false/null/numbers are special, like js-yaml's JSON_SCHEMA."""


JsonLikeLoader.yaml_implicit_resolvers = {}
for tag, regexp, first in [
    ("tag:yaml.org,2002:bool", r"^(?:true|false)$", list("tf")),
    ("tag:yaml.org,2002:null", r"^(?:null)$", ["n"]),
    ("tag:yaml.org,2002:int", r"^-?(?:0|[1-9][0-9]*)$", list("-0123456789")),
    ("tag:yaml.org,2002:float", r"^-?(?:0|[1-9][0-9]*)(?:\.[0-9]*)?(?:[eE][-+]?[0-9]+)?$", list("-0123456789")),
]:
    JsonLikeLoader.add_implicit_resolver(tag, re.compile(regexp), first)


def metadata(source):
    block = re.search(r"^//[ \t]+==WindhawkMod==[ \t]*$([\s\S]+?)^//[ \t]+==/WindhawkMod==[ \t]*$", source, re.M)
    result = {}
    for line in block.group(1).split("\n"):
        line = line.rstrip()
        if not line:
            continue
        m = re.match(r"^//[ \t]+@(_?[a-zA-Z]+)(?::([a-z]{2}(?:-[A-Z]{2})?))?[ \t]+(.*)$", line)
        if m and not m.group(2):
            result.setdefault(m.group(1), []).append(m.group(3))
    one = lambda k: result.get(k, [""])[0]
    return {
        "id": one("id"),
        "name": one("name"),
        "version": one("version"),
        "include": result.get("include", []),
        "exclude": result.get("exclude", []),
        "architecture": result.get("architecture", []),
        "compilerOptions": one("compilerOptions"),
    }


def initial_settings(source):
    m = re.search(r"^//[ \t]+==WindhawkModSettings==[ \t]*$\s*/\*\s*([\s\S]+?)\s*\*/\s*^//[ \t]+==/WindhawkModSettings==[ \t]*$", source, re.M)
    if not m:
        return {}
    parsed = {}

    def parse_settings(items, prefix=""):
        for item in items:
            keys = [k for k in item if not k.startswith("$")]
            assert len(keys) == 1, keys
            key = (prefix + "." if prefix else "") + keys[0]
            parse_value(item[keys[0]], key)

    def parse_value(value, key):
        if isinstance(value, bool):
            parsed[key] = 1 if value else 0
        elif isinstance(value, (int, float, str)):
            parsed[key] = value
        elif value and isinstance(value[0], (int, float, str)) and not isinstance(value[0], bool):
            for i, v in enumerate(value):
                parse_value(v, f"{key}[{i}]")
        elif value and isinstance(value[0], list):
            for i, v in enumerate(value):
                parse_settings(v, f"{key}[{i}]")
        else:
            parse_settings(value, key)

    parse_settings(yaml.load(m.group(1), Loader=JsonLikeLoader))
    return parsed


def entry(src_text, overrides, **extra):
    meta = metadata(src_text)
    settings = initial_settings(src_text)
    for k, v in overrides.items():
        if k not in settings:
            sys.exit(f"{meta['id']}: unknown setting {k}")
        settings[k] = v
    return {**meta, "settings": settings, **extra}


def main():
    catalog_dir = Path(sys.argv[1])
    mods = []
    for mod_id, overrides in CATALOG_MODS.items():
        src = (catalog_dir / f"{mod_id}.wh.cpp").read_text(encoding="utf-8")
        mods.append(entry(src, overrides, source="catalog"))
    for file, (overrides, optional, question) in LOCAL_MODS.items():
        src = (ROOT / "Files" / file).read_text(encoding="utf-8")
        mods.append(entry(src, overrides, source="local", file=file, optional=optional, question=question))
    out = ROOT / "setup" / "mods.json"
    out.parent.mkdir(exist_ok=True)
    out.write_text(json.dumps({"mods": mods, "conflicting": CONFLICTING_MODS}, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    for m in mods:
        print(f"{m['source']:8} {m['id']:35} {m['version']:8} {len(m['settings'])} settings")


if __name__ == "__main__":
    main()

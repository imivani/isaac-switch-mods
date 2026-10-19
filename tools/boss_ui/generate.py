"""Extract the pinned Enhanced Boss Bars default artwork and verified rules.

This is deliberately a bounded data importer, not a Lua interpreter. Unsupported
game-state conditions are listed in the report instead of guessing native fields.
The manifest contains original files; packaging converts PNG to Switch RGBA PCX.
"""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import posixpath
import re
import struct
import subprocess
import xml.etree.ElementTree as ET

PROJECT = Path(__file__).resolve().parents[2]
REFERENCE = PROJECT / "reference/Enhanced-Boss-Bars"
RESOURCES = REFERENCE / "resources"
OUT = PROJECT / "evidence/boss_ui"
HEADER = PROJECT / "native/source/program/generated/boss_ui_data.hpp"
STAGED = PROJECT / "assets/boss_ui/resources"
PIN = "c2c0cbe1fe2f8205a822a4cb4b1b19c16465429e"
ICON = "gfx/ui/bosshp_icons/"
BAR = "gfx/ui/bosshp_bars/"


def without_comments(source: str) -> str:
    token = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|--\[\[.*?\]\]|--[^\n]*', re.S)
    return token.sub(lambda m: "".join("\n" if c == "\n" else " " for c in m[0]) if m[0].startswith("--") else m[0], source)


def braces(text: str, start: int) -> tuple[str, int]:
    assert text[start] == "{"
    depth, quote, escaped = 0, None, False
    for i in range(start, len(text)):
        c = text[i]
        if quote:
            if escaped:
                escaped = False
            elif c == "\\":
                escaped = True
            elif c == quote:
                quote = None
        elif c in "\"'":
            quote = c
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return text[start + 1:i], i + 1
    raise ValueError("Unbalanced Lua table")


def table(source: str, name: str) -> str:
    pos = source.index("HPBars." + name + " =")
    return braces(source, source.index("{", pos))[0]


def entries(body: str) -> dict[str, str]:
    result, cursor = {}, 0
    pattern = re.compile(r'\["([^"]+)"\]\s*=\s*\{')
    while match := pattern.search(body, cursor):
        value, cursor = braces(body, match.end() - 1)
        assert match[1] not in result
        result[match[1]] = value
    return result


def string_field(body: str, key: str, default=None):
    match = re.search(r'\b' + key + r'\s*=\s*(?:(path|barPath)\s*\.\.\s*)?"([^"]+)"', body)
    if not match:
        return default
    return {None: "", "path": ICON, "barPath": BAR}[match[1]] + match[2]


def cpp(value) -> str:
    if value is None:
        return "nullptr"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, str):
        return json.dumps(value)
    if isinstance(value, float):
        return f"{value:.8g}" + ("f" if "." in f"{value:.8g}" else ".0f")
    if isinstance(value, (list, tuple)):
        return "{" + ", ".join(cpp(v) for v in value) + "}"
    return str(value)


def main() -> None:
    commit = subprocess.check_output(["git", "-C", str(REFERENCE), "rev-parse", "HEAD"], text=True).strip()
    assert commit == PIN, f"Review upstream changes before regenerating: {commit}"
    source = without_comments((REFERENCE / "bossbar_data.lua").read_text(encoding="utf-8-sig"))
    raw_bosses = entries(table(source, "BossDefinitions"))
    raw_styles = entries(table(source, "BarStyles"))
    assets: set[str] = set()
    animation_data, animation_ids = [], {}
    sequences = []
    patched_animations, path_repairs = {}, []

    def asset(path: str) -> str:
        assert (RESOURCES / path).is_file(), f"Missing upstream resource: {path}"
        assets.add(path)
        return path

    def animation(path: str) -> int:
        if path in animation_ids:
            return animation_ids[path]
        root = ET.parse(RESOURCES / asset(path)).getroot()
        layers = root.findall("Content/Layers/Layer")
        assert sorted(int(layer.attrib["Id"]) for layer in layers) == list(range(len(layers)))
        changed = False
        for sheet in root.findall("Content/Spritesheets/Spritesheet"):
            # Isaac ANM2 sheets are relative to the animation directory.
            resolved = posixpath.normpath((Path(path).parent / sheet.attrib["Path"].replace("\\", "/")).as_posix())
            if not (RESOURCES / resolved).is_file():
                # Upstream default_overlay retains a ../ placeholder path from
                # another directory, immediately replaced by Lua. Make that
                # temporary dependency valid before the native loader runs.
                candidates = list(RESOURCES.rglob(Path(resolved).name))
                assert len(candidates) == 1, f"Ambiguous/missing sheet: {path}: {resolved}"
                resolved = candidates[0].relative_to(RESOURCES).as_posix()
                corrected = posixpath.relpath(resolved, posixpath.dirname(path))
                path_repairs.append({"animation": path, "old": sheet.attrib["Path"], "new": corrected})
                sheet.set("Path", corrected)
                changed = True
            asset(resolved)
        if changed:
            patched_animations[path] = ET.tostring(root, encoding="utf-8")
        first = len(sequences)
        for sequence in root.findall("Animations/Animation"):
            frames = int(sequence.attrib["FrameNum"])
            assert frames > 0
            sequences.append([sequence.attrib["Name"], frames])
        idx = len(animation_data)
        animation_ids[path] = idx
        animation_data.append([path, root.find("Animations").attrib["DefaultAnimation"], len(layers), first, len(sequences) - first])
        return idx

    colorings = {
        "vanillaDefault": [0.8, 0.0, 0.0, 1.0],
        "vanillaHit": [0.48, 0.0, 0.0, 1.0],
        "vanillaHeal": [0.87, 0.4, 0.0, 1.0],
        "none": [1.0, 1.0, 1.0, 1.0],
        # White additive flash is explicitly a limited approximation with
        # the already verified multiplicative-only SetTint native wrapper.
        "white": [1.0, 1.0, 1.0, 1.0],
    }

    def color(body, key, fallback):
        match = re.search(r"\b" + key + r"\s*=\s*HPBars.BarColorings.(\w+)", body)
        if match:
            return colorings[match[1]]
        match = re.search(r"\b" + key + r"\s*=\s*Color\(([^)]+)\)", body)
        return [float(v.strip()) for v in match[1].split(",")[:4]] if match else fallback

    styles, style_ids = [], {}
    used_styles = ["Default"] + sorted({string_field(v, "barStyle", "Default") for v in raw_bosses.values()} - {"Default"})
    for name in used_styles:
        body = raw_styles[name]
        default = raw_styles["Default"]
        sheet = asset(string_field(body, "sprite", string_field(default, "sprite")))
        anim = animation(string_field(body, "barAnm2", string_field(default, "barAnm2")))
        overlay_path = string_field(body, "overlayAnm2")
        overlay_sheet = string_field(body, "overlaySprite")
        if overlay_sheet:
            asset(overlay_sheet)
        style_ids[name] = len(styles)
        styles.append([name, sheet, anim, string_field(body, "barAnimationType", "HP") == "Animated",
                       animation(overlay_path) if overlay_path else -1, overlay_sheet,
                       string_field(body, "overlayAnimationType", "HP") == "Animated",
                       color(body, "idleColoring", colorings["vanillaDefault"]),
                       color(body, "hitColoring", colorings["vanillaHit"]),
                       color(body, "healColoring", colorings["vanillaHeal"])])

    rule_kinds = {"animationNameStartsWith": "AnimationStarts", "animationNameEndsWith": "AnimationEnds",
                  "animationNameContains": "AnimationContains", "animationNameEqual": "AnimationEquals",
                  "isHPSmaller": "HpBelow", "isHPSmallerPercent": "PercentBelow", "isStageType": "StageType",
                  "isAbsoluteStage": "AbsoluteStage"}
    enum_values = {"StageType.STAGETYPE_REPENTANCE": 4, "StageType.STAGETYPE_REPENTANCE_B": 5,
                   "LevelStage.STAGE4_1": 7, "LevelStage.STAGE4_2": 8}
    rules, unsupported, bosses = [], [], []
    default_icon = asset(ICON + "boss.png")
    default_icon_animation = animation(ICON + "bosshp_icon_32px.anm2")
    for key, body in sorted(raw_bosses.items(), key=lambda pair: tuple(map(int, pair[0].split("."))) if pair[0] != "UNDEFINED" else (-1, -1)):
        if key == "UNDEFINED":
            continue
        t, v = map(int, key.split("."))
        icon = asset(string_field(body, "sprite", default_icon))
        anim = animation(string_field(body, "iconAnm2", ICON + "bosshp_icon_32px.anm2"))
        offset = re.search(r"\boffset\s*=\s*Vector\(\s*([-\d.]+)\s*,\s*([-\d.]+)\s*\)", body)
        xy = [float(offset[1]), float(offset[2])] if offset else [-4.0, 0.0]
        first = len(rules)
        if cond := re.search(r"\bconditionalSprites\s*=\s*\{", body):
            conditional_text, _ = braces(body, cond.end() - 1)
            pos = 0
            while pos < len(conditional_text):
                begin = conditional_text.find("{", pos)
                if begin < 0:
                    break
                value, pos = braces(conditional_text, begin)
                match = re.fullmatch(r'\s*"(\w+)"\s*,\s*path\s*\.\.\s*"([^"]+)"\s*(?:,\s*\{\s*(.*?)\s*\})?\s*,?\s*', value, re.S)
                if not match or match[1] not in rule_kinds:
                    unsupported.append({"boss": key, "condition": match[1] if match else "custom Lua function"})
                    continue
                arg = (match[3] or "").strip()
                text_value, numeric_value = None, 0.0
                if match[1].startswith("animationName"):
                    text_value = json.loads(arg)
                    # These strings currently contain no Lua pattern syntax.
                    assert re.fullmatch(r"[A-Za-z0-9_ ]+", text_value)
                else:
                    numeric_value = float(enum_values[arg] if arg in enum_values else arg)
                rules.append([rule_kinds[match[1]], asset(ICON + match[2]), text_value, numeric_value])
        bosses.append([t, v, icon, anim, style_ids[string_field(body, "barStyle", "Default")],
                       string_field(body, "iconAnimationType", "HP") == "Animated", *xy, first, len(rules) - first])

    ignores = sorted([list(map(int, key.split("."))) for key in re.findall(r'\["(\d+\.\d+)"\]\s*=\s*true', table(source, "BossIgnoreList"))])
    ignore_source = table(source, "BossIgnoreList")
    parent_ignores = sorted([list(map(int, key.split("."))) for key in re.findall(
        r'\["(\d+\.\d+)"\]\s*=\s*function\(entity\)[^\n]*\n\s*return entity\.Parent ~= nil', ignore_source)])
    child_ignores = sorted([list(map(int, key.split("."))) for key in re.findall(
        r'\["(\d+\.\d+)"\]\s*=\s*function\(entity\)[^\n]*\n\s*return entity\.Child == nil', ignore_source)])
    assert len(parent_ignores) == 11 and child_ignores == [[84, 10]], "Review changed upstream ownership rules"
    # All ordinary bars use the original transparent overrides. Gideon's
    # separate wave display stays vanilla until its full renderer is ported.
    overrides = ["gfx/ui/ui_bosshealthbar.png", "gfx/ui/ui_bosshealthbar_mini.png", "gfx/ui/ui_bosshealthbar_static.png"]
    for path in overrides:
        assert (RESOURCES / path).is_file()

    lines = ["// Generated by tools/boss_ui/generate.py from the pinned upstream mod.", "#pragma once", "#include <cstdint>",
             "namespace isaac::boss_ui_data {", "struct Color { float r, g, b, a; };",
             "struct Sequence { const char* name; unsigned frames; };",
             "struct Animation { const char* path; const char* defaultName; unsigned layers, sequenceStart, sequenceCount; };",
             "enum class Condition : std::uint8_t { AnimationStarts, AnimationEnds, AnimationContains, AnimationEquals, HpBelow, PercentBelow, StageType, AbsoluteStage };",
             "struct Rule { Condition condition; const char* sprite; const char* text; float value; };",
             "struct Style { const char* name; const char* sprite; unsigned animation; bool animated; int overlayAnimation; const char* overlaySprite; bool overlayAnimated; Color idle, hit, heal; };",
             "struct Boss { unsigned type, variant; const char* sprite; unsigned animation, style; bool animated; float x, y; unsigned ruleStart, ruleCount; };",
             "struct Ignored { unsigned type, variant; };", f"inline constexpr char UpstreamCommit[] = {cpp(PIN)};"]
    for type_name, name, values in [("Sequence", "Sequences", sequences), ("Animation", "Animations", animation_data), ("Style", "Styles", styles), ("Boss", "Bosses", bosses), ("Ignored", "Ignores", ignores), ("Ignored", "IgnoreWhenParent", parent_ignores), ("Ignored", "IgnoreWithoutChild", child_ignores)]:
        lines += [f"inline constexpr {type_name} {name}[] = {{"] + ["    " + cpp(value) + "," for value in values] + ["};"]
    lines += ["inline constexpr Rule Rules[] = {"] + ["    {Condition::" + r[0] + ", " + ", ".join(cpp(v) for v in r[1:]) + "}," for r in rules] + ["};"]
    lines += ["inline constexpr Boss DefaultBoss = " + cpp([0, 0, default_icon, default_icon_animation, 0, False, -4.0, 0.0, 0, 0]) + ";", "} // namespace isaac::boss_ui_data", ""]
    HEADER.parent.mkdir(parents=True, exist_ok=True)
    HEADER.write_text("\n".join(lines), encoding="utf-8", newline="\n")

    for path in sorted(assets | set(overrides)):
        destination = STAGED / path
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(patched_animations.get(path, (RESOURCES / path).read_bytes()))

    def record(path):
        raw = (STAGED / path).read_bytes()
        rec = {"source": (STAGED / path).relative_to(PROJECT).as_posix(), "resource_path": path,
               "upstream_source": (RESOURCES / path).relative_to(PROJECT).as_posix(),
               "sha256": hashlib.sha256(raw).hexdigest(), "bytes": len(raw),
               "kind": "png_to_rgba_pcx" if path.endswith(".png") else "copy"}
        if path.endswith(".png"):
            assert raw.startswith(b"\x89PNG\r\n\x1a\n")
            rec["width"], rec["height"] = struct.unpack(">II", raw[16:24])
            rec["converted_resource_path"] = path[:-4] + ".pcx"
        return rec

    manifest = {"schema": 1, "feature": "boss_ui", "upstream_commit": PIN,
                "destination_roots": ["resources", "rp_patch/resources"],
                "groups": {"boss_ui_assets": [record(path) for path in sorted(assets)],
                           "vanilla_bar_overrides": [record(path) for path in overrides]},
                "preserve_vanilla": ["gfx/ui/ui_bosshealthbar_stone.png"],
                "notes": ["PNG names in ANM2 and native calls are intentional; Switch asset loader resolves matching PCX.",
                          "Back up vanilla_bar_overrides separately before installation.",
                          "The Gideon stone bar and wave text remain vanilla; native boss UI skips the Gideon room."]}
    report = {"upstream_commit": PIN, "boss_definitions": len(bosses), "styles": len(styles), "animations": len(animation_data),
              "animation_path_repairs": path_repairs,
              "supported_phase_rules": len(rules), "unsupported_phase_rules": unsupported,
              "ownership_filters": {"ignore_with_parent": parent_ignores, "ignore_without_child": child_ignores,
                                    "delirium_ancestors": "Native renderer follows current-room parents with a 64-link bound and cycle detection."},
              "asset_files": len(assets), "transparent_vanilla_overrides": len(overrides),
              "limits": ["Original default bottom/segments layout; no configuration menu.",
                         "Parent/child ignore rules are ported; parent/child icon phase rules, NPC I1/I2 rules, champion palettes and invincibility/status icons remain incomplete.",
                         "Hit flashes that use additive white are approximated using the verified multiplicative tint API.",
                         "Gideon's original stone bar/wave counter is preserved.",
                         "Runtime ANM2 load checks establish animation readiness, not GPU texture presence; package hashes and asset closure must pass too."]}
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "asset-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (OUT / "import-report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    (OUT / "model.json").write_text(json.dumps({"animations": animation_data, "sequences": sequences,
                                               "styles": styles, "bosses": bosses, "rules": rules}, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({k: v for k, v in report.items() if k not in {"limits", "unsupported_phase_rules"}}, indent=2))


if __name__ == "__main__":
    main()

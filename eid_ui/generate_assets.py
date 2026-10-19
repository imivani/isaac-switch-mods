#!/usr/bin/env python3
"""Build exact EID bitmap-font metrics and isolated sprite resources.

No raster artwork is edited. Existing PNGs are copied byte-for-byte; ANM2
frames expose each required glyph using its original BMFont crop and offsets.
The package owner performs the Switch texture-format conversion separately.
"""
from __future__ import annotations
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT / "reference/External-Item-Descriptions"
COMMIT = "8b59d00c6828e333294639ad2b2b6875eae821cb"
OUT = ROOT / "eid_ui"
RESOURCES = OUT / "resources/gfx/codex_eid_ui"


def font_data(path):
    raw = path.read_bytes()
    if raw[:4] != b"BMF\x03":
        raise ValueError("Expected binary BMFont version 3")
    blocks = {}
    position = 4
    while position < len(raw):
        kind, length = struct.unpack_from("<BI", raw, position)
        position += 5
        if position + length > len(raw):
            raise ValueError("Truncated font block")
        blocks[kind] = raw[position:position + length]
        position += length
    common = struct.unpack("<HHHHHBBBBB", blocks[2])
    pages = [p.decode("utf-8") for p in blocks[3].split(b"\0") if p]
    glyphs = {}
    for values in struct.iter_unpack("<IHHHHhhhBB", blocks[4]):
        keys = ("code", "x", "y", "width", "height", "xoffset", "yoffset", "advance", "page", "channel")
        glyphs[values[0]] = dict(zip(keys, values))
    if blocks.get(5):
        raise ValueError("This renderer requires explicit kerning support for this font revision")
    return {"line_height": common[0], "base": common[1], "width": common[2], "height": common[3],
            "pages": pages, "glyphs": glyphs}


def frame(**overrides):
    data = {"XPosition": "0", "YPosition": "0", "XScale": "100", "YScale": "100", "Delay": "1",
            "Visible": "true", "RedTint": "255", "GreenTint": "255", "BlueTint": "255", "AlphaTint": "255",
            "RedOffset": "0", "GreenOffset": "0", "BlueOffset": "0", "Rotation": "0", "Interpolated": "false"}
    data.update({key: str(value) for key, value in overrides.items()})
    return ET.Element("Frame", data)


def actor(sheets, animation, count):
    root = ET.Element("AnimatedActor")
    ET.SubElement(root, "Info", {"CreatedBy": "Isaac Switch EID UI importer", "Version": "1", "Fps": "30"})
    content = ET.SubElement(root, "Content")
    textures = ET.SubElement(content, "Spritesheets")
    layers = ET.SubElement(content, "Layers")
    for index, path in enumerate(sheets):
        ET.SubElement(textures, "Spritesheet", {"Path": path, "Id": str(index)})
        ET.SubElement(layers, "Layer", {"Name": str(index), "Id": str(index), "SpritesheetId": str(index)})
    ET.SubElement(content, "Nulls")
    ET.SubElement(content, "Events")
    animations = ET.SubElement(root, "Animations", {"DefaultAnimation": animation})
    anm = ET.SubElement(animations, "Animation", {"Name": animation, "FrameNum": str(count), "Loop": "false"})
    ET.SubElement(anm, "RootAnimation").append(frame(Delay=count))
    return root, ET.SubElement(anm, "LayerAnimations"), anm


def write_xml(path, root, animation):
    ET.SubElement(animation, "NullAnimations")
    ET.SubElement(animation, "Triggers")
    ET.indent(root, space="\t")
    ET.ElementTree(root).write(path, encoding="utf-8", xml_declaration=False)


def parse_icons(text):
    start = text.index("EID.InlineIcons = {")
    end = text.index("-- General Stats", start)
    icons = {}
    pattern = re.compile(r'^\s*\["([^"\n]+)"\]\s*=\s*\{"([^"\n]+)",\s*(-?\d+),\s*(\d+),\s*(\d+)([^\n]*)$', re.M)
    for match in pattern.finditer(text[start:end]):
        name, animation, number, width, height, tail = match.groups()
        offsets = re.match(r"\s*,\s*(-?\d+)(?:\s*,\s*(-?\d+))?", tail)
        left = int(offsets[1]) if offsets else -1
        top = int(offsets[2]) if offsets and offsets[2] else 0
        sheet = 1 if "EID.PlayerSprite" in tail else 2 if "EID.IconSprite" in tail else 0
        if "controllerSprites" in tail or "getControllerSprite" in tail:
            sheet, animation = 4, "Switch"
        icons[name] = {"name": name, "animation": animation, "frame": int(number), "width": int(width),
                       "height": int(height), "left": left, "top": top, "sheet": sheet}
    for name in ("Damage", "Speed", "Tears", "Range", "Shotspeed", "Luck", "AngelChance", "DevilChance", "Tearsize"):
        icons[name] = dict(icons[name + "REP"], name=name)
    return icons


def cpp_string(text):
    return json.dumps(text, ensure_ascii=True)


def main():
    head = subprocess.check_output(["git", "-C", str(UPSTREAM), "rev-parse", "HEAD"], text=True).strip()
    if head != COMMIT:
        raise SystemExit(f"Source revision differs: {head}")
    RESOURCES.mkdir(parents=True, exist_ok=True)
    generated = OUT / "generated"
    generated.mkdir(exist_ok=True)
    catalogue = json.loads((ROOT / "eid/generated/descriptions.json").read_text(encoding="utf-8"))
    data_lua = (UPSTREAM / "features/eid_data.lua").read_text(encoding="utf-8")
    font = font_data(UPSTREAM / "resources/font/eid_default.fnt")
    text = "".join(entry["name"] + entry["markup"] for entry in catalogue["entries"])
    context = json.loads((ROOT / "eid/generated/context.json").read_text(encoding="utf-8"))
    text += "".join(row["name"] + row["markup"] for rows in context["groups"].values() for row in rows)
    text += "".join(row["new"] for row in context["conditions"])
    wanted_codes = sorted(set(map(ord, text)) | set(range(32, 127)) | {7, 0x2022, 0x2026})
    glyphs = []
    missing_codes = []
    for code in wanted_codes:
        if code < 32 and code != 7:
            continue
        glyph = font["glyphs"].get(code)
        if glyph:
            glyphs.append(dict(glyph, frame=len(glyphs)))
        else:
            missing_codes.append(code)
    for path in font["pages"]:
        shutil.copyfile(UPSTREAM / "resources/font" / path, RESOURCES / path)
    root, layers, animation = actor(font["pages"], "Glyphs", len(glyphs))
    for page in range(len(font["pages"])):
        layer = ET.SubElement(layers, "LayerAnimation", {"LayerId": str(page), "Visible": "true"})
        for glyph in glyphs:
            visible = glyph["page"] == page and glyph["width"] > 0 and glyph["height"] > 0
            layer.append(frame(XPosition=glyph["xoffset"], YPosition=glyph["yoffset"], XPivot=0, YPivot=0,
                               XCrop=glyph["x"], YCrop=glyph["y"], Width=glyph["width"], Height=glyph["height"],
                               Visible="true" if visible else "false"))
    write_xml(RESOURCES / "eid_glyphs.anm2", root, animation)
    icons = parse_icons(data_lua)
    wanted_icons = set(re.findall(r"\{\{([^{}]+)\}\}", text)) | {"ArrowUp", "ArrowDown", "Warning", "Collectible", "Trinket", "Card", "Pill"}
    wanted_icons |= {"SacrificeRoom", "DiceRoom", "Guppy", "Beelzebub", "FunGuy", "Seraphim", "Bob", "Spun", "Mom", "Conjoined", "Leviathan", "OhCrap", "Bookworm", "Adult", "SpiderBaby"}
    wanted_icons |= {"Quality" + str(n) for n in range(5)} | {"Player" + str(n) for n in range(41)}
    wanted_icons |= {"Card" + str(n) for n in range(1, 98)} | {"Pill" + str(n) for n in range(1, 15)}
    # Preserve reference artwork for all static tokens used by the imported data.
    # Numbered item references are resolved by native item config at render time.
    for token in wanted_icons:
        match = re.fullmatch(r"(Card|Pill)(\d+)", token)
        if match:
            icons[token] = {"name": token, "animation": "Cards" if match[1] == "Card" else "Pills",
                            "frame": int(match[2]) - 1, "width": 8 if match[1] == "Card" else 9,
                            "height": 8, "left": 0, "top": 1, "sheet": 3}
    icons = {key: value for key, value in icons.items() if key in wanted_icons}
    sheets = ["eid_inline_icons", "eid_player_icons", "eid_transform_icons", "eid_cardspills"]
    source_files = ["features/eid_data.lua", "resources/font/eid_default.fnt"]
    for sheet in sheets:
        for extension in (".anm2", ".png"):
            name = sheet + extension
            shutil.copyfile(UPSTREAM / "resources/gfx" / name, RESOURCES / name)
            source_files.append("resources/gfx/" + name)
    for page in font["pages"]:
        source_files.append("resources/font/" + page)
    # A single item image, without the collectible pedestal or a background.
    root, layers, animation = actor(["eid_inline_icons.png"], "Item", 1)
    layer = ET.SubElement(layers, "LayerAnimation", {"LayerId": "0", "Visible": "true"})
    layer.append(frame(XPivot=0, YPivot=0, XCrop=0, YCrop=0, Width=32, Height=32, XScale=50, YScale=50))
    write_xml(RESOURCES / "item_icon.anm2", root, animation)
    unsupported = sorted(token for token in wanted_icons if token not in icons and
                         not re.fullmatch(r"(Collectible|Trinket)\d+", token) and
                         not token.startswith("Color") and token not in ("CR", "NoLB", "Blank", "Indent", "IND", "Reset", "BlinkBirthright", "BlinkYellowGreen"))
    if unsupported:
        raise ValueError(f"Unclassified EID markup: {unsupported}")
    header = ["// Generated by eid_ui/generate_assets.py from pinned EID assets. Do not hand-edit.",
              "#pragma once", "#include <cstddef>", "#include <cstdint>", "namespace isaac_eid_ui::assets {",
              "struct Glyph { std::uint32_t code; std::uint16_t frame; std::int16_t advance; };",
              "struct Icon { const char* name; const char* animation; std::uint16_t frame; std::int16_t width, height, left, top; std::uint8_t sheet; };",
              f"inline constexpr unsigned LineHeight = {font['line_height']};", "inline constexpr Glyph Glyphs[] = {"]
    header.extend(f"    {{{g['code']}, {g['frame']}, {g['advance']}}}," for g in glyphs)
    header += ["};", "inline constexpr Icon Icons[] = {"]
    header.extend("    {%s, %s, %d, %d, %d, %d, %d, %d}," %
                  (cpp_string(i["name"]), cpp_string(i["animation"]), i["frame"], i["width"], i["height"], i["left"], i["top"], i["sheet"])
                  for _, i in sorted(icons.items()))
    header += ["};", "} // namespace isaac_eid_ui::assets", ""]
    (generated / "assets.hpp").write_text("\n".join(header), encoding="utf-8")
    metadata = {"font": {key: value for key, value in font.items() if key != "glyphs"}, "glyphs": glyphs,
                "missing_codepoints": missing_codes, "icons": icons, "source_commit": COMMIT}
    (generated / "assets.json").write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    manifest = {"source_url": "https://github.com/wofsauge/External-Item-Descriptions", "source_commit": COMMIT,
                "source_sha256": {name: hashlib.sha256((UPSTREAM / name).read_bytes()).hexdigest() for name in source_files},
                "logical_root": "gfx/codex_eid_ui", "texture_conversion": "Parent stages these PNGs as compatible Switch textures; ANM2 keeps logical .png names.",
                "files": [{"source": str(path.relative_to(OUT)).replace("\\", "/"), "logical_path": "gfx/codex_eid_ui/" + path.name,
                           "sha256": hashlib.sha256(path.read_bytes()).hexdigest()} for path in sorted(RESOURCES.iterdir())],
                "native_game_assets": ["gfx/ui/buttons.anm2"], "glyphs": len(glyphs), "icons": len(icons),
                "unclassified_markup": unsupported, "missing_codepoints": missing_codes}
    (OUT / "assets-manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    shutil.copyfile(UPSTREAM / "CREDITS.md", OUT / "UPSTREAM-CREDITS.md")
    print(json.dumps({"glyphs": len(glyphs), "icons": len(icons), "resources": len(manifest["files"]),
                      "missing_codepoints": missing_codes, "unclassified_markup": unsupported}))


if __name__ == "__main__":
    main()

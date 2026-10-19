#!/usr/bin/env python3
"""Render the C++ layout with real EID pixels; this is not a console screenshot."""
from __future__ import annotations
import importlib.util
import json
from pathlib import Path
import subprocess
import xml.etree.ElementTree as ET
from PIL import Image, ImageChops, ImageDraw

ROOT = Path(__file__).resolve().parents[1]
HERE = ROOT / "eid_ui"
RES = HERE / "resources/gfx/codex_eid_ui"
DUMP = ROOT.parent / "Isaac-dump/romfs"
META = json.loads((HERE / "generated/assets.json").read_text(encoding="utf-8"))
GLYPHS = {g["code"]: g for g in META["glyphs"]}
ICONS = [value for _, value in sorted(META["icons"].items())]
SHEETS = [RES / name for name in ("eid_inline_icons.anm2", "eid_player_icons.anm2", "eid_transform_icons.anm2", "eid_cardspills.anm2")]
SHEETS += [DUMP / "rp_patch/resources/gfx/ui/buttons.anm2"]
SPEC = importlib.util.spec_from_file_location("texture_codec", ROOT / "tools/convert_ui_textures.py")
CODEC = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CODEC)
IMAGE_CACHE = {}
ANIMATION_CACHE = {}
ITEM_CACHE = {}


def image(path):
    path = Path(path)
    if not path.exists() and path.suffix.lower() == ".png":
        path = path.with_suffix(".pcx")
    if path not in IMAGE_CACHE:
        IMAGE_CACHE[path] = CODEC.decode_pcx(path.read_bytes()) if path.suffix.lower() == ".pcx" else Image.open(path).convert("RGBA")
    return IMAGE_CACHE[path]


def tint(pixels, rgb, alpha=.92):
    color = ((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, round(alpha * 255))
    return ImageChops.multiply(pixels, Image.new("RGBA", pixels.size, color))


def glyph(canvas, code, x, y, color=0xffffff):
    data = GLYPHS.get(code, GLYPHS[ord('?')])
    page = image(RES / META["font"]["pages"][data["page"]])
    pixels = page.crop((data["x"], data["y"], data["x"] + data["width"], data["y"] + data["height"]))
    canvas.alpha_composite(tint(pixels, color), (round(x + data["xoffset"]), round(y + data["yoffset"])))


def string(canvas, text, x, y, color=0xffffff):
    for character in text:
        glyph(canvas, ord(character), x, y, color)
        x += GLYPHS.get(ord(character), GLYPHS[ord('?')])["advance"]


def icon(canvas, definition, x, y):
    path = SHEETS[definition["sheet"]]
    if path not in ANIMATION_CACHE:
        ANIMATION_CACHE[path] = ET.parse(path).getroot()
    root = ANIMATION_CACHE[path]
    sheets = {int(s.attrib["Id"]): s.attrib["Path"] for s in root.findall("./Content/Spritesheets/Spritesheet")}
    layers = {int(s.attrib["Id"]): int(s.attrib["SpritesheetId"]) for s in root.findall("./Content/Layers/Layer")}
    animation = next(a for a in root.findall("./Animations/Animation") if a.attrib["Name"] == definition["animation"])
    for layer in animation.findall("./LayerAnimations/LayerAnimation"):
        if layer.attrib.get("Visible") == "false":
            continue
        remaining = definition["frame"]
        selected = None
        for candidate in layer.findall("Frame"):
            selected = candidate
            duration = int(candidate.attrib["Delay"])
            if remaining < duration:
                break
            remaining -= duration
        if selected is None or selected.attrib.get("Visible") == "false":
            continue
        f = selected.attrib
        sheet_path = path.parent / sheets[layers[int(layer.attrib["LayerId"])]]
        pixels = image(sheet_path).crop((int(f["XCrop"]), int(f["YCrop"]), int(f["XCrop"]) + int(f["Width"]), int(f["YCrop"]) + int(f["Height"])))
        sx, sy = float(f.get("XScale", 100)) / 100, float(f.get("YScale", 100)) / 100
        pixels = pixels.resize((max(1, round(pixels.width * sx)), max(1, round(pixels.height * sy))), Image.Resampling.NEAREST)
        left = x + definition["left"] + float(f.get("XPosition", 0)) - float(f.get("XPivot", 0)) * sx
        top = y + definition["top"] + float(f.get("YPosition", 0)) - float(f.get("YPivot", 0)) * sy
        canvas.alpha_composite(tint(pixels, 0xffffff), (round(left), round(top)))


def item(canvas, category, number, x, y):
    key = category, number
    if key not in ITEM_CACHE:
        config = ET.parse(DUMP / "rp_patch/resources/items.xml").getroot()
        row = next((r for r in config if r.attrib.get("id") == str(number) and (r.tag == "trinket") == (category == 350)), None)
        ITEM_CACHE[key] = None
        if row is not None:
            name = Path(row.attrib["gfx"]).with_suffix(".pcx").name.lower()
            directory = "trinkets" if category == 350 else "collectibles"
            for base in (DUMP / "rp_patch/resources", DUMP / "resources"):
                path = base / "gfx/items" / directory / name
                if path.exists():
                    ITEM_CACHE[key] = image(path).crop((0, 0, 32, 32)).resize((16, 16), Image.Resampling.NEAREST)
                    break
    if ITEM_CACHE[key] is not None:
        canvas.alpha_composite(tint(ITEM_CACHE[key], 0xffffff, .96), (round(x), round(y - 2)))


def get_layout(category, number):
    executable = "%USERPROFILE%/Desktop/sw/Isaac-switch-mods/eid_ui/test-build/eid-ui-optimized"
    output = subprocess.check_output(["wsl", "-d", "IsaacDevkit", "--exec", executable, "--preview", str(category), str(number)], text=True)
    return [json.loads(line) for line in output.splitlines()]


def draw_layout(canvas, layout, x, y, first=0, rows=999):
    for kind, value, color, cx, cy, _ in layout["cells"]:
        line = cy // 11
        if not first <= line < first + rows:
            continue
        px, py = x + cx, y + cy - first * 11
        if kind == 0:
            glyph(canvas, value, px, py, color)
        elif kind == 1:
            icon(canvas, ICONS[value], px, py)
        else:
            item(canvas, 350 if kind == 3 else 100, value, px, py)


def main():
    cases = [(100, 1), (100, 182), (100, 584), (350, 39)]
    columns = []
    for category, number in cases:
        title, body = get_layout(category, number)
        # Same 45px top and 11px baseline spacing as the native renderer;
        # background is deliberately neutral, not a fabricated game screenshot.
        canvas = Image.new("RGBA", (225, 270), (43, 37, 35, 255))
        x, top = 35, 45
        body_top = top + max(17, title["line_count"] * 11 + 4)
        item(canvas, category, number, x - 2, top - 1)
        draw_layout(canvas, title, x + 18, top - 2)
        rows = int((270 - body_top - 42) / 11)
        draw_layout(canvas, body, x, body_top, rows=rows)
        pages = max(1, (body["line_count"] + rows - 1) // rows)
        if pages > 1:
            string(canvas, f"1 / {pages}", x + 12, body_top + min(rows, body["line_count"]) * 11 + 2, 0xbeb4c6)
        columns.append(canvas)
    preview = Image.new("RGBA", (len(columns) * 225, 292), (28, 27, 29, 255))
    for i, canvas in enumerate(columns):
        preview.alpha_composite(canvas, (i * 225, 22))
    ImageDraw.Draw(preview).text((12, 6), "EID layout preview - original font and icon pixels; not a Switch screenshot", fill=(210, 205, 217))
    output = HERE / "preview.png"
    preview.resize((preview.width * 2, preview.height * 2), Image.Resampling.NEAREST).save(output)
    print(output)


if __name__ == "__main__":
    main()

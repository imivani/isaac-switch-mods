#!/usr/bin/env python3
"""Render menu draw lists (from preview_dump) with the game's real textures and fonts.

Not a console screenshot: EID descriptions are simplified to plain wrapped text
and the background is a flat stand-in for the game. Everything else uses the
same art crops, fonts and item pictures the Switch will draw.
"""
from __future__ import annotations
import importlib.util
import json
import struct
import sys
import xml.etree.ElementTree as ET
from pathlib import Path
from PIL import Image, ImageChops

ROOT = Path(__file__).resolve().parents[2]
DUMP = ROOT.parent / 'Isaac-dump/romfs'
LAYERS = [DUMP / 'rp_patch/resources', DUMP / 'resources']
SPEC = importlib.util.spec_from_file_location('codec', ROOT / 'tools/convert_ui_textures.py')
CODEC = importlib.util.module_from_spec(SPEC); SPEC.loader.exec_module(CODEC)
EID_SPEC = importlib.util.spec_from_file_location('eid_preview', ROOT / 'eid_ui/preview.py')
EID = importlib.util.module_from_spec(EID_SPEC); EID_SPEC.loader.exec_module(EID)
IMAGES: dict = {}


def find(relative: str) -> Path | None:
    import posixpath
    relative = posixpath.normpath(relative.replace('\\', '/'))
    for base in LAYERS:
        parts = relative.split('/')
        current = base
        ok = True
        for part in parts:
            if part == '..':
                current = current.parent; continue
            matches = [c for c in current.iterdir()] if current.is_dir() else []
            hit = next((c for c in matches if c.name.lower() == part.lower()), None)
            if hit is None and part.lower().endswith('.png'):
                hit = next((c for c in matches if c.name.lower() == part.lower()[:-4] + '.pcx'), None)
            if hit is None:
                ok = False; break
            current = hit
        if ok and current.is_file():
            return current
    return None


def image(path: Path) -> Image.Image:
    if path not in IMAGES:
        IMAGES[path] = CODEC.decode_pcx(path.read_bytes()) if path.suffix.lower() == '.pcx' else Image.open(path).convert('RGBA')
    return IMAGES[path]


def tinted(pixels: Image.Image, rgb: int, alpha: float) -> Image.Image:
    color = ((rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255, max(0, min(255, round(alpha * 255))))
    return ImageChops.multiply(pixels, Image.new('RGBA', pixels.size, color))


def paste(canvas, pixels, x, y, scale=1.0, alpha=1.0, mirror=False):
    if scale != 1:
        pixels = pixels.resize((max(1, round(pixels.width * scale)), max(1, round(pixels.height * scale))), Image.NEAREST)
    if mirror:
        pixels = pixels.transpose(Image.FLIP_LEFT_RIGHT)
    if alpha < 1:
        pixels = tinted(pixels, 0xffffff, alpha)
    layer = Image.new('RGBA', canvas.size)
    layer.paste(pixels, (round(x), round(y)))
    canvas.alpha_composite(layer)


# --- menu.anm2 ---
ANM = ET.parse(ROOT / 'assets/menu_ui/resources/gfx/codex_menu/menu.anm2').getroot()
SHEETS = {s.get('Id'): s.get('Path') for s in ANM.iter('Spritesheet')}
ART = {}
for animation in ANM.iter('Animation'):
    layer = animation.find('./LayerAnimations/LayerAnimation')
    frames = [{k: int(float(f.get(k))) for k in ('XCrop', 'YCrop', 'Width', 'Height', 'XPivot', 'YPivot', 'XScale')} for f in layer.findall('Frame')]
    ART[animation.get('Name')] = (SHEETS[layer.get('LayerId')], frames)


def art(canvas, name, frame, x, y, scale, alpha, mirror):
    sheet, frames = ART[name]
    f = frames[min(frame, len(frames) - 1)]
    path = find('gfx/codex_menu/' + sheet)
    if path is None:
        raise FileNotFoundError(sheet)
    pixels = image(path).crop((f['XCrop'], f['YCrop'], f['XCrop'] + f['Width'], f['YCrop'] + f['Height']))
    mirror = mirror or f['XScale'] < 0
    paste(canvas, pixels, x - f['XPivot'] * scale, y - f['YPivot'] * scale, scale, alpha, mirror)


# --- BMFont binary fonts ---
class Font:
    def __init__(self, relative):
        path = find(relative)
        data = path.read_bytes()
        assert data[:4] == b'BMF\x03'
        pos, self.chars, pages = 4, {}, []
        while pos < len(data):
            kind, size = data[pos], struct.unpack_from('<I', data, pos + 1)[0]
            block = data[pos + 5:pos + 5 + size]
            if kind == 2:
                self.line = struct.unpack_from('<H', block, 0)[0]
            elif kind == 3:
                pages = [p.decode() for p in block.split(b'\0') if p]
            elif kind == 4:
                for i in range(0, len(block), 20):
                    cid, x, y, w, h, xo, yo, xa, page, _ = struct.unpack_from('<IHHHHhhhBB', block, i)
                    self.chars[cid] = (x, y, w, h, xo, yo, xa, page)
            pos += 5 + size
        self.pages = [image(find(str(Path(relative).parent / p).replace('\\', '/'))) for p in pages]

    def width(self, text, scale):
        return sum(self.chars.get(ord(c), self.chars.get(ord('?'), (0,) * 6 + (6, 0)))[6] for c in text) * scale

    def draw(self, canvas, text, x, y, color, scale, alpha):
        for c in text:
            g = self.chars.get(ord(c)) or self.chars.get(ord('?'))
            if not g:
                continue
            gx, gy, w, h, xo, yo, xa, page = g
            if w and h:
                pixels = tinted(self.pages[page].crop((gx, gy, gx + w, gy + h)), color, alpha)
                paste(canvas, pixels, x + xo * scale, y + yo * scale, scale)
            x += xa * scale


FONTS = {0: Font('font/teammeatfont16bold.fnt'), 1: Font('font/teammeatfont20bold.fnt'),
         2: Font('font/pftempestasevencondensed.fnt')}


def small_width(text):
    return sum(EID.GLYPHS.get(ord(c), EID.GLYPHS[ord('?')])['advance'] for c in text)


# Every text drawn in the current scene: (x, y, width, height, colour, alpha, text, background luminance, layer).
# A full-screen dim (a card opening over the page) starts a new layer; text behind it is covered.
DRAWN = []
LAYER = [0]


def luminance(rgb):
    def channel(c):
        c /= 255
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4
    return 0.2126 * channel((rgb >> 16) & 255) + 0.7152 * channel((rgb >> 8) & 255) + 0.0722 * channel(rgb & 255)


def text(canvas, p):
    value, font, scale = p['text'], p['f'], p['scale']
    measure = (lambda t, s: small_width(t)) if font == 3 else (lambda t, s: FONTS[font].width(t, s))
    width, y = measure(value, scale), p['y']
    if p['w'] > 0 and width > p['w']:
        # As mod_menu.cpp DrawText: shrink up to 15%, then shorten with "...".
        if font != 3:
            fitted = max(scale * p['w'] / width, p['scale'] * .85)
            y += (scale - fitted) * 6
            scale = fitted
            width = measure(value, scale)
        if width > p['w']:
            while value and measure(value + '...', scale) > p['w']:
                value = value[:-1]
            value += '...'
            width = measure(value, scale)
    x = p['x'] - (width / 2 if p['a'] == 1 else width if p['a'] == 2 else 0)
    height = 9 if font == 3 else FONTS[font].line * scale * .75
    # Background under the text before it is drawn, for the contrast audit.
    left, top = min(479, max(0, round(x))), min(269, max(0, round(y)))
    box = canvas.crop((left, top, max(left + 1, min(480, round(x + width))), max(top + 1, min(270, round(y + height))))).convert('RGB')
    raw = box.tobytes()
    pixels = sorted(luminance((raw[i] << 16) | (raw[i + 1] << 8) | raw[i + 2]) for i in range(0, len(raw), 3))
    DRAWN.append((x, y, width, height, p['color'], p['alpha'], value, pixels[len(pixels) // 2], LAYER[0]))
    if font == 3:
        cx = x
        for c in value:
            data = EID.GLYPHS.get(ord(c), EID.GLYPHS[ord('?')])
            page = EID.image(EID.RES / EID.META['font']['pages'][data['page']])
            pixels = tinted(page.crop((data['x'], data['y'], data['x'] + data['width'], data['y'] + data['height'])), p['color'], p['alpha'])
            paste(canvas, pixels, cx + data['xoffset'], p['y'] + data['yoffset'])
            cx += data['advance']
    else:
        FONTS[font].draw(canvas, value, x, y, p['color'], scale, p['alpha'])


ITEMS = {}
for element in ET.parse(ROOT / 'features/balance/resources/items.xml').getroot():
    if element.get('id') and element.get('gfx'):
        ITEMS[(element.tag == 'trinket', int(element.get('id')))] = element.get('gfx')


def item(canvas, p):
    gfx = ITEMS.get((bool(p['flag']), p['id']))
    folder = 'gfx/items/trinkets/' if p['flag'] else 'gfx/items/collectibles/'
    path = find(folder + gfx) if gfx else None
    if path:
        paste(canvas, image(path).crop((0, 0, 32, 32)), p['x'], p['y'], p['scale'], p['alpha'])


def icon(canvas, p):
    definition = EID.META['icons'].get(p['name'])
    if definition:
        EID.icon(canvas, definition, p['x'], p['y'])


# EID colours as menu ink (features/menu_ui/ink.hpp).
INK = {'ColorBlue': 0x1f3f9a, 'ColorOrange': 0x9a4a00, 'ColorRed': 0xa31d14, 'ColorSilver': 0x55504c, 'ColorYellow': 0x6e5200,
       'ColorEIDObjName': 0x7d2a7d, 'ColorObjName': 0x7d2a7d, 'ColorGray': 0x5e5753, 'ColorLime': 0x2b6b1f,
       'ColorCyan': 0x0f6670, 'ColorEIDTransform': 0x4a3fa0, 'ColorGold': 0x7d5000, 'ColorShinyPurple': 0x6a3a9a,
       'ColorIsaac': 0x7a4a48, 'BlinkBirthright': 0x7a4a48, 'BlinkYellowGreen': 0x4a6410}


def description(canvas, p):
    import re
    lines = []  # each line: list of (word, colour)
    for paragraph in p['text'].split('#'):
        colour, line, used = 0x2a1f1d, [], 0
        for token in re.findall(r'\{\{[^}]*\}\}|[^\s{]+', paragraph):
            if token.startswith('{{'):
                name = token[2:-2]
                colour = 0x2a1f1d if name in ('CR', 'ColorReset', 'Reset', 'ColorText', 'ColorEIDText', 'ColorWhite') else INK.get(name, colour)
                continue
            width = small_width(token + ' ')
            if used + width > p['w'] and line:
                lines.append(line); line, used = [], 0
            line.append((token, colour)); used += width
        if line:
            lines.append(line)
    rows = max(1, int(p['h'] // 11))
    page = max(0, p.get('frame', -1))
    for i, line in enumerate(lines[page * rows:page * rows + rows]):
        x = p['x']
        for word, colour in line:
            text(canvas, {'text': word, 'f': 3, 'scale': 1, 'w': 0, 'a': 0, 'x': x, 'y': p['y'] + 11 * i, 'color': colour, 'alpha': 1})
            x += small_width(word + ' ')


def render(scene) -> Image.Image:
    canvas = Image.new('RGBA', (480, 270), (74, 58, 50, 255))
    for i in range(0, 480, 26):  # floor-tile stand-in for the game behind the menu
        for j in range(0, 270, 26):
            if (i // 26 + j // 26) % 2:
                canvas.paste((82, 64, 55, 255), (i, j, i + 26, j + 26))
    for p in scene['paint']:
        kind = p['k']
        if kind == 0:
            if p['w'] >= 1000:
                LAYER[0] += 1
            layer = Image.new('RGBA', canvas.size)
            c = p['color']
            box = (round(p['x']), round(p['y']), round(p['x'] + p['w']), round(p['y'] + p['h']))
            layer.paste(((c >> 16) & 255, (c >> 8) & 255, c & 255, round(255 * max(0, min(1, p['alpha'])))), box)
            canvas.alpha_composite(layer)
        elif kind == 1:
            art(canvas, p['name'], p['frame'], p['x'], p['y'], p['scale'], p['alpha'], bool(p['flag']))
        elif kind == 2:
            text(canvas, p)
        elif kind == 3:
            item(canvas, p)
        elif kind == 4:
            icon(canvas, p)
        elif kind == 5:
            description(canvas, p)
        elif kind == 6:
            value = 'Hits: 3  Floor: 1'
            w = FONTS[2].width(value, 1)
            FONTS[2].draw(canvas, value, 0, 240, 0xffffff, 1, 1)
            if p['flag']:
                layer = Image.new('RGBA', canvas.size)
                from PIL import ImageDraw
                ImageDraw.Draw(layer).rectangle((-3, 238, w + 3, 252), outline=(228, 180, 237, 240))
                canvas.alpha_composite(layer)
    return canvas


def audit(name):
    """Readability problems in one rendered scene: text past the screen, text
    over other text, and dark text on a dark background (or light on light)."""
    issues = []
    if name.startswith(('open-', 'closing-', 'mods-turning')):
        return issues  # mid-animation: papers are still sliding in or out
    top = max((d[8] for d in DRAWN), default=0)
    visible = [d for d in DRAWN if d[5] >= .5 and d[6].strip() and (d[8] == top or d[1] > 240)]
    for x, y, w, h, colour, alpha, value, background, _ in visible:
        if x < -0.5 or x + w > 480.5 or y < -0.5 or y + h > 270.5:
            issues.append(f'{name}: "{value}" runs off the screen')
        fg = luminance(colour)
        ratio = (max(fg, background) + .05) / (min(fg, background) + .05)
        if alpha >= .9 and colour != 0x9a8c84 and ratio < 4.5:
            issues.append(f'{name}: "{value}" has {ratio:.1f}:1 contrast against what is behind it')
    for i, a in enumerate(visible):
        for b in visible[i + 1:]:
            if abs(a[0] - b[0]) < 1.5 and abs(a[1] - b[1]) < 1.5:
                continue  # a shadow or stamp drawn twice on purpose
            overlap_x = min(a[0] + a[2], b[0] + b[2]) - max(a[0], b[0])
            overlap_y = min(a[1] + a[3], b[1] + b[3]) - max(a[1], b[1])
            if overlap_x > 1 and overlap_y > 2:
                issues.append(f'{name}: "{a[6]}" overlaps "{b[6]}"')
    return issues


def export_metrics(path):
    """Glyph advances for preview_dump.cpp, so its layout measures like the game."""
    lines = []
    for font in (0, 1, 2):
        lines += [f'{font} {code} {FONTS[font].chars[code][6]}' for code in range(32, 127) if code in FONTS[font].chars]
    lines += [f'3 {code} {EID.GLYPHS[code]["advance"]}' for code in range(32, 127) if code in EID.GLYPHS]
    Path(path).write_text('\n'.join(lines) + '\n')
    print(f'Wrote {len(lines)} glyph advances to {path}')


def main():
    if len(sys.argv) > 2 and sys.argv[1] == '--metrics':
        export_metrics(sys.argv[2])
        return
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'evidence/menu/preview/paints.json'
    out = source.parent
    scenes = json.loads(source.read_text())
    images, issues = [], []
    for scene in scenes:
        DRAWN.clear()
        LAYER[0] = 0
        frame = render(scene)
        issues += audit(scene['name'])
        frame = frame.resize((960, 540), Image.NEAREST)
        frame.save(out / f"{scene['name']}.png")
        images.append((scene['name'], frame))
    (out / 'audit.json').write_text(json.dumps(issues, indent=2) + '\n')
    print('\n'.join(issues) if issues else 'Readability audit: no text off screen, overlapping or below 4.5:1')
    columns = 3
    rows = (len(images) + columns - 1) // columns
    sheet = Image.new('RGBA', (columns * 480, rows * 270), (0, 0, 0, 255))
    for index, (_, frame) in enumerate(images):
        sheet.paste(frame.resize((480, 270), Image.LANCZOS), ((index % columns) * 480, (index // columns) * 270))
    sheet.save(out / 'contact-sheet.png')
    print(f'Rendered {len(images)} preview scenes to {out}')


if __name__ == '__main__':
    main()

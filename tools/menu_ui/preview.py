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


def text(canvas, p):
    value, font, scale = p['text'], p['f'], p['scale']
    if font == 3:
        width = small_width(value)
    else:
        width = FONTS[font].width(value, scale)
    if p['w'] > 0 and width > p['w']:
        while value and (small_width(value + '...') if font == 3 else FONTS[font].width(value + '...', scale)) > p['w']:
            value = value[:-1]
        value += '...'
        width = small_width(value) if font == 3 else FONTS[font].width(value, scale)
    x = p['x'] - (width / 2 if p['a'] == 1 else width if p['a'] == 2 else 0)
    if font == 3:
        cx = x
        for c in value:
            data = EID.GLYPHS.get(ord(c), EID.GLYPHS[ord('?')])
            page = EID.image(EID.RES / EID.META['font']['pages'][data['page']])
            pixels = tinted(page.crop((data['x'], data['y'], data['x'] + data['width'], data['y'] + data['height'])), p['color'], p['alpha'])
            paste(canvas, pixels, cx + data['xoffset'], p['y'] + data['yoffset'])
            cx += data['advance']
    else:
        FONTS[font].draw(canvas, value, x, p['y'], p['color'], scale, p['alpha'])


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


def description(canvas, p):
    import re
    raw = re.sub(r'\{\{[^}]*\}\}', '', p['text'])
    lines = []
    for paragraph in raw.split('#'):
        words, line = paragraph.split(), ''
        for word in words:
            candidate = (line + ' ' + word).strip()
            if small_width(candidate) > p['w'] and line:
                lines.append(line); line = word
            else:
                line = candidate
        if line:
            lines.append(line)
    for i, line in enumerate(lines[:max(1, int(p['h'] // 10))]):
        text(canvas, {'text': line, 'f': 3, 'scale': 1, 'w': 0, 'a': 0, 'x': p['x'], 'y': p['y'] + 10 * i, 'color': 0x2a1f1d, 'alpha': 1})


def render(scene) -> Image.Image:
    canvas = Image.new('RGBA', (480, 270), (74, 58, 50, 255))
    for i in range(0, 480, 26):  # floor-tile stand-in for the game behind the menu
        for j in range(0, 270, 26):
            if (i // 26 + j // 26) % 2:
                canvas.paste((82, 64, 55, 255), (i, j, i + 26, j + 26))
    for p in scene['paint']:
        kind = p['k']
        if kind == 0:
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


def main():
    source = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'evidence/menu/preview/paints.json'
    out = source.parent
    scenes = json.loads(source.read_text())
    images = []
    for scene in scenes:
        frame = render(scene).resize((960, 540), Image.NEAREST)
        frame.save(out / f"{scene['name']}.png")
        images.append((scene['name'], frame))
    columns = 3
    rows = (len(images) + columns - 1) // columns
    sheet = Image.new('RGBA', (columns * 480, rows * 270), (0, 0, 0, 255))
    for index, (_, frame) in enumerate(images):
        sheet.paste(frame.resize((480, 270), Image.LANCZOS), ((index % columns) * 480, (index // columns) * 270))
    sheet.save(out / 'contact-sheet.png')
    print(f'Rendered {len(images)} preview scenes to {out}')


if __name__ == '__main__':
    main()

"""Write gfx/codex_menu/menu.anm2: named crops of the game's own menu textures.

No artwork is copied. The animation file points at textures that ship with the
game (Mods, Options, Stats and Collection pages, the menu shadow, the controller
button sheet) using relative paths, exactly like the game's own menu files.
Crops come from the game's ANM2 files (see evidence/menu) or the texture grid.
Writes assets/menu_ui/resources/gfx/codex_menu/menu.anm2 and
features/menu_ui/generated/art.hpp (animation names/frames for the C++ side).
"""
import hashlib
import json
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DUMP = ROOT.parent / 'Isaac-dump/romfs'
OUT_DIR = ROOT / 'assets/menu_ui/resources/gfx/codex_menu'
SHEETS = [  # path relative to gfx/codex_menu/, game texture that must exist
    ('../ui/main menu/ModsMenu.png', 'resources/gfx/ui/main menu/modsmenu.pcx'),
    ('../ui/main menu/OptionsMenu.png', 'resources/gfx/ui/main menu/optionsmenu.pcx'),
    ('../ui/main menu/StatsMenu.png', 'resources/gfx/ui/main menu/statsmenu.pcx'),
    ('../ui/main menu/CollectionMenu.png', 'resources/gfx/ui/main menu/collectionmenu.pcx'),
    ('../ui/main menu/MenuShadow.png', 'resources/gfx/ui/main menu/menushadow.pcx'),
    ('../ui/buttons.png', 'rp_patch/resources/gfx/ui/buttons.pcx'),
]
# name: (sheet, [(x, y, w, h)], pivot)
ART = {
    'ModsHeader': (0, [(0, 0, 256, 48)], (0, 0)),
    'ModsPiece': (0, [(0, 48 + 32 * i, 256, 32) for i in range(5)], (0, 0)),
    'ModsBottom': (0, [(0, 208, 256, 48)], (0, 0)),
    'ModsCursor': (0, [(272, 0, 16, 16)], (8, 8)),
    'OptionsTop': (1, [(0, 0, 224, 128)], (0, 0)),
    'OptionsBottom': (1, [(0, 96, 230, 144)], (0, 0)),
    'OptionsCursor': (1, [(235, 24, 18, 22)], (9, 11)),
    'Slider': (1, [(192, 400 - 16 * i, 64, 16) for i in range(11)], (0, 0)),
    'StatsTop': (2, [(0, 0, 208, 224)], (0, 0)),
    'StatsBlank': (2, [(0, 336, 208, 176)], (0, 0)),
    'Digits': (2, [(208 + 16 * (i % 3), 64 + 16 * (i // 3), 16, 16) for i in range(10)], (0, 0)),
    'Collection': (3, [(0, 0, 390, 185)], (0, 0)),
    'ItemNote': (3, [(8, 185, 354, 68)], (0, 0)),
    'PageArrow': (3, [(432, 32, 16, 32), (432, 32, 16, 32)], (8, 16)),  # frame 1 = mirrored (left)
    'Bullet': (3, [(432, 80, 16, 16), (432, 96, 16, 16)], (8, 8)),
    'Shadow': (4, [(0, 0, 256, 149)], (0, 0)),
}
# Switch Pro glyphs on the 32 x 16 grid of gfx/ui/buttons.png (frame order = ButtonGlyph enum).
BUTTONS = [('A', 64, 144), ('B', 64, 160), ('X', 64, 176), ('Y', 64, 128), ('L', 96, 160), ('R', 96, 176),
           ('ZL', 96, 192), ('ZR', 96, 208), ('Plus', 96, 224), ('Minus', 96, 240), ('LStick', 96, 128),
           ('RStick', 96, 144), ('DUp', 64, 80), ('DDown', 64, 64), ('DLeft', 64, 112), ('DRight', 64, 96)]
ART['Button'] = (5, [(x, y, 32, 16) for _, x, y in BUTTONS], (0, 0))
# Mirrored frames are flipped by the animation itself, not by a negative sprite scale.
MIRRORED = {('PageArrow', 1)}


def frame(attrs):
    base = {'XPosition': '0', 'YPosition': '0', 'XScale': '100', 'YScale': '100', 'Delay': '1', 'Visible': 'true',
            'RedTint': '255', 'GreenTint': '255', 'BlueTint': '255', 'AlphaTint': '255', 'RedOffset': '0',
            'GreenOffset': '0', 'BlueOffset': '0', 'Rotation': '0', 'Interpolated': 'false'}
    base.update(attrs)
    return base


def main():
    for _, texture in SHEETS:
        assert (DUMP / texture).is_file(), texture
    root = ET.Element('AnimatedActor')
    ET.SubElement(root, 'Info', CreatedBy='Isaac Switch mod menu', Version='1', Fps='30')
    content = ET.SubElement(root, 'Content')
    sheets = ET.SubElement(content, 'Spritesheets')
    layers = ET.SubElement(content, 'Layers')
    for index, (path, _) in enumerate(SHEETS):
        ET.SubElement(sheets, 'Spritesheet', Path=path, Id=str(index))
        ET.SubElement(layers, 'Layer', Name=f'Sheet{index}', Id=str(index), SpritesheetId=str(index))
    ET.SubElement(content, 'Nulls'); ET.SubElement(content, 'Events')
    animations = ET.SubElement(root, 'Animations', DefaultAnimation='ModsHeader')
    for name, (sheet, crops, (px, py)) in ART.items():
        animation = ET.SubElement(animations, 'Animation', Name=name, FrameNum=str(len(crops)), Loop='false')
        ET.SubElement(ET.SubElement(animation, 'RootAnimation'), 'Frame', frame({'Delay': str(len(crops))}))
        layer_animations = ET.SubElement(animation, 'LayerAnimations')
        # Like the game's own menus, an animation lists only the layer it draws.
        layer = ET.SubElement(layer_animations, 'LayerAnimation', LayerId=str(sheet), Visible='true')
        if True:
            for index, (x, y, w, h) in enumerate(crops):
                ET.SubElement(layer, 'Frame', frame({'XPivot': str(px), 'YPivot': str(py), 'XCrop': str(x), 'YCrop': str(y),
                                                     'Width': str(w), 'Height': str(h),
                                                     'XScale': '-100' if (name, index) in MIRRORED else '100'}))
        ET.SubElement(animation, 'NullAnimations'); ET.SubElement(animation, 'Triggers')
    ET.indent(root, '\t')
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    target = OUT_DIR / 'menu.anm2'
    target.write_bytes(ET.tostring(root, encoding='utf-8', xml_declaration=False) + b'\n')
    header = ['// Generated by tools/menu_ui/generate_assets.py.', '#pragma once', 'namespace isaac::menu_ui::art {',
              'inline constexpr char File[] = "gfx/codex_menu/menu.anm2";',
              'struct Piece { const char* name; int frames, width, height, pivotX, pivotY; };']
    for name, (_, crops, (px, py)) in ART.items():
        w, h = crops[0][2], crops[0][3]
        header.append(f'inline constexpr Piece {name}{{"{name}",{len(crops)},{w},{h},{px},{py}}};')
    header.append('enum class Glyph : unsigned { ' + ', '.join(n for n, _, _ in BUTTONS) + ' };')
    header.append('}')
    generated = ROOT / 'features/menu_ui/generated/art.hpp'
    generated.write_text('\n'.join(header) + '\n', encoding='ascii')
    report = {'result': 'PASS', 'animations': {n: {'sheet': SHEETS[s][0], 'frames': c} for n, (s, c, _) in ART.items()},
              'buttons': [n for n, _, _ in BUTTONS], 'game_textures': [t for _, t in SHEETS],
              'source_sha256': {str(p.relative_to(ROOT)).replace('\\', '/'): hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in (Path(__file__), target, generated)}}
    (ROOT / 'evidence/menu/art.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f'Menu art: {len(ART)} animations over {len(SHEETS)} game textures; {len(BUTTONS)} button glyphs')


if __name__ == '__main__':
    main()

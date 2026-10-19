#!/usr/bin/env python3
"""Verify imported source art, original runtime symbols, and every hook guard."""
from collections import Counter
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import posixpath
import re
import xml.etree.ElementTree as ET
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
EXPECTED_NRO = "cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a"


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def canonical(node):
    return (node.tag, tuple(sorted(node.attrib.items())), (node.text or "").strip(), tuple(canonical(child) for child in node))


def main():
    manifest = json.loads((ROOT / "features/item_visuals/asset-build.json").read_text())
    assets = ROOT / "assets/item_visuals/resources"
    source_root = ROOT / "reference/iteration3"
    for path, row in manifest["source_files"].items():
        raw = (source_root / path).read_bytes()
        assert len(raw) == row["size"] and sha(raw) == row["sha256"], path
    assert sorted(path.relative_to(assets).as_posix() for path in assets.rglob("*") if path.is_file()) == manifest["resource_paths"]
    resources = {}
    frames = 0
    inherited_crops = []
    resolved_sheets = []
    for row in manifest["animations"]:
        path = assets / row["path"]
        raw = path.read_bytes()
        assert sha(raw) == row["sha256"]
        actual = ET.fromstring(raw)
        original = ET.parse(source_root / "2570913695/resources/gfx" / row["source_animation"]).getroot()
        assert len(actual.findall("./Content/Spritesheets/Spritesheet")) == len(row["sheets"])
        dimensions = {}
        for sheet, source_sheet, imported in zip(actual.findall("./Content/Spritesheets/Spritesheet"), original.findall("./Content/Spritesheets/Spritesheet"), row["sheets"]):
            source_sheet.attrib["Path"] = sheet.attrib["Path"]
            assert not sheet.attrib["Path"].startswith("gfx/")
            resolved = posixpath.normpath(posixpath.join(posixpath.dirname(row["path"]), sheet.attrib["Path"]))
            if imported.get("native_asset_dependency"):
                assert resolved == "gfx/" + imported["source"]
                assert imported["source"] in manifest["native_asset_dependencies"]
                proof = manifest["native_asset_dependencies"][imported["source"]]
                resolved_sheets.append({"item": row["id"], "anm2": row["path"], "sheet": sheet.attrib["Path"],
                                        "resolved": resolved, "native_dependency": True})
                if proof["physical_texture_in_this_dump"]:
                    assert sha(Path(proof["path"]).read_bytes()) == proof["sha256"]
                    dimensions[int(sheet.attrib["Id"])] = proof["dimensions"]
                else:
                    assert sha(Path(proof["native_animation_proof"]).read_bytes()) == proof["proof_sha256"]
                continue
            image_path = assets / imported["path"]
            assert resolved == imported["path"] and (assets / resolved).is_file(), (row["id"], resolved)
            resolved_sheets.append({"item": row["id"], "anm2": row["path"], "sheet": sheet.attrib["Path"],
                                    "resolved": resolved, "native_dependency": False})
            image_raw = image_path.read_bytes()
            assert sha(image_raw) == imported["sha256"]
            with Image.open(image_path) as image:
                assert list(image.size) == imported["dimensions"]
                assert sha(image.convert("RGBA").tobytes()) == imported["rgba_sha256"]
            assert image_raw == (source_root / "2570913695/resources/gfx" / imported["source"]).read_bytes()
            dimensions[int(sheet.attrib["Id"])] = imported["dimensions"]
        # Every keyframe, delay, layer order, tint and interpolation flag stays
        # exactly as supplied. Only namespaced sheet paths are allowed to vary.
        assert canonical(original) == canonical(actual), row["source_animation"]
        layer_sheet = {int(layer.attrib["Id"]): int(layer.attrib["SpritesheetId"]) for layer in actual.findall("./Content/Layers/Layer")}
        for animation in actual.findall("./Animations/Animation"):
            assert animation.attrib["Name"] == "AnimatedPedestal"
            assert int(animation.attrib["FrameNum"]) == row["frames"]
            assert animation.attrib["Loop"] == "true" and not list(animation.find("Triggers"))
            for layer in animation.findall("./LayerAnimations/LayerAnimation"):
                size = dimensions.get(layer_sheet[int(layer.attrib["LayerId"])])
                for frame in layer:
                    frames += 1
                    assert int(frame.attrib["Delay"]) > 0
                    x, y, width, height = (float(frame.attrib.get(key, 0)) for key in ("XCrop", "YCrop", "Width", "Height"))
                    if size and (x < 0 or y < 0 or x + width > size[0] or y + height > size[1]):
                        inherited_crops.append({"item": row["id"], "crop": [x, y, width, height], "sheet_dimensions": size})
    for row in manifest["birthrights"]:
        raw = (assets / row["path"]).read_bytes()
        assert sha(raw) == row["sha256"]
        assert raw == (source_root / "2690434875" / row["source"]).read_bytes()
        with Image.open(assets / row["path"]) as image:
            assert image.size == (32, 32)
    for path in manifest["resource_paths"]:
        resources[path] = sha((assets / path).read_bytes())
    assert len(manifest["animations"]) == 98 and len(manifest["birthrights"]) == 41 and len(resources) == 235
    art_report = {"status": "PASS", "timestamp_utc": datetime.now(timezone.utc).isoformat(),
                  "animations": 98, "birthright_characters": 41, "layer_keyframes_compared": frames,
                  "resource_sha256": resources,
                  "native_relative_path_resolution": resolved_sheets,
                  "source_sha256": {name: sha((ROOT / name).read_bytes()) for name in (
                      "features/item_visuals/asset-build.json", "features/item_visuals/generated.hpp",
                      "tools/item_visuals/build_assets.py", "tools/item_visuals/verify_sources.py")},
                  "inherited_outside_sheet_crops": {"count": len(inherited_crops),
                      "per_item": dict(Counter(row["item"] for row in inherited_crops)), "frames": inherited_crops,
                      "handling": "Preserved exactly from source, including blank/outside-sheet coordinates. No invented art corrections."},
                  "native_asset_dependencies": manifest["native_asset_dependencies"], "hardware_test": False}
    (ROOT / "features/item_visuals/asset-validation.json").write_text(json.dumps(art_report, indent=2) + "\n")

    binary = (ROOT.parent / "Isaac-dump/romfs/.nro/Repentance.nro").read_bytes()
    assert sha(binary) == EXPECTED_NRO
    symbols = {row["name"]: row for row in json.loads((ROOT / "evidence/native-analysis/symbols.json").read_text())}
    native = (ROOT / "native/source/program/item_visuals.cpp").read_text()
    match = re.search(r"OriginalPrologues\[\]\[20\]\s*=\s*\{(.*?)\n\};", native, re.S)
    guards = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-f]{2})", match.group(1)))
    assert len(guards) == 80
    targets = [
        (0x270c34, 912, "_ZN15IsaacRepentance13Entity_Pickup6RenderERKN4KAGE4Math7Vector2E", "void(Pickup*, const Vec2*)"),
        (0x2d9398, 3236, "_ZN15IsaacRepentance13Entity_Player20render_player_spriteERKN4KAGE4Math7Vector2Eb", "void(Player*, const Vec2*, bool)"),
        (0x9d70, 356, "_ZN15IsaacRepentance4ANM211RenderLayerEiRKN4KAGE4Math7Vector2ES5_S5_", "void(ANM2*, unsigned, const Vec2*, const Vec2*, const Vec2*)"),
        (0x9bd4, 412, "_ZN15IsaacRepentance4ANM26RenderERKN4KAGE4Math7Vector2ES5_S5_", "void(ANM2*, const Vec2*, const Vec2*, const Vec2*)")]
    hook_rows = []
    for index, (offset, size, name, abi) in enumerate(targets):
        assert symbols[name]["offset"] == offset and symbols[name]["size"] == size
        assert binary[offset:offset+20] == guards[index*20:index*20+20]
        hook_rows.append({"symbol": name, "offset": hex(offset), "size": size, "raw_abi": abi,
                          "first20": binary[offset:offset+20].hex()})
    evidence = ["evidence/native-analysis/270c34.asm.txt", "evidence/native-analysis/expanded/9bd4.c",
                "evidence/native-analysis/expanded/26c9b8.c",
                "evidence/iteration3/native-visuals/2d9398.asm.txt", "evidence/iteration3/native-visuals/2924a4.asm.txt",
                "evidence/iteration3/native-visuals/2dd150.asm.txt", "evidence/iteration3/native-core/290c38.c",
                "evidence/iteration3/native-transforms/9790.c", "evidence/iteration3/native-transforms/65b8.c",
                "evidence/iteration3/native-textures/751c.c", "evidence/iteration3/native-textures/87dc.c",
                "evidence/iteration4/item-visuals/texture-load/f96c.asm.txt",
                "evidence/iteration4/item-visuals/path-resolution/361a0.c",
                "evidence/iteration4/item-visuals/path-resolution/361a0.asm.txt",
                "evidence/iteration4/item-visuals/native/d2cc.c"]
    native_report = {"status": "PASS", "timestamp_utc": datetime.now(timezone.utc).isoformat(),
                     "nro_sha256": sha(binary), "hooks": hook_rows,
                     "source_sha256": {name: sha((ROOT/name).read_bytes()) for name in evidence + [
                         "native/source/program/item_visuals.cpp", "native/source/program/native_sprite.cpp",
                         "native/source/program/native_sprite.hpp", "tools/item_visuals/verify_sources.py"]},
                     "verified_fields": {"held_ANM2": "player+0x12d8", "queued_ItemConfig_pointer": "player+0x1c38",
                         "ItemConfig_ID": "+4 (pointer also checked against native GetCollectible)",
                         "layer_states": "ANM2+0xa8; count+0xb0; stride0xb0", "layer_visibility": "+0x7c",
                         "layer_image_handle": "+0xa0 data/+0xa8 control; both exchanged together",
                         "source_offset_to_pixels": "GetRenderDistance0x35ae0 multiplies X/Y by0.65",
                         "player_type": "player+0x1738", "position": "entity+0x310/314", "sprite_offset": "entity+0x320/324",
                         "identity": "entity address+InitSeed0x3e8+birthframe0x2f4+subtype0x40"},
                     "hardware_test": False}
    (ROOT / "features/item_visuals/native-layout.json").write_text(json.dumps(native_report, indent=2) + "\n")
    print(f"PASS: {len(resources)} resources, {frames} unchanged layer keyframes, 4 exact native hook guards")
    print(f"Source retained {len(inherited_crops)} outside-sheet crops; see asset-validation.json, not a new conversion defect")


if __name__ == "__main__":
    main()

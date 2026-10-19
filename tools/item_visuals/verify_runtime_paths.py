#!/usr/bin/env python3
"""Check the installed failure against the exact native relative-path rule.

Read-only inputs: frozen installed package, local source art, original NRO.
Writes only this feature's evidence report. Does not stage or install files.
"""
from datetime import datetime, timezone
import hashlib
import importlib.util
import json
from pathlib import Path
import posixpath
import xml.etree.ElementTree as ET
import zipfile

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
BASELINE = ROOT / "deployments/20261015T113822-iteration4-baseline/installed-package.zip"
PREFIX = "atmosphere/contents/010021C000B6A000/romfs/resources/"


def sha(data):
    return hashlib.sha256(data).hexdigest()


def resolve(animation, sheet):
    # Verified 0x361a0: normalize separators; discard the ANM2 filename;
    # append sheet components, removing one base component for each '..'.
    return posixpath.normpath(posixpath.join(posixpath.dirname(animation.replace("\\", "/")), sheet.replace("\\", "/")))


def main():
    manifest_path = ROOT / "features/item_visuals/asset-build.json"
    manifest = json.loads(manifest_path.read_text())
    assets = ROOT / "assets/item_visuals/resources"
    spec = importlib.util.spec_from_file_location("switch_codec", ROOT / "tools/convert_ui_textures.py")
    codec = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(codec)
    binary = (ROOT.parent / "Isaac-dump/romfs/.nro/Repentance.nro").read_bytes()
    assert sha(binary) == "cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a"
    assert binary[0x8b23f6:].split(b"\0", 1)[0] == b"Path"
    assert binary[0x896e40:].split(b"\0", 1)[0] == b"Id"
    assert binary[0xff54:0xff58].hex() == "1f821994"  # BL PLT 0x6707d0 GetAbsolutePathFromBasePath
    rows, pixels, focus = [], [], []
    with zipfile.ZipFile(BASELINE) as package:
        paths = set(package.namelist())
        for animation in manifest["animations"]:
            current = ET.parse(assets / animation["path"]).getroot()
            old = ET.fromstring(package.read(PREFIX + animation["path"]))
            sheets = current.findall("./Content/Spritesheets/Spritesheet")
            previous = old.findall("./Content/Spritesheets/Spritesheet")
            assert len(sheets) == len(previous) == len(animation["sheets"])
            assert sorted(int(layer.attrib["Id"]) for layer in current.findall("./Content/Layers/Layer")) == list(range(animation["layers"]))
            images = {}
            for sheet, before, source in zip(sheets, previous, animation["sheets"]):
                expected = "gfx/" + source["source"] if source.get("native_asset_dependency") else source["path"]
                new_path = resolve(animation["path"], sheet.attrib["Path"])
                old_path = resolve(animation["path"], before.attrib["Path"])
                assert new_path == expected and old_path != expected
                assert PREFIX + str(Path(old_path).with_suffix(".pcx")).replace("\\", "/") not in paths
                rows.append({"item": animation["id"], "animation": animation["path"],
                             "old_sheet": before.attrib["Path"], "old_resolved_missing": old_path,
                             "new_sheet": sheet.attrib["Path"], "new_resolved": new_path,
                             "native_dependency": bool(source.get("native_asset_dependency"))})
                if source.get("native_asset_dependency"):
                    continue
                image = Image.open(assets / expected).convert("RGBA")
                images[int(sheet.attrib["Id"])] = image
                assert (assets / expected).is_file()
            if animation["id"] in (25, 581):
                layers = {int(layer.attrib["Id"]): int(layer.attrib["SpritesheetId"]) for layer in current.findall("./Content/Layers/Layer")}
                tested = 0
                for layer in current.findall("./Animations/Animation/LayerAnimations/LayerAnimation"):
                    image = images[layers[int(layer.attrib["LayerId"])]]
                    for frame in layer:
                        if frame.attrib.get("Visible") != "true":
                            continue
                        x, y, width, height = [int(frame.attrib[key]) for key in ("XCrop", "YCrop", "Width", "Height")]
                        assert 0 <= x < x+width <= image.width and 0 <= y < y+height <= image.height
                        assert image.crop((x, y, x+width, y+height)).getchannel("A").getbbox() is not None
                        tested += 1
                focus.append({"item": animation["id"], "all_visible_keyframes_have_pixels": True, "visible_keyframes_checked": tested})
        # The complete installed texture set is intact, so do not invent an
        # encoder or alpha change to explain a path-resolution bug.
        for relative in manifest["resource_paths"]:
            if not relative.endswith(".png"):
                continue
            image = Image.open(assets / relative).convert("RGBA")
            pcx_relative = str(Path(relative).with_suffix(".pcx")).replace("\\", "/")
            installed = package.read(PREFIX + pcx_relative)
            decoded = codec.decode_pcx(installed)
            assert decoded.size == image.size and decoded.tobytes() == image.tobytes()
            assert installed == codec.encode_pcx(image)
            pixels.append({"path": pcx_relative, "size": list(image.size), "rgba_sha256": sha(image.tobytes()),
                           "installed_pcx_sha256": sha(installed), "unchanged_source_pixels": True})
    tracked = ["tools/item_visuals/verify_runtime_paths.py", "tools/convert_ui_textures.py",
               "features/item_visuals/asset-build.json", "features/item_visuals/asset-validation.json",
               "native/source/program/item_visuals.cpp", "native/source/program/native_sprite.cpp",
               "native/source/program/native_sprite.hpp", "evidence/iteration4/item-visuals/texture-load/f96c.asm.txt",
               "evidence/iteration4/item-visuals/path-resolution/361a0.c",
               "evidence/iteration4/item-visuals/path-resolution/361a0.asm.txt",
               "evidence/iteration4/item-visuals/native/d2cc.c"]
    report = {"status": "PASS", "timestamp_utc": datetime.now(timezone.utc).isoformat(), "hardware_test": False,
              "baseline_archive": str(BASELINE.relative_to(ROOT)), "baseline_sha256": sha(BASELINE.read_bytes()),
              "root_cause": "ANM2 sheet paths were incorrectly treated as relative to gfx instead of the relocated ANM2 directory.",
              "animations_corrected": len(manifest["animations"]), "sheet_references_corrected": len(rows),
              "installed_textures_pixel_verified": len(pixels), "reported_items": focus,
              "sheet_resolution": rows, "texture_checks": pixels,
              "source_sha256": {name: sha((ROOT / name).read_bytes()) for name in tracked},
              "limits": ["The exact 20880-byte Preload2 decompile timed out; its assembly and complete 0x361a0 helper decompile establish the call and resolution rule.",
                         "Fruity Plum's physical stock texture remains absent from the local base/update dump; the stock ANM2 establishes its logical path. Console validation is pending.",
                         "Existing source crop exceptions are retained and reported by asset-validation.json; they do not affect Breakfast or Psy Fly."]}
    output = ROOT / "evidence/iteration4/item-visuals/runtime-path-validation.json"
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: {len(manifest['animations'])} animations / {len(rows)} sheet references corrected; {len(pixels)} installed textures pixel-identical; Breakfast/Psy Fly visible crops nonempty")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Check source metrics, generated font frames, icon frame references and crops."""
import json
import hashlib
from datetime import datetime, timezone
from pathlib import Path
import xml.etree.ElementTree as ET
from PIL import Image
from generate_assets import font_data, ROOT, UPSTREAM, COMMIT


def main():
    here = ROOT / "eid_ui"
    resources = here / "resources/gfx/codex_eid_ui"
    metadata = json.loads((here / "generated/assets.json").read_text(encoding="utf-8"))
    font = font_data(UPSTREAM / "resources/font/eid_default.fnt")
    generated = ET.parse(resources / "eid_glyphs.anm2")
    layers = generated.findall("./Animations/Animation/LayerAnimations/LayerAnimation")
    for glyph in metadata["glyphs"]:
        assert all(glyph[key] == font["glyphs"][glyph["code"]][key] for key in font["glyphs"][glyph["code"]])
        for page, layer in enumerate(layers):
            frame = layer.findall("Frame")[glyph["frame"]].attrib
            expected_visible = page == glyph["page"] and glyph["width"] > 0 and glyph["height"] > 0
            assert (frame["Visible"] == "true") == expected_visible
            if expected_visible:
                assert int(frame["XPosition"]) == glyph["xoffset"] and int(frame["YPosition"]) == glyph["yoffset"]
                assert int(frame["XCrop"]) == glyph["x"] and int(frame["YCrop"]) == glyph["y"]
                assert int(frame["Width"]) == glyph["width"] and int(frame["Height"]) == glyph["height"]
                assert int(frame["XScale"]) == int(frame["YScale"]) == 100
        assert 0 <= glyph["x"] < font["width"] and glyph["x"] + glyph["width"] <= font["width"]
        assert 0 <= glyph["y"] < font["height"] and glyph["y"] + glyph["height"] <= font["height"]

    sheet_paths = [resources / (name + ".anm2") for name in
                   ("eid_inline_icons", "eid_player_icons", "eid_transform_icons", "eid_cardspills")]
    sheet_paths.append(ROOT.parent / "Isaac-dump/romfs/rp_patch/resources/gfx/ui/buttons.anm2")
    roots = [ET.parse(path).getroot() for path in sheet_paths]
    for icon in metadata["icons"].values():
        root = roots[icon["sheet"]]
        animation = next(a for a in root.findall("./Animations/Animation") if a.attrib["Name"] == icon["animation"])
        frame_capacity = max(int(animation.attrib["FrameNum"]),
                             max(sum(int(f.attrib["Delay"]) for f in layer.findall("Frame"))
                                 for layer in animation.findall("./LayerAnimations/LayerAnimation")))
        assert 0 <= icon["frame"] < frame_capacity, icon
        sheets = {int(s.attrib["Id"]): s.attrib["Path"] for s in root.findall("./Content/Spritesheets/Spritesheet")}
        layers = {int(l.attrib["Id"]): int(l.attrib["SpritesheetId"]) for l in root.findall("./Content/Layers/Layer")}
        for layer in animation.findall("./LayerAnimations/LayerAnimation"):
            remaining = icon["frame"]
            selected = None
            for candidate in layer.findall("Frame"):
                selected = candidate
                if remaining < int(candidate.attrib["Delay"]):
                    break
                remaining -= int(candidate.attrib["Delay"])
            if selected is None or selected.attrib.get("Visible") == "false" or icon["sheet"] == 4:
                continue
            f = selected.attrib
            path = sheet_paths[icon["sheet"]].parent / sheets[layers[int(layer.attrib["LayerId"])]]
            with Image.open(path) as texture:
                assert int(f["XCrop"]) >= 0 and int(f["YCrop"]) >= 0
                assert int(f["XCrop"]) + int(f["Width"]) <= texture.width, icon
                assert int(f["YCrop"]) + int(f["Height"]) <= texture.height, icon
    inputs = [here/"check_assets.py",here/"generate_assets.py",here/"generated/assets.hpp",
              here/"generated/assets.json",here/"assets-manifest.json",UPSTREAM/"resources/font/eid_default.fnt"]
    inputs += sorted(path for path in resources.iterdir() if path.is_file())
    report = {"status": "PASS", "timestamp_utc":datetime.now(timezone.utc).isoformat(),
              "source_sha256":{path.relative_to(ROOT).as_posix():hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs},
              "external_source_sha256":{str(sheet_paths[4]):hashlib.sha256(sheet_paths[4].read_bytes()).hexdigest()},
              "source_commit": COMMIT, "glyphs": len(metadata["glyphs"]),
              "icons": len(metadata["icons"]), "note": "Glyph metrics/frames match BMFont; referenced icon frames and upstream PNG crop bounds are valid. Game controller image is not repackaged."}
    (here / "asset-check-results.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({key:report[key] for key in ("status","glyphs","icons","note")}))


if __name__ == "__main__":
    main()

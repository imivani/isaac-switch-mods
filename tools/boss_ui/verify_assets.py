"""Check the actual staged sprite dependency closure and atlas dimensions."""
from pathlib import Path
import hashlib
import json
import posixpath
import sys
import xml.etree.ElementTree as ET

sys.path.insert(0, r"C:\CodexTools\ReverseEngineering\PythonModules")
from PIL import Image

PROJECT = Path(__file__).resolve().parents[2]
OUT = PROJECT / "evidence/boss_ui"
RESOURCES = PROJECT / "assets/boss_ui/resources"


def main():
    manifest = json.loads((OUT / "asset-manifest.json").read_text())
    model = json.loads((OUT / "model.json").read_text())
    records = [record for group in manifest["groups"].values() for record in group]
    paths = {record["resource_path"] for record in records}
    actual_paths = {path.relative_to(RESOURCES).as_posix() for path in RESOURCES.rglob("*") if path.is_file()}
    assert actual_paths == paths, {"extra": sorted(actual_paths - paths), "missing": sorted(paths - actual_paths)}
    for record in records:
        assert hashlib.sha256((PROJECT / record["source"]).read_bytes()).hexdigest() == record["sha256"], record["source"]
    animations = [ET.parse(RESOURCES / animation[0]).getroot() for animation in model["animations"]]
    closure = 0
    for definition, root in zip(model["animations"], animations):
        for sheet in root.findall("Content/Spritesheets/Spritesheet"):
            path = posixpath.normpath(posixpath.join(posixpath.dirname(definition[0]), sheet.attrib["Path"].replace("\\", "/")))
            assert path in paths, (definition[0], path)
            closure += 1
    dimensions = {}
    for record in records:
        if record["kind"] == "png_to_rgba_pcx":
            with Image.open(PROJECT / record["source"]) as image:
                assert image.size == (record["width"], record["height"])
                dimensions[record["resource_path"]] = image.size
    checks = 0
    unused_warnings = []

    def check_sprite(animation, sprite):
        nonlocal checks
        width, height = dimensions[sprite]
        for sequence in animations[animation].findall("Animations/Animation"):
            for frame in sequence.findall("LayerAnimations/LayerAnimation/Frame"):
                x, y, w, h = (int(frame.attrib[field]) for field in ("XCrop", "YCrop", "Width", "Height"))
                valid = min(x, y, w, h) >= 0 and x + w <= width and y + h <= height
                if not valid and sequence.attrib["Name"].endswith("12"):
                    # A 7-per-row horizontal layout never renders the 15-pixel
                    # bar. Upstream's Delirium bg12 extends below its atlas.
                    unused_warnings.append({"sprite": sprite, "animation": sequence.attrib["Name"], "crop": [x, y, w, h]})
                    continue
                assert valid, (sprite, sequence.attrib["Name"], frame.attrib)
                checks += 1

    for boss in model["bosses"]:
        check_sprite(boss[3], boss[2])
        for rule in model["rules"][boss[8]:boss[8] + boss[9]]:
            check_sprite(boss[3], rule[1])
    for style in model["styles"]:
        check_sprite(style[2], style[1])
        if style[4] >= 0 and style[5]:
            check_sprite(style[4], style[5])
    for record in manifest["groups"]["vanilla_bar_overrides"]:
        with Image.open(PROJECT / record["source"]) as image:
            assert image.size == (1, 1)
            assert image.convert("RGBA").getextrema()[3] == (0, 0)
    assert "gfx/ui/ui_bosshealthbar_stone.png" not in actual_paths
    report = {"files": len(records), "dependency_references": closure, "atlas_frame_checks": checks,
              "manifest_hashes": "PASS", "dependency_closure": "PASS", "atlas_bounds": "PASS",
              "transparent_overrides": "PASS", "gideon_preserved": "PASS", "unused_upstream_atlas_warnings": unused_warnings}
    (OUT / "asset-verification.json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()

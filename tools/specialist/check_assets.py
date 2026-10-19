#!/usr/bin/env python3
"""Check every staged Specialist frame reference, texture and original song."""
import hashlib
import json
from pathlib import Path
import struct
import xml.etree.ElementTree as ET
from collections import Counter
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "assets/specialist/resources"


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def ogg_info(path):
    data = path.read_bytes()
    offset, pages, sample_rate, channels, last_granule = 0, 0, 0, 0, 0
    serial = None
    while offset < len(data):
        if data[offset:offset+5] != b"OggS\x00" or offset + 27 > len(data):
            raise ValueError(f"Invalid Ogg page: {path}")
        count = data[offset + 26]
        header_end = offset + 27 + count
        if header_end > len(data):
            raise ValueError("Truncated Ogg segment table")
        size = sum(data[offset+27:header_end])
        end = header_end + size
        if end > len(data):
            raise ValueError("Truncated Ogg page payload")
        page_serial, sequence = struct.unpack_from("<II", data, offset + 14)
        if serial is not None and (serial != page_serial or sequence != pages):
            raise ValueError("Unexpected Ogg stream or page sequence")
        serial = page_serial
        if pages == 0:
            packet = data[header_end:end]
            if packet[:7] != b"\x01vorbis" or len(packet) < 30 or not data[offset+5] & 2:
                raise ValueError("Expected original Ogg Vorbis identification packet")
            channels = packet[11]
            sample_rate = struct.unpack_from("<I", packet, 12)[0]
            if channels not in (1, 2) or not 8000 <= sample_rate <= 192000:
                raise ValueError("Unexpected audio format")
        granule = struct.unpack_from("<Q", data, offset + 6)[0]
        if granule != 0xffffffffffffffff:
            last_granule = granule
        flags = data[offset + 5]
        pages += 1
        offset = end
    if not flags & 4:
        raise ValueError("Ogg stream has no final page")
    return {"pages": pages, "channels": channels, "sample_rate": sample_rate,
            "duration_seconds": round(last_granule / sample_rate, 3), "sha256": digest(path)}


def main():
    manifest_path = ROOT / "assets/specialist/manifest.json"
    manifest = json.loads(manifest_path.read_text())
    original_root = Path(manifest["source"])
    for relative, expected_hash in manifest["source_sha256"].items():
        if digest(original_root/relative) != expected_hash:
            raise ValueError(f"Workshop source changed since preparation: {relative}")
    expected = set(manifest["outputs"])
    actual = {p.relative_to(SOURCE).as_posix() for p in SOURCE.rglob("*") if p.is_file()}
    if actual != expected:
        raise ValueError(f"Manifest/tree mismatch: {actual ^ expected}")
    for relative, row in manifest["outputs"].items():
        if digest(SOURCE/relative) != row["sha256"]:
            raise ValueError(f"Staged asset differs from its recorded source conversion: {relative}")
        if not relative.startswith(("gfx/codex_specialist/", "codex_specialist/")):
            raise ValueError("Resource escapes the owned namespace")
    frames = 0
    animations = 0
    sheets = set()
    original_crops = []
    for path in sorted(SOURCE.rglob("*.anm2")):
        root = ET.parse(path).getroot()
        relative = path.relative_to(SOURCE/"gfx/codex_specialist")
        original = ET.parse(original_root/"resources/the"/relative).getroot()
        for sheet in original.findall("./Content/Spritesheets/Spritesheet"):
            sheet.attrib["Path"] = sheet.attrib["Path"].replace("\\", "/")
        if ET.tostring(original) != ET.tostring(root):
            raise ValueError(f"Animation differs from original beyond path separators: {path}")
        textures = {}
        for sheet in root.findall("./Content/Spritesheets/Spritesheet"):
            target = (path.parent/sheet.attrib["Path"]).resolve()
            if not target.is_relative_to(SOURCE.resolve()) or not target.is_file():
                raise ValueError(f"Unresolved animation texture: {path}")
            with Image.open(target) as image:
                image.load()
                textures[sheet.attrib["Id"]] = image.size
                if image.width > 8192 or image.height > 8192:
                    raise ValueError("Texture exceeds converter bounds")
            sheets.add(target)
            original_texture = original_root/"resources/the"/target.relative_to(SOURCE/"gfx/codex_specialist")
            if digest(target) != digest(original_texture):
                raise ValueError(f"Original dance texture changed: {target}")
        layers = {x.attrib["Id"]: x.attrib["SpritesheetId"] for x in root.findall("./Content/Layers/Layer")}
        for animation in root.findall("./Animations/Animation"):
            animations += 1
            if int(animation.attrib["FrameNum"]) <= 0:
                raise ValueError("Animation has no frames")
            for layer in animation.findall("./LayerAnimations/LayerAnimation"):
                width, height = textures[layers[layer.attrib["LayerId"]]]
                for frame in layer.findall("Frame"):
                    frames += 1
                    values = frame.attrib
                    x, y, w, h = (int(float(values.get(key, "0"))) for key in ("XCrop", "YCrop", "Width", "Height"))
                    if min(x, y, w, h) < 0:
                        raise ValueError(f"Negative original frame rectangle: {path}")
                    if x+w > width or y+h > height:
                        # The published Workshop files already have these crop
                        # rectangles. Their exact XML and texture bytes were
                        # checked above: preserve the artist's source and report
                        # the inherited exception, never silently pad/crop it.
                        original_crops.append({"file": relative.as_posix(), "animation": animation.attrib["Name"],
                            "layer": layer.attrib["LayerId"], "rectangle": [x,y,w,h], "texture": [width,height],
                            "overflow_right": max(0,x+w-width), "overflow_bottom": max(0,y+h-height)})
    config = ET.parse(SOURCE/"codex_specialist/costumes.xml").getroot()
    if len(config) != 42 or config.attrib["anm2root"] != "gfx/codex_specialist/" or any(x.attrib.get("type") != "none" for x in config):
        raise ValueError("Costume registry is not the expected isolated 42 null costumes")
    music = ET.parse(SOURCE/"codex_specialist/music.xml").getroot()
    if len(music) != 4 or music.attrib["root"] != "codex_specialist/music/":
        raise ValueError("Expected four namespaced songs")
    songs = {track.attrib["name"]: ogg_info(SOURCE/music.attrib["root"]/track.attrib["path"]) for track in music}
    for track in music:
        relative = "resources/the/" + track.attrib["path"]
        if songs[track.attrib["name"]]["sha256"] != manifest["source_sha256"][relative]:
            raise ValueError("Original song bytes changed")
    files = [Path(__file__).resolve(), ROOT/"tools/specialist/prepare_assets.py", manifest_path,
             ROOT/"features/specialist/generated/assets.hpp", ROOT/"features/specialist/generated/native.hpp"]
    files.extend(sorted(p for p in SOURCE.rglob("*") if p.is_file()))
    report = {"status": "PASS", "hardware_test": False, "costumes": len(config), "textures": len(sheets), "animations": animations,
              "frame_rectangles": frames, "animation_and_texture_source_fidelity": True,
              "upstream_crop_exceptions": {"count": len(original_crops),
                  "by_file": dict(sorted(Counter(x["file"] for x in original_crops).items())),
                  "by_animation": dict(sorted(Counter(x["animation"] for x in original_crops).items())),
                  "largest_examples": sorted(original_crops, key=lambda x: max(x["overflow_bottom"],x["overflow_right"]), reverse=True)[:12],
                  "disposition": "Unchanged original Workshop frame rectangles and textures; native visual behavior remains a hardware check."},
              "songs": songs, "source_sha256": {p.relative_to(ROOT).as_posix(): digest(p) for p in files}}
    (ROOT/"assets/specialist/validation.json").write_text(json.dumps(report, indent=2)+"\n", encoding="utf-8")
    print(f"PASS: 42 original costumes, {len(sheets)} unchanged textures, {animations} source-identical animations, {frames} frame rectangles and 4 unchanged Ogg songs; {len(original_crops)} inherited crop exceptions recorded.")


if __name__ == "__main__": main()

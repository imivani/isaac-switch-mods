#!/usr/bin/env python3
"""Import the supplied Workshop scripts/default art without executing Lua."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import posixpath
import re
import struct
import xml.etree.ElementTree as ET
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
WORKSHOP = Path(r"C:\Program Files (x86)\Steam\steamapps\workshop\content\250900")
ASSETS = ROOT / "assets/item_visuals/resources"
REFERENCE = ROOT / "reference/iteration3"


def digest(raw):
    return hashlib.sha256(raw).hexdigest()


def normal(value):
    return re.sub(r"[^A-Z0-9]", "", value.removeprefix("THE_"))


def main():
    used = {}
    native_sheet_index = None
    def source(mod, relative):
        relative = Path(str(relative).replace("\\", "/"))
        original = WORKSHOP / mod / relative
        raw = original.read_bytes()
        target = REFERENCE / mod / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        if target.exists() and target.read_bytes() != raw:
            raise ValueError(f"Existing source snapshot changed: {target}")
        if not target.exists():
            target.write_bytes(raw)
        used[f"{mod}/{relative.as_posix()}"] = {"sha256": digest(raw), "size": len(raw)}
        return raw

    scripts = {}
    for mod in ("2570913695", "2690434875"):
        scripts[mod] = source(mod, "main.lua").decode("utf-8-sig")
        source(mod, "metadata.xml")
    effect_config = ET.fromstring(source("2570913695", "content/entities2.xml"))
    effect = effect_config.find("./entity[@name='Pedestal Animation']")
    assert effect is not None and effect.attrib["id"] == "1000" and effect.attrib["collisionDamage"] == "0"
    items_path = ROOT.parent / "Isaac-dump/romfs/rp_patch/resources/items.xml"
    items = list(ET.fromstring(items_path.read_bytes()))
    native_names = {}
    for item in items:
        if "id" not in item.attrib or not item.attrib.get("name", "").startswith("#"):
            continue
        name = item.attrib["name"].removeprefix("#").removesuffix("_NAME")
        native_names.setdefault(normal(name), []).append((int(item.attrib["id"]), item.attrib["name"]))
    rows = re.findall(r'animationfiles\[CollectibleType\.(COLLECTIBLE_[A-Z0-9_]+)\]\s*=\s*"([^"]+)"', scripts["2570913695"])
    assert len(rows) > 90 and len(rows) == len(set(name for name, _ in rows))
    animations, resources, game_assets = [], [], {}
    for constant, filename in rows:
        candidates = native_names.get(normal(constant.removeprefix("COLLECTIBLE_")), [])
        if len(candidates) != 1:
            raise ValueError(f"Ambiguous/missing native item: {constant}: {candidates}")
        item_id, native_name = candidates[0]
        animation_path = f"gfx/isaac_mods/item_visuals/animated/{item_id}.anm2"
        animation_directory = posixpath.dirname(animation_path)
        tree = ET.fromstring(source("2570913695", "resources/gfx/" + filename))
        fps = int(tree.find("Info").attrib["Fps"])
        animation = tree.find("./Animations/Animation[@Name='AnimatedPedestal']")
        assert animation is not None and animation.attrib.get("Loop") == "true" and fps == 30
        frames = int(animation.attrib["FrameNum"])
        assert 0 < frames <= 10000
        assert not list(animation.find("Triggers")), filename
        sheets = []
        for sheet in tree.findall("./Content/Spritesheets/Spritesheet"):
            original_sheet = sheet.attrib["Path"].replace("\\", "/")
            original_path = WORKSHOP / "2570913695/resources/gfx" / original_sheet
            if not original_path.is_file():
                # Some source animations deliberately reuse stock familiars.
                # Keep their logical PNG path; verify the game's PCX exists,
                # but never copy a dumped game asset into the mod package.
                found = None
                for layer in ("rp_patch/resources", "resources"):
                    candidate = ROOT.parent / "Isaac-dump/romfs" / layer / "gfx" / Path(original_sheet).with_suffix(".pcx")
                    if candidate.is_file():
                        found = candidate
                        break
                if found is None:
                    if native_sheet_index is None:
                        native_sheet_index = {}
                        for layer in ("rp_patch/resources", "resources"):
                            for native_animation in (ROOT.parent / "Isaac-dump/romfs" / layer / "gfx").rglob("*.anm2"):
                                with native_animation.open("rb") as handle:
                                    header = handle.read(32768).split(b"</Spritesheets>", 1)[0]
                                for dependency in re.findall(rb'<Spritesheet\b[^>]*Path="([^"]+)"', header):
                                    key = dependency.decode("utf-8").replace("\\", "/").casefold()
                                    native_sheet_index.setdefault(key, native_animation)
                    proof = native_sheet_index.get(original_sheet.casefold())
                    if proof is None:
                        raise FileNotFoundError(f"Neither mod texture nor native dependency proof exists: {original_sheet}")
                    game_assets[original_sheet] = {"native_animation_proof": str(proof),
                                                   "proof_sha256": digest(proof.read_bytes()),
                                                   "physical_texture_in_this_dump": False}
                else:
                    raw = found.read_bytes()
                    assert raw[:4] == b"\x0a\x05\x01\x08" and raw[65] == 4
                    xmin, ymin, xmax, ymax = struct.unpack_from("<4H", raw, 4)
                    game_assets[original_sheet] = {"path": str(found), "sha256": digest(raw),
                                                   "dimensions": [xmax-xmin+1, ymax-ymin+1],
                                                   "physical_texture_in_this_dump": True}
                # Native Preload2 resolves sheet paths against the ANM2's
                # own directory (0xff54 -> GetAbsolutePathFromBasePath).
                sheet.attrib["Path"] = posixpath.relpath("gfx/" + original_sheet, animation_directory)
                sheets.append({"source": original_sheet, "native_asset_dependency": True})
                continue
            raw = source("2570913695", "resources/gfx/" + original_sheet)
            sheet_id = int(sheet.attrib["Id"])
            relative = f"gfx/isaac_mods/item_visuals/animated/sheets/{item_id}_{sheet_id}.png"
            target = ASSETS / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(raw)
            with Image.open(target) as image:
                image.load()
                dimensions = list(image.size)
                pixel_hash = digest(image.convert("RGBA").tobytes())
            sheet.attrib["Path"] = posixpath.relpath(relative, animation_directory)
            sheets.append({"source": original_sheet, "path": relative, "dimensions": dimensions,
                           "rgba_sha256": pixel_hash, "sha256": digest(raw)})
            resources.append(relative)
        relative = animation_path
        target = ASSETS / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        ET.indent(tree, space="  ")
        target.write_bytes(ET.tostring(tree, encoding="utf-8", xml_declaration=True))
        resources.append(relative)
        animations.append({"id": item_id, "enum": constant, "native_item_name": native_name,
                           "source_animation": filename, "path": relative, "frames": frames, "fps": fps,
                           "layers": len(tree.findall("./Content/Layers/Layer")), "sheets": sheets,
                           "sha256": digest(target.read_bytes())})
    match = re.search(r"local PlayerList\s*=\s*\{(.*?)\}", scripts["2690434875"], re.S)
    names = re.findall(r'"([^"]+)"', match.group(1))
    assert len(names) == 41
    birthrights = []
    for player, name in enumerate(names):
        original = f"resources/gfx/items/collectibles/birthright/{player}{name}_birthright.png"
        raw = source("2690434875", original)
        relative = f"gfx/isaac_mods/item_visuals/birthright/{player}.png"
        target = ASSETS / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(raw)
        with Image.open(target) as image:
            image.load()
            dimensions = list(image.size)
            assert dimensions == [32, 32]
        resources.append(relative)
        birthrights.append({"player": player, "source": original, "path": relative, "sha256": digest(raw)})
    animations.sort(key=lambda row: row["id"])
    header = ["// Generated from supplied Workshop files; see docs/item-visuals-port.md.", "#pragma once",
              "namespace isaac_item_visuals::data {", "struct Animation { unsigned item, frames, layers; const char* file; };",
              "inline constexpr Animation Animations[] = {"]
    header += [f'    {{{row["id"]}, {row["frames"]}, {row["layers"]}, "{row["path"]}"}},' for row in animations]
    header += ["};", "inline constexpr const char* Birthrights[] = {"]
    header += [f'    "{row["path"]}",' for row in birthrights]
    header += ["};", "} // namespace isaac_item_visuals::data", ""]
    generated = ROOT / "features/item_visuals/generated.hpp"
    generated.parent.mkdir(parents=True, exist_ok=True)
    generated.write_text("\n".join(header), encoding="utf-8")
    report = {"timestamp_utc": datetime.now(timezone.utc).isoformat(), "status": "PASS", "executed_lua": False,
              "source_root": str(WORKSHOP), "source_files": used, "animations": animations,
              "birthrights": birthrights, "resource_paths": sorted(set(resources)),
              "native_asset_dependencies": game_assets,
              "native_items_sha256": digest(items_path.read_bytes()), "generated_sha256": digest(generated.read_bytes()),
              "source_defaults": "All vanilla animations enabled; no PC settings/custom-character files imported",
              "source_art_changes": "Only ANM2 sheet paths relocated relative to the ANM2 directory; PNG bytes unchanged"}
    (generated.parent / "asset-build.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: {len(animations)} animated items, {len(birthrights)} Birthright characters, {len(set(resources))} resources")


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
"""Stage the installed Workshop source in isolated resource paths, with hashes."""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
DEFAULT_SOURCE = Path(r"C:\Program Files (x86)\Steam\steamapps\workshop\content\250900\2575911103")
CHARACTERS = ["isaac", "magdalene", "cain", "judas", "xxx", "eve", "samson", "azazel", "lazarus", "eden", "lost", "lazarus2", "dark_judas", "lilith", "keeper", "apollyon", "forgor_bone", "forgor_soul", "bethany", "jacob", "esau", "t_isaac", "t_magdalene", "t_cain", "t_judas", "t_bluebaby", "t_eve", "t_samson", "t_azazel", "t_lazarus", "t_eden", "t_lost", "t_lilith", "t_keeper", "t_apollyon", "t_forgor_bone", "t_bethany", "t_jacob", "t_lazarus2", "t_jacob2", "t_forgor_soul"]
TRACK_NAMES = ["codex_specialist_classic", "codex_specialist_tainted", "codex_specialist_cain", "codex_specialist_eden"]
NATIVE = {
    "ModEntryConstruct": 0x420D18, "ModEntryDestroy": 0x41F8DC,
    "LoadCostumes": 0x3B9448, "GetNullItem": 0x3C0CC4,
    "AddNullCostume": 0x291BBC, "RemoveNullCostume": 0x2F2170,
    "LoadMusic": 0x425218, "PlayMusic": 0x427238, "CurrentMusic": 0x426C38,
    "UpdateVolume": 0x427908, "RoomPlayMusic": 0x451834,
    "HasCollectible": 0x27D3F4, "GetCurses": 0x3D57CC,
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE)
    args = parser.parse_args()
    source = args.source.resolve()
    source_resources = source / "resources/the"
    output = ROOT / "assets/specialist/resources"
    gfx = output / "gfx/codex_specialist"
    data = output / "codex_specialist"
    generated = ROOT / "features/specialist/generated"
    for folder in (gfx, data / "music", generated):
        folder.mkdir(parents=True, exist_ok=True)
    source_hashes, outputs = {}, {}

    def read(path):
        raw = path.read_bytes()
        source_hashes[path.relative_to(source).as_posix()] = sha(raw)
        return raw

    def write(path, raw):
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(raw)
        outputs[path.relative_to(output).as_posix()] = {"sha256": sha(raw), "bytes": len(raw)}

    read(source / "main.lua")
    read(source / "metadata.xml")
    original = ET.fromstring(read(source / "content/costumes2.xml"))
    costumes = ET.Element("costumes", anm2root="gfx/codex_specialist/")
    costume_paths, skipped = [], []
    referenced = set()
    for node in original:
        relative = node.attrib["anm2path"].replace("\\", "/")
        if node.attrib.get("type") != "none":
            raise ValueError("Only null costumes are allowed in the append-only registry")
        path = source_resources / relative
        if not path.is_file():
            # The upstream config has an unused, absent Berserk Samson costume.
            if relative != "specialist_t_samson2.anm2":
                raise FileNotFoundError(path)
            skipped.append(relative)
            continue
        animation = ET.fromstring(read(path))
        for sheet in animation.findall("./Content/Spritesheets/Spritesheet"):
            raw = sheet.attrib["Path"].replace("\\", "/")
            texture = (path.parent / raw).resolve()
            if not texture.is_relative_to(source_resources) or not texture.is_file():
                raise ValueError(f"Unresolved spritesheet: {path}: {raw}")
            sheet.attrib["Path"] = raw
            referenced.add(texture)
        write(gfx / relative, ET.tostring(animation, encoding="utf-8", xml_declaration=True))
        ET.SubElement(costumes, "costume", dict(node.attrib, anm2path=relative))
        costume_paths.append(relative)
    for texture in sorted(referenced):
        write(gfx / texture.relative_to(source_resources), read(texture))
    write(data / "costumes.xml", ET.tostring(costumes, encoding="utf-8", xml_declaration=True))

    # Upstream's explanatory comment contains a literal nested '<!--'. Ignore
    # comment text, while still parsing and validating every actual track.
    music_source = re.sub(rb"<!--.*?-->", b"", read(source / "content/music.xml"), flags=re.S)
    original_music = ET.fromstring(music_source)
    if len(original_music) != 4:
        raise ValueError("Expected the four original Specialist songs")
    music = ET.Element("music", root="codex_specialist/music/")
    for index, track in enumerate(original_music):
        path = source_resources / track.attrib["path"]
        raw = read(path)
        if not raw.startswith(b"OggS"):
            raise ValueError("Music source is not Ogg")
        write(data / "music" / path.name, raw)
        ET.SubElement(music, "track", name=TRACK_NAMES[index], path=path.name, loop="true")
    write(data / "music.xml", ET.tostring(music, encoding="utf-8", xml_declaration=True))
    mapping = [costume_paths.index("specialist_" + name + ".anm2") for name in CHARACTERS]
    binky = costume_paths.index("items/binky.anm2")
    metadata_path = ROOT / "eid/generated/context.json"
    quality_four = [row["id"] for row in json.loads(metadata_path.read_text())["metadata"] if row["quality"] >= 4]
    header = ["// Generated by tools/specialist/prepare_assets.py.\n#pragma once\n#include <cstddef>\nnamespace isaac::specialist::assets {",
              'inline constexpr char CostumeConfig[] = "codex_specialist/costumes.xml";',
              'inline constexpr char MusicConfig[] = "codex_specialist/music.xml";',
              "inline constexpr const char* Costumes[] = {"]
    header += [json.dumps("gfx/codex_specialist/" + path) + "," for path in costume_paths]
    header += ["};", "inline constexpr unsigned CharacterCostumes[] = {" + ",".join(map(str, mapping)) + "};",
               f"inline constexpr unsigned BinkyCostume = {binky};", "inline constexpr const char* MusicNames[] = {"]
    header += [json.dumps(name) + "," for name in TRACK_NAMES]
    header += ["};", "inline constexpr unsigned QualityFour[] = {" + ",".join(map(str, quality_four)) + "};", "}"]
    (generated / "assets.hpp").write_text("\n".join(header) + "\n", encoding="utf-8")
    nro = (ROOT.parent / "Isaac-dump/romfs/.nro/Repentance.nro").read_bytes()
    expected = "cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a"
    if sha(nro) != expected:
        raise ValueError("Native source is not the verified game build")
    native = ["// Generated native entry guards for the exact user's game build.", "#pragma once", "#include <cstddef>", "#include <cstdint>", "namespace isaac::specialist::native {"]
    native += [f"inline constexpr std::uintptr_t {name} = 0x{offset:x};" for name, offset in NATIVE.items()]
    native += ["inline constexpr std::size_t ModEntrySize = 0x12b8;", "struct Guard { std::uintptr_t offset; unsigned char bytes[16]; };", "inline constexpr Guard Guards[] = {"]
    native += [f"{{0x{offset:x},{{" + ",".join(f"0x{value:02x}" for value in nro[offset:offset+16]) + "}}," for offset in NATIVE.values()]
    native += ["};", "}"]
    (generated / "native.hpp").write_text("\n".join(native) + "\n", encoding="utf-8")
    manifest = {"source": str(source), "workshop_id": "2575911103", "version": "1.5.2", "source_sha256": source_hashes,
                "outputs": outputs, "costumes": len(costume_paths), "character_types": len(mapping), "referenced_textures": len(referenced),
                "songs": 4, "skipped_missing_unreferenced_upstream_asset": skipped, "nro_sha256": expected,
                "quality_metadata_sha256": sha(metadata_path.read_bytes()),
                "generated_sha256": {path.relative_to(ROOT).as_posix(): sha(path.read_bytes()) for path in sorted(generated.glob("*.hpp"))}}
    (ROOT / "assets/specialist/manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    (ROOT / "assets/specialist/credits.txt").write_text(
        "The Specialist for Good Items v1.5.2, Steam Workshop 2575911103.\n"
        "Jiftoo - programming; Devector - graphics/trailer.\n"
        "Original Pog mod by its authors (Workshop 2490515687); MMD skeleton by SAB64.\n"
        "Upstream metadata permits modifying/fixing the mod. Original metadata and source hashes are retained in manifest.json.\n"
        "This private Switch adaptation preserves the original animation frames and four audio files.\n", encoding="utf-8")
    print(f"Staged {len(costume_paths)} costumes for 41 character types, {len(referenced)} textures and 4 original songs.")


if __name__ == "__main__":
    main()

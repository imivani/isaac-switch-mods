#!/usr/bin/env python3
"""Pin the exact native frame draw and inspect the user's minimap resources."""
from pathlib import Path
import hashlib
import json
import struct
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))
from convert_ui_textures import decode_pcx

NRO_HASH = "cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a"


def sha(raw):
    return hashlib.sha256(raw).hexdigest()


def main():
    dump = ROOT.parent / "Isaac-dump/romfs"
    evidence = ROOT / "evidence/iteration4/clean-map"
    generated = ROOT / "features/clean_map/generated"
    generated.mkdir(parents=True, exist_ok=True)
    evidence.mkdir(parents=True, exist_ok=True)
    nro = (dump / ".nro/Repentance.nro").read_bytes()
    if sha(nro) != NRO_HASH:
        raise ValueError("Only the verified Switch Repentance build is supported")
    values = {
        "MinimapRender": 0x41d54c, "MinimapFrameSetup": 0x41d744,
        "FrameCall": 0x41d79c, "FrameReturn": 0x41d7a0,
        "GameMinimapSprite": 0x24ee88,
        "MenuManagerGlobal": 0xabcd48, "MenuRender": 0x3fe7b4, "MenuUpdate": 0x3fdc8c,
        "MenuController": 0x34, "MenuState": 0x38, "InputManagerGot": 0xaac768,
        "IsButtonPressed": 0x4f9504, "IsButtonTriggered": 0x4f9648,
        "DoesDeviceExist": 0x4fa838, "LoadShader": 0x36cd0,
        "IsActionPressed": 0x4f9920, "IsActionTriggered": 0x4f9a64,
        "FontIsLoaded": 0x4ce898, "FontDrawStringScaled": 0x4cef6c,
        "FontStringWidth": 0x4ce9bc, "MenuNativeFont": 0x117120,
        "BossHealthGuardSite": 0x3ae374, "BossHiddenReturn": 0x3ae3b0,
        "SetBossHealth": 0x3aacf0, "GameHud": 0xe8468,
        "HudBossHealth": 0xc53e8, "HudBossPosition": 0xc53f4,
        "RoomBossId": 0x1fc0,
    }
    guard_ranges = {"MinimapRender": (0x41d54c, 20), "MinimapFrameSetup": (0x41d744, 92),
        "MenuRender": (0x3fe7b4, 20), "MenuUpdate": (0x3fdc8c, 20),
        "IsButtonTriggered": (0x4f9648, 20), "IsButtonPressed": (0x4f9504, 20),
        "DoesDeviceExist": (0x4fa838, 20), "LoadShader": (0x36cd0, 24),
        "FontIsLoaded": (0x4ce898, 8), "SetBossHealth": (0x3aacf0, 20),
        "BossHealthGuardSite": (0x3ae374, 64), "BossHiddenReturn": (0x3ae3b0, 52)}
    lines = ["// Generated from the exact user-owned NRO; do not hand-edit.", "#pragma once", "#include <cstdint>",
             "namespace isaac::clean_map::native {"]
    lines += [f"inline constexpr std::uintptr_t {name} = 0x{value:x};" for name, value in values.items()]
    for name, (offset, size) in guard_ranges.items():
        lines.append(f"inline constexpr unsigned char {name}Guard[] = {{" + ",".join(f"0x{b:02x}" for b in nro[offset:offset+size]) + "};")
    lines += ["}", ""]
    (generated / "native.hpp").write_text("\n".join(lines), encoding="utf-8")
    instruction, = struct.unpack_from("<I", nro, values["FrameCall"])
    immediate = instruction & 0x3ffffff
    if immediate & 0x2000000: immediate -= 0x4000000
    if instruction >> 26 != 0b100101 or values["FrameCall"] + immediate * 4 != 0x66fcf0:
        raise ValueError("Frame call is not BL to the verified ANM2::Render PLT")
    if nro[0x8b3dd8:0x8b3dde] != b"Frame\0":
        raise ValueError("Frame animation literal changed")
    resources = []
    for name in ("minimap1", "minimap2"):
        animation_path = dump / f"rp_patch/resources/gfx/ui/{name}.anm2"
        texture_path = dump / f"resources/gfx/ui/{name}.pcx"
        animation = ET.parse(animation_path).getroot()
        image = decode_pcx(texture_path.read_bytes())
        image.save(evidence / f"{name}-decoded.png")
        frames = []
        for anim in animation.findall("./Animations/Animation"):
            frames.append({"name": anim.attrib["Name"], "frames": int(anim.attrib["FrameNum"]),
                "rectangles": [{key: frame.attrib.get(key) for key in ("XCrop","YCrop","Width","Height","Visible")}
                    for frame in anim.findall("./LayerAnimations/LayerAnimation/Frame")]})
        resources.append({"animation": str(animation_path), "animation_sha256": sha(animation_path.read_bytes()),
            "texture": str(texture_path), "texture_sha256": sha(texture_path.read_bytes()), "size": list(image.size),
            "animations": frames})
    result = {"status": "PASS", "hardware_test": False, "nro_sha256": NRO_HASH, "native": values,
        "guard_ranges": {name: {"offset": offset,"bytes":nro[offset:offset+size].hex()} for name,(offset,size) in guard_ranges.items()},
        "frame_call_target": "0x66fcf0 ANM2::Render PLT", "resources": resources,
        "source_sha256": {"tools/clean_map/inspect_resources.py": sha(Path(__file__).read_bytes()),
            "tools/convert_ui_textures.py": sha((ROOT/"tools/convert_ui_textures.py").read_bytes()),
            "features/clean_map/generated/native.hpp": sha((generated/"native.hpp").read_bytes())}}
    (evidence/"resource-inspection.json").write_text(json.dumps(result, indent=2)+"\n", encoding="utf-8")
    print("PASS: exact Frame draw, native guards and both original minimap resource sets recorded.")


if __name__ == "__main__": main()

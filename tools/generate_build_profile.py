"""Bind this prototype to the verified Switch Repentance module, offline."""
import hashlib
import json
import pathlib
import struct
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
DUMP = ROOT.parent / "Isaac-dump"
TARGETS = {
    "HudRender": "_ZN15IsaacRepentance3HUD6RenderEv",
    "DrawStringScaled": "_ZNK4KAGE8Graphics4Font16DrawStringScaledEPKcffffNS0_5ColorEjb",
    "FillQuad": "_ZN4KAGE8Graphics13ShapeRenderer8FillQuadEffffRKNS0_5ColorE",
    "GameGlobal": "_ZN15IsaacRepentance6g_GameE",
    "ManagerGlobal": "_ZN15IsaacRepentance9g_ManagerE",
    "ShapeRenderer": "_ZN4KAGE8Graphics15g_ShapeRendererE",
    "GetCurses": "_ZNK15IsaacRepentance5Level9GetCursesEv",
}


def main():
    raw = (DUMP / "romfs/.nro/Repentance.nro").read_bytes()
    known = json.loads((DUMP / "evidence/nro-inspection.json").read_text())
    expected = next(m for m in known["modules"] if m["file"].endswith("Repentance.nro"))
    assert hashlib.sha256(raw).hexdigest() == expected["sha256"]
    ro = struct.unpack_from("<I", raw, 0x28)[0]
    so, ss, yo, ys = struct.unpack_from("<4I", raw, 0x70)
    strings = raw[ro + so:ro + so + ss]
    symbols = {}
    for pos in range(ro + yo, ro + yo + ys, 24):
        name, info, other, section, value, size = struct.unpack_from("<IBBHQQ", raw, pos)
        if section:
            label = strings[name:].split(b"\0", 1)[0].decode("ascii")
            symbols[label] = (value, size)
    lines = ["// Generated from the user's verified dump. Do not hand-edit.",
             "#pragma once", "#include <cstdint>", "namespace isaac::profile {"]
    lines.append('inline constexpr char LookupSymbol[] = "' + TARGETS["HudRender"] + '";')
    profile = {"nro_sha256": expected["sha256"], "nro_build_id": raw[0x40:0x60].hex(),
               "title_id": "010021C000B6A000", "version": "1.7.9b", "symbols": {}}
    for key, name in TARGETS.items():
        value, size = symbols[name]
        lines.append(f"inline constexpr std::uintptr_t {key} = 0x{value:x};")
        profile["symbols"][key] = {"name": name, "offset": hex(value), "size": size}
    arrays = {"NroBuildId": raw[0x40:0x60],
              "MainBuildId": (DUMP / "exefs/main").read_bytes()[0x40:0x60],
              "HudPrologue": raw[symbols[TARGETS["HudRender"]][0]:symbols[TARGETS["HudRender"]][0] + 20]}
    sys.path.insert(0, r"C:\CodexTools\ReverseEngineering\PythonModules")
    import lz4.block
    sdk = (DUMP / "exefs/sdk").read_bytes()
    assert hashlib.sha256(sdk).hexdigest() == "d30e3b441f4cfce2722eac09474354cad82d56741c86240fe4ae0cc4b7fd7df2"
    offset, mem, size = struct.unpack_from("<III", sdk, 0x10)
    packed_size = struct.unpack_from("<I", sdk, 0x60)[0]
    text = lz4.block.decompress(sdk[offset:offset+packed_size], uncompressed_size=size)
    assert mem == 0 and hashlib.sha256(text).digest() == sdk[0xa0:0xc0]
    arrays["SdkLoadPrologue"] = text[0x3918d0:0x3918d0+20]
    arrays["SdkUnloadPrologue"] = text[0x391d20:0x391d20+20]
    assert symbols[TARGETS["HudRender"]][0] % 0x1000 <= 0x1000 - 20
    for name, data in arrays.items():
        lines.append(f"inline constexpr std::uint8_t {name}[] = {{" +
                     ", ".join(f"0x{x:02x}" for x in data) + "};")
    # These layout values are separately proven by native disassembly, not
    # inferred from the original PC or iOS builds. See docs/native-api.md.
    fields = {"GameRoom": 0x21550, "RoomEntities": 0x19c8, "RoomEntityCount": 0x19d4,
              "ManagerFont": 0x76f90, "HudVisible": 0xc53fa,
              "EntityType": 0x38, "EntityVariant": 0x3c, "EntitySubtype": 0x40,
              "EntityVisible": 0x1c1, "EntityExists": 0x1c2, "EntityDead": 0x1c3,
              "EntityX": 0x310, "EntityY": 0x314, "EntityHp": 0x354, "EntityMaxHp": 0x358,
              "EntityIsBossVtableSlot": 0x98, "PickupForceBlind": 0x562,
              "PlayersBegin": 0x25c50, "PlayersEnd": 0x25c58,
              "ScreenWidth": 0xab1360, "ScreenHeight": 0xab1364,
              "ChallengeId": 0x26fa88, "AlternateChallenge": 0x26fb10,
              "TextSize": struct.unpack_from("<I", raw, 0x24)[0]}
    for key, value in fields.items():
        lines.append(f"inline constexpr std::uintptr_t {key} = 0x{value:x};")
    lines.append("}")
    dest = ROOT / "native/source/program/generated/build_profile.hpp"
    dest.parent.mkdir(parents=True, exist_ok=True)
    dest.write_text("\n".join(lines) + "\n", encoding="utf-8")
    (ROOT / "evidence").mkdir(exist_ok=True)
    (ROOT / "evidence/build-profile.json").write_text(json.dumps(profile, indent=2) + "\n", encoding="utf-8")
    print(f"Generated exact-build profile for {profile['version']}; {len(TARGETS)} symbols verified")


if __name__ == "__main__":
    main()

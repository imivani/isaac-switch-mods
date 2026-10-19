#!/usr/bin/env python3
"""Record the exact dump and instruction evidence behind the stat HUD adapter."""
import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re


def sha256(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    project = Path(__file__).resolve().parents[2]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--nro", type=Path, default=project.parent / "Isaac-dump/romfs/.nro/Repentance.nro")
    args = parser.parse_args()
    binary = args.nro.read_bytes()
    source = (project / "native/source/program/planetarium.cpp").read_text(encoding="utf-8")
    symbols = json.loads((project / "evidence/native-analysis/symbols.json").read_text(encoding="utf-8"))
    by_name = {item["name"]: item for item in symbols if item["defined"]}
    checks = []
    for label, symbol, offset, guard_name in [
        ("StatHUD::Render", "_ZN15IsaacRepentance3HUD7StatHUD6RenderERKN4KAGE4Math7Vector2E", 0x3af194, "StatHudPrologue"),
        ("Font::DrawString", "_ZNK4KAGE8Graphics4Font10DrawStringEPKcffNS0_5ColorEjb", 0x4ceef0, "DrawStringPrologue"),
    ]:
        entry = by_name[symbol]
        assert entry["offset"] == offset, (label, entry)
        # Keep the ELF-exported size in the report; the guarded entry does not
        # depend on a decompiler's guessed function boundary.
        expression = re.search(r"constexpr std::uint8_t " + guard_name + r"\[\] = \{([^}]+)\}", source)
        guard = bytes(int(value, 16) for value in re.findall(r"0x([0-9a-fA-F]+)", expression.group(1)))
        assert len(guard) == 20 and binary[offset:offset + 20] == guard, label
        checks.append({"function": label, "symbol": symbol, "offset": hex(offset),
                       "exported_size": entry["size"], "source_guard_matches_dump": True,
                       "first_twenty_bytes": guard.hex()})
    assert binary[0x10:0x14] == b"NRO0"
    assert binary[0x40:0x60].hex() == "91c73fdd575061318d68886316afeac72388b2ab000000000000000000000000"
    # Native IsLoaded is exactly LDRB W0,[X0]; RET. This validates reading its
    # first byte without adding a guessed object layout or another native call.
    assert binary[0x4ce898:0x4ce8a0].hex() == "00004039c0035fd6"
    proofs = [
        "evidence/native-analysis/expanded/3af194.c",
        "evidence/native-analysis/expanded/3af194.asm.txt",
        "evidence/native-analysis/3ac71c.c",
        "evidence/native-analysis/3ac71c.asm.txt",
        "evidence/native-analysis/35231c.c",
        "evidence/native-analysis/4ceef0.asm.txt",
        "evidence/native-analysis/4cef6c.asm.txt",
        "reference/isaac-planetarium-chance/main.lua",
        "native/source/program/planetarium.cpp",
        "native/source/program/planetarium.hpp",
        "features/planetarium/planetarium.hpp",
        "features/planetarium/stat_layout.hpp",
    ]
    report = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(),
        "status": "PASS", "hardware_test": False,
        "nro_size": len(binary), "nro_sha256": hashlib.sha256(binary).hexdigest(),
        "nro_build_id": binary[0x40:0x60].hex(), "entry_guards": checks,
        "native_font": {"manager_relative_address": "0xd7080", "scale": [1, 1],
                        "loaded_byte_verified_at": "0x4ce898", "rgba": [1, 1, 1, .5],
                        "same_font_object_as_original_stat_percentages": True},
        "position": {"game_relative_stat_hud": hex(0xe8468 + 0xc5968),
                     "origin": "Copied from the original StatHUD::Render Vector2 reference during this HUD pass",
                     "normal_rows": 8, "combined_deal_rows": 7,
                     "normal_spacing": 12, "two_populated_stat_entries_spacing": 14,
                     "chance_text_relative_to_icon": [16, 1],
                     "difference_text_relative_to_icon_before_slide": [46, 1],
                     "screen_shake": "Added once to text; not to icon",
                     "native_counters_and_hud_offset": "Already included in the observed origin"},
        "proof_sha256": {name: sha256(project / name) for name in proofs},
    }
    output = project / "evidence/planetarium-layout/native-layout.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: native stat entry guards, stat font ABI and evidence hashes: {output}")


if __name__ == "__main__":
    main()

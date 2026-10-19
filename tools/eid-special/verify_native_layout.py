#!/usr/bin/env python3
"""Record exact-build symbol/byte evidence for the read-only EID special adapter."""
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXPECTED_SHA = "cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a"
ENTRIES = {
    "_ZN15IsaacRepentance13Entity_Pickup13InitFlipStateEv": 0x26b6c8,
    "_ZN15IsaacRepentance13Entity_Pickup7TryFlipEPNS_13Entity_PlayerEj": 0x26b924,
    "_ZN15IsaacRepentance13Entity_Pickup6UpdateEv": 0x26c9b8,
    "_ZNK15IsaacRepentance13Entity_Player14HasCollectibleENS_16eCollectibleTypeEb": 0x27d3f4,
    "_ZNK15IsaacRepentance13Entity_Player20GetTrinketMultiplierENS_12eTrinketTypeE": 0x29b030,
    "_ZN15IsaacRepentance13Entity_Player18SalvageCollectibleERKN4KAGE4Math7Vector2ENS_16eCollectibleTypeEjNS_8ItemPool13eItemPoolTypeE": 0x2dd374,
    "_ZNK15IsaacRepentance10ItemConfig14GetCollectibleEi": 0x3c0c50,
    "_ZNK15IsaacRepentance10ItemConfig4Item13IsAvailableExEj": 0x3c101c,
    "_ZNK15IsaacRepentance8ItemPool14GetPoolForRoomENS_9eRoomTypeEj": 0x3c5c7c,
    "_ZN15IsaacRepentance7Manager15IsActionPressedEjjPNS_6EntityE": 0x3f9b6c,
    "_ZN15IsaacRepentance7Manager17IsActionTriggeredEjjPNS_6EntityE": 0x3f9b7c,
    "_ZNK15IsaacRepentance18PersistentGameData8UnlockedENS_12eAchievementE": 0x439654,
}


def main():
    binary = (ROOT.parent / "Isaac-dump/romfs/.nro/Repentance.nro").read_bytes()
    digest = hashlib.sha256(binary).hexdigest()
    assert digest == EXPECTED_SHA
    symbols = json.loads((ROOT / "evidence/native-analysis/symbols.json").read_text(encoding="utf-8"))
    symbols = {row["name"]: row for row in symbols if row["defined"]}
    entries = []
    for name, offset in ENTRIES.items():
        row = symbols[name]
        assert row["offset"] == offset
        entries.append({"symbol": name, "offset": hex(offset), "exported_size": row["size"],
                        "first_bytes": binary[offset:offset + min(20, row["size"])].hex()})
    # Input getters are non-consuming wrapper calls with identical argument
    # forwarding. Triggered targets the native input manager's edge getter.
    assert symbols["_ZN15IsaacRepentance7Manager17IsActionTriggeredEjjPNS_6EntityE"]["size"] == 16
    proofs = ["evidence/eid-special/flip.asm.txt", "evidence/eid-special/unlocked.asm.txt",
              "evidence/eid-reminder-input/device/constants.json", "evidence/eid-reminder-input/device/4fbae8.c",
              "evidence/eid-special/room-pool.asm.txt", "evidence/eid-special/salvage-review/2dd374.c",
              "evidence/eid-special/salvage-review/2dd374.asm.txt",
              "evidence/native-analysis/expanded/26c9b8.asm.txt", "evidence/native-analysis/eid-expansion/29b030.asm.txt",
              "reference/External-Item-Descriptions/features/eid_modifiers.lua",
              "reference/External-Item-Descriptions/features/eid_api.lua",
              "native/source/program/eid_special.cpp", "native/source/program/eid_special.hpp"]
    report = {
        "timestamp_utc": datetime.now(timezone.utc).isoformat(), "status": "PASS",
        "hardware_test": False, "nro_sha256": digest, "nro_size": len(binary),
        "nro_build_id": binary[0x40:0x60].hex(), "symbol_checks": entries,
        "reviewed_layout": {
            "flip": {"shared_data_pointer": "pickup+0x610", "shared_control_pointer_unused": "pickup+0x618",
                     "saved_type_variant_subtype": [0, 4, 8], "proof": "InitFlipState 0x26b7f4..0x26b850; TryFlip 0x26b9f8..0x26b9fc"},
            "crown": {"alternate_array": "pickup+0x5e4", "alternate_count": "pickup+0x604",
                      "alternate_capacity": 8, "current_plus_alternate_capacity": 9,
                      "proof": "Pickup::Update 0x26dee8..0x26dfb4"},
            "salvage": {"room_type": "room+0x10", "item_pool": "game+0x242c0",
                        "normal_room_pool_table": "itempool+0x920", "greed_room_pool_table": "itempool+0x998",
                        "unlock_achievements": {"bone_heart": 391, "cracked_key": 415, "rotten_heart": 411},
                        "persistent_data": "manager+0x18 (embedded object)"},
        },
        "mutation_calls": [], "hooks_added": [], "new_assets": [],
        "input_devices": {"native_ids": [1, 2, 3, 4, 5, 6, 7, 8, 33], "mask_bits": "Validated player vector slots0..7, never native device IDs"},
        "runtime_scope": "Parent loader's exact NRO build guard; bounded pointers/IDs, read-only getters, fixed cache of32pedestals",
        "proof_sha256": {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in proofs},
    }
    output = ROOT / "evidence/eid-special/native-layout.json"
    output.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(f"PASS: {len(entries)} exact-build symbols and evidence hashes: {output}")


if __name__ == "__main__":
    main()

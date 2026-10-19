"""Check binary structure, imports and the derived descriptor before packaging.

These offline checks never claim that the mod has run on a console.
"""
import argparse
import hashlib
import json
import pathlib
import struct
import sys
sys.path.insert(0, r"C:\CodexTools\ReverseEngineering\PythonModules")
import lz4.block
from prepare_npdm import derive

ROOT = pathlib.Path(__file__).resolve().parents[1]
# Confirmed against this exact SDK's defined dynamic symbols, not its strings.
SDK_IMPORTS = {
    "_ZN2nn2ro10LoadModuleEPNS0_6ModuleEPKvPvmi",
    "_ZN2nn2ro12UnloadModuleEPNS0_6ModuleE",
    "_ZN2nn2ro18LookupModuleSymbolEPmPKNS0_6ModuleEPKc",
}


def elf_symbols(raw, mode):
    assert raw[:6] == b"\x7fELF\x02\x01", "Expected little-endian 64-bit ELF"
    assert struct.unpack_from("<H", raw, 0x12)[0] == 183, "ELF is not AArch64"
    start = struct.unpack_from("<Q", raw, 0x28)[0]
    size, count = struct.unpack_from("<HH", raw, 0x3a)
    assert size == 64 and start + size * count <= len(raw)
    sections = [struct.unpack_from("<IIQQQQIIQQ", raw, start + i * size) for i in range(count)]
    imports = []
    defined = []
    for section in sections:
        if section[1] not in (2, 11):
            continue
        string_section = sections[section[6]]
        strings = raw[string_section[4]:string_section[4] + string_section[5]]
        assert section[9] == 24
        for pos in range(section[4], section[4] + section[5], 24):
            name, info, other, index, value, length = struct.unpack_from("<IBBHQQ", raw, pos)
            if section[1] == 11 and index == 0 and info >> 4 == 1:
                imports.append(strings[name:].split(b"\0", 1)[0].decode("ascii"))
            if index != 0:
                defined.append(strings[name:].split(b"\0", 1)[0].decode("ascii"))
    assert set(imports) == SDK_IMPORTS, f"Unexpected required imports: {set(imports) ^ SDK_IMPORTS}"
    if mode != "loader-test":
        for required in ("RenderOverlay", "EnglishRepentance", "RenderPlanetarium", "RenderRegret"):
            assert any(required in name for name in defined), f"Missing combined feature code: {required}"
    if mode in ("combined-polished", "combined-expanded", "combined-extended", "combined-refined", "combined-plus", "combined-balance", "combined-menu"):
        description_renderer = "RenderEidContext" if mode in ("combined-expanded", "combined-extended", "combined-refined", "combined-plus", "combined-balance", "combined-menu") else "RenderEidDescription"
        for required in ("RenderBossUi", description_renderer, "ResetBossUi", "ResetEidUi"):
            assert any(required in name for name in defined), f"Missing polished feature code: {required}"
    if mode in ("combined-expanded", "combined-extended", "combined-refined", "combined-plus", "combined-balance", "combined-menu"):
        for required in ("InitializeTimeMachine", "TimeMachineGameUpdate", "InitializePlanetarium", "RenderEidFeatures"):
            assert any(required in name for name in defined), f"Missing iteration-2 feature code: {required}"
    if mode in ("combined-extended", "combined-refined", "combined-plus", "combined-balance", "combined-menu"):
        for required in ("InitializeDebugKit", "UpdateDebugKit", "InitializeSpecialist", "UpdateSpecialist", "InitializeInstantBreak", "UpdateInstantBreak", "InitializeItemVisuals"):
            assert any(required in name for name in defined), f"Missing iteration-3 feature code: {required}"
    if mode in ("combined-plus", "combined-balance", "combined-menu"):
        for required in ("InitializeHitCounter", "InitializeGreedMachine", "RenderHitCounter", "HoldLock", "UpdateGreedMachine", "PlayerTakeDamage8Callback", "GreedBreakChance8Callback"):
            assert any(required in name for name in defined), f"Missing iteration-5 code: {required}"
    if mode in ("combined-refined", "combined-plus", "combined-balance", "combined-menu"):
        for required in ("ModsMenuUpdate", "ProcessDebugKitInput", "ShouldHideCleanMapFrame", "BeforeNativeGameInput", "TimeMachineGameInput", "HasLayerTexture"):
            assert any(required in name for name in defined), f"Missing iteration-4 code: {required}"
    if mode == "combined-menu":
        for required in ("InitializeModMenu", "RenderModMenu", "UpdateModMenu", "PollModMenuPreferences", "DrawHitCounterPreview",
                         "DrawEidMenuBody", "SetTimeMachineMaxSteps", "SetEidOptions"):
            assert any(required in name for name in defined), f"Missing iteration-7 code: {required}"
    return sorted(imports)


def check_nso(raw, mode):
    assert raw[:4] == b"NSO0" and len(raw) >= 0x100
    flags = struct.unpack_from("<I", raw, 0xc)[0]
    segments = []
    contents = []
    for i in range(3):
        offset, virtual, size = struct.unpack_from("<III", raw, 0x10 + i * 0x10)
        compressed = flags & (1 << i)
        stored_size = struct.unpack_from("<I", raw, 0x60 + i * 4)[0] if compressed else size
        assert 0 < size < 32 * 1024 * 1024
        assert offset >= 0x100 and offset + stored_size <= len(raw)
        assert virtual % 0x1000 == 0
        stored = raw[offset:offset + stored_size]
        data = lz4.block.decompress(stored, uncompressed_size=size) if compressed else stored
        assert len(data) == size
        assert flags & (1 << (i + 3)), "Segment checksum flag is missing"
        assert hashlib.sha256(data).digest() == raw[0xa0 + i * 32:0xc0 + i * 32]
        contents.append(data)
        segments.append({"virtual": hex(virtual), "bytes": size, "sha256_valid": True})
    for a, b in zip(segments, segments[1:]):
        assert int(a["virtual"], 16) + a["bytes"] <= int(b["virtual"], 16)
    joined = b"".join(contents)
    if mode in ("combined-polished", "combined-expanded", "combined-extended", "combined-refined", "combined-plus", "combined-balance", "combined-menu"):
        assert b"Native mod test: LOADED" not in joined and b"Isaac: four-mod test build" not in joined, "Polished build still has a test banner"
    else:
        expected = b"Native mod test: LOADED" if mode == "loader-test" else b"Isaac: four-mod test build"
        assert expected in joined, "The compiled module does not match the requested build mode"
    return segments


def check_hook_pool():
    """Every trampoline hook needs one slot in exlaunch's fixed JIT pool. Too many
    hooks abort the game at start-up (iteration 7's first install did exactly that)."""
    import re
    setting = (ROOT / "native/source/program/setting.hpp").read_text()
    jit = int(re.search(r"constexpr size_t JitSize = (0x[0-9a-fA-F]+|\d+);", setting).group(1), 0)
    impl = (ROOT / "native/source/lib/hook/nx64/hook_impl.cpp").read_text()
    instructions = int(re.search(r"constexpr s64 MaxInstructions = (\d+);", impl).group(1))
    trampoline = int(re.search(r"constexpr size_t TrampolineSize = MaxInstructions \* (\d+);", impl).group(1)) * instructions
    capacity = jit // (trampoline * 4)
    sites = sorted(f"{path.name}:{match.group(1)}"
                   for path in (ROOT / "native/source/program").glob("*.cpp")
                   for match in re.finditer(r"\b(\w+)::InstallAt(?:Ptr|FuncPtr|Offset|Symbol)\(", path.read_text()))
    assert len(sites) == len(set(sites)), "A hook is installed from more than one place"
    assert len(sites) <= capacity, f"{len(sites)} hooks but the exlaunch pool holds {capacity}: raise JitSize"
    return {"hook_install_sites": len(sites), "hook_pool_capacity": capacity, "hooks": sites}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--mode", choices=("loader-test", "combined-prototype", "combined-polished", "combined-expanded", "combined-extended", "combined-refined", "combined-plus", "combined-balance", "combined-menu"), required=True)
    args = parser.parse_args()
    nso = (ROOT / "native/deploy/subsdk9").read_bytes()
    elf = (ROOT / "native/native.elf").read_bytes()
    original = (ROOT.parent / "Isaac-dump/exefs/main.npdm").read_bytes()
    descriptor = (ROOT / "native/deploy/main.npdm").read_bytes()
    assert descriptor == derive(original)[0], "Descriptor differs from verified minimal derivation"
    imports = elf_symbols(elf, args.mode)
    report = {"mode": args.mode, "result": "PASS", "architecture": "AArch64",
              "nso_sha256": hashlib.sha256(nso).hexdigest(), "nso_bytes": len(nso),
              "nso_segments": check_nso(nso, args.mode), "required_sdk_imports": imports,
              "npdm_sha256": hashlib.sha256(descriptor).hexdigest(),
              "console_test": "NOT_RUN", "full_upstream_feature_parity": False,
              "feature_code_linked": ["boss_bars_and_item_descriptions", "planetarium", "regret", "time_machine"]
                if args.mode != "loader-test" else ["loader_label"]}
    report["hook_pool"] = check_hook_pool()
    if args.mode in ("combined-extended", "combined-refined", "combined-plus", "combined-balance", "combined-menu"):
        report["feature_code_linked"] += ["map_inventory", "instant_fires_and_poops", "animated_items", "unique_birthright_sprites", "specialist", "debug_kit"]
    if args.mode in ("combined-refined", "combined-plus", "combined-balance", "combined-menu"):
        report["feature_code_linked"] += ["item_picker", "mod_menu", "clean_minimap", "stock_boss_bar_toggle"]
    if args.mode in ("combined-plus", "combined-balance", "combined-menu"):
        report["feature_code_linked"] += ["machine_lock", "greed_machine_never_jams", "hit_counter"]
    if args.mode in ("combined-balance", "combined-menu"):
        report["feature_code_linked"] += ["repentance_plus_balance_metadata"]
    if args.mode == "combined-menu":
        report["feature_code_linked"] += ["paper_mod_menu", "pause_menu_entry", "saved_menu_settings"]
    if args.mode in ("combined-expanded", "combined-extended", "combined-refined", "combined-plus", "combined-balance", "combined-menu"):
        sources = []
        for directory in ("native/source", "eid", "eid_ui", "eid_special", "features"):
            sources.extend(path for path in (ROOT / directory).rglob("*")
                           if path.is_file() and path.suffix in (".c", ".cpp", ".h", ".hpp", ".s", ".S", ".inc")
                           and "test-build" not in path.parts)
        sources.append(ROOT / "native/config.mk")
        report["build_source_sha256"] = {
            path.relative_to(ROOT).as_posix(): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in sorted(sources)
        }
    (ROOT / "evidence").mkdir(exist_ok=True)
    (ROOT / f"evidence/check-{args.mode}.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    display = dict(report)
    if "build_source_sha256" in display:
        display["source_snapshot_files_recorded"] = len(display.pop("build_source_sha256"))
    print(json.dumps(display, indent=2))


if __name__ == "__main__":
    main()

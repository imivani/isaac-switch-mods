"""Extract exact-build guards for the Repentance+ Rules mod from the verified Switch NRO.

Functions come from the NRO's own symbols. The call sites and field reads below
were read from Room::SpawnClearAward, Room::Update and Ambush::Update in this
exact build (docs/rplus-port.md); their bytes are pinned so a different build
fails here instead of producing a header that patches the wrong instruction.
"""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXPECTED = 'cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a'
raw = (ROOT.parent / 'Isaac-dump/romfs/.nro/Repentance.nro').read_bytes()
assert hashlib.sha256(raw).hexdigest() == EXPECTED
symbols = json.loads((ROOT / 'evidence/native-analysis/symbols.json').read_text())
functions = {
    'RngRandom': ('_ZN15IsaacRepentance3RNG6RandomEj', 20),
    'SpawnClearAward': ('_ZN15IsaacRepentance4Room15SpawnClearAwardEv', 20),
    'SpawnGridEntity': ('_ZN15IsaacRepentance4Room15SpawnGridEntityEiNS_15eGridEntityTypeEjji', 20),
    'GetCenterPos': ('_ZNK15IsaacRepentance4Room12GetCenterPosEv', 20),
    'FindFreePickupSpawnPosition': ('_ZNK15IsaacRepentance4Room27FindFreePickupSpawnPositionERKN4KAGE4Math7Vector2Efbb', 20),
    'FixTrapdoorPos': ('_ZN15IsaacRepentance4Room16fix_trapdoor_posERi', 20),
    'RandomU32': ('_ZN15IsaacRepentance9RandomU32Ev', 20),
    'IsStageAvailable': ('_ZNK15IsaacRepentance5Level16IsStageAvailableENS_11eLevelStageENS_10eStageTypeE', 20),
    'GetCurses': ('_ZNK15IsaacRepentance5Level9GetCursesEv', 20),
    # Proves the cutscene counter layout read below: add x8,x0,w1,uxtw #2; ldr w9,[x8,#0xe20].
    'IncreaseCutsceneCounter': ('_ZN15IsaacRepentance18PersistentGameData23IncreaseCutsceneCounterENS_8Cutscene9eCutsceneE', 28),
}
# (offset, expected bytes, meaning)
sites = {
    # mov w1,#2; bl RNG::Random; ... cbnz w0 -> Game::End(8): Mega Satan's end-or-portal roll.
    'MegaSatanRoll': (0x4582c4, '41008052da5e0894ba320090f90f40f95a4f43f9a0150035'),
    # Game::End(8) on a non-zero roll (ldr x0,[x26]; mov w1,#8; bl Game::End; b ...).
    'MegaSatanEnd': (0x45858c, '400340f901018052ef7c0894a0feff17'),
    # mov w1,#5; sub x0,x29,#0x88; bl RNG::Random; cbnz w0: the 1-in-5 portal after ???/The Lamb.
    'ChestRoll': (0x459184, 'a1008052a0a300d1295b08944074ff35'),
    # Level current room index (game+0x21558) compared with -10: Mother's room check.
    'MotherRoomCheck': (0x4572a4, '08ab82524800a072086868b81f29003101030054'),
    # Room::Update: Ambush active byte at game+0x26fae0, Ambush::Update(game+0x26faa8).
    'AmbushCall': (0x46e650, '880340f9095c9f52c904a07209696938a900003409559f52c904a0720001098b44410894'),
    # Ambush::Update reads the wave total (+0x30), current wave (+0x3c) and countdown (+0x40).
    'AmbushFields': (0x004fd8, '693240b9610000546b3e40b96b3200b96b4240b9'),
}
lines = ['// Generated from the verified user dump; see tools/rplus/generate_guards.py.',
         '#pragma once', '#include <cstdint>', 'namespace isaac::rplus::native {']
report = {'nro_sha256': EXPECTED, 'functions': {}, 'sites': {}}
for label, (name, length) in functions.items():
    entry = next(x for x in symbols if x['defined'] and x['name'] == name)
    address, size = entry['offset'], entry['size']
    length = min(length, size)
    guard = raw[address:address + length]
    assert len(guard) == length and length >= 4
    lines.append(f'inline constexpr std::uintptr_t {label} = 0x{address:x};')
    lines.append(f'inline constexpr std::uint8_t {label}Guard[] = {{' + ','.join(f'0x{x:02x}' for x in guard) + '};')
    report['functions'][label] = {'symbol': name, 'offset': hex(address), 'size': size, 'bytes': guard.hex()}
for label, (address, expected) in sites.items():
    guard = raw[address:address + len(expected) // 2]
    assert guard.hex() == expected, f'{label}: bytes differ at {address:#x}'
    lines.append(f'inline constexpr std::uintptr_t {label} = 0x{address:x};')
    lines.append(f'inline constexpr std::uint8_t {label}Guard[] = {{' + ','.join(f'0x{x:02x}' for x in guard) + '};')
    report['sites'][label] = {'offset': hex(address), 'bytes': expected}
lines += [
    '// Return addresses of the two RNG::Random calls (the instruction after each bl).',
    'inline constexpr std::uintptr_t MegaSatanRollReturn = MegaSatanRoll + 8;',
    'inline constexpr std::uintptr_t ChestRollReturn = ChestRoll + 12;',
    'inline constexpr std::uintptr_t GameRoomIndex = 0x21558, GameStage = 0, GameAmbush = 0x26faa8;',
    'inline constexpr std::uintptr_t AmbushActive = 0x38, AmbushWaveTotal = 0x30, AmbushWave = 0x3c;',
    'inline constexpr std::uintptr_t RoomType = 0x10, RoomWidth = 0x14, RoomHeight = 0x18;',
    'inline constexpr std::uintptr_t ManagerPersistentData = 0x18, CutsceneCounters = 0xe20;',
    'inline constexpr int MotherRoomIndex = -10, VoidStage = 12, BossRoomType = 5, BossRushRoomType = 17;',
    'inline constexpr unsigned MotherCutscene = 24, GridTrapdoor = 17;',
    '}',
]
(ROOT / 'native/source/program/generated/rplus_guards.hpp').write_text('\n'.join(lines) + '\n')
(ROOT / 'evidence/rplus').mkdir(parents=True, exist_ok=True)
(ROOT / 'evidence/rplus/guards.json').write_text(json.dumps(report, indent=2) + '\n')
print(f'Repentance+ Rules: {len(functions)} function and {len(sites)} site guards generated')

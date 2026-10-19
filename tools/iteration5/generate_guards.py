"""Extract exact-build guards for the iteration-5 hooks from the immutable NRO.

Hit Counter hooks Entity_Player::TakeDamage. Greed Machine Never Jams hooks
Entity_Player::GetGreedDonationBreakChance and clears the run's jam flag; the
instructions that compare the chance, set the flag and read it on Init are
guarded too, so a different build leaves everything untouched.
"""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXPECTED = 'cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a'
raw = (ROOT.parent / 'Isaac-dump/romfs/.nro/Repentance.nro').read_bytes()
assert hashlib.sha256(raw).hexdigest() == EXPECTED
symbols = json.loads((ROOT / 'evidence/native-analysis/symbols.json').read_text())

def symbol(name):
    entry = next(x for x in symbols if x['defined'] and x['name'] == name)
    return entry['offset'], entry['size']

functions = {
    'PlayerTakeDamage': '_ZN15IsaacRepentance13Entity_Player10TakeDamageEfmRKNS_6Entity9EntityRefEi',
    'GreedBreakChance': '_ZNK15IsaacRepentance13Entity_Player27GetGreedDonationBreakChanceEv',
    'SlotUpdate': '_ZN15IsaacRepentance11Entity_Slot6UpdateEv',
    'SlotInit': '_ZN15IsaacRepentance11Entity_Slot4InitENS_11eEntityTypeEjjj',
    # Pure reads: int GetStringWidth(const char*) and ldrh w0,[x0,#20]; ret.
    'FontStringWidth': '_ZNK4KAGE8Graphics4Font14GetStringWidthEPKc',
    'FontLineHeight': '_ZNK4KAGE8Graphics4Font13GetLineHeightEv',
}
entries = ('PlayerTakeDamage', 'GreedBreakChance', 'FontStringWidth', 'FontLineHeight')
# (label, absolute offset, length, owning function, expected instruction words)
sites = [
    # bl GetGreedDonationBreakChance ... fmul s0,s0,#100; fcmp s0,s8; b.pl no_jam
    ('JamDecision', 0x32e368, 0x30, 'SlotUpdate', None),
    # mov w9,#0xfa50; movk w9,#0x26,lsl16; ldr x10,[x8,x9]; orr x10,x10,#0x400000000
    ('JamFlagWrite', 0x32e3c4, 0x10, 'SlotUpdate', [0x529f4a09, 0x72a004c9, 0xf869690a, 0xb25e014a]),
    # mov w9,#0xfa54; movk w9,#0x26,lsl16; ldrb w8,[x8,x9]; tbz w8,#2,...
    ('JamFlagInit', 0x328ee0, 0x10, 'SlotInit', [0x529f4a89, 0x72a004c9, 0x38696908, 0x36100a28]),
]
lines = ['// Generated from the verified user dump; see tools/iteration5/generate_guards.py.',
         '#pragma once', '#include <cstdint>', 'namespace isaac::iteration5::native {',
         '// Game::m_StateFlags at game+0x26fa50; bit 34 marks the Greed machine as jammed.',
         'inline constexpr std::uintptr_t GameStateFlags = 0x26fa50;',
         'inline constexpr std::uint64_t GreedJammedFlag = 1ull << 34;',
         '// Entity_Player fields read around TakeDamage (see evidence/iteration5).',
         'inline constexpr std::uintptr_t PlayerInvincibility = 0x1734;',
         '// DAMAGE_NO_PENALTIES | DAMAGE_RED_HEARTS: the game skips its own penalty counters.',
         'inline constexpr std::uint64_t SelfDamageFlags = 0x10000020;',
         'inline constexpr std::uint64_t FakeDamageFlag = 0x200000;']
report = {'nro_sha256': EXPECTED, 'guards': {}}
ranges = {}
for label, name in functions.items():
    address, size = symbol(name)
    ranges[label] = (address, size)
    if label in entries:
        guard = raw[address:address + min(20, size)]
        lines.append(f'inline constexpr std::uintptr_t {label} = 0x{address:x};')
        lines.append(f'inline constexpr std::uint8_t {label}Guard[] = {{' + ','.join(f'0x{x:02x}' for x in guard) + '};')
        report['guards'][label] = {'symbol': name, 'offset': hex(address), 'size': size, 'bytes': guard.hex()}
for label, address, length, owner, words in sites:
    start, size = ranges[owner]
    assert start <= address and address + length <= start + size, label
    guard = raw[address:address + length]
    if words:
        actual = [int.from_bytes(guard[i:i + 4], 'little') for i in range(0, length, 4)]
        assert actual == words, (label, [hex(x) for x in actual])
    lines.append(f'inline constexpr std::uintptr_t {label} = 0x{address:x};')
    lines.append(f'inline constexpr std::uint8_t {label}Guard[] = {{' + ','.join(f'0x{x:02x}' for x in guard) + '};')
    report['guards'][label] = {'owner': functions[owner], 'offset': hex(address), 'bytes': guard.hex()}
# The jam decision must call the exact break-chance function through its PLT stub.
call = int.from_bytes(raw[0x32e368:0x32e36c], 'little')
assert call >> 26 == 0x25
plt = json.loads((ROOT / 'evidence/native-analysis/plt.json').read_text())
target = 0x32e368 + ((call & 0x3ffffff) << 2)
assert any(p['offset'] == target and p['symbol']['name'] == functions['GreedBreakChance'] for p in plt)
lines.append('}')
(ROOT / 'native/source/program/generated/iteration5_guards.hpp').write_text('\n'.join(lines) + '\n')
(ROOT / 'evidence/iteration5').mkdir(parents=True, exist_ok=True)
(ROOT / 'evidence/iteration5/guards.json').write_text(json.dumps(report, indent=2) + '\n')
print(f'Iteration 5: {len(report["guards"])} exact-build guards generated')

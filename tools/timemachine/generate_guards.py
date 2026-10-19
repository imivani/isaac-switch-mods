"""Extract exact-build guards from the immutable, verified Switch NRO."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
EXPECTED = 'cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a'
raw = (ROOT.parent / 'Isaac-dump/romfs/.nro/Repentance.nro').read_bytes()
assert hashlib.sha256(raw).hexdigest() == EXPECTED
symbols = json.loads((ROOT / 'evidence/native-analysis/symbols.json').read_text())
targets = {
    'ProcessInput': ('_ZN15IsaacRepentance4Game12ProcessInputEv', 20),
    'InterpolatePositions': ('_ZN15IsaacRepentance4Game20InterpolatePositionsEv', 20),
    'FireplaceAi': ('_ZN15IsaacRepentance10Entity_NPC12ai_fireplaceEv', 20),
    'GameUpdate': ('_ZN15IsaacRepentance4Game6UpdateEv', 20),
    'IsPaused': ('_ZNK15IsaacRepentance4Game8IsPausedEv', 20),
    'IsActionPressed': ('_ZN15IsaacRepentance7Manager15IsActionPressedEjjPNS_6EntityE', 16),
    'IsActionTriggered': ('_ZN15IsaacRepentance7Manager17IsActionTriggeredEjjPNS_6EntityE', 16),
    'MovementInput': ('_ZN15IsaacRepentance13Entity_Player18get_movement_inputEv', 20),
}
lines = ['// Generated from the verified user dump; see tools/timemachine/generate_guards.py.',
         '#pragma once', '#include <cstdint>', 'namespace isaac::timemachine::native {']
report = {'nro_sha256': EXPECTED, 'guards': {}}
for label, (name, length) in targets.items():
    entry = next(x for x in symbols if x['defined'] and x['name'] == name)
    address, size = entry['offset'], entry['size']
    length = min(length, size)
    guard = raw[address:address + length]
    assert len(guard) == length and length >= 4
    lines.append(f'inline constexpr std::uintptr_t {label} = 0x{address:x};')
    lines.append(f'inline constexpr std::uint8_t {label}Guard[] = {{' + ','.join(f'0x{x:02x}' for x in guard) + '};')
    report['guards'][label] = {'symbol': name, 'offset': hex(address), 'size': size, 'bytes': guard.hex()}
lines.append('}')
(ROOT / 'native/source/program/generated/timemachine_guards.hpp').write_text('\n'.join(lines) + '\n')
(ROOT / 'evidence/timemachine/guards.json').write_text(json.dumps(report, indent=2) + '\n')
print(f'Time Machine: {len(targets)} exact-build guards generated')

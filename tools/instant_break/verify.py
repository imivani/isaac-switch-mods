"""Run isolated adapter tests and record the exact source that passed."""
import hashlib
import json
import re
import subprocess
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'evidence/instant_break'
sources = ['features/instant_break/adapter.hpp', 'features/instant_break/state.hpp',
           'features/instant_break/native_profile.hpp', 'native/source/program/instant_break.cpp',
           'native/source/program/instant_break.hpp', 'tools/instant_break/test_adapter.cpp',
           'tools/instant_break/run_tests.sh', 'tools/instant_break/generate.py',
           'tools/instant_break/verify.py']
before = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in sources}
linux_path = '/mnt/' + str(ROOT)[0].lower() + str(ROOT)[2:].replace('\\', '/')
command = ['wsl', '-d', 'IsaacDevkit', '--exec', '/bin/sh', linux_path + '/tools/instant_break/run_tests.sh']
result = subprocess.run(command, cwd=ROOT, text=True, capture_output=True)
print(result.stdout, end='')
print(result.stderr, end='')
assert result.returncode == 0, f'test command failed with exit {result.returncode}'
after = {name: hashlib.sha256((ROOT / name).read_bytes()).hexdigest() for name in sources}
assert before == after, 'source changed while tests were running'
match = re.search(r'PASS: (\d+) Instant Break adapter and lifecycle scenarios', result.stdout)
assert match and 'PASS: isolated Switch compiler syntax check' in result.stdout
(OUT / 'tests.log').write_text(result.stdout + result.stderr)
report = {
    'utc': datetime.now(timezone.utc).isoformat(), 'status': 'PASS',
    'source_sha256': before, 'scenario_count': int(match.group(1)),
    'command': command, 'host_validation': 'Actual adapter and state code using bounded simulated game memory and native-call recording; AddressSanitizer and UndefinedBehaviorSanitizer enabled.',
    'switch_compilation': 'Isolated aarch64-none-elf-g++ syntax check of instant_break.cpp with the actual project ABI flags and -Wall -Werror; no shared native target build.',
    'limits': ['Tests execute adapter control flow and simulated callbacks, not the closed-source native routines.',
               'Native routine behavior is supported by exact-build disassembly, not a hardware run of this addition.',
               'Combined module build and Switch gameplay validation are separate integration steps.'],
    'hardware_test': False,
}
(OUT / 'verification.json').write_text(json.dumps(report, indent=2) + '\n')

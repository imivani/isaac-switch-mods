"""Compile and run the Repentance+ Rules tests (run inside IsaacDevkit).

1. Portable rule decisions (features/rplus/tests.cpp).
2. The complete production native/source/program/rplus.cpp with only platform
   boundaries replaced (hooks, the return-address read, memory checks and the
   game calls behind Native<>).
3. An isolated Switch-compiler syntax check of rplus.cpp.
Set ISAAC_HOST_CXX to use another host compiler (clang++ works).
"""
from datetime import datetime, timezone
import hashlib
import json
import os
import re
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]
build = root / 'features/rplus/test-build'
build.mkdir(parents=True, exist_ok=True)
cxx = os.environ.get('ISAAC_HOST_CXX', 'g++')

# Host copy of feature_api.hpp: identical definitions, with the Switch
# memory query replaced by a declaration the test provides.
api = (root / 'native/source/program/feature_api.hpp').read_text()
svc = 'extern "C" {\n#include "lib/nx/kernel/svc.h"\n}\n'
assert api.count(svc) == 1
api = api.replace(svc, '')
readable = re.search(r'inline bool Readable\(std::uintptr_t p, std::size_t size\) \{.*?\n\}\n', api, re.S)
assert readable
api = api.replace(readable.group(0), 'bool Readable(std::uintptr_t p, std::size_t size);\n')
(build / 'feature_api_host.hpp').write_text(api)

native_path = root / 'native/source/program/rplus.cpp'
source = native_path.read_text()
for old, new in [
    ('template<class F> F Native(std::uintptr_t offset) { return reinterpret_cast<F>(s_Module + offset); }',
     'template<class F> F Native(std::uintptr_t offset) { return TestNative<F>(offset); }'),
    ('reinterpret_cast<std::uintptr_t>(__builtin_return_address(0))', 'host_native::ReturnAddress()')]:
    assert source.count(old) == 1, old
    source = source.replace(old, new)
stray = [line for line in source.splitlines() if 's_Module +' in line and '(*)' in line]
assert not stray, f'rplus.cpp calls the game outside Native<>: {stray}'

def include(match):
    name = match.group(1)
    if name in ('lib.hpp', 'mod_settings.hpp'):
        return '// Platform boundary provided by tools/rplus/test_hook.cpp'
    if name in ('feature_api.hpp',):
        return '#include "feature_api_host.hpp"'
    if name == 'rplus.hpp':
        header = (native_path.parent / name).read_text().replace('#include "feature_api.hpp"', '#include "feature_api_host.hpp"')
        (build / 'rplus_host.hpp').write_text(header)
        return '#include "rplus_host.hpp"'
    return '#include "' + str((native_path.parent / name).resolve()) + '"'
source = re.sub(r'#include "([^"\n]+)"', include, source)
(build / 'native_rplus_under_test.cpp').write_text(source)

sanitize = ['-O1', '-g', '-fno-omit-frame-pointer', '-fsanitize=address,undefined', '-fno-sanitize-recover=all']
checks = []
for name, test_source in [('portable', 'features/rplus/tests.cpp'), ('native-hooks', 'tools/rplus/test_hook.cpp')]:
    binary = build / name
    command = [cxx, '-std=c++2b', '-Wall', '-Wextra', '-Werror', *sanitize, '-I' + str(build), test_source, '-o', str(binary)]
    compiled = subprocess.run(command, cwd=root, text=True, capture_output=True, timeout=180)
    ran = subprocess.run([str(binary)], cwd=root, text=True, capture_output=True, timeout=60) if compiled.returncode == 0 else None
    check = {'name': name, 'compile_exit': compiled.returncode, 'compile_output': compiled.stdout + compiled.stderr,
             'test_exit': ran.returncode if ran else None, 'test_output': (ran.stdout + ran.stderr) if ran else ''}
    checks.append(check)
    print(check['compile_output'] + check['test_output'], end='')

compiler = Path('/opt/devkitpro/devkitA64/bin/aarch64-none-elf-g++')
syntax = None
if compiler.exists():
    syntax = subprocess.run([str(compiler), '-std=gnu++23', '-fsyntax-only',
        '-march=armv8-a+crc+crypto', '-mtune=cortex-a57', '-mtp=soft', '-fPIC', '-fvisibility=hidden',
        '-Wall', '-Werror', '-fno-rtti', '-fno-exceptions', '-D__SWITCH__', '-D__RTLD_6XX__',
        '-DEXL_LOAD_KIND=Module', '-DEXL_LOAD_KIND_ENUM=2', '-DEXL_PROGRAM_ID=0x010021C000B6A000',
        '-I/opt/devkitpro/libnx/include', '-Inative/source', '-Inative/include', '-Inative/source/lib', '-Ieid',
        'native/source/program/rplus.cpp'], cwd=root, text=True, capture_output=True, timeout=180)
    print(syntax.stdout + syntax.stderr, end='')
    if syntax.returncode == 0:
        print('PASS: isolated Repentance+ Rules Switch compiler syntax check')
else:
    print('SKIP: devkitA64 not found; Switch compiler syntax check not run')
status = 'PASS' if all(c['compile_exit'] == 0 and c['test_exit'] == 0 for c in checks) and (syntax is None or syntax.returncode == 0) else 'FAIL'
sources = ['features/rplus/policy.hpp', 'features/rplus/tests.cpp', 'native/source/program/rplus.cpp',
           'native/source/program/rplus.hpp', 'native/source/program/feature_api.hpp', 'native/source/program/mod_settings.hpp',
           'native/source/program/generated/rplus_guards.hpp', 'tools/rplus/generate_guards.py',
           'tools/rplus/test_hook.cpp', 'tools/rplus/run_tests.py']
report = {'timestamp_utc': datetime.now(timezone.utc).isoformat(), 'result': status, 'hardware_test': False,
          'scope': 'portable Repentance+ rule decisions; production rplus.cpp hooks with platform boundaries replaced; Switch syntax when devkitA64 is present',
          'checks': checks, 'switch_syntax_exit': syntax.returncode if syntax else None,
          'switch_syntax_output': (syntax.stdout + syntax.stderr) if syntax else 'not run',
          'source_sha256': {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in sources}}
(root / 'features/rplus/test-results.json').write_text(json.dumps(report, indent=2) + '\n')
raise SystemExit(0 if status == 'PASS' else 1)

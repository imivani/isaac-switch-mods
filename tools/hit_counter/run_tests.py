"""Compile and run the Hit Counter / Greed machine tests (run inside IsaacDevkit).

1. Portable counter, placement and settings-page logic (features/hit_counter/tests.cpp).
2. The complete production native/source/program/hit_counter.cpp with only
   platform boundaries replaced (hooks, memory checks, three native font calls).
3. An isolated Switch-compiler syntax check of hit_counter.cpp.
"""
from datetime import datetime, timezone
import hashlib
import json
import re
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]
build = root / 'features/hit_counter/test-build'
build.mkdir(parents=True, exist_ok=True)

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

native_path = root / 'native/source/program/hit_counter.cpp'
source = native_path.read_text()
for old, new in [
    ('reinterpret_cast<StringWidth>(s_Module+api::FontStringWidth)(text.font,value)', 'host_native::Width(text.font,value)'),
    ('reinterpret_cast<LineHeight>(s_Module+api::FontLineHeight)(text.font)', 'host_native::Line(text.font)'),
    ('reinterpret_cast<DrawScaled>(s_Module+profile::DrawStringScaled)(text.font,value,x,y,text.scale,text.scale,&color,0,false)',
     'host_native::DrawText(text.font,value,x,y,text.scale,&color)')]:
    assert source.count(old) == 1, old
    source = source.replace(old, new)

def include(match):
    name = match.group(1)
    if name in ('lib.hpp', 'eid_ui.hpp', 'mod_settings.hpp', 'mod_menu.hpp'):
        return '// Platform boundary provided by tools/hit_counter/test_hook.cpp'
    if name == 'hit_counter.hpp':
        header = (native_path.parent / name).read_text().replace('#include "feature_api.hpp"', '#include "feature_api_host.hpp"')
        (build / 'hit_counter_host.hpp').write_text(header)
        return '#include "hit_counter_host.hpp"'
    return '#include "' + str((native_path.parent / name).resolve()) + '"'
source = re.sub(r'#include "([^"\n]+)"', include, source)
(build / 'native_hit_counter_under_test.cpp').write_text(source)

checks = []
for name, test_source in [('portable', 'features/hit_counter/tests.cpp'), ('native-hooks', 'tools/hit_counter/test_hook.cpp')]:
    binary = build / name
    command = ['g++', '-std=c++23', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-fno-omit-frame-pointer', '-no-pie',
               '-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-I' + str(build), test_source, '-o', str(binary)]
    compiled = subprocess.run(command, cwd=root, text=True, capture_output=True, timeout=180)
    ran = subprocess.run([str(binary)], cwd=root, text=True, capture_output=True, timeout=60) if compiled.returncode == 0 else None
    check = {'name': name, 'compile_exit': compiled.returncode, 'compile_output': compiled.stdout + compiled.stderr,
             'test_exit': ran.returncode if ran else None, 'test_output': (ran.stdout + ran.stderr) if ran else ''}
    checks.append(check)
    print(check['compile_output'] + check['test_output'], end='')

syntax_command = ['/opt/devkitpro/devkitA64/bin/aarch64-none-elf-g++', '-std=gnu++23', '-fsyntax-only',
    '-march=armv8-a+crc+crypto', '-mtune=cortex-a57', '-mtp=soft', '-fPIC', '-fvisibility=hidden',
    '-Wall', '-Werror', '-fno-rtti', '-fno-exceptions', '-D__SWITCH__', '-D__RTLD_6XX__',
    '-DEXL_LOAD_KIND=Module', '-DEXL_LOAD_KIND_ENUM=2', '-DEXL_PROGRAM_ID=0x010021C000B6A000',
    '-I/opt/devkitpro/libnx/include', '-Inative/source', '-Inative/include', '-Inative/source/lib', '-Ieid',
    'native/source/program/hit_counter.cpp']
syntax = subprocess.run(syntax_command, cwd=root, text=True, capture_output=True, timeout=180)
print(syntax.stdout + syntax.stderr, end='')
if syntax.returncode == 0:
    print('PASS: isolated Hit Counter Switch compiler syntax check')
status = 'PASS' if all(c['compile_exit'] == 0 and c['test_exit'] == 0 for c in checks) and syntax.returncode == 0 else 'FAIL'
sources = ['features/hit_counter/counter.hpp', 'features/hit_counter/tests.cpp', 'native/source/program/mod_menu.hpp',
           'native/source/program/hit_counter.cpp', 'native/source/program/hit_counter.hpp',
           'native/source/program/feature_api.hpp', 'native/source/program/mod_settings.hpp',
           'native/source/program/generated/iteration5_guards.hpp', 'tools/iteration5/generate_guards.py',
           'tools/hit_counter/test_hook.cpp', 'tools/hit_counter/run_tests.py']
report = {'timestamp_utc': datetime.now(timezone.utc).isoformat(), 'result': status, 'hardware_test': False,
          'scope': 'portable hit/run/floor/placement logic; menu-driven layout; production hit_counter.cpp hooks with platform boundaries replaced; Switch syntax',
          'checks': checks, 'switch_syntax_exit': syntax.returncode, 'switch_syntax_output': syntax.stdout + syntax.stderr,
          'source_sha256': {name: hashlib.sha256((root / name).read_bytes()).hexdigest() for name in sources}}
(root / 'features/hit_counter/test-results.json').write_text(json.dumps(report, indent=2) + '\n')
raise SystemExit(0 if status == 'PASS' else 1)

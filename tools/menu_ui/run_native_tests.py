"""Native tests for the paper mod menu (run inside IsaacDevkit).

Compiles the complete production debug_kit.cpp, runtime_extras.cpp, mod_settings.cpp
and mod_menu.cpp against a fake console (tools/menu_ui/test_native_menu.cpp and
native_menu_boundary.cpp). Only native boundaries change:
  * debug_kit.cpp: its four game calls (clock, device check, pause check, buttons);
  * mod_menu.cpp: the bodies of Native<F>() and Fs<F>(), which every game call
    and every file-system call goes through. The fakes must match the exact
    function types the production code uses.
Writes evidence/menu/native-tests.json (and native-draws.json from the run).
"""
from datetime import datetime, timezone
import hashlib
import json
import re
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]
build = root / 'evidence/menu/native-test-build'
build.mkdir(parents=True, exist_ok=True)
program = root / 'native/source/program'

SUBSTITUTIONS = {
    'debug_kit.cpp': [
        ('reinterpret_cast<std::uint64_t (*)()>(s_Module+api::Milliseconds)()', 'host->now'),
        ('reinterpret_cast<bool (*)(void*,unsigned)>(s_Module+menuApi::DoesDeviceExist)', 'HostDeviceExists'),
        ('reinterpret_cast<bool (*)(void*)>(s_Module+api::IsPaused)(reinterpret_cast<void*>(game))', '(game==host->game.Address() && host->paused)'),
        ('reinterpret_cast<bool (*)(void*,unsigned,unsigned,unsigned*)>(s_Module+api::IsButtonPressed)', '&HostPressed'),
    ],
    'mod_menu.cpp': [
        ('template<class F> F Native(std::uintptr_t offset) { return reinterpret_cast<F>(s_Module + offset); }',
         'template<class F> F Native(std::uintptr_t offset) { return TestNative<F>(offset); }'),
        ('template<class F> F Fs(std::uintptr_t stub) { return s_FsReady ? reinterpret_cast<F>(s_Module + stub) : nullptr; }',
         'template<class F> F Fs(std::uintptr_t stub) { return s_FsReady ? TestFs<F>(stub) : nullptr; }'),
    ],
    'runtime_extras.cpp': [],
    'mod_settings.cpp': [],
}
generated = {}
substitutions = []
for name, replacements in SUBSTITUTIONS.items():
    path = program / name
    source = path.read_text()
    for old, new in replacements:
        assert source.count(old) == 1, f'{name}: expected exactly one native boundary: {old}'
        source = source.replace(old, new)
        substitutions.append({'source': f'native/source/program/{name}', 'original': old, 'replacement': new})
    if name == 'mod_menu.cpp':
        # Nothing else in the backend may reach the game except through Native<>/Fs<>.
        stray = [line for line in source.splitlines() if 's_Module +' in line and 'reinterpret_cast<' in line and '(*)' in line]
        assert not stray, f'mod_menu.cpp calls the game outside Native<>: {stray}'

    def include(match):
        header = match.group(1)
        if header == 'lib.hpp':
            return '// Platform hook API supplied by tools/menu_ui/native_test_host.hpp'
        if header.startswith('lib/'):
            return match.group(0)  # test stub from eid/test_support
        return '#include "' + str((path.parent / header).resolve()) + '"'
    source = re.sub(r'#include "([^"\n]+)"', include, source)
    output = build / (path.stem + '_under_test.cpp')
    output.write_text(source)
    generated[str(output.relative_to(root))] = hashlib.sha256(output.read_bytes()).hexdigest()

sources = ['native/source/program/' + n for n in ('debug_kit.cpp', 'debug_kit.hpp', 'mod_menu.cpp', 'mod_menu.hpp',
           'runtime_extras.cpp', 'runtime_extras.hpp', 'mod_settings.cpp', 'mod_settings.hpp', 'hit_counter.hpp',
           'eid_ui.hpp', 'feature_api.hpp', 'native_sprite.hpp', 'generated/menu_guards.hpp',
           'generated/debug_kit_guards.hpp', 'generated/iteration5_guards.hpp', 'generated/build_profile.hpp')]
sources += ['features/menu_ui/' + n for n in ('menu.hpp', 'host.hpp', 'prefs.hpp', 'paint.hpp', 'anim.hpp',
            'generated/catalogue.hpp', 'generated/art.hpp')]
sources += ['features/item_menu/adapter.hpp', 'features/debug_kit/policy.hpp', 'features/clean_map/generated/native.hpp',
            'eid/generated/descriptions.cpp', 'tools/menu_ui/native_test_host.hpp', 'tools/menu_ui/native_menu_boundary.cpp',
            'tools/menu_ui/test_native_menu.cpp', 'tools/menu_ui/run_native_tests.py']
before = {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in sources}

checks = []
for mode, flags in [('optimized', ['-O2']),
                    ('asan-ubsan', ['-O1', '-g', '-fno-omit-frame-pointer', '-no-pie', '-fsanitize=address,undefined',
                                    '-fno-sanitize-recover=all'])]:
    binary = build / ('native-menu-' + mode)
    command = ['g++', '-std=c++23', '-Wall', '-Wextra', '-Werror', *flags,
               '-I' + str(build), '-I' + str(root / 'eid/test_support'), '-I' + str(root / 'eid'),
               'tools/menu_ui/test_native_menu.cpp', 'tools/menu_ui/native_menu_boundary.cpp',
               'eid/generated/descriptions.cpp', '-o', str(binary)]
    compiled = subprocess.run(command, cwd=root, text=True, capture_output=True, timeout=600)
    ran = subprocess.run([str(binary)], cwd=root, text=True, capture_output=True, timeout=120) if compiled.returncode == 0 else None
    check = {'name': mode, 'compile_exit': compiled.returncode, 'compile_output': (compiled.stdout + compiled.stderr)[-20000:],
             'test_exit': ran.returncode if ran else None, 'test_output': (ran.stdout + ran.stderr)[-20000:] if ran else ''}
    checks.append(check)
    print(check['compile_output'] + check['test_output'], end='', flush=True)

status = 'PASS' if all(c['compile_exit'] == 0 and c['test_exit'] == 0 for c in checks) else 'FAIL'
changed = [p for p in sources if hashlib.sha256((root / p).read_bytes()).hexdigest() != before[p]]
if changed:
    print('FAIL: source files changed during test: ' + ', '.join(changed))
    status = 'FAIL'
report = {
    'timestamp_utc': datetime.now(timezone.utc).isoformat(), 'result': status, 'hardware_test': False,
    'scope': 'Complete production debug_kit.cpp, runtime_extras.cpp, mod_settings.cpp and mod_menu.cpp on a fake console.',
    'checks': checks, 'boundary_substitutions': substitutions, 'generated_source_sha256': generated,
    'source_sha256': before, 'changed_during_tests': changed,
    'coverage': ['shortcut hold (2 s default, 0.5 s, off) opens the menu without spawning',
                 'pedestal / give / shop for collectibles, every other category, blocked story items, no-space and failures',
                 'every tool and its exact native call', 'run page: seed, stats, item history, damaged history',
                 'settings saved to the save mount after closing, committed; nothing written when unchanged; failed write retried',
                 'settings loaded once after the save mount appears; damaged file ignored; layout pushed to features',
                 'pause screen Y entry, prompt, release gate, options page excluded, pause guards optional',
                 'title Mods screen X entry, owner controller, usable tabs, release gate, screen change',
                 'run/room/player/controller changes close the menu', 'every native guard blocks installation',
                 'icon loading budget and sprite release on unload', 'native function signatures match the fakes'],
    'limitations': ['Game functions, hook installation, sprites and EID drawing helpers are fakes; real rendering, '
                    'sounds, saving to the console and controller feel need console testing.'],
}
draws = root / 'evidence/menu/native-draws.json'
if draws.exists():
    report['native_draws'] = {'path': str(draws.relative_to(root)), 'sha256': hashlib.sha256(draws.read_bytes()).hexdigest()}
(root / 'evidence/menu/native-tests.json').write_text(json.dumps(report, indent=2) + '\n')
raise SystemExit(0 if status == 'PASS' else 1)

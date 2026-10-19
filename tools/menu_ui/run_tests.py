"""Menu model tests and preview draw lists (run inside IsaacDevkit).

1. features/menu_ui/tests.cpp: the portable menu (tabs, items, filters, popup,
   favourites, mods, settings pages, hit counter move, tools, run page,
   animation, saved-settings format) against a fake host, -O2 and ASan/UBSan.
2. tools/menu_ui/preview_dump.cpp: the same menu's draw lists for the preview
   scenes -> evidence/menu/preview/paints.json. Render them with
   `python tools/menu_ui/preview.py` (needs Pillow and the game dump).
Writes features/menu_ui/test-results.json.
"""
from datetime import datetime, timezone
import hashlib
import json
import subprocess
from pathlib import Path

root = Path(__file__).resolve().parents[2]
build = root / 'features/menu_ui/test-build'
build.mkdir(parents=True, exist_ok=True)
(root / 'evidence/menu/preview').mkdir(parents=True, exist_ok=True)
sources = ['features/menu_ui/' + n for n in ('menu.hpp', 'host.hpp', 'prefs.hpp', 'paint.hpp', 'anim.hpp', 'tests.cpp',
           'generated/catalogue.hpp', 'generated/art.hpp')]
sources += ['tools/menu_ui/preview_dump.cpp', 'tools/menu_ui/run_tests.py', 'tools/menu_ui/generate_catalogue.py',
            'tools/menu_ui/generate_assets.py', 'assets/menu_ui/resources/gfx/codex_menu/menu.anm2']
before = {p: hashlib.sha256((root / p).read_bytes()).hexdigest() for p in sources}

runs = []
jobs = [('model-optimized', ['-O2'], 'features/menu_ui/tests.cpp', []),
        ('model-asan-ubsan', ['-O1', '-g', '-fno-omit-frame-pointer', '-no-pie', '-fsanitize=address,undefined',
                              '-fno-sanitize-recover=all'], 'features/menu_ui/tests.cpp', []),
        ('preview-dump', ['-O1'], 'tools/menu_ui/preview_dump.cpp', ['evidence/menu/preview/paints.json'])]
for name, flags, source, args in jobs:
    binary = build / name
    command = ['g++', '-std=c++23', '-Wall', '-Wextra', '-Werror', *flags, '-Ifeatures/menu_ui', source, '-o', str(binary)]
    compiled = subprocess.run(command, cwd=root, text=True, capture_output=True, timeout=600)
    ran = subprocess.run([str(binary), *args], cwd=root, text=True, capture_output=True, timeout=120) if compiled.returncode == 0 else None
    run = {'name': name, 'compile_exit': compiled.returncode, 'compile_output': (compiled.stdout + compiled.stderr)[-20000:],
           'exit': ran.returncode if ran else None, 'output': (ran.stdout + ran.stderr)[-20000:] if ran else ''}
    runs.append(run)
    print(run['compile_output'] + run['output'], end='', flush=True)

status = 'PASS' if all(r['compile_exit'] == 0 and r['exit'] == 0 for r in runs) else 'FAIL'
changed = [p for p in sources if hashlib.sha256((root / p).read_bytes()).hexdigest() != before[p]]
if changed:
    print('FAIL: source files changed during test: ' + ', '.join(changed))
    status = 'FAIL'
paints = root / 'evidence/menu/preview/paints.json'
report = {'timestamp_utc': datetime.now(timezone.utc).isoformat(), 'result': status, 'hardware_test': False,
          'scope': 'Portable menu model with a fake host; preview draw lists from the same code.',
          'runs': runs, 'source_sha256': before, 'changed_during_tests': changed,
          'preview_paints_sha256': hashlib.sha256(paints.read_bytes()).hexdigest() if paints.exists() else None}
(root / 'features/menu_ui/test-results.json').write_text(json.dumps(report, indent=2) + '\n')
raise SystemExit(0 if status == 'PASS' else 1)

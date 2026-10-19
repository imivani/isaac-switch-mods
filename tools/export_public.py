"""Copy the shareable parts of this project into a separate folder for GitHub.

Never exported: the game dump, the PC Repentance+ extraction (reference/),
decompiled game code and other evidence (evidence/), deployment records and
SD-card backups (deployments/), build output (dist/, native/build ...), test
reports with local paths, and the author's MTP installers. Personal paths in
text files become placeholders; the export fails if any remain.

Usage: python tools/export_public.py <destination folder>
"""
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
INCLUDE = ['native', 'features', 'eid', 'eid_ui', 'eid_special', 'assets', 'tools', 'docs']
SKIP_DIRS = {'build', 'deploy', 'test-build', 'native-test-build', '__pycache__', 'switch-ui', 'plans',
             'legacy_descriptions', '.pytest_cache', '.build'}
SKIP_FILES = {'native/native.elf', 'native/native.nso', 'docs/build-environment.md',
              'tools/create_iteration3_baseline.py'}
SKIP_PATTERNS = [re.compile(p) for p in (
    r'(^|/)[^/]*results?[^/]*\.json$',          # test reports (local paths, timings)
    r'(^|/)(asset-build|asset-validation|data-build|native-layout)\.json$',
    r'^tools/install-[^/]*\.ps1$', r'^tools/tests/install-[^/]*\.ps1$',  # the author's own SD-card installers
    r'^tools/verify_[^/]*\.py$',                 # check against the author's deployment records
    r'^assets/specialist/validation\.json$',
    r'\.pyc$',
)]
TEXT = {'.md', '.txt', '.py', '.ps1', '.cpp', '.hpp', '.h', '.c', '.json', '.mk', '.sh', '.java', '.tsv', '.xml',
        '.anm2', '.fnt', '.lua', '.s', '.S', '.inc', '.ld', '.specs', ''}
# The exporting user's own name, taken from their home folder (never written into this file).
USER = re.escape(Path.home().name)
PERSONAL = re.compile(r'(?i)(?:[a-z]:[\\/]+users[\\/]+' + USER + r'|/mnt/[a-z]/users/' + USER + r'|/[a-z]/users/' + USER + r')\b')
FORBIDDEN = re.compile(r'(?i)users[\\/]+' + USER + r'\b|privaterelay\.appleid')
PUBLIC = ROOT / 'tools/public'


def skipped(relative):
    parts = relative.split('/')
    if any(p in SKIP_DIRS for p in parts[:-1]) or relative in SKIP_FILES:
        return True
    if relative.startswith('tools/public/'):
        return True  # these become the repository's top-level files
    return any(p.search(relative) for p in SKIP_PATTERNS)


def copy_file(source, target):
    target.parent.mkdir(parents=True, exist_ok=True)
    if source.suffix in TEXT:
        try:
            text = source.read_text(encoding='utf-8')
        except UnicodeDecodeError:
            shutil.copyfile(source, target)
            return
        text = PERSONAL.sub('%USERPROFILE%', text)
        target.write_text(text, encoding='utf-8', newline='')
    else:
        shutil.copyfile(source, target)


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    out = Path(sys.argv[1]).resolve()
    assert not out.is_relative_to(ROOT) and not ROOT.is_relative_to(out), 'Export outside the project folder'
    if out.exists():
        # Only ever refresh a previous export: keep its .git, replace everything else.
        assert (out / '.isaac-public-export').exists(), f'{out} exists and is not a previous export'
        for child in out.iterdir():
            if child.name != '.git':
                shutil.rmtree(child) if child.is_dir() else child.unlink()
    out.mkdir(parents=True, exist_ok=True)
    count = 0
    for top in INCLUDE:
        for source in sorted((ROOT / top).rglob('*')):
            if not source.is_file():
                continue
            relative = source.relative_to(ROOT).as_posix()
            if skipped(relative):
                continue
            copy_file(source, out / relative)
            count += 1
    # Top-level files for players and the license.
    for name, target in (('README.md', 'README.md'), ('INSTALL.md', 'INSTALL.md'),
                         ('THIRD_PARTY_NOTICES.md', 'THIRD_PARTY_NOTICES.md'), ('building.md', 'docs/building.md')):
        copy_file(PUBLIC / name, out / target)
    shutil.copyfile(ROOT / 'features/planetarium/LICENSE', out / 'LICENSE')  # GPL-3.0 text
    copy_file(ROOT / 'README.md', out / 'docs/project-history.md')
    (out / 'docs/images').mkdir(parents=True, exist_ok=True)
    for image in ('items.png', 'mods.png', 'run.png'):
        shutil.copyfile(ROOT / 'evidence/menu/preview' / image, out / 'docs/images' / image)
    (out / '.gitignore').write_text('\n'.join([
        'dist/', 'native/build/', 'native/deploy/', 'native/native.elf', 'native/native.nso',
        'test-build/', 'native-test-build/', '__pycache__/', '*.pyc', 'evidence/', 'reference/',
        'deployments/', '../Isaac-dump/', ''
    ]), encoding='utf-8')
    (out / '.isaac-public-export').write_text('Created by tools/export_public.py\n', encoding='utf-8')

    # Final scan: nothing personal or private may remain.
    problems, large = [], []
    for path in out.rglob('*'):
        if not path.is_file() or '.git' in path.relative_to(out).parts:
            continue
        if path.stat().st_size > 50 * 1024 * 1024:
            large.append(path)
        if path.suffix in TEXT:
            try:
                if FORBIDDEN.search(path.read_text(encoding='utf-8')):
                    problems.append(path)
            except UnicodeDecodeError:
                pass
    assert not problems, f'Personal data left in: {problems}'
    assert not large, f'Files too large for GitHub: {large}'
    for forbidden in ('evidence', 'reference', 'deployments', 'dist'):
        assert not (out / forbidden).exists(), forbidden
    print(f'Exported {count} project files to {out}')


if __name__ == '__main__':
    main()

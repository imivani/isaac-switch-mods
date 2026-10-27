"""Make the two release downloads from a verified build (run after tools/build.ps1).

    python tools/make_release.py --version 1.1.0

dist/<mode>/atmosphere becomes:
- IsaacSwitchMods-v<version>.zip: INSTALL.md plus the atmosphere folder,
  without the four Repentance+ balance data files;
- IsaacSwitchMods-v<version>-balance-addon.zip: those four files plus a README.
Both go to dist/release, with fixed timestamps so the same build gives the same zips.
"""
import argparse
import hashlib
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
TITLE = 'atmosphere/contents/010021C000B6A000'
BALANCE = [f'{TITLE}/romfs/rp_patch/resources/{name}'
           for name in ('itempools.xml', 'items.xml', 'items_metadata.xml', 'pocketitems.xml')]
STAMP = (2026, 1, 1, 0, 0, 0)
ADDON_README = """Isaac Switch Mods - optional Repentance+ balance add-on

Copy this atmosphere folder to the root of your SD card (merge), on top of the
main IsaacSwitchMods download. Finish your current run first and start a new
run afterwards.

It adds four modified game data files with the item buffs and nerfs of the PC
Repentance+ update. To remove it, delete these four files from
atmosphere/contents/010021C000B6A000/romfs/rp_patch/resources:
items.xml, items_metadata.xml, itempools.xml, pocketitems.xml
"""


def add(archive, name, data):
    info = zipfile.ZipInfo(name, STAMP)
    info.compress_type = zipfile.ZIP_DEFLATED
    info.external_attr = 0o644 << 16
    archive.writestr(info, data)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--version', required=True)
    parser.add_argument('--mode', default='combined-menu')
    args = parser.parse_args()
    package = ROOT / 'dist' / args.mode
    required = [f'{TITLE}/exefs/subsdk9', f'{TITLE}/exefs/main.npdm', *BALANCE]
    missing = [name for name in required if not (package / name).is_file()]
    if missing or not (package / 'build-check.json').is_file():
        raise SystemExit(f'Run tools/build.ps1 -Mode {args.mode} first; missing: {missing or ["build-check.json"]}')
    files = sorted(p.relative_to(package).as_posix() for p in (package / 'atmosphere').rglob('*') if p.is_file())
    out = ROOT / 'dist/release'
    out.mkdir(parents=True, exist_ok=True)
    main_zip = out / f'IsaacSwitchMods-v{args.version}.zip'
    addon_zip = out / f'IsaacSwitchMods-v{args.version}-balance-addon.zip'
    with zipfile.ZipFile(main_zip, 'w') as archive:
        add(archive, 'INSTALL.md', (ROOT / 'INSTALL.md').read_bytes())
        for name in files:
            if name not in BALANCE:
                add(archive, name, (package / name).read_bytes())
    with zipfile.ZipFile(addon_zip, 'w') as archive:
        add(archive, 'README.txt', ADDON_README)
        for name in BALANCE:
            add(archive, name, (package / name).read_bytes())
    for path in (main_zip, addon_zip):
        print(f'{path.relative_to(ROOT)}  {path.stat().st_size} bytes  sha256 {hashlib.sha256(path.read_bytes()).hexdigest()}')
    print(f'{len(files) - len(BALANCE)} files in the main download, {len(BALANCE)} in the add-on.')


if __name__ == '__main__':
    main()

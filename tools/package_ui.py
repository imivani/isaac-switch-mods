"""Stage the original mod artwork in both resource layers used by this build."""
import hashlib
import argparse
import json
from pathlib import Path
import shutil

from convert_ui_textures import stage

ROOT = Path(__file__).resolve().parents[1]
TITLE = "atmosphere/contents/010021C000B6A000/romfs"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('combined-polished', 'combined-expanded', 'combined-extended', 'combined-refined', 'combined-plus', 'combined-balance', 'combined-menu'), default='combined-polished')
    args = parser.parse_args()
    package = ROOT / 'dist' / args.mode
    reports = []
    logical = {}
    groups = [("eid", ROOT / "eid_ui/resources"), ("boss", ROOT / "assets/boss_ui/resources")]
    if args.mode in ('combined-extended','combined-refined','combined-plus','combined-balance','combined-menu'):
        groups += [("item_visuals",ROOT / "assets/item_visuals/resources"),
                   ("specialist",ROOT / "assets/specialist/resources")]
    if args.mode == 'combined-menu':
        # Only gfx/codex_menu/menu.anm2: crops of the game's own menu textures.
        groups += [("menu", ROOT / "assets/menu_ui/resources")]
    for name, source in groups:
        destination = ROOT / f"assets/switch-ui/{name}"
        records = stage(source, destination, extra_copy_extensions=(".xml", ".ogg") if name == 'specialist' else ())
        for record in records:
            relative = record["output"]
            # Refined builds suppress stock drawing at its exact HUD call, so
            # turning enhanced bars off can show the original game artwork.
            if args.mode in ('combined-refined','combined-plus','combined-balance','combined-menu') and relative in (
                'gfx/ui/ui_bosshealthbar.pcx','gfx/ui/ui_bosshealthbar_mini.pcx','gfx/ui/ui_bosshealthbar_static.pcx'):
                continue
            valid_prefix = relative.startswith("gfx/") or (name == 'specialist' and relative.startswith("codex_specialist/"))
            if not valid_prefix or ".." in Path(relative).parts or Path(relative).is_absolute():
                raise ValueError(f"Unexpected UI resource path: {relative}")
            if relative.lower() in logical:
                raise ValueError(f"Resource collision: {relative}")
            logical[relative.lower()] = (destination / relative, record)
        reports.append({"group": name, "files": records})

    packaged = []
    for source, record in logical.values():
        for layer in ("resources", "rp_patch/resources"):
            relative = f"{TITLE}/{layer}/{record['output']}"
            target = package / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            digest = hashlib.sha256(target.read_bytes()).hexdigest()
            if digest != record["sha256"]:
                raise ValueError(f"Package resource changed: {relative}")
            packaged.append({"path": relative, "bytes": target.stat().st_size, "sha256": digest})
    if args.mode in ('combined-refined','combined-plus','combined-balance','combined-menu'):
        for layer in ('resources','rp_patch/resources'):
            for name in ('ui_bosshealthbar.pcx','ui_bosshealthbar_mini.pcx','ui_bosshealthbar_static.pcx'):
                stale=package / TITLE / layer / 'gfx/ui' / name
                # Only these known generated overrides inside this package.
                if stale.exists():
                    assert stale.resolve().is_relative_to(package.resolve())
                    stale.unlink()
    data_files = []
    if args.mode in ('combined-balance', 'combined-menu'):
        # Repentance+ data-only balance: only the Repentance layer holds these files.
        changes = json.loads((ROOT / 'features/balance/changes.json').read_text())
        for name, digest in sorted(changes['sha256'].items()):
            source = ROOT / 'features/balance/resources' / name
            relative = f"{TITLE}/rp_patch/resources/{name}"
            target = package / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, target)
            if hashlib.sha256(target.read_bytes()).hexdigest() != digest:
                raise ValueError(f"Balance data changed after generation: {name}")
            data_files.append({"path": relative, "bytes": target.stat().st_size, "sha256": digest})
    actual = {path.relative_to(package).as_posix() for path in (package / TITLE).rglob("*") if path.is_file()}
    expected = {record["path"] for record in packaged}
    expected |= {record["path"] for record in data_files}
    if actual != expected:
        raise ValueError(f"Untracked resources in package: {sorted(actual ^ expected)}")
    report = {"mode": args.mode, "result": "PASS", "texture_format": "Four-plane RGBA PCX, straight alpha flag 0",
              "source_groups": reports, "files": packaged,
              "original_artwork": ["wofsauge/External-Item-Descriptions", "wofsauge/Enhanced-Boss-Bars"],
              "vanilla_boss_bar_overrides": ["ui_bosshealthbar.pcx", "ui_bosshealthbar_mini.pcx", "ui_bosshealthbar_static.pcx"],
              "gideon_stone_bar": "PRESERVED", "console_test": "NOT_RUN", "data_files": data_files}
    if args.mode in ('combined-extended','combined-refined','combined-plus','combined-balance','combined-menu'):
        baseline=ROOT/'deployments/20261014T200957Z-iteration3-baseline/installed-package'
        old=json.loads((baseline/'ui-assets.json').read_text())
        current={r['path']:r['sha256'] for r in packaged}
        for record in old['files']:
            if args.mode in ('combined-refined','combined-plus','combined-balance','combined-menu') and record['path'].endswith(('/gfx/ui/ui_bosshealthbar.pcx','/gfx/ui/ui_bosshealthbar_mini.pcx','/gfx/ui/ui_bosshealthbar_static.pcx')):
                assert record['path'] not in current
                continue
            assert current.get(record['path'])==record['sha256'], f"Existing resource changed: {record['path']}"
        report['preserved_baseline_resources']=len(old['files'])-(6 if args.mode in ('combined-refined','combined-plus','combined-balance','combined-menu') else 0)
        if args.mode in ('combined-refined','combined-plus','combined-balance','combined-menu'):
            report['vanilla_boss_bar_overrides']=[]
            report['stock_bar_control']='Exact HUD draw guard with restoration; original textures visible when enhanced mode is off' 
        report['original_artwork'] += ['Steam Workshop 2570913695 Animated Items','Steam Workshop 2690434875 Unique Birthright Sprites','Steam Workshop 2575911103 The Specialist for Good Items']
    (package / "ui-assets.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps({"resource_files": len(packaged), "unique_assets": len(logical), "result": "PASS"}))


if __name__ == "__main__":
    main()

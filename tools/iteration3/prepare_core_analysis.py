"""Prepare a focused read-only Ghidra pass for debug inputs and Specialist."""
import json
from pathlib import Path
import shutil

root = Path(__file__).resolve().parents[2]
out = root / 'evidence/iteration3/native-core'
out.mkdir(parents=True, exist_ok=True)
offsets = {0x4f9504, 0x4f90a0, 0x4fc600, 0x3f9b6c, 0x3540d0, 0x455bfc,
           0x3b9448, 0x291bbc, 0x290c38, 0x2f2170, 0x425218, 0x427238,
           0x2da820, 0x2f6ca8}
symbols = json.loads((root / 'evidence/native-analysis/symbols.json').read_text())
targets = {}
for symbol in symbols:
    if symbol['defined'] and (symbol['offset'] in offsets or 'GetNullItemE' in symbol['name'] or 'Room9PlayMusicE' in symbol['name']):
        targets[symbol['offset']] = symbol
assert offsets.issubset(targets)
(out / 'targets.tsv').write_text('\n'.join(f"{s['offset']:x}\t{s['size']}\t{s['name']}" for s in targets.values()))
shutil.copyfile(root / 'evidence/native-analysis/plt.tsv', out / 'plt.tsv')
project = root / 'evidence/iteration3-analysis-project'
if not project.exists():
    shutil.copytree(root / 'evidence/timemachine-analysis-project', project)
print(json.dumps({'project': str(project), 'output': str(out), 'targets': len(targets)}))

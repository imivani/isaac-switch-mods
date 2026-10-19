"""Checks for the Repentance+ data-only balance overrides (Windows Python).

1. The overrides regenerate byte-identically from the untouched Switch files.
2. Every changed field equals the PC reference; every other field equals Switch.
3. Every tag written is one the Switch 1.7.9b tag parser (0x3b7650) knows.
4. EID's compiled metadata, its two edited texts and Specialist's quality-four
   list are derived from the same effective data.
Writes features/balance/test-results.json.
"""
from datetime import datetime, timezone
import hashlib
import json
import re
import shutil
import sys
import tempfile
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools/balance'))
import generate_overrides as gen  # noqa: E402

checks = []


def check(name, condition, detail=''):
    checks.append({'name': name, 'result': 'PASS' if condition else 'FAIL', 'detail': detail})
    if not condition:
        print('FAIL', name, detail)


def digest(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def records(path):
    return {(el.tag, el.get('id')): dict(el.attrib) for el in ET.parse(path).getroot() if el.get('id') is not None}


def main():
    balance = ROOT / 'features/balance/resources'
    committed = {name: digest(balance / name) for name in gen.FILES}
    committed_changes = (ROOT / 'features/balance/changes.json').read_text()
    # 1. Reproducible: regenerate into a temporary folder.
    with tempfile.TemporaryDirectory() as temp:
        original_out = gen.OUT
        changes_path = ROOT / 'features/balance/changes.json'
        saved = changes_path.read_text()
        gen.OUT = Path(temp)
        try:
            gen.main()
            regenerated = {name: digest(Path(temp) / name) for name in gen.FILES}
        finally:
            gen.OUT = original_out
            changes_path.write_text(saved)
    check('regenerates byte-identically', regenerated == committed, json.dumps(regenerated))
    changes = json.loads(committed_changes)
    check('recorded hashes match files', changes['sha256'] == committed)
    for name in gen.FILES:
        raw = (balance / name).read_bytes()
        source = (gen.SWITCH / name).read_bytes()
        check(f'{name}: same line endings as the original', (b'\r\n' in raw) == (b'\r\n' in source))
        check(f'{name}: parses', ET.fromstring(raw) is not None)

    # 2. Field-level comparison against both sources.
    for name in ('items.xml', 'items_metadata.xml', 'pocketitems.xml'):
        s, p, b = records(gen.SWITCH / name), records(gen.PC / name), records(balance / name)
        applied = {(c['tag'], str(c['id']), c['attribute']) for c in changes[{'items.xml': 'items', 'items_metadata.xml': 'metadata', 'pocketitems.xml': 'pocket'}[name]]}
        wrong = []
        for key in s:
            for attr in set(s[key]) | set(b[key]):
                if (key[0], key[1], attr) in applied:
                    want = p[key].get(attr)
                    if attr == 'tags':
                        want = ' '.join(t for t in (want or '').split() if t in gen.KNOWN_TAGS)
                else:
                    want = s[key].get(attr)
                if b[key].get(attr) != want:
                    wrong.append((key, attr, b[key].get(attr), want))
        check(f'{name}: applied fields equal PC, others equal Switch', not wrong, str(wrong[:5]))
    pools = {n: {p.get('Name'): {i.get('Id'): dict(i.attrib) for i in p} for p in ET.parse(path / 'itempools.xml').getroot()}
             for n, path in (('s', gen.SWITCH), ('p', gen.PC), ('b', balance))}
    wrong = []
    for pool in pools['s']:
        for item_id in set(pools['s'][pool]) | set(pools['p'][pool]):
            excluded = (pool, int(item_id)) in gen.POOL_EXCLUDED
            want = pools['s' if excluded else 'p'][pool].get(item_id)
            if pools['b'][pool].get(item_id) != want:
                wrong.append((pool, item_id))
    check('itempools.xml: every pool equals PC except Portable Slot', not wrong, str(wrong[:5]))
    check('Devil pool: no Brother Bobby/Sister Maggy, has Dry Baby',
          '8' not in pools['b']['devil'] and '67' not in pools['b']['devil'] and '265' in pools['b']['devil'])
    check('Portable Slot stays in Shop and Beggar pools', '177' in pools['b']['shop'] and '177' in pools['b']['beggar'])

    # 3. Tags: written tags are a subset of the Switch parser's list.
    meta = records(balance / 'items_metadata.xml')
    written = {t for attrs in meta.values() for t in attrs.get('tags', '').split()}
    check('all written tags are known to the Switch parser', written <= gen.KNOWN_TAGS, str(sorted(written - gen.KNOWN_TAGS)))
    parser = (ROOT / 'evidence/balance/native-tags/3b7650.c').read_text()
    # Replay the libc++ short-string stores (byte 0 = length*2) in source order
    # and decode the key at each map lookup.
    decoded, stack = set(), {}
    for m in re.finditer(r'uStack_(70|6[1-9a-c]) = (0x[0-9a-f]+|\d+);|_find_equal', parser):
        if m.group(0) == '_find_equal':
            length = stack.get(0, 0) // 2
            text = bytes(stack.get(i, 0) for i in range(1, length + 1))
            if length and all(32 < c < 127 for c in text):
                decoded.add(text.decode())
            continue
        value = int(m.group(2), 0)
        if m.group(1) == '70':
            for i, byte in enumerate(value.to_bytes(4, 'little')):
                stack[i] = byte
        else:
            stack[0x70 - int(m.group(1), 16)] = value & 0xff
    nro = (ROOT.parent / 'Isaac-dump/romfs/.nro/Repentance.nro').read_bytes()
    in_rodata = {t for t in gen.KNOWN_TAGS if b'\x00' + t.encode() + b'\x00' in nro}
    check('every known tag is in the native parser or its string table', gen.KNOWN_TAGS <= decoded | in_rodata,
          str(sorted(gen.KNOWN_TAGS - decoded - in_rodata)))
    check('devilsacrifice is absent from the Switch build', 'devilsacrifice' not in decoded and b'devilsacrifice' not in nro)

    # 4. EID and Specialist use the same effective data.
    context = json.loads((ROOT / 'eid/generated/context.json').read_text(encoding='utf-8'))
    items = records(balance / 'items.xml')
    by_id = {int(k[1]): v for k, v in items.items() if k[0] in ('active', 'passive', 'familiar')}
    meta_by_id = {int(k[1]): v for k, v in meta.items() if k[0] == 'item'}
    form_tags = {'guppy': 0, 'fly': 1, 'mushroom': 2, 'angel': 3, 'bob': 4, 'syringe': 5, 'mom': 6, 'baby': 7, 'devil': 8, 'poop': 9, 'book': 10, 'spider': 12}
    mismatched = []
    for row in context['metadata']:
        item, detail = by_id[row['id']], meta_by_id.get(row['id'], {})
        tags = detail.get('tags', '').split()
        want = (int(detail.get('quality', 0)), int(item.get('maxcharges', 0)),
                {'normal': 0, 'timed': 1, 'special': 2}.get(item.get('chargetype', 'normal'), 2),
                sum(1 << i for t, i in form_tags.items() if t in tags))
        if (row['quality'], row['charges'], row['charge_type'], row['forms']) != want:
            mismatched.append(row['id'])
    check('EID metadata equals the balance data for all items', not mismatched, str(mismatched[:10]))
    rows = {r['id']: r for r in context['metadata']}
    check('EID: Void 4 charges, beans timed', rows[477]['charges'] == 4 and rows[294]['charges'] == 60 and
          rows[111]['charge_type'] == rows[421]['charge_type'] == 1 and rows[111]['charges'] == 120)
    check('EID: Lil Delirium Conjoined, Seraphim angel+Conjoined', rows[519]['forms'] == 1 << 7 and rows[390]['forms'] == (1 << 7) | (1 << 3))
    descriptions = json.loads((ROOT / 'eid/generated/descriptions.json').read_text(encoding='utf-8'))
    text = {(e['category'], e['lookup_id']): e['markup'] for e in descriptions['entries']}
    check('EID text: Jupiter heals 2 hearts', 'Heals 2 hearts' in text[('collectibles', 594)] and 'half a heart' not in text[('collectibles', 594)])
    check('EID text: Revelation has no soul hearts', 'Soul Heart' not in text[('collectibles', 643)])
    cpp = (ROOT / 'eid/generated/descriptions.cpp').read_bytes()
    check('compiled descriptions contain the new Jupiter text', b'Heals 2 hearts' in cpp or b'\\110\\145\\141\\154\\163\\0402' in cpp or digest(ROOT / 'eid/generated/descriptions.cpp') == json.loads((ROOT / 'eid/generated/data-build.json').read_text())['files']['descriptions.cpp'])
    assets = (ROOT / 'features/specialist/generated/assets.hpp').read_text()
    quality_four = [int(x) for x in re.search(r'QualityFour\[\] = \{([^}]*)\}', assets).group(1).split(',')]
    check('Specialist quality-four list equals EID quality >= 4', quality_four == [r['id'] for r in context['metadata'] if r['quality'] >= 4])
    check('Specialist: Rock Bottom in, Void/Abyss/Satanic Bible out', 562 in quality_four and not {477, 706, 292} & set(quality_four))
    label = (ROOT / 'eid/context.hpp').read_text()
    check('EID shows timed recharge in seconds', 'Recharges in ' in label and 'charges / 30' in label)

    status = 'PASS' if all(c['result'] == 'PASS' for c in checks) else 'FAIL'
    sources = ['tools/balance/generate_overrides.py', 'tools/balance/run_tests.py', 'tools/balance/compare_reference.py',
               'tools/balance/full_diff.py', 'features/balance/changes.json', 'eid/context.hpp',
               'eid/generated/context.json', 'eid/generated/descriptions.json', 'eid/generated/descriptions.cpp',
               'features/specialist/generated/assets.hpp', 'tools/eid/import_context.py', 'tools/eid/import_eid.py',
               'evidence/balance/native-tags/3b7650.c'] + [f'features/balance/resources/{n}' for n in gen.FILES]
    report = {'timestamp_utc': datetime.now(timezone.utc).isoformat(), 'result': status, 'hardware_test': False,
              'checks': checks, 'counts': {k: len(v) for k, v in changes.items() if isinstance(v, list)},
              'source_sha256': {s: digest(ROOT / s) for s in sources}}
    (ROOT / 'features/balance/test-results.json').write_text(json.dumps(report, indent=2) + '\n')
    print(f"Balance: {sum(c['result'] == 'PASS' for c in checks)}/{len(checks)} checks {status}")
    raise SystemExit(0 if status == 'PASS' else 1)


if __name__ == '__main__':
    main()

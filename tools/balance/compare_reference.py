"""Compare the small Repentance+ shortlist between the Switch dump and the PC reference.

Reads only. Writes evidence/balance/comparison.json. The PC files are the
user's installed v1.9.7.17 data, extracted locally with the game's own
ResourceExtractor (afterbirthp.a is the last-applied layer that holds them).
"""
import csv
import hashlib
import json
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SWITCH = ROOT.parent / 'Isaac-dump/romfs/rp_patch/resources'
PC = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'reference/pc-repentance-plus/resources'
PLAN = ROOT / 'docs/plans/repentance-plus-balance-2026-10-18'


def load(path):
    return ET.parse(path).getroot()


def items(root):
    out = {}
    for el in root:
        if 'id' in el.attrib:
            out[(el.tag, int(el.get('id')))] = el
    return out


def pool(root, name):
    for p in root.findall('Pool'):
        if p.get('Name') == name:
            return {int(i.get('Id')): dict(i.attrib) for i in p.findall('Item')}
    raise KeyError(name)


def main():
    files = {}
    for name in ('items.xml', 'items_metadata.xml', 'itempools.xml'):
        files[name] = {'switch_sha256': hashlib.sha256((SWITCH / name).read_bytes()).hexdigest(),
                       'pc_sha256': hashlib.sha256((PC / name).read_bytes()).hexdigest()}
    s_items, p_items = items(load(SWITCH / 'items.xml')), items(load(PC / 'items.xml'))
    s_meta, p_meta = items(load(SWITCH / 'items_metadata.xml')), items(load(PC / 'items_metadata.xml'))
    s_pools, p_pools = load(SWITCH / 'itempools.xml'), load(PC / 'itempools.xml')

    def item_record(table, item_id):
        for tag in ('passive', 'active', 'familiar'):
            if (tag, item_id) in table:
                return table[(tag, item_id)]
        return None

    quality = []
    with open(PLAN / 'switch-quality-baseline.csv', newline='') as f:
        for row in csv.DictReader(f):
            item_id = int(row['id'])
            s, p = s_meta[('item', item_id)], p_meta.get(('item', item_id))
            assert int(s.get('quality')) == int(row['actualSwitch']), row
            pc_q = int(p.get('quality')) if p is not None else None
            quality.append({'id': item_id, 'name': row['name'], 'switch': int(s.get('quality')),
                            'old_list_target': int(row['wikitarget']), 'pc': pc_q,
                            'apply': pc_q is not None and pc_q != int(s.get('quality')),
                            'matches_old_list': pc_q == int(row['wikitarget'])})
    # Damocles: the hidden familiar counterpart is checked separately.
    damocles_hidden = {'id': 656, 'switch': s_meta[('item', 656)].get('quality'),
                       'pc': p_meta[('item', 656)].get('quality') if ('item', 656) in p_meta else None}

    def attrs(table, item_id, keys):
        el = item_record(table, item_id)
        return {k: el.get(k) for k in keys}

    named = {
        'void_477': {'switch': attrs(s_items, 477, ['maxcharges', 'chargetype']), 'pc': attrs(p_items, 477, ['maxcharges', 'chargetype'])},
        'jupiter_594': {'switch': attrs(s_items, 594, ['hearts', 'maxhearts', 'cache']), 'pc': attrs(p_items, 594, ['hearts', 'maxhearts', 'cache'])},
        'revelation_643': {'switch': attrs(s_items, 643, ['soulhearts', 'cache', 'devilprice']), 'pc': attrs(p_items, 643, ['soulhearts', 'cache', 'devilprice'])},
        'butter_bean_294': {'switch': attrs(s_items, 294, ['maxcharges', 'chargetype']), 'pc': attrs(p_items, 294, ['maxcharges', 'chargetype'])},
        'lil_delirium_519_tags': {'switch': s_meta[('item', 519)].get('tags'), 'pc': p_meta[('item', 519)].get('tags')},
        'seraphim_390_tags': {'switch': s_meta[('item', 390)].get('tags'), 'pc': p_meta[('item', 390)].get('tags')},
    }
    s_devil, p_devil = pool(s_pools, 'devil'), pool(p_pools, 'devil')
    named['devil_pool'] = {
        'switch': {i: s_devil.get(i) for i in (8, 67, 265)},
        'pc': {i: p_devil.get(i) for i in (8, 67, 265)},
        'switch_only_ids': sorted(set(s_devil) - set(p_devil)),
        'pc_only_ids': sorted(set(p_devil) - set(s_devil)),
    }
    report = {'switch_source': str(SWITCH.relative_to(ROOT.parent)), 'pc_reference': 'Steam build 22878971, v1.9.7.17.J460, afterbirthp.a',
              'files': files, 'quality': quality, 'damocles_hidden_656': damocles_hidden, 'named': named,
              'quality_changes_to_apply': sum(1 for q in quality if q['apply']),
              'quality_rows_matching_old_list': sum(1 for q in quality if q['matches_old_list'])}
    out = ROOT / 'evidence/balance/comparison.json'
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k not in ('quality', 'files')}, indent=2))
    for q in quality:
        if not q['matches_old_list'] or not q['apply']:
            print('DIFF', q)


if __name__ == '__main__':
    main()

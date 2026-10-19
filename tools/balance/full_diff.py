"""Full read-only diff of item data between the Switch dump and the PC reference.

Writes evidence/balance/full-diff.json: every differing item attribute,
metadata attribute and pool membership/weight, so each one can be classified
as a data-only change or one that depends on reworked behavior.
"""
import json
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SWITCH = ROOT.parent / 'Isaac-dump/romfs/rp_patch/resources'
PC = ROOT / 'reference/pc-repentance-plus/resources'


def records(path):
    return {(el.tag, int(el.get('id'))): dict(el.attrib) for el in ET.parse(path).getroot() if el.get('id')}


def pools(path):
    return {p.get('Name'): {int(i.get('Id')): dict(i.attrib) for i in p} for p in ET.parse(path).getroot()}


def names():
    out = {}
    for (tag, item_id), attrs in records(SWITCH / 'items.xml').items():
        out[(tag, item_id)] = attrs.get('name', '')
    return out


def main():
    label = names()
    result = {'items': [], 'metadata': [], 'pools': [], 'pc_only_records': [], 'switch_only_records': []}
    for name, key in (('items.xml', 'items'), ('items_metadata.xml', 'metadata')):
        s, p = records(SWITCH / name), records(PC / name)
        result['pc_only_records'] += [f'{name}:{k}' for k in sorted(set(p) - set(s))]
        result['switch_only_records'] += [f'{name}:{k}' for k in sorted(set(s) - set(p))]
        for k in sorted(set(s) & set(p)):
            for attr in sorted(set(s[k]) | set(p[k])):
                if s[k].get(attr) != p[k].get(attr):
                    item_label = label.get(k) or label.get(('passive', k[1])) or label.get(('active', k[1])) or label.get(('familiar', k[1])) or ''
                    result[key].append({'tag': k[0], 'id': k[1], 'name': item_label, 'attribute': attr,
                                        'switch': s[k].get(attr), 'pc': p[k].get(attr)})
    s, p = pools(SWITCH / 'itempools.xml'), pools(PC / 'itempools.xml')
    result['pool_names'] = {'switch_only': sorted(set(s) - set(p)), 'pc_only': sorted(set(p) - set(s))}
    for name in sorted(set(s) & set(p)):
        for item_id in sorted(set(s[name]) | set(p[name])):
            a, b = s[name].get(item_id), p[name].get(item_id)
            if a != b:
                result['pools'].append({'pool': name, 'id': item_id, 'switch': a, 'pc': b})
    out = ROOT / 'evidence/balance/full-diff.json'
    out.write_text(json.dumps(result, indent=2) + '\n')
    print({k: len(v) for k, v in result.items() if isinstance(v, list)}, result['pool_names'])


if __name__ == '__main__':
    main()

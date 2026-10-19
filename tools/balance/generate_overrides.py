"""Generate the Repentance+ data-only balance overrides from the untouched Switch files.

Scope (user, 2026-10-19): every Repentance+ change that is data the Switch
1.7.9b engine already reads, unless it belongs to a reworked behavior.
Sources: Isaac-dump/romfs/rp_patch/resources (immutable) and the user's PC
v1.9.7.17 data in reference/pc-repentance-plus (local-only).

Edits are textual and scoped to single elements; comments, formatting and
every other attribute stay byte-identical. The result is parsed and checked
field by field: approved fields must equal the PC value, everything else must
equal the Switch value.
Output: features/balance/resources/*.xml and features/balance/changes.json.
"""
import hashlib
import json
import re
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
SWITCH = ROOT.parent / 'Isaac-dump/romfs/rp_patch/resources'
PC = ROOT / 'reference/pc-repentance-plus/resources'
OUT = ROOT / 'features/balance/resources'
FILES = ('items.xml', 'items_metadata.xml', 'itempools.xml', 'pocketitems.xml')
EXPECTED_SWITCH = {
    'items.xml': 'ee567ed920b1bcad', 'items_metadata.xml': '42b3eb47a2f39d71',
    'itempools.xml': 'da5391ba59626b0f', 'pocketitems.xml': None,
}
# Tags the Switch parser (0x3b7650) knows. 'devilsacrifice' is new in Repentance+.
KNOWN_TAGS = set('''dead syringe mom tech battery guppy fly bob mushroom baby angel devil poop book
spider quest monstermanual nogreed food tearsup offensive nokeeper nolostbr stars summonable
nocantrip wisp uniquefamiliar nochallenge nodaily lazarusshared lazarussharedglobal noeden'''.split())

# items.xml: approved (id, attribute) changes and the player-visible effect.
ITEM_FIELDS = {
    (477, 'maxcharges'): 'Void recharges in 4 rooms (was 6)',
    (594, 'hearts'): 'Jupiter heals 2 red hearts on pickup (was half a heart)',
    (643, 'soulhearts'): 'Revelation no longer grants 2 soul hearts',
    (294, 'maxcharges'): 'Butter Bean recharges in 2 seconds (was 3)',
    (111, 'maxcharges'): 'The Bean recharges after 4 seconds instead of a room',
    (111, 'chargetype'): 'The Bean recharges after 4 seconds instead of a room',
    (421, 'maxcharges'): 'Kidney Bean recharges after 4 seconds instead of a room',
    (421, 'chargetype'): 'Kidney Bean recharges after 4 seconds instead of a room',
    (78, 'devilprice'): 'Book of Revelations costs 2 hearts in a devil deal',
    (651, 'devilprice'): 'Star of Bethlehem costs 2 hearts in a devil deal',
    (374, 'devilprice'): 'Holy Light costs 2 hearts in a devil deal',
    (533, 'devilprice'): 'Trisagion costs 2 hearts in a devil deal',
    (243, 'devilprice'): 'Trinity Shield costs 1 heart in a devil deal (was 2)',
    (606, 'devilprice'): 'Ocular Rift costs 1 heart in a devil deal (was 2)',
    (386, 'shopprice'): 'D12 is cheaper in the Greed Mode shop',
    (478, 'shopprice'): 'Pause is cheaper in the Greed Mode shop',
}
ITEM_EXCLUDED = {
    (351, 'maxcharges'): 'Mega Bean: 6-charge cost belongs to its reworked room-wide fart',
    (489, 'maxcharges'): 'D Infinity: PC change fixes a charge display; the Switch code owns its real charge',
    (436, 'persistent'): 'Milk!: part of its familiar rework',
    'cache': 'Stat-refresh flags only matter with the Repentance+ stat code behind them',
}
POOL_EXCLUDED = {('shop', 177), ('beggar', 177)}  # Portable Slot's replacement route is code


def element_span(text, tags, item_id):
    pattern = re.compile(r'<(?:%s)\b[^>]*?\bid="%s"[^>]*?/?>' % ('|'.join(tags), item_id))
    matches = list(pattern.finditer(text))
    assert len(matches) == 1, (tags, item_id, len(matches))
    return matches[0]


def set_attribute(element, name, old, new):
    found = list(re.finditer(r'(\s+)%s="([^"]*)"' % re.escape(name), element))
    if old is None:
        assert not found and new is not None, (element, name)
        end = element.rfind('/>') if element.rstrip().endswith('/>') else element.rfind('>')
        head, tail = element[:end], element[end:]
        stripped = head.rstrip()
        return stripped + f' {name}="{new}"' + head[len(stripped):] + tail
    assert len(found) == 1 and found[0].group(2) == old, (element, name, old)
    if new is None:
        return element[:found[0].start()] + element[found[0].end():]
    return element[:found[0].start()] + f'{found[0].group(1)}{name}="{new}"' + element[found[0].end():]


def replace_span(text, match, new):
    return text[:match.start()] + new + text[match.end():]


def records(xml):
    return {(el.tag, el.get('id')): dict(el.attrib) for el in ET.fromstring(xml) if el.get('id') is not None}


def ordered(keys):
    return sorted(keys, key=lambda k: (k[0], int(k[1]) if str(k[1]).isdigit() else 0, str(k[1])))


def main():
    sources, newline = {}, {}
    for name in FILES:
        raw = (SWITCH / name).read_bytes()
        if EXPECTED_SWITCH[name]:
            assert hashlib.sha256(raw).hexdigest().startswith(EXPECTED_SWITCH[name]), name
        text = raw.decode('utf-8')
        newline[name] = '\r\n' if '\r\n' in text else '\n'
        sources[name] = text.replace('\r\n', '\n')
    pc = {name: (PC / name).read_bytes().decode('utf-8').replace('\r\n', '\n') for name in FILES}
    changes = {'items': [], 'metadata': [], 'pools': [], 'pocket': [], 'excluded': []}
    expected = {}  # (file, tag, id, attr) -> value

    # items.xml
    s_rec, p_rec = records(sources['items.xml']), records(pc['items.xml'])
    text = sources['items.xml']
    for key in ordered(s_rec):
        tag, item_id = key
        for attr in sorted(set(s_rec[key]) | set(p_rec.get(key, {}))):
            old, new = s_rec[key].get(attr), p_rec.get(key, {}).get(attr)
            if old == new:
                continue
            approved = tag in ('active', 'passive', 'familiar') and (int(item_id), attr) in ITEM_FIELDS
            if not approved:
                reason = ITEM_EXCLUDED.get((int(item_id), attr)) or ITEM_EXCLUDED.get(attr) or 'Not a listed data-only change'
                changes['excluded'].append({'file': 'items.xml', 'tag': tag, 'id': int(item_id), 'attribute': attr, 'switch': old, 'pc': new, 'reason': reason})
                continue
            m = element_span(text, (tag,), item_id)
            text = replace_span(text, m, set_attribute(m.group(0), attr, old, new))
            expected[('items.xml', tag, item_id, attr)] = new
            changes['items'].append({'tag': tag, 'id': int(item_id), 'attribute': attr, 'switch': old, 'new': new,
                                     'reason': ITEM_FIELDS[(int(item_id), attr)]})
    assert {(c['id'], c['attribute']) for c in changes['items']} == set(ITEM_FIELDS), 'Approved item change missing in PC data'
    items_xml = text

    # items_metadata.xml: quality, craftquality and known tags for items and trinkets.
    s_rec, p_rec = records(sources['items_metadata.xml']), records(pc['items_metadata.xml'])
    text = sources['items_metadata.xml']
    for key in ordered(s_rec):
        tag, item_id = key
        if key not in p_rec:
            continue
        for attr in sorted(set(s_rec[key]) | set(p_rec[key])):
            old, new = s_rec[key].get(attr), p_rec[key].get(attr)
            if attr == 'tags' and new is not None:
                dropped = sorted(set(new.split()) - KNOWN_TAGS)
                if dropped:
                    new = ' '.join(t for t in new.split() if t in KNOWN_TAGS)
                    changes['excluded'].append({'file': 'items_metadata.xml', 'tag': tag, 'id': int(item_id), 'attribute': 'tags',
                                                'dropped': dropped, 'reason': 'Tag unknown to the Switch 1.7.9b parser'})
                if old is not None and set(new.split()) == set(old.split()):
                    continue
            if old == new:
                continue
            if attr not in ('quality', 'craftquality', 'tags'):
                changes['excluded'].append({'file': 'items_metadata.xml', 'tag': tag, 'id': int(item_id), 'attribute': attr, 'switch': old, 'pc': new, 'reason': 'Unlisted attribute'})
                continue
            m = element_span(text, (tag,), item_id)
            text = replace_span(text, m, set_attribute(m.group(0), attr, old, new))
            expected[('items_metadata.xml', tag, item_id, attr)] = new
            changes['metadata'].append({'tag': tag, 'id': int(item_id), 'attribute': attr, 'switch': old, 'new': new})
    metadata_xml = text

    # itempools.xml
    pools_xml = sources['itempools.xml']
    s_pools = {p.get('Name'): [dict(i.attrib) for i in p] for p in ET.fromstring(pools_xml)}
    p_pools = {p.get('Name'): [dict(i.attrib) for i in p] for p in ET.fromstring(pc['itempools.xml'])}
    assert list(s_pools) == list(p_pools)
    pc_comments = {}
    for m in re.finditer(r'<!--([^\n]*?)-->\n[ \t]*<Item Id="(\d+)"', pc['itempools.xml']):
        pc_comments.setdefault(int(m.group(2)), m.group(1))
    expected_pools = {}
    for name in s_pools:
        s_map = {int(e['Id']): e for e in s_pools[name]}
        p_map = {int(e['Id']): e for e in p_pools[name]}
        result = [dict(e) for e in s_pools[name]]
        match = re.search(r'(<Pool Name="%s">)(.*?)(</Pool>)' % re.escape(name), pools_xml, re.S)
        assert match and pools_xml.count('<Pool Name="%s">' % name) == 1
        body = match.group(2)
        for item_id in sorted(set(s_map) | set(p_map)):
            a, b = s_map.get(item_id), p_map.get(item_id)
            if a == b:
                continue
            if (name, item_id) in POOL_EXCLUDED:
                changes['excluded'].append({'file': 'itempools.xml', 'pool': name, 'id': item_id, 'switch': a, 'pc': b,
                                            'reason': 'Portable Slot: its replacement source is reworked code'})
                continue
            line = re.compile(r'(\n[ \t]*<!--[^\n]*-->)?(\n[ \t]*<Item Id="%d"[^\n]*/>)' % item_id)
            if b is None:
                found = list(line.finditer(body)); assert len(found) == 1, (name, item_id)
                body = body[:found[0].start()] + body[found[0].end():]
                result = [e for e in result if int(e['Id']) != item_id]
                changes['pools'].append({'pool': name, 'id': item_id, 'change': 'removed', 'switch': a})
            elif a is None:
                indent = re.search(r'\n([ \t]*)<Item ', body).group(1)
                closing = re.search(r'\n[ \t]*$', body)
                comment = pc_comments.get(item_id, f'Item {item_id}')
                entry = f'\n{indent}<!--{comment}-->\n{indent}<Item ' + ' '.join(f'{k}="{v}"' for k, v in b.items()) + '/>'
                body = body[:closing.start()] + entry + body[closing.start():]
                result.append(dict(b))
                changes['pools'].append({'pool': name, 'id': item_id, 'change': 'added', 'pc': b})
            else:
                found = list(re.finditer(r'<Item Id="%d"[^\n]*?/>' % item_id, body)); assert len(found) == 1, (name, item_id)
                new_entry = '<Item ' + ' '.join(f'{k}="{v}"' for k, v in b.items()) + '/>'
                body = body[:found[0].start()] + new_entry + body[found[0].end():]
                result = [dict(b) if int(e['Id']) == item_id else e for e in result]
                changes['pools'].append({'pool': name, 'id': item_id, 'change': 'weights', 'switch': a, 'pc': b})
        pools_xml = pools_xml[:match.start(2)] + body + pools_xml[match.end(2):]
        expected_pools[name] = result

    # pocketitems.xml: Blank Card/Clear Rune/Placebo charge costs and Greed Mode availability.
    s_rec, p_rec = records(sources['pocketitems.xml']), records(pc['pocketitems.xml'])
    text = sources['pocketitems.xml']
    for key in ordered(s_rec):
        tag, item_id = key
        if key not in p_rec:
            continue
        for attr in sorted(set(s_rec[key]) | set(p_rec[key])):
            old, new = s_rec[key].get(attr), p_rec[key].get(attr)
            if old == new:
                continue
            if attr not in ('mimiccharge', 'greedmode'):
                changes['excluded'].append({'file': 'pocketitems.xml', 'tag': tag, 'id': item_id, 'attribute': attr, 'switch': old, 'pc': new, 'reason': 'Unlisted attribute'})
                continue
            m = element_span(text, (tag,), item_id)
            text = replace_span(text, m, set_attribute(m.group(0), attr, old, new))
            expected[('pocketitems.xml', tag, item_id, attr)] = new
            changes['pocket'].append({'tag': tag, 'id': item_id, 'attribute': attr, 'switch': old, 'new': new})
    pocket_xml = text

    outputs = {'items.xml': items_xml, 'items_metadata.xml': metadata_xml, 'itempools.xml': pools_xml, 'pocketitems.xml': pocket_xml}
    validate(sources, outputs, expected, expected_pools)
    OUT.mkdir(parents=True, exist_ok=True)
    for name, value in outputs.items():
        (OUT / name).write_bytes(value.replace('\n', newline[name]).encode('utf-8'))
    changes['sha256'] = {name: hashlib.sha256((OUT / name).read_bytes()).hexdigest() for name in outputs}
    changes['switch_sha256'] = {name: hashlib.sha256((SWITCH / name).read_bytes()).hexdigest() for name in outputs}
    changes['pc_sha256'] = {name: hashlib.sha256((PC / name).read_bytes()).hexdigest() for name in outputs}
    changes['pc_reference'] = 'Steam build 22878971, v1.9.7.17.J460 (afterbirthp.a), local extraction'
    (ROOT / 'features/balance/changes.json').write_text(json.dumps(changes, indent=2) + '\n')
    print('Balance overrides:', {k: len(v) for k, v in changes.items() if isinstance(v, list)})


def validate(sources, outputs, expected, expected_pools):
    for name in ('items.xml', 'items_metadata.xml', 'pocketitems.xml'):
        old, new = records(sources[name]), records(outputs[name])
        assert list(old) == list(new), name
        for key in old:
            for attr in set(old[key]) | set(new[key]):
                want = expected.get((name, key[0], key[1], attr), old[key].get(attr))
                assert new[key].get(attr) == want, (name, key, attr, old[key].get(attr), new[key].get(attr), want)
    s_names = [p.get('Name') for p in ET.fromstring(sources['itempools.xml'])]
    new_pools = {p.get('Name'): [dict(i.attrib) for i in p] for p in ET.fromstring(outputs['itempools.xml'])}
    assert list(new_pools) == s_names
    for name in s_names:
        assert new_pools[name] == expected_pools[name], name


if __name__ == '__main__':
    main()

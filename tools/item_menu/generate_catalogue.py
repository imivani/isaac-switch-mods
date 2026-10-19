"""Make the picker from the exact installed game's real collectible records."""
from pathlib import Path
import hashlib
import json
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
source = ROOT.parent / 'Isaac-dump/romfs/rp_patch/resources/items.xml'
eid_path = ROOT / 'eid/generated/descriptions.json'
eid = {e['lookup_id']: e for e in json.loads(eid_path.read_text())['entries'] if e['category'] == 'collectibles'}
items = []
for element in ET.parse(source).getroot():
    if element.tag not in ('passive', 'active', 'familiar') or 'id' not in element.attrib:
        continue
    item_id = int(element.attrib['id'])
    if item_id <= 0:
        continue
    entry = eid.get(item_id)
    name = entry['name'] if entry and entry.get('name') else f'Item {item_id}'
    items.append({'id': item_id, 'name': name, 'kind': element.tag.title()})
assert len({i['id'] for i in items}) == len(items)
items.sort(key=lambda i: (i['name'].casefold(), i['id']))
lines = ['// Generated from this game build; no invented or unused item IDs.', '#pragma once',
         '#include "../../../../features/item_menu/state.hpp"', 'namespace isaac::item_menu {',
         'inline constexpr Item Catalogue[] = {']
for item in items:
    literal=json.dumps(item['name'], ensure_ascii=True).replace('?', r'\?')
    lines.append('    {%d, Kind::%s, %s},' % (item['id'], item['kind'], literal))
lines += ['};', 'inline constexpr unsigned CatalogueCount = sizeof(Catalogue)/sizeof(Catalogue[0]);', '}']
target = ROOT / 'native/source/program/generated/item_menu_catalogue.hpp'
target.write_text('\n'.join(lines)+'\n', encoding='utf-8')
out = ROOT / 'evidence/iteration4/item-menu'; out.mkdir(parents=True, exist_ok=True)
(out / 'catalogue.json').write_text(json.dumps({'result': 'PASS', 'count': len(items), 'game_items_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
    'source_sha256': {'tools/item_menu/generate_catalogue.py': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
                      'eid/generated/descriptions.json': hashlib.sha256(eid_path.read_bytes()).hexdigest(),
                      target.relative_to(ROOT).as_posix(): hashlib.sha256(target.read_bytes()).hexdigest()},
    'items': items}, indent=2)+'\n')
print(f'Picker: {len(items)} exact-build collectibles, sorted by name')

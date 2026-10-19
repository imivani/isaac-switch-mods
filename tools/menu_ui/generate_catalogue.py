"""Generate the mod menu's browsable catalogue from this build's effective data.

Collectibles use the balance-adjusted Repentance data (quality, tags, pools);
names come from the bundled EID English text. Writes
features/menu_ui/generated/catalogue.hpp and evidence/menu/catalogue.json.
"""
import hashlib
import json
import xml.etree.ElementTree as ET
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
DUMP = ROOT.parent / 'Isaac-dump/romfs'
BALANCE = ROOT / 'features/balance/resources'


def effective(name):
    path = BALANCE / name
    return path if path.is_file() else DUMP / 'rp_patch/resources' / name


ITEMS, META, POOLS, POCKET = (effective(n) for n in ('items.xml', 'items_metadata.xml', 'itempools.xml', 'pocketitems.xml'))
EID = ROOT / 'eid/generated/descriptions.json'
OUT = ROOT / 'features/menu_ui/generated/catalogue.hpp'
FORMS = {'guppy': 0, 'fly': 1, 'mushroom': 2, 'angel': 3, 'bob': 4, 'syringe': 5, 'mom': 6, 'baby': 7,
         'devil': 8, 'poop': 9, 'book': 10, 'spider': 12}
KIND = {'passive': 1, 'familiar': 2, 'active': 3}
# Spawnable pickups: (variant, subtype, name, EID icon token).
PICKUPS = [
    (10, 1, 'Red Heart', 'Heart'), (10, 2, 'Half Red Heart', 'HalfHeart'), (10, 5, 'Double Red Heart', 'Heart'),
    (10, 3, 'Soul Heart', 'SoulHeart'), (10, 8, 'Half Soul Heart', 'HalfSoulHeart'), (10, 6, 'Black Heart', 'BlackHeart'),
    (10, 4, 'Eternal Heart', 'EternalHeart'), (10, 7, 'Golden Heart', 'GoldenHeart'), (10, 11, 'Bone Heart', 'BoneHeart'),
    (10, 12, 'Rotten Heart', 'RottenHeart'), (10, 10, 'Blended Heart', 'Heart'),
    (20, 1, 'Penny', 'Coin'), (20, 2, 'Nickel', 'Coin'), (20, 3, 'Dime', 'Coin'), (20, 5, 'Lucky Penny', 'Coin'),
    (20, 7, 'Golden Penny', 'Coin'), (30, 1, 'Key', 'Key'), (30, 2, 'Golden Key', 'GoldenKey'),
    (30, 4, 'Charged Key', 'Key'), (40, 1, 'Bomb', 'Bomb'), (40, 2, 'Double Bomb', 'Bomb'),
    (40, 4, 'Golden Bomb', 'GoldenBomb'), (40, 7, 'Giga Bomb', 'Bomb'), (90, 1, 'Lil Battery', 'Battery'),
    (90, 3, 'Mega Battery', 'Battery'), (69, 1, 'Grab Bag', 'GrabBag'), (50, 0, 'Chest', 'Chest'),
    (60, 0, 'Golden Chest', 'GoldenChest'), (360, 0, 'Red Chest', 'RedChest'), (56, 0, 'Wooden Chest', 'Chest'),
    (52, 0, 'Bomb Chest', 'Chest'), (55, 0, 'Eternal Chest', 'Chest'), (54, 0, 'Spiked Chest', 'Chest'),
]


def cpp_string(text):
    out = '"'
    for byte in text.encode('utf-8'):
        if byte in (0x22, 0x5c, 0x3f) or byte < 0x20 or byte > 0x7e:
            out += '\\%03o' % byte
        else:
            out += chr(byte)
    return out + '"'


def main():
    entries = json.loads(EID.read_text(encoding='utf-8'))['entries']
    names = {(e['category'], e['lookup_id']): e['name'] for e in entries if e.get('name')}
    meta = {int(e.get('id')): e.attrib for e in ET.parse(META).getroot() if e.tag == 'item' and e.get('id')}
    pool_root = ET.parse(POOLS).getroot()
    pool_names = [p.get('Name') for p in pool_root]
    assert len(pool_names) <= 32
    membership = {}
    for index, pool in enumerate(pool_root):
        for entry in pool:
            membership[int(entry.get('Id'))] = membership.get(int(entry.get('Id')), 0) | (1 << index)
    collectibles = []
    for element in ET.parse(ITEMS).getroot():
        if element.tag not in KIND or not element.get('id') or int(element.get('id')) <= 0:
            continue
        item_id = int(element.get('id'))
        detail = meta.get(item_id, {})
        tags = detail.get('tags', '').split()
        collectibles.append({'id': item_id, 'kind': KIND[element.tag], 'quality': int(detail.get('quality', 0)),
                             'pools': membership.get(item_id, 0), 'forms': sum(1 << b for t, b in FORMS.items() if t in tags),
                             'hidden': element.get('hidden') == 'true',
                             'name': names.get(('collectibles', item_id), f'Item {item_id}')})
    collectibles.sort(key=lambda c: (c['name'].casefold(), c['id']))
    trinkets = []
    for element in ET.parse(ITEMS).getroot():
        if element.tag == 'trinket' and element.get('id') and int(element.get('id')) > 0:
            trinket_id = int(element.get('id'))
            trinkets.append({'id': trinket_id, 'name': names.get(('trinkets', trinket_id), f'Trinket {trinket_id}')})
    trinkets.sort(key=lambda t: (t['name'].casefold(), t['id']))
    cards, pills = [], []
    for element in ET.parse(POCKET).getroot():
        if element.tag in ('card', 'rune') and element.get('id') and 0 < int(element.get('id')) <= 97:
            card_id = int(element.get('id'))
            cards.append({'id': card_id, 'rune': element.tag == 'rune' or element.get('type') == 'rune',
                          'name': names.get(('cards', card_id), f'Card {card_id}')})
        if element.tag == 'pilleffect' and element.get('id'):
            effect = int(element.get('id'))
            pills.append({'id': effect, 'name': names.get(('pills', effect + 1), f'Pill {effect}')})
    cards.sort(key=lambda c: c['id'])
    pills.sort(key=lambda p: (p['name'].casefold(), p['id']))
    lines = ['// Generated by tools/menu_ui/generate_catalogue.py from this build\'s effective data.', '#pragma once',
             '#include <cstdint>', 'namespace isaac::menu_ui::catalogue {',
             'struct Collectible { std::uint16_t id; std::uint8_t kind, quality; std::uint32_t pools; std::uint16_t forms; bool hidden; const char* name; };',
             'struct Named { std::uint16_t id; bool flag; const char* name; };',
             'struct Pickup { std::uint16_t variant, subtype; const char* name; const char* icon; };',
             'inline constexpr Collectible Collectibles[] = {']
    for c in collectibles:
        lines.append('    {%d,%d,%d,0x%xu,0x%x,%s,%s},' % (c['id'], c['kind'], c['quality'], c['pools'], c['forms'],
                                                         'true' if c['hidden'] else 'false', cpp_string(c['name'])))
    lines.append('};\ninline constexpr Named Trinkets[] = {')
    lines += ['    {%d,false,%s},' % (t['id'], cpp_string(t['name'])) for t in trinkets]
    lines.append('};\ninline constexpr Named Cards[] = {')
    lines += ['    {%d,%s,%s},' % (c['id'], 'true' if c['rune'] else 'false', cpp_string(c['name'])) for c in cards]
    lines.append('};\ninline constexpr Named Pills[] = {')
    lines += ['    {%d,false,%s},' % (p['id'], cpp_string(p['name'])) for p in pills]
    lines.append('};\ninline constexpr Pickup Pickups[] = {')
    lines += ['    {%d,%d,%s,%s},' % (v, s, cpp_string(n), cpp_string(i)) for v, s, n, i in PICKUPS]
    lines.append('};\ninline constexpr const char* PoolNames[] = {' + ','.join(cpp_string(n) for n in pool_names) + '};')
    lines += ['inline constexpr unsigned CollectibleCount = sizeof(Collectibles)/sizeof(*Collectibles);',
              'inline constexpr unsigned TrinketCount = sizeof(Trinkets)/sizeof(*Trinkets);',
              'inline constexpr unsigned CardCount = sizeof(Cards)/sizeof(*Cards);',
              'inline constexpr unsigned PillCount = sizeof(Pills)/sizeof(*Pills);',
              'inline constexpr unsigned PickupCount = sizeof(Pickups)/sizeof(*Pickups);',
              'inline constexpr unsigned PoolCount = sizeof(PoolNames)/sizeof(*PoolNames);', '}']
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text('\n'.join(lines) + '\n', encoding='ascii')
    sources = [ITEMS, META, POOLS, POCKET, EID, Path(__file__), OUT]
    report = {'result': 'PASS', 'collectibles': len(collectibles), 'trinkets': len(trinkets), 'cards': len(cards),
              'pills': len(pills), 'pickups': len(PICKUPS), 'pools': pool_names,
              'source_sha256': {p.resolve().relative_to(ROOT.parent).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest() for p in sources}}
    evidence = ROOT / 'evidence/menu/catalogue.json'
    evidence.parent.mkdir(parents=True, exist_ok=True)
    evidence.write_text(json.dumps(report, indent=2) + '\n')
    print(f"Menu catalogue: {len(collectibles)} collectibles, {len(trinkets)} trinkets, {len(cards)} cards/runes, "
          f"{len(pills)} pills, {len(PICKUPS)} pickups, {len(pool_names)} pools")


if __name__ == '__main__':
    main()

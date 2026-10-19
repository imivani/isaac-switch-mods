"""Extract symbol/PLT evidence from the immutable Switch NRO; never execute it."""
import hashlib
import json
import pathlib
import struct

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "evidence" / "native-analysis"
DATA = (ROOT.parent / "Isaac-dump" / "romfs" / ".nro" / "Repentance.nro").read_bytes()
OUT.mkdir(parents=True, exist_ok=True)
rodata = struct.unpack_from("<I", DATA, 0x28)[0]
str_off, str_size, sym_off, sym_size = struct.unpack_from("<4I", DATA, 0x70)
strings = DATA[rodata + str_off:rodata + str_off + str_size]
SYMS = []
for pos in range(rodata + sym_off, rodata + sym_off + sym_size, 24):
    name_offset, info, other, section, value, size = struct.unpack_from("<IBBHQQ", DATA, pos)
    name = strings[name_offset:].split(b"\0", 1)[0].decode("ascii")
    SYMS.append(dict(name=name, index=len(SYMS), offset=value, size=size, defined=bool(section), info=info))
(OUT / "symbols.json").write_text(json.dumps(SYMS, indent=2))
mod0 = struct.unpack_from("<I", DATA, 4)[0]
dynamic = mod0 + struct.unpack_from("<i", DATA, mod0 + 4)[0]
TAGS = {}
for pos in range(dynamic, len(DATA), 16):
    tag, value = struct.unpack_from("<qQ", DATA, pos)
    if not tag:
        break
    TAGS[tag] = value
(OUT / "dynamic.json").write_text(json.dumps(TAGS, indent=2))
anchors = {
    "sha256": hashlib.sha256(DATA).hexdigest(), "size": len(DATA),
    "build_id": DATA[0x40:0x60].hex(), "header_0x80": DATA[:0x80].hex(),
    "hud_render_offset": "0x3ac71c",
    "hud_render_first_20_bytes": DATA[0x3ac71c:0x3ac71c + 20].hex(),
    "hud_render_first_20_words": [hex(v) for v in struct.unpack_from("<5I", DATA, 0x3ac71c)],
}
(OUT / "build-anchors.json").write_text(json.dumps(anchors, indent=2))
data_relocs = {}
for pos in range(TAGS[7], TAGS[7] + TAGS[8], 24):
    offset, info, addend = struct.unpack_from("<QQq", DATA, pos)
    data_relocs[offset] = (info, addend)
by_offset = {s["offset"]: s["name"] for s in SYMS if s["defined"]}
vtables = []
for symbol in SYMS:
    if symbol["name"] not in ("_ZTVN15IsaacRepentance6EntityE", "_ZTVN15IsaacRepentance10Entity_NPCE"):
        continue
    entries = []
    for offset in range(symbol["offset"] + 16, symbol["offset"] + symbol["size"], 8):
        info, addend = data_relocs.get(offset, (0, struct.unpack_from("<Q", DATA, offset)[0]))
        index = info >> 32
        target = SYMS[index]["offset"] + addend if index else addend
        name = SYMS[index]["name"] if index else by_offset.get(target, "")
        entries.append(dict(slot=hex(offset - symbol["offset"] - 16), target=hex(target), name=name))
    vtables.append(dict(name=symbol["name"], offset=hex(symbol["offset"]), address_point=hex(symbol["offset"] + 16), entries=entries))
(OUT / "entity-vtables.json").write_text(json.dumps(vtables, indent=2))
relocs = {}
for pos in range(TAGS[23], TAGS[23] + TAGS[2], 24):
    offset, info, addend = struct.unpack_from("<QQq", DATA, pos)
    relocs[offset] = SYMS[info >> 32]
stubs = []
for pos in range(0, struct.unpack_from("<I", DATA, 0x24)[0] - 16, 4):
    adrp, ldr, add, br = struct.unpack_from("<4I", DATA, pos)
    if adrp & 0x9f00001f != 0x90000010 or ldr & 0xffc003ff != 0xf9400211 or br != 0xd61f0220:
        continue
    imm = ((adrp >> 5) & 0x7ffff) << 2 | ((adrp >> 29) & 3)
    if imm & (1 << 20):
        imm -= 1 << 21
    got = (pos & ~0xfff) + (imm << 12) + (((ldr >> 10) & 0xfff) << 3)
    if got in relocs:
        stubs.append(dict(offset=pos, got=got, symbol=relocs[got]))
(OUT / "plt.json").write_text(json.dumps(stubs, indent=2))
(OUT / "plt.tsv").write_text("\n".join(f"{s['offset']:x}\t{s['symbol']['name']}\t{s['symbol']['offset']:x}\t{s['symbol']['size']}\t{int(s['symbol']['defined'])}" for s in stubs))
names = [
    "_ZN15IsaacRepentance3HUD6RenderEv",
    "_ZN15IsaacRepentance4Game9DebugTextERKN4KAGE4Math7Vector2ERKNS1_8Graphics5ColorEPKcz",
    "_ZNK4KAGE8Graphics4Font10DrawStringEPKcffNS0_5ColorEjb",
    "_ZNK4KAGE8Graphics4Font16DrawStringScaledEPKcffffNS0_5ColorEjb",
    "_ZN4KAGE8Graphics13ShapeRenderer8FillQuadEffffRKNS0_5ColorE",
    "_ZN4KAGE8Graphics5ColorC2Effff",
    "_ZN15IsaacRepentance10EntityList9QueryTypeENS_11eEntityTypeEiibb",
    "_ZN15IsaacRepentance4Room9AddEntityEPNS_6EntityE",
    "_ZNK15IsaacRepentance10ItemConfig14GetCollectibleEi",
    "_ZN15IsaacRepentance4Game16GetNearestPlayerERKN4KAGE4Math7Vector2E",
    "_ZN15IsaacRepentance4Room16TriggerBossSpawnEPNS_10Entity_NPCE",
    "_ZN15IsaacRepentance4Room16TriggerBossDeathEPNS_10Entity_NPCE",
    "_ZN15IsaacRepentance3HUD15RenderHealthBarERKN4KAGE4Math7Vector2EPNS_6EntityE",
    "_ZN15IsaacRepentance6Entity4KillEv",
    "_ZN15IsaacRepentance6Entity6RemoveEv",
    "_ZN15IsaacRepentance6Entity6RenderERKN4KAGE4Math7Vector2E",
    "_ZN15IsaacRepentance10EntityList6UpdateEb",
    "_ZN15IsaacRepentance10EntityList3AddEPNS_6EntityE",
    "_ZNK15IsaacRepentance6Entity6IsBossEv",
    "_ZNK15IsaacRepentance10Entity_NPC6IsBossEv",
    "_ZN4KAGE8Graphics15DestinationQuadC1ERKfS3_S3_S3_",
    "_ZN15IsaacRepentance4Game6RenderEv",
    "_ZN15IsaacRepentance3HUD6UpdateEv",
    "_ZNK15IsaacRepentance6Entity17IsVulnerableEnemyEPS0_",
    "_ZN15IsaacRepentance13Entity_Pickup6RenderERKN4KAGE4Math7Vector2E",
    "_ZN15IsaacRepentance13Entity_Pickup13SetForceBlindEb",
    "_ZNK15IsaacRepentance5Level9GetCursesEv",
    "_ZN15IsaacRepentance5Level12RemoveCursesEj",
    "_ZN15IsaacRepentance13Entity_Pickup24SetupCollectibleGraphicsERNS_4ANM2EjNS_16eCollectibleTypeEjb",
    "_ZN15IsaacRepentance13Entity_Pickup14ReloadGraphicsEb",
    "_ZN15IsaacRepentance4ANM28GetLayerEi",
    "_ZN15IsaacRepentance4ANM218ReplaceSpritesheetEiRKNSt3__112basic_stringIcNS1_11char_traitsIcEENS1_9allocatorIcEEEE",
    "_ZN15IsaacRepentance4ANM210LayerStateC1Ev",
    "_ZN15IsaacRepentance13Entity_Pickup12MakeShopItemEi",
    "_ZNK15IsaacRepentance6Entity13GetFrameCountEv",
    "_ZNK15IsaacRepentance4Game29GetSpecialSeedPermanentCursesEv",
    "_ZNK15IsaacRepentance4Game26GetSpecialSeedBannedCursesEv",
]
targets = [s for name in names for s in SYMS if s["name"] == name and s["defined"]]
(OUT / "targets.tsv").write_text("\n".join(f"{s['offset']:x}\t{s['size']}\t{s['name']}" for s in targets))
print(f"Prepared {len(stubs)} named PLT stubs and {len(targets)} targets")

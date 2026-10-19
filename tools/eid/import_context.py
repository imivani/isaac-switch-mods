#!/usr/bin/env python3
"""Import pinned EID room/state text and verified local game metadata as data.

Never executes Lua or the game. Derived variants follow the named upstream
modifier rules; REPENTANCE+ sections are not evaluated.
"""
from __future__ import annotations
import hashlib
import json
import re
import subprocess
from pathlib import Path
import xml.etree.ElementTree as ET
from import_eid import LuaLiterals, ParseError, PINNED_COMMIT, git_commit, cpp_string

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "reference/External-Item-Descriptions"
OUT = ROOT / "eid/generated"
DUMP = ROOT.parent / "Isaac-dump/romfs"
BALANCE = ROOT / "features/balance/resources"


def effective(relative: str) -> Path:
    """Repentance-layer data with the installed balance overrides applied."""
    name = Path(relative).name
    if relative.startswith("rp_patch/resources/") and (BALANCE / name).is_file():
        return BALANCE / name
    return DUMP / relative


class DataReader(LuaLiterals):
    """Broader literal grammar, still rejecting function calls/expressions."""
    def value(self):
        self.space()
        if self.text[self.pos] == "{": return self.table()
        if self.text[self.pos] in "\"'": return self.string()
        for literal, value in (("true", True), ("false", False), ("nil", None)):
            if re.match(literal + r"\b", self.text[self.pos:]):
                self.pos += len(literal)
                return value
        match = re.match(r"-?(?:0x[0-9a-fA-F]+|\d+(?:\.\d+)?(?:[eE][+-]?\d+)?)", self.text[self.pos:])
        if not match: self.fail("expected a data literal")
        self.pos += len(match[0])
        return int(match[0], 0) if "x" in match[0] else float(match[0]) if any(c in match[0] for c in ".eE") else int(match[0])

    def table(self, record_lines=False):
        self.expect("{")
        result, implicit = {}, 1
        while True:
            self.space()
            if self.text[self.pos] == "}":
                self.pos += 1
                return result
            key, explicit = implicit, False
            if self.text[self.pos] == "[":
                self.pos += 1
                key = self.value()
                self.expect("]")
                self.expect("=")
                explicit = True
            else:
                match = re.match(r"([A-Za-z_][A-Za-z_0-9]*)\s*=", self.text[self.pos:])
                if match:
                    key = match[1]
                    self.pos += len(match[0])
                    explicit = True
            if key in result: self.fail(f"duplicate key {key}")
            result[key] = self.value()
            if not explicit: implicit += 1
            self.space()
            if self.text[self.pos] in ",;": self.pos += 1
            elif self.text[self.pos] != "}": self.fail("expected comma or closing brace")


def table(relative, assignment):
    text = (SOURCE / relative).read_text(encoding="utf-8-sig")
    matches = list(re.finditer(r"(?m)^\s*" + re.escape(assignment) + r"\s*=\s*(?=\{)", text))
    if len(matches) != 1: raise ParseError(f"Expected one assignment: {assignment}")
    reader = DataReader(text, matches[0].end())
    return reader.table()


def ordered(value):
    return [value[index] for index in range(1, len(value) + 1)]


def number(value):
    return f"{value:.4g}"


def gold_variant(original, raw, effects, golden, box):
    if not raw: return original
    data = {"t": {1: raw}} if isinstance(raw, (int, float)) else raw
    if data.get("goldenOnly") and not golden: return original
    multiplier = 3 if golden and box else 2
    choice = 3 if golden and box else 2 if box else 1
    if "mults" in data: multiplier = data["mults"][2 if golden and box else 1]
    elif "mult" in data and (golden and box or data["mult"] < 2): multiplier = data["mult"]
    description, changed = original, 0
    if "t" in data:
        wanted = {number(v): v for v in data["t"].values()}
        def replace(match):
            nonlocal changed
            if match[0] not in wanted: return match[0]
            changed += 1
            value = wanted[match[0]]
            if value == 17 and multiplier in (2, 3): value = 16.5 if multiplier == 2 else 100 / 6
            elif value == 33 and multiplier in (1.5, 3): value = 100 / 3
            return "{{ColorGold}}" + number(value * multiplier) + "{{CR}}"
        # Keep markup identifiers intact: only description text is numeric prose.
        description = "".join(part if part.startswith("{{") else re.sub(r"\d*\.?\d+", replace, part)
                              for part in re.split(r"(\{\{[^{}]*\}\})", description))
    entries = ordered(effects) if effects else []
    if data.get("findReplace") and entries:
        for i in range(0, len(entries), 3):
            replacement = entries[min(int(i + multiplier - 1), len(entries) - 1)]
            if entries[i] in description:
                description = description.replace(entries[i], "{{ColorGold}}" + replacement + "{{CR}}", 1)
                changed += 1
    if entries and (data.get("append") or data.get("fullReplace")):
        picked = 1 if len(entries) == 2 and choice == 2 else min(choice, len(entries))
        value = entries[picked - 1]
        if data.get("fullReplace"): description = value
        elif value: description += "#{{ColorGold}}" + value
        changed += 1
    if not changed and multiplier > 1:
        description += "#{{ColorGold}}Effect " + {2:"doubled!", 3:"tripled!", 4:"quadrupled!"}.get(multiplier, "increased!")
    return description


def modified(original, spec, color):
    """Upstream general-condition shorthand: append, full replacement or pairs."""
    if isinstance(spec, str): return original + "#{{" + color + "}}" + spec
    values = ordered(spec)
    if len(values) == 1: return str(values[0])
    text = original
    for old, new in zip(values[::2], values[1::2]):
        old = number(old) if isinstance(old, (int, float)) else old
        new = number(new) if isinstance(new, (int, float)) else new
        # EID replacements are sequential; prevent matching an integer inside
        # another integer, as its general-condition handler does.
        pattern = re.escape(old)
        if re.fullmatch(r"[0-9.]+", old): pattern = r"(?<![0-9.])" + pattern + r"(?![0-9.])"
        pieces = re.split(r"(\{\{[^{}]*\}\})", text)
        done = False
        for i, piece in enumerate(pieces):
            if piece.startswith("{{") or done: continue
            pieces[i], count = re.subn(pattern, lambda _: "{{" + color + "}}" + new + "{{CR}}", piece, count=1)
            done = count > 0
        text = "".join(pieces)
    if len(values) % 2: text += "#" + str(values[-1])
    return text


def context_conditions(characters):
    """Exact literal registrations in the three explicitly supported families.

    This is deliberately not a Lua evaluator. Function-valued conditions and
    registrations outside these source sections stay out of the native port.
    """
    source = (SOURCE / "features/eid_conditionals.lua").read_text(encoding="utf-8")
    descriptions = table("descriptions/ab+/en_us.lua", "EID.descriptions[languageCode].ConditionalDescs")
    descriptions.update(table("descriptions/rep/en_us.lua", "local repConditions"))
    tainted = table("features/eid_data.lua", "EID.TaintedToRegularID")
    ranges = [("------ GREED MODE", "------ ACHIEVEMENT CHECKS"),
              ("------ NO RED HEALTH PLAYERS", "------ DUPLICATE COPIES OF ITEMS")]
    conditions, registrations = [], []
    hearts = {"Soul": [4,17,25,35,36,40], "Black": [12,24], "Coin": [14,33], "None": [10,31]}
    for start, end in ranges:
        section = source[source.index(start):source.index(end)]
        for line in section.splitlines():
            match = re.match(r'\s*EID:(AddClosestPlayerConditional|AddPlayerConditional|AddConditional)\((.*)\)', line)
            if not match: continue
            method, args = match.groups()
            if method == "AddConditional" and not re.search(r"\bEID\.IsGreedMode(?:,|$)",args): continue
            args = args.replace("EID.IsGreedMode", '"greed"')
            args = args.replace("SoulBlackChars", "{" + ",".join(map(str,hearts["Soul"]+hearts["Black"])) + "}")
            for name, ids in hearts.items(): args = args.replace('EID.SpecialHeartPlayers["'+name+'"]', "{"+",".join(map(str,ids))+"}")
            try: values = ordered(DataReader("{"+args+"}").table())
            except (ParseError, IndexError): continue
            if len(values) < 2: continue
            target, trigger = values[:2]
            targets = ordered(target) if isinstance(target,dict) else [target]
            modtext = values[2] if len(values)>2 else None
            extra = values[3] if len(values)>3 and isinstance(values[3],dict) else {}
            if set(extra) - {"layer"}: continue
            include_tainted = values[4] is not False if len(values)>4 else True
            triggers = ["greed"] if trigger == "greed" else ordered(trigger) if isinstance(trigger,dict) else [trigger]
            if any(not isinstance(value,int) for value in triggers) and triggers != ["greed"]: continue
            imported = False
            for item in targets:
                key = "5.100."+str(item) if isinstance(item,int) else item
                identity = re.fullmatch(r"5\.(100|350|300|70)\.(\d+)", key)
                if not identity: continue
                spec = descriptions.get(key+" ("+str(modtext)+")",descriptions.get(modtext)) if modtext else descriptions.get(key)
                if spec is None: continue
                for who in triggers:
                    mask = 0 if who == "greed" else 1 << who
                    if who != "greed" and include_tainted:
                        for tainted_id, regular_id in tainted.items():
                            if regular_id == who: mask |= 1 << tainted_id
                    charname = "" if who == "greed" else characters[who][1]
                    icon = "" if who == "greed" else "{{Player"+str(who)+"}} "
                    transform = lambda value: str(value).replace("{1}",charname)
                    parts = ordered(spec) if isinstance(spec,dict) else [spec]
                    operations = [(0,"",parts[0])] if isinstance(spec,str) else [(1,"",parts[0])] if len(parts)==1 else [(2,a,b) for a,b in zip(parts[::2],parts[1::2])]
                    if isinstance(spec,dict) and len(parts)>1 and len(parts)%2: operations.append((0,"",parts[-1]))
                    for operation, old, new in operations:
                        new = transform(new)
                        if operation != 2 and icon: new = icon + new.replace("#", "#"+icon)
                        if re.search(r"(?<!\{)\{[^{}]+\}(?!\})",new): continue
                        conditions.append({"category":int(identity[1]),"id":int(identity[2]),"players":mask,
                                           "greed":who=="greed","layer":extra.get("layer",0),"operation":operation,
                                           "old":transform(old),"new":new})
                        imported = True
            if imported: registrations.append(line.strip())
    return sorted(conditions,key=lambda row:row["layer"],reverse=True), registrations


def main():
    if git_commit(SOURCE) != PINNED_COMMIT: raise ValueError("Upstream pin changed")
    if subprocess.check_output(["git","-C",str(SOURCE),"status","--porcelain"],text=True).strip():
        raise ValueError("Pinned upstream checkout is modified")
    basefile, repfile, datafile = "descriptions/ab+/en_us.lua", "descriptions/rep/en_us.lua", "features/eid_data.lua"
    prefix = "EID.descriptions[languageCode]."
    catalogue = json.loads((OUT / "descriptions.json").read_text(encoding="utf-8"))
    lookup = {(row["category"], row["lookup_id"]): row for row in catalogue["entries"] if not row["placeholder"]}
    result, metadata = {}, []
    for group, file in (("sacrifice", basefile), ("dice", basefile), ("birthright", repfile)):
        values = table(file, prefix + group)
        result[group] = [{"id": key, "name": row[1] if group == "birthright" else "", "markup": row[3]} for key, row in sorted(values.items())]
    characters = table(basefile, prefix + "CharacterInfo")
    characters.update(table(repfile, "local repCharacterInfo"))
    result["character"] = [{"id": key + 1, "name": row[1], "markup": row[2]} for key, row in sorted(characters.items())]
    conditions, registrations = context_conditions(characters)
    rules = table(datafile, "EID.GoldenTrinketData")
    effects = table(repfile, prefix + "goldenTrinketEffects")
    for name, golden, box in (("golden", True, False), ("momsbox", False, True), ("goldenbox", True, True)):
        result[name] = []
        for (category, item_id), row in lookup.items():
            if category != "trinkets": continue
            text = gold_variant(row["markup"], rules.get(item_id), effects.get(item_id), golden, box)
            if text != row["markup"]: result[name].append({"id":item_id, "name":row["name"], "markup":text})
    for name, category, source_table, overrides in (("tarot", "cards", "tarotClothBuffs", None),
            ("carbattery", "collectibles", "carBattery", "local repCarBattery")):
        rules = table(repfile if name == "tarot" else basefile, prefix + source_table)
        if overrides: rules.update(table(repfile, overrides))
        result[name] = []
        for item_id, rule in rules.items():
            row = lookup.get((category, item_id))
            if row: result[name].append({"id":item_id, "name":row["name"], "markup":modified(row["markup"], rule, "ColorShinyPurple" if name == "tarot" else "ColorYellow")})
    for name in ("bingeEaterBuffs", "bookOfBelialBuffs", "tarotClothBlankCardBuffs"):
        rules = table(repfile, prefix + name)
        result[name] = [{"id":key,"name":"","markup":value} for key,value in rules.items() if isinstance(key,int) and isinstance(value,str)]

    # Immutable dump metadata gives the target build's exact quality, item type,
    # charge mode and transformation tags, without copying game artwork.
    source_xml = [DUMP / "resources/items.xml", effective("rp_patch/resources/items.xml")]
    items = {}
    for path in source_xml:
        for node in ET.parse(path).getroot():
            if "id" in node.attrib and node.tag in ("passive", "active", "familiar"):
                items[int(node.attrib["id"])] = dict(node.attrib, kind=node.tag)
    metadata_path = effective("rp_patch/resources/items_metadata.xml")
    details = {int(node.attrib["id"]): node.attrib for node in ET.parse(metadata_path).getroot() if node.tag == "item"}
    tags = {"guppy":0,"fly":1,"mushroom":2,"angel":3,"bob":4,"syringe":5,"mom":6,"baby":7,"devil":8,"poop":9,"book":10,"spider":12}
    for item_id, item in sorted(items.items()):
        if not 0 < item_id <= 732: continue
        detail = details.get(item_id, {})
        item_tags = detail.get("tags", "").split()
        metadata.append({"id":item_id,"type":{"passive":1,"active":3,"familiar":2}[item["kind"]],
            "quality":int(detail.get("quality", 0)),"charges":int(item.get("maxcharges",0)),
            "charge_type":{"normal":0,"timed":1,"special":2}.get(item.get("chargetype","normal"),2),
            "hidden":item.get("hidden","false")=="true",
            "forms":sum(1 << index for name,index in tags.items() if name in item_tags)})
    pocket = {}
    pocket_paths = [DUMP/"resources/pocketitems.xml", effective("rp_patch/resources/pocketitems.xml")]
    for path in pocket_paths:
        for node in ET.parse(path).getroot():
            if "id" in node.attrib and node.tag in ("card","rune","pilleffect"):
                kind = "pilleffect" if node.tag == "pilleffect" else "card"
                identity = int(node.attrib["id"])
                if kind == "card" and not 0 < identity <= 97: continue
                pocket[(kind,identity)] = node.attrib
    pockets = [{"category":300 if kind=="card" else 70,"id":item_id if kind=="card" else item_id+1,
        "mimic":int(row.get("mimiccharge",-1)),"rune":row.get("type")=="rune","class":row.get("class","")}
        for (kind,item_id),row in sorted(pocket.items())]
    payload = {"source_commit":PINNED_COMMIT,"groups":result,"metadata":metadata,"pockets":pockets,
        "conditions":conditions,"condition_registrations":registrations,
        "source_sha256":{name:hashlib.sha256((SOURCE/name).read_bytes()).hexdigest() for name in (basefile,repfile,datafile,"features/eid_conditionals.lua","features/eid_conditionals_funcs.lua")},
        "game_metadata_sha256":{(str(path.relative_to(DUMP)) if path.is_relative_to(DUMP) else "balance/"+path.name):hashlib.sha256(path.read_bytes()).hexdigest() for path in source_xml+[metadata_path]+pocket_paths}}
    (OUT/"context.json").write_text(json.dumps(payload,ensure_ascii=False,indent=2)+"\n",encoding="utf-8")
    lines=["// Generated by tools/eid/import_context.py; see THIRD_PARTY_NOTICES.md.","#pragma once","#include <cstdint>","namespace isaac_eid::context_data {",
        "struct Text { unsigned id; const char* name; const char* markup; };",
        "struct Item { unsigned id, type, quality, charges, charge_type; bool hidden; std::uint16_t forms; };",
        "struct Pocket { unsigned category, id; int mimic; bool rune; const char* classification; };",
        "struct Condition { unsigned category, id; std::uint64_t players; bool greed; unsigned operation; const char* old_text; const char* new_text; };"]
    for name, rows in result.items():
        lines.append("inline constexpr Text "+name+"[] = {")
        for row in sorted(rows,key=lambda r:r["id"]): lines.append("    {%d, %s, %s},"%(row["id"],cpp_string(row["name"]),cpp_string(row["markup"])))
        lines.append("};")
    lines.append("inline constexpr Item items[] = {")
    lines.extend("    {%d,%d,%d,%d,%d,%s,%d},"%(v["id"],v["type"],v["quality"],v["charges"],v["charge_type"],str(v["hidden"]).lower(),v["forms"]) for v in metadata)
    lines.append("};\ninline constexpr Pocket pockets[] = {")
    lines.extend("    {%d,%d,%d,%s,%s},"%(v["category"],v["id"],v["mimic"],str(v["rune"]).lower(),cpp_string(v["class"])) for v in pockets)
    lines.append("};\ninline constexpr Condition conditions[] = {")
    lines.extend("    {%d,%d,%dull,%s,%d,%s,%s},"%(v["category"],v["id"],v["players"],str(v["greed"]).lower(),v["operation"],cpp_string(v["old"]),cpp_string(v["new"])) for v in conditions)
    lines.append("};\n} // namespace isaac_eid::context_data\n")
    (OUT/"context.hpp").write_text("\n".join(lines),encoding="ascii")
    print(json.dumps({"groups":{key:len(value) for key,value in result.items()},"items":len(metadata),"pockets":len(pockets),"condition_registrations":len(registrations),"condition_operations":len(conditions)}))


if __name__ == "__main__": main()

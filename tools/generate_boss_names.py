"""Generate local boss labels from the user's exact version of entities2.xml."""
import json
import pathlib
import xml.etree.ElementTree as ET
root = pathlib.Path(__file__).resolve().parents[1]
source = root.parent / "Isaac-dump/romfs/rp_patch/resources/entities2.xml"
labels = {}
for entry in ET.parse(source).getroot().findall("entity"):
    if entry.get("boss") != "1":
        continue
    key = (int(entry.get("id", "0")), int(entry.get("variant", "0")), int(entry.get("subtype", "0")))
    labels[key] = entry.get("name", "Boss")
lines = ["// Generated locally from the verified game data; no public redistribution.",
         "#pragma once", "namespace isaac {", "struct BossName { unsigned type, variant, subtype; const char* name; };",
         "inline constexpr BossName BossNames[] = {"]
for (kind, variant, subtype), name in sorted(labels.items()):
    lines.append(f"    {{{kind}, {variant}, {subtype}, {json.dumps(name, ensure_ascii=True)}}},")
lines += ["};", "inline const char* FindBossName(unsigned type, unsigned variant, unsigned subtype) {",
          '    const char* fallback = "Boss";',
          "    for (const auto& entry : BossNames) {",
          "        if (entry.type == type && entry.variant == variant) {",
          "            if (entry.subtype == subtype) return entry.name;",
          "            if (entry.subtype == 0) fallback = entry.name;",
          "        }", "    }", "    return fallback;", "}", "}"]
output = root / "native/source/program/generated/boss_names.hpp"
output.write_text("\n".join(lines) + "\n", encoding="utf-8")
print(f"Generated {len(labels)} boss labels from the verified local game data")

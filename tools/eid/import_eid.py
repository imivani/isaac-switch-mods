#!/usr/bin/env python3
"""Build local C++ EID tables from pinned upstream Lua data, without running Lua.

This deliberately imports the AB+ base and standard Repentance replacement
tables only. It does not execute the game's PC mod API or conditional effects.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path

PINNED_COMMIT = "8b59d00c6828e333294639ad2b2b6875eae821cb"
SOURCE_URL = "https://github.com/wofsauge/External-Item-Descriptions"
CATEGORIES = {
    "collectibles": (100, "repCollectibles"),
    "trinkets": (350, "repTrinkets"),
    "cards": (300, "repCards"),
    "pills": (70, "repPills"),
    "horsepills": (1070, None),
}


class ParseError(ValueError):
    pass


class LuaLiterals:
    """Strict data-only Lua table reader. Unknown syntax is an error, not a skip."""

    def __init__(self, text: str, offset: int = 0, key_prefix: str | None = None):
        self.text, self.pos = text, offset
        self.row_lines: dict[int, int] = {}
        self.key_prefix = key_prefix

    def fail(self, message: str):
        line = self.text.count("\n", 0, self.pos) + 1
        raise ParseError(f"line {line}: {message}")

    def space(self):
        while self.pos < len(self.text):
            if self.text[self.pos].isspace():
                self.pos += 1
            elif self.text.startswith("--", self.pos):
                self.pos += 2
                long = re.match(r"\[(=*)\[", self.text[self.pos:])
                if long:
                    closing = "]" + long[1] + "]"
                    end = self.text.find(closing, self.pos + long.end())
                    if end < 0:
                        self.fail("unterminated block comment")
                    self.pos = end + len(closing)
                else:
                    end = self.text.find("\n", self.pos)
                    self.pos = len(self.text) if end < 0 else end + 1
            else:
                return

    def expect(self, value: str):
        self.space()
        if not self.text.startswith(value, self.pos):
            self.fail(f"expected {value!r}")
        self.pos += len(value)

    def string(self) -> str:
        self.space()
        if self.pos >= len(self.text) or self.text[self.pos] not in "\"'":
            self.fail("expected quoted string")
        quote = self.text[self.pos]
        self.pos += 1
        result = []
        escapes = {"a": "\a", "b": "\b", "f": "\f", "n": "\n", "r": "\r", "t": "\t", "v": "\v", "\\": "\\", "\"": "\"", "'": "'"}
        while self.pos < len(self.text):
            char = self.text[self.pos]
            self.pos += 1
            if char == quote:
                return "".join(result)
            if char != "\\":
                if char in "\r\n":
                    self.fail("unescaped newline in short string")
                result.append(char)
                continue
            if self.pos >= len(self.text):
                self.fail("unfinished string escape")
            escape = self.text[self.pos]
            self.pos += 1
            if escape in escapes:
                result.append(escapes[escape])
            elif escape.isdigit() and escape.isascii():
                digits = escape
                while len(digits) < 3 and self.pos < len(self.text) and self.text[self.pos] in "0123456789":
                    digits += self.text[self.pos]
                    self.pos += 1
                value = int(digits)
                if value > 255:
                    self.fail("decimal escape exceeds one byte")
                result.append(chr(value))
            elif escape == "x":
                digits = self.text[self.pos:self.pos + 2]
                if not re.fullmatch(r"[0-9A-Fa-f]{2}", digits):
                    self.fail("invalid hexadecimal string escape")
                result.append(chr(int(digits, 16)))
                self.pos += 2
            elif escape == "z":
                while self.pos < len(self.text) and self.text[self.pos].isspace():
                    self.pos += 1
            elif escape in "\r\n":
                if escape == "\r" and self.pos < len(self.text) and self.text[self.pos] == "\n":
                    self.pos += 1
                result.append("\n")
            else:
                self.fail(f"unsupported string escape {escape!r}")
        self.fail("unterminated string")

    def table(self, record_lines: bool = False) -> dict:
        self.expect("{")
        result, implicit = {}, 1
        while True:
            self.space()
            if self.pos >= len(self.text):
                self.fail("unterminated table")
            if self.text[self.pos] == "}":
                self.pos += 1
                return result
            row_line = self.text.count("\n", 0, self.pos) + 1
            key = implicit
            if self.text[self.pos] == "[":
                self.pos += 1
                self.space()
                if self.key_prefix:
                    prefix = re.match(re.escape(self.key_prefix) + r"\s*\.\.\s*", self.text[self.pos:])
                    if not prefix:
                        self.fail(f"expected known name prefix {self.key_prefix}")
                    self.pos += prefix.end()
                match = re.match(r"\d+", self.text[self.pos:])
                if not match:
                    self.fail("only integer table keys are accepted")
                key = int(match[0])
                self.pos += len(match[0])
                self.expect("]")
                self.expect("=")
            else:
                implicit += 1
            self.space()
            if self.text[self.pos] == "{":
                value = self.table()
            else:
                value = self.string()
            if key in result:
                self.fail(f"duplicate table key {key}")
            result[key] = value
            if record_lines:
                self.row_lines[key] = row_line
            self.space()
            if self.pos < len(self.text) and self.text[self.pos] in ",;":
                self.pos += 1
            elif self.pos >= len(self.text) or self.text[self.pos] != "}":
                self.fail("expected comma or closing brace")


def read_table(path: Path, assignment: str, key_prefix: str | None = None) -> tuple[dict, dict]:
    text = path.read_text(encoding="utf-8-sig")
    matches = list(re.finditer(r"(?m)^\s*" + re.escape(assignment) + r"\s*=\s*(?=\{)", text))
    if len(matches) != 1:
        raise ParseError(f"{path}: expected one table assignment for {assignment}, found {len(matches)}")
    reader = LuaLiterals(text, matches[0].end(), key_prefix)
    return reader.table(record_lines=True), reader.row_lines


def git_commit(source: Path) -> str:
    return subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()


# Exact text edits matching features/balance (Repentance+ data on Switch).
# Only effects that actually change in this build; each must match once.
BALANCE_TEXT = {
    ("collectibles", 594): ("{{HealingRed}} Heals half a heart", "{{HealingRed}} Heals 2 hearts"),
    ("collectibles", 643): ("{{SoulHeart}} +2 Soul Hearts#", ""),
}


def load_entries(source: Path) -> dict:
    if git_commit(source) != PINNED_COMMIT:
        raise ValueError("Source revision differs from pinned commit. Review the new tables before updating the importer pin.")
    selected_files = ["descriptions/ab+/en_us.lua", "descriptions/rep/en_us.lua", "descriptions/names/en_us.lua"]
    if subprocess.call(["git", "-C", str(source), "diff", "--quiet", "HEAD", "--", *selected_files]) != 0:
        raise ValueError("Pinned description source files have local changes; review them before generating tables.")
    collected, source_files = [], {}
    names_file = "descriptions/names/en_us.lua"
    source_files[names_file] = hashlib.sha256((source / names_file).read_bytes()).hexdigest()
    name_prefixes = {"collectibles": "C_ID", "trinkets": "T_ID", "cards": "Card_ID", "pills": "Pill_ID", "horsepills": "Pill_ID"}
    for category, (variant, override) in CATEGORIES.items():
        layers = [("descriptions/ab+/en_us.lua", f"EID.descriptions[languageCode].{category}")]
        if category == "horsepills":
            layers = [("descriptions/rep/en_us.lua", "EID.descriptions[languageCode].horsepills")]
        elif override:
            layers.append(("descriptions/rep/en_us.lua", f"local {override}"))
        merged = {}
        for relative, assignment in layers:
            path = source / relative
            raw = path.read_bytes()
            source_files[relative] = hashlib.sha256(raw).hexdigest()
            rows, lines = read_table(path, assignment)
            for key, row in rows.items():
                if row == "":
                    merged.pop(key, None)
                    continue
                if not isinstance(row, dict) or set(row) != {1, 2, 3} or not all(isinstance(v, str) for v in row.values()):
                    raise ParseError(f"{relative}:{lines[key]}: expected exactly three string fields")
                source_id, name, description = row[1], row[2], row[3]
                expected_id = key - 1 if category in {"pills", "horsepills"} else key
                if key != 9999 and source_id != str(expected_id):
                    raise ParseError(f"{relative}:{lines[key]}: id {source_id!r} disagrees with Lua key {key}")
                if not 0 < key <= 65535 or "\0" in name + description:
                    raise ParseError("unsupported key or embedded NUL")
                merged[key] = {
                    "category": category, "variant": variant, "lookup_id": key,
                    "source_id": source_id, "name": name, "markup": description,
                    "source_file": relative, "source_line": lines[key],
                    "placeholder": not name or "<Item does not exist>" in description,
                }
        names_category = "pills" if category == "horsepills" else category
        names, name_lines = read_table(source / names_file, f"local {names_category}", name_prefixes[category])
        for key, row in merged.items():
            name_key = key - 1 if category in {"pills", "horsepills"} and key != 9999 else key
            if name_key in names and not row["placeholder"]:
                if not isinstance(names[name_key], str):
                    raise ParseError("expected a literal item name")
                row["name"] = names[name_key]
                row["name_source_file"] = names_file
                row["name_source_line"] = name_lines[name_key]
        for (balance_category, balance_id), (old, new) in BALANCE_TEXT.items():
            if balance_category != category:
                continue
            row = merged[balance_id]
            if row["markup"].count(old) != 1:
                raise ParseError(f"Balance text for {balance_id} no longer matches its source")
            row["markup"] = row["markup"].replace(old, new)
            row["balance_text"] = True
        collected.extend(merged.values())
    collected.sort(key=lambda row: (row["variant"], row["lookup_id"]))
    return {
        "format": 1, "source_url": SOURCE_URL, "source_commit": PINNED_COMMIT,
        "language": "en_us", "source_layers": ["ab+", "rep"],
        "balance_text": {f"{c}:{i}": {"from": o, "to": n} for (c, i), (o, n) in BALANCE_TEXT.items()},
        "target_game": "Repentance 1.7.9b", "license": "No upstream license file found at pinned revision; see THIRD_PARTY_NOTICES.md",
        "source_sha256": source_files,
        "counts": {category: sum(row["category"] == category for row in collected) for category in CATEGORIES},
        "entries": collected,
    }


def cpp_string(value: str) -> str:
    # Fixed-width octal UTF-8 escapes prevent source-encoding and hex-greed issues.
    escaped = []
    for byte in value.encode("utf-8"):
        if byte == 34:
            escaped.append('\\"')
        elif byte == 92:
            escaped.append("\\\\")
        elif byte == 63:
            # Avoid legacy trigraph diagnostics for actual Isaac names such as
            # ???'s Soul, without changing the stored question-mark bytes.
            escaped.append("\\?")
        elif 32 <= byte < 127:
            escaped.append(chr(byte))
        else:
            escaped.append(f"\\{byte:03o}")
    return '"' + "".join(escaped) + '\\000"'


def write_outputs(payload: dict, output: Path):
    output.mkdir(parents=True, exist_ok=True)
    (output / "descriptions.json").write_text(json.dumps(payload, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
    offsets, strings, pool_size, index = {}, [], 0, []
    for row in payload["entries"]:
        pair = []
        for key in ("name", "markup"):
            value = row[key]
            if value not in offsets:
                offsets[value] = pool_size
                pool_size += len(value.encode("utf-8")) + 1
                strings.append(cpp_string(value))
            pair.append(offsets[value])
        index.append(f"    {{{row['variant']}, {row['lookup_id']}, {pair[0]}, {pair[1]}, {1 if row['placeholder'] else 0}}},")
    source = [
        "// Generated by tools/eid/import_eid.py. Do not edit.",
        f"// EID source commit: {PINNED_COMMIT}",
        "// Description text belongs to EID contributors; see eid/THIRD_PARTY_NOTICES.md.",
        '#include "catalogue.hpp"', "", "namespace isaac_eid {", "namespace {",
        "const char kStrings[] =", *["    " + string for string in strings], ";",
        "const EntryIndex kEntries[] = {", *index, "};", "} // namespace",
        "", "Catalogue EnglishRepentance() {", "    return {kEntries, sizeof(kEntries) / sizeof(kEntries[0]), kStrings, sizeof(kStrings)};", "}",
        f'const char* SourceCommit() {{ return "{PINNED_COMMIT}"; }}', "} // namespace isaac_eid", "",
    ]
    (output / "descriptions.cpp").write_text("\n".join(source), encoding="ascii", newline="\n")
    tokens = {}
    for row in payload["entries"]:
        for token in re.findall(r"\{\{(.*?)\}\}", row["markup"]):
            tokens[token] = tokens.get(token, 0) + 1
    (output / "markup-inventory.json").write_text(json.dumps(dict(sorted(tokens.items())), indent=2) + "\n", encoding="utf-8", newline="\n")
    (output / "data-build.json").write_text(json.dumps({
        "source_commit": PINNED_COMMIT, "counts": payload["counts"],
        "entries": len(index), "string_pool_bytes": pool_size + 1,
        "placeholder_entries": sum(row["placeholder"] for row in payload["entries"]),
        "unique_markup_tokens": len(tokens),
        "files": {name: hashlib.sha256((output / name).read_bytes()).hexdigest() for name in ["descriptions.json", "descriptions.cpp", "markup-inventory.json"]},
    }, indent=2) + "\n", encoding="utf-8", newline="\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    payload = load_entries(args.source.resolve())
    write_outputs(payload, args.output.resolve())
    print(json.dumps({"source_commit": PINNED_COMMIT, "counts": payload["counts"], "output": str(args.output.resolve())}))


if __name__ == "__main__":
    main()

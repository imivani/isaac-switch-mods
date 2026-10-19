#!/usr/bin/env python3
"""Data-import regressions; never opens or executes a game binary."""
import hashlib
import json
from pathlib import Path
import tempfile
import unittest

import import_eid

PROJECT = Path(__file__).resolve().parents[2]
SOURCE = PROJECT / "reference" / "External-Item-Descriptions"
GENERATED = PROJECT / "eid" / "generated"


class ImportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.payload = import_eid.load_entries(SOURCE)
        cls.rows = {(row["category"], row["lookup_id"]): row for row in cls.payload["entries"]}

    def test_complete_category_ranges_and_special_pill_key(self):
        expected = {"collectibles": 732, "trinkets": 189, "cards": 97, "pills": 51, "horsepills": 51}
        self.assertEqual(self.payload["counts"], expected)
        for category, count in expected.items():
            expected_ids = set(range(1, count + 1))
            if category in {"pills", "horsepills"}:
                expected_ids.remove(51)
                expected_ids.add(9999)
            self.assertEqual({key for kind, key in self.rows if kind == category}, expected_ids)
        self.assertEqual(len(self.rows), len(self.payload["entries"]))

    def test_repentance_overrides_replace_base(self):
        mirror = self.rows["collectibles", 5]
        self.assertEqual(mirror["source_file"], "descriptions/rep/en_us.lua")
        self.assertIn("+1.5 Damage", mirror["markup"])
        self.assertIn("-1 Luck", mirror["markup"])
        onion = self.rows["collectibles", 1]
        self.assertEqual(onion["source_file"], "descriptions/ab+/en_us.lua")
        self.assertEqual(onion["markup"], "↑ {{Tears}} +0.7 Tears")

    def test_pill_effect_and_color_are_not_confused(self):
        bombs = self.rows["pills", 4]
        self.assertEqual(bombs["source_id"], "3")
        self.assertEqual(bombs["name"], "Bombs are Key")
        self.assertIn("Golden bombs and keys", bombs["markup"])
        steel = self.rows["horsepills", 3]
        self.assertEqual(steel["source_id"], "2")
        self.assertIn("+4 Soul Hearts", steel["markup"])
        for category in ("pills", "horsepills"):
            self.assertEqual(self.rows[category, 9999]["name"], "Golden Pill")
            self.assertEqual(self.rows[category, 9999]["source_id"], "")

    def test_placeholder_slots_are_marked(self):
        self.assertTrue(self.rows["collectibles", 718]["placeholder"])
        self.assertFalse(self.rows["collectibles", 732]["placeholder"])
        self.assertEqual(sum(row["placeholder"] for row in self.rows.values()), 12)

    def test_markup_and_text_remain_verbatim(self):
        self.assertIn('{{', self.rows["collectibles", 732]["markup"])
        self.assertIn('#', self.rows["collectibles", 732]["markup"])
        self.assertIn('"helpful"', self.rows["cards", 44]["markup"])
        self.assertEqual(self.rows["cards", 97]["name"], "Soul of Jacob and Esau")
        self.assertEqual(self.rows["pills", 28]["name"], "R U A Wizard?")
        self.assertEqual(self.rows["pills", 28]["name_source_file"], "descriptions/names/en_us.lua")

    def test_rebuild_matches_checked_outputs(self):
        with tempfile.TemporaryDirectory(prefix="isaac-eid-test-") as temporary:
            output = Path(temporary)
            import_eid.write_outputs(self.payload, output)
            for name in ("descriptions.cpp", "descriptions.json", "markup-inventory.json", "data-build.json"):
                self.assertEqual((output / name).read_bytes(), (GENERATED / name).read_bytes(), name)
        manifest = json.loads((GENERATED / "data-build.json").read_text())
        for name, sha256 in manifest["files"].items():
            self.assertEqual(hashlib.sha256((GENERATED / name).read_bytes()).hexdigest(), sha256)

    def test_lua_reader_preserves_escaped_text(self):
        reader = import_eid.LuaLiterals(r'''{-- line
            [4] = {"3", "Bob's \"thing\"", "↑ {{Tears}} A#B\nC"},
            --[[ { bogus comment } ]]
        }''')
        value = reader.table()
        self.assertEqual(value[4][2], 'Bob\'s "thing"')
        self.assertEqual(value[4][3], "↑ {{Tears}} A#B\nC")

    def test_unknown_lua_code_is_rejected(self):
        with self.assertRaises(import_eid.ParseError):
            import_eid.LuaLiterals('{[1] = DangerousCall()}').table()
        with self.assertRaises(import_eid.ParseError):
            import_eid.LuaLiterals('{[1] = "a", [1] = "b"}').table()
        with self.assertRaises(import_eid.ParseError):
            import_eid.LuaLiterals('{[1] = "unfinished}').table()

    def test_cpp_strings_do_not_form_trigraphs(self):
        literal = import_eid.cpp_string("???'s Soul / ???/Void")
        self.assertNotIn("??", literal)
        self.assertIn(r"\?\?\?'s Soul", literal)


if __name__ == "__main__":
    unittest.main(verbosity=2)

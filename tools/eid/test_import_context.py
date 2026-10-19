#!/usr/bin/env python3
import contextlib
import io
import json
from pathlib import Path
import shutil
import tempfile
import unittest
import import_context as module


class ContextImportTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.data = json.loads((module.OUT / "context.json").read_text(encoding="utf-8"))

    def test_literal_parser_never_executes_lua(self):
        with self.assertRaises(module.ParseError): module.DataReader('{ [1] = os.execute("anything") }').table()
        with self.assertRaises(module.ParseError): module.DataReader('{ [1] = "first", [1] = "second" }').table()
        self.assertEqual(module.DataReader('{ t={1,2}, goldenOnly=true, mult=1.5 }').table(),
                         {"t":{1:1,2:2},"goldenOnly":True,"mult":1.5})

    def test_reproducible_from_pinned_source(self):
        original = module.OUT
        with tempfile.TemporaryDirectory(prefix="isaac-eid-context-") as folder:
            module.OUT = Path(folder)
            try:
                shutil.copyfile(original / "descriptions.json", module.OUT / "descriptions.json")
                with contextlib.redirect_stdout(io.StringIO()): module.main()
                for name in ("context.json","context.hpp"):
                    self.assertEqual((module.OUT/name).read_bytes(),(original/name).read_bytes())
            finally: module.OUT = original

    def test_target_item_metadata_does_not_collide_with_trinkets(self):
        items = {row["id"]:row for row in self.data["metadata"]}
        self.assertEqual(items[1]["quality"],3) # Sad Onion; trinket1 has quality0.
        self.assertEqual(items[145]["quality"],2)
        self.assertTrue(items[145]["forms"] & 1) # Guppy's Head, not trinket145.
        self.assertEqual(items[164]["charge_type"],1)
        self.assertEqual(items[164]["charges"],110) # Game frames, never110seconds.
        self.assertEqual(len(items),721)

    def test_complete_room_and_character_tables(self):
        groups = self.data["groups"]
        for name,count in (("sacrifice",12),("dice",6),("birthright",41),("character",41)):
            self.assertEqual({row["id"] for row in groups[name]},set(range(1,count+1)))
        self.assertEqual(self.data["source_commit"],module.PINNED_COMMIT)
        pockets = {(row["category"],row["id"]):row for row in self.data["pockets"]}
        self.assertEqual(pockets[300,33]["mimic"],12) # Jera is a rune XML node.
        self.assertTrue(pockets[300,33]["rune"])
        self.assertEqual({item for category,item in pockets if category==300},set(range(1,98)))

    def test_numeric_modifiers_preserve_inline_item_ids(self):
        text = module.gold_variant("{{Collectible2}} Gives 2 hearts",2,None,True,False)
        self.assertIn("{{Collectible2}}",text)
        self.assertIn("{{ColorGold}}4{{CR}} hearts",text)
        text = module.modified("1 damage and 10 hearts",{1:1,2:2},"ColorYellow")
        self.assertIn("{{ColorYellow}}2{{CR}} damage and 10 hearts",text)

    def test_custom_gold_multiplier(self):
        def get(group): return next(row["markup"] for row in self.data["groups"][group] if row["id"]==2)
        self.assertIn("60",get("golden")); self.assertIn("70",get("goldenbox"))
        self.assertIn("60",get("momsbox"))

    def test_character_and_greed_registrations(self):
        rows = self.data["conditions"]
        self.assertEqual(len(self.data["condition_registrations"]),53)
        self.assertEqual(len(rows),233)
        lost = [row for row in rows if row["category"]==100 and row["id"]==108 and row["operation"]==1]
        self.assertTrue(any(row["players"] & (1<<10) and row["players"] & (1<<31) for row in lost))
        self.assertTrue(any(row["category"]==300 and row["id"]==19 and row["greed"] and "first room" in row["new"] for row in rows))


if __name__ == "__main__": unittest.main()

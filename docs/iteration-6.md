# Iteration 6: Repentance+ balance (data only)

Built on the confirmed-working iteration 5 (`combined-plus`, module
`fdaa2c9e…`). Adds the Repentance+ item buffs and nerfs that are plain data the
Switch Repentance 1.7.9b game already reads. No new item abilities, no
reworks. Full list: [balance-changes.md](balance-changes.md).

## Source of truth

The values come from the user's installed PC Repentance+ (Steam build
22878971, `v1.9.7.17.J460`). Its data was unpacked locally with the game's own
`tools/ResourceExtractor` into a temporary folder; `afterbirthp.a` is the last
layer and holds the current `items.xml`, `items_metadata.xml`, `itempools.xml`
and `pocketitems.xml`. Copies are kept in `reference/pc-repentance-plus` for
local comparison only and are never packaged. The PC install was not changed.

## What changes

New files in `romfs/rp_patch/resources` (the Repentance data layer):
`items.xml`, `items_metadata.xml`, `itempools.xml`, `pocketitems.xml`. Each is
the original Switch file with single-element edits; comments, formatting,
line endings and every other attribute are byte-identical.

- **Item effects/prices (16 fields):** Void 4 charges; Jupiter heals 2 hearts;
  Revelation without its 2 soul hearts; Butter Bean 2-second recharge; The Bean
  and Kidney Bean recharge after 4 seconds; devil prices for Book of
  Revelations, Star of Bethlehem, Holy Light, Trisagion (2), Trinity Shield and
  Ocular Rift (1); cheaper D12 and Pause in the Greed shop.
- **Quality:** all 143 differences, including the 75 originally requested.
- **Bag of Crafting quality overrides:** 22.
- **Tags:** 64 items/trinkets, e.g. Lil Delirium counts toward Conjoined,
  Seraphim toward Seraphim, several items unobtainable in Greed Mode.
- **Item pools:** 105 edits, e.g. Devil pool loses Brother Bobby and Sister
  Maggy and gains Dry Baby; Greed Mode pools, Baby Shop, Key Master, Bomb Bum.
- **Cards/runes/pills:** Blank Card/Clear Rune/Placebo charge costs for four,
  Amnesia and ??? pills no longer in Greed Mode.

## Deliberately not applied

- Mega Bean's 6 charges (its stronger fart is code), D Infinity's charge
  number (a PC display fix), Milk!'s persistence (rework), stat-refresh flags,
  Portable Slot's removal from Shop/Beggar (its replacement source is code),
  and the `devilsacrifice` tag (unknown to the Switch tag parser at 0x3b7650,
  decompiled in `evidence/balance/native-tags`).
- `players.xml`: Repentance+ moved starting items into code; copying it would
  remove Cain's, Eve's, Samson's, Lazarus's, Lilith's and Bethany's items.
- Wisp, challenge and costume tweaks; all behavior reworks.
- Known side effect: Repentance+ also made Book of Virtues wisps from The Bean
  and Kidney Bean expire between rooms (code). Here they last as usual, so
  that combination makes more wisps than on PC.

## Descriptions and Specialist

- EID's compiled ratings, charges and transformation icons are imported from
  the new files (`tools/eid/import_context.py`), so descriptions match play.
- Exact text edits: Jupiter "Heals 2 hearts"; Revelation no "+2 Soul Hearts".
- Timed actives show their real recharge: "Recharges in 2 seconds" (30 charge
  units per second, confirmed by Butter Bean's 90 → 60 for 3 → 2 seconds).
- Specialist's quality-4 list: Rock Bottom joins; Satanic Bible, Void and
  Abyss leave.

## Saves

The game reads these files at startup. Finish or abandon the current run
before installing and start a new one afterward: an in-progress run's item
pools were built with the old data.

## Tests (local, not the Switch)

- `tools/balance/run_tests.py`: 28 checks — byte-identical regeneration,
  every changed field equals PC and every other field equals Switch, pools,
  tag list decoded from the native parser, EID metadata/text and Specialist
  derived from the same data.
- EID import, catalogue, context, reminder suites; Specialist asset and
  adapter suites; all iteration-5 and earlier suites re-run.

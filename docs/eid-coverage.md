# EID expansion coverage — Repentance 1.7.9b

This is the coverage of the expanded local implementation, not a claim that the
whole PC mod has been recreated. The previous item/trinket display was confirmed
working by the user on the Switch. The additions below have source and portable
test verification. On October 14, 2026, the user confirmed that sacrifice-room
descriptions appear and advance after use; the other new gameplay cases still
need individual console checks.

Source: [External Item Descriptions at the pinned revision](https://github.com/wofsauge/External-Item-Descriptions/tree/8b59d00c6828e333294639ad2b2b6875eae821cb).
The source and exact dumped game are standard Repentance, not Repentance+ or
REPENTOGON. The source checkout remains unchanged. The private Ghidra copy was
used read-only; original game files and the SD card were not modified here.

## What the expanded code does

Walk near a supported object to see its description. The nearest living main
player supplies the character/item context. Hold Map to see held items, and use
shoot-left/right while holding Map to browse. A selected Flip or Glitched Crown
preview gets priority over the held-item display when its own controls apply.
The controls use the game's action bindings, so remapped controls follow those
actions. Inputs remain available to the game, as in the PC default.

| PC EID feature family | Local status | Exact behavior or remaining gap |
|---|---|---|
| Collectible and trinket descriptions | Implemented | Pinned English tables; nearest visible pickup; native item pictures. |
| Hidden pedestals and Curse of the Blind | Implemented | Forced-blind flag, question-mark artwork and native curse checks remain required. Unknown fields suppress identity. |
| Sacrifice rooms | Implemented; basic console check passed | User confirmed text appears and advances after use on October 14, 2026. Reads the real spike payout count. Shows the next step, capped at12; preserves upstream odds and conditional first-floor bomb text. No invented counter or save file. |
| Sanguine Bond spikes | Implemented | Shows the source's payout probabilities. Optional exact-next-result RNG prediction is absent. |
| Dice rooms | Implemented | All six dice effects, selected from the visible room effect. |
| Card Reading portals | Implemented | Destination descriptions for the three standard Repentance portal colors. |
| Broken Shovel pickup | Implemented | Shows the first shovel-piece description while its pickup animation is Idle. |
| Crane Game prize | Implemented | Uses the native prize ID, visible prize artwork, curse and exhausted/broken-animation guards. |
| Cards, runes and soul stones | Implemented | All97 catalogue IDs; ordinary shop/Options? cards stay concealed. Standard source defaults permit obstructed/shop soul stones. Cantripped cards map to their visible item effects. |
| Normal and horse pills | Implemented | Color is resolved through the native pill pool only after the native identification byte allows it. Unknown pills say only “Unidentified pill.” Golden pills show their generic randomized behavior. |
| Pickup reachability | Conservative implementation | Bounded room-grid search for ordinary cards/pills; solid tiles, pits and active spikes obstruct walking. The native flight check bypasses this as the source default does. Complex path/physics equivalence still needs live checks. |
| Quality | Implemented | Target game's item metadata; original EID quality icons. Item and trinket IDs are kept separate. |
| Recharge information | Partial | Room charges, timed recharge and special-charge labels. Dynamic current charge counts, battery bonuses and every charge modifier are not reproduced. |
| Transformations | Partial | Item transformation tags, original icons and actual game counters. Full transformation preview, completion notices and every special counting exception are absent. |
| Birthright | Implemented | Replaces generic text with the nearest player's actual character effect. Co-op alternate-character explanatory panels are absent. |
| Golden trinkets and Mom's Box | Implemented table subset | Applies the pinned standard-Repentance rules, including custom multipliers and replacement text. No Repentance+ overrides. |
| Tarot Cloth, Car Battery | Implemented table subset | Uses31 Tarot and90 Car Battery descriptions. Reverse synergy summaries on the enhancer's own pedestal and every conditional exception are absent. |
| False PHD / Experimental Pill | Implemented core additions | Native effect resolution plus source stat-down damage/black-heart and experimental-pill notes. Co-op different-effect lists are absent. |
| Blank Card / Clear Rune / Placebo | Implemented charge lookup | Target pocket metadata includes separate rune XML nodes. Matching held copy item adds charge information; Tarot+Blank Card notes included. |
| Binge Eater | Partial | Eight food-stat additions. Complete healing/character interaction and reverse synergy summaries remain. |
| Character and Greed-mode conditions | Partial, explicit subset |53 literal source registrations, expanded to233 operations, including Lost/Keeper/no-red-health warnings and selected character changes. Evaluated for the nearest player. This is not all226 source registrations or all dynamic health changes. |
| Spindown Dice | Partial | Three successive results, hidden/missing ID skips, native unlock markings, Car Battery double steps, Dad's Note stop. Full hold-Map result browsing and absorbed-Void ownership are absent. |
| Flip | Special adapter | Reads the already-created alternate pedestal state without invoking Flip or RNG. Empty pedestals require actual Flip ownership and a verified alternate; see special adapter report for controls and remaining cases. |
| Glitched Crown | Special adapter | Reads the native current/alternate cycle; see special adapter report for browsing and safety limits. |
| Tainted Cain salvage | Special adapter | Source-derived contextual salvage information with target-game unlock checks; see special adapter report. |
| Bag of Crafting current result | Partial | Reads the game's cached output from a full bag; first held-Map page when available. Does not call the mutating recipe path. |
| Bag of Crafting full recipe list | Missing | No floor pickup tracking, combination search, probabilistic recipes, learned-recipe history, sort/search or recipe browser. The PC default is the recipe list; current-result text is only a subset. |
| Held-item reminder | Partial | Character, active slots, pocket cards/pills, two equipped trinkets and owned passive/familiar items; flat left/right browsing. No PC overview/category navigation, gulped-trinket list, wisps, absorbed Void items, recent-item ordering or persistent reminder preferences. |
| General item synergies and live values | Missing beyond listed subsets | Most function-valued Lua conditions, luck-based chances, exact current damage/stat deltas, inventory-specific effects and co-op differences remain. Static table import alone does not implement them. |
| Other item-specific predictions | Missing / optional in PC | Teleport destinations, Void stat forecasts, exact Sanguine result, Wild Card/Stars?/Pandora history and other RNG/history previews are not generally available. |
| General entity, grid and custom-mod descriptions | Missing | Only the explicitly listed native objects are supported. No arbitrary Lua/mod registration API. |
| TMTRAINER/glitched item descriptions | Missing, PC default off | Generated effect analysis is not ported; unrecognized item IDs do not show invented descriptions. |
| EID configuration/localization ecosystem | Missing | English preset, no Mod Config Menu, external Lua mod integrations, PC keyboard shortcuts, language selection or per-user save configuration. |
| Repentance+ / REPENTOGON features | Inapplicable to this target | No unsupported platform APIs or newer-game features are claimed. |

## Evidence and verification

`docs/eid-native-context.md` maps each new native field/function to saved evidence.
`eid/generated/context.json` contains text groups, target metadata hashes, all53
imported registrations,233 expanded operations and source hashes. The broader
read-only audit is `docs/eid-feature-audit.md`.

- 16 Python import checks passed, including byte-for-byte rebuild from the pinned
  source, rejected executable Lua, item/trinket metadata separation, rune charge
  metadata and golden multiplier rules.
- `eid/context-test-results.json`: optimized and UBSan builds each passed 3,557
  constructed descriptions and 7,322 measured layouts, plus focused sacrifice,
  hidden-pill, empty-Flip visibility, room, modifier and reachability cases.
- `eid_ui/test-results.json`: optimized and UBSan builds each passed all 1,108
  usable catalogue descriptions, wrapping, colors, UTF-8 and bounds.
- `eid_ui/asset-check-results.json`: 99 glyphs and 281 icon references checked
  against original font metrics and valid source texture crops. All referenced
  local inputs are hashed; original game controller animation is recorded
  separately and is not repackaged.

The parent task owns the final combined cross-compile, package verification,
installation and hardware check. These desktop results do not prove those
steps happened. Source/data were frozen after these focused checks; later
changes must be reflected in the corresponding report hashes.

## Rebuild these owned parts

```text
python tools/eid/import_context.py
python eid_ui/generate_assets.py
python -m unittest discover -s tools/eid -p "test_import*.py"
python eid_ui/check_assets.py
python3 tools/eid/run_context_tests.py
python3 eid_ui/run_tests.py
```

The two final commands require the host C++ compiler. In this workspace they
ran in the existing IsaacDevkit WSL distribution with GCC 12.2. The generator
never executes Lua or game code. No additional file permissions, network
requests, persistent console files or public uploads are introduced.

# Native External Item Descriptions work

The EID portion now has real upstream English description data and a small C++
library for selecting a nearby pickup and deciding what information can be
shown. The user confirmed the live description overlay works on the Switch.
It does not replace item pictures. The polished renderer is installed, using
the upstream font, icons, colors and wrapped bullet points. The user confirmed
that normal item pictures and the new description look good on the Switch.
The older picture-based description mod has been backed up and removed from
active locations. This checks the ordinary item appearance, not all EID cases.

The first installed integration covered collectibles and trinkets. The current
source adds native rooms, cards/pills, held-item reminders and selected live
conditions. Its exact supported and missing families are in
[eid-coverage.md](eid-coverage.md), with the newly verified native inputs in
[eid-native-context.md](eid-native-context.md). The older catalogue notes below
describe the original foundation. Antibirth music is not part of these files.

## Source and coverage

The source checkout is pinned to
[`8b59d00c6828e333294639ad2b2b6875eae821cb`](https://github.com/wofsauge/External-Item-Descriptions/tree/8b59d00c6828e333294639ad2b2b6875eae821cb).
It identifies itself as EID 5.25. The import combines the AB+ base tables and
standard Repentance changes, then applies the separate English item-name list.
Repentance+ and REPENTOGON data are excluded because the dumped Switch game is
standard Repentance 1.7.9b.

| Category | Imported slots | Lookup identity |
| --- | ---: | --- |
| Collectibles | 732 | Native collectible ID |
| Trinkets | 189 | Native trinket ID with golden flag removed |
| Cards and runes | 97 | Native card subtype |
| Pills | 51 | Resolved effect + 1; golden pill uses 9999 |
| Horse pills | 51 | Same effect key, separate internal category |

There are 1,120 table entries, including 12 explicitly flagged unused slots.
The C++ lookup hides those unused slots. These are description slots, not a claim
that the game contains 1,120 usable items. Original markup, line breaks, and
placeholder variables are retained. Every JSON record includes its upstream
source file and line.

`eid/generated/data-build.json` records counts, byte size and hashes.
`eid/generated/markup-inventory.json` lists the 177 distinct inline directives
used by these tables. Read `eid/THIRD_PARTY_NOTICES.md` and the retained upstream
credits for attribution and license status.

## Implemented files

- `tools/eid/import_eid.py`: strict data-only Lua-table reader and generator.
  It requires the pinned Git revision and unchanged imported source files.
  It never executes Lua. Unsupported syntax fails the import instead of
  silently dropping rows.
- `eid/generated/descriptions.json`: reviewable English data and provenance.
- `eid/generated/descriptions.cpp`: read-only string pool and sorted index for
  a native module, requiring no Lua, JSON parser, dynamic allocation or STL.
- `eid/catalogue.hpp`: binary-search lookup, preserving the pill identity
  convention and rejecting unknown IDs and unused slots.
- `eid/markup.hpp`: bounded token reader returning ordinary text, line breaks,
  inline directives and substitution variables. Unknown directives are kept.
- `eid/pickup_policy.hpp`: plain data snapshots, visibility decisions and
  nearest eligible pickup selection, with no hard-coded memory addresses.
- `tools/eid/test_import_eid.py`: import, identity, escape, source-layer,
  placeholder and reproducibility checks.
- `eid/tests/portable_test.cpp`: C++ catalogue, markup, visibility and nearest
  pickup regressions. `tools/eid/run_cpp_tests.py` builds and runs them.

## Integration contract

1. Take a bounded snapshot of the current room and player on the game's own
   update/render thread. Discard it after that frame. Do not retain raw pickup
   pointers across room changes.
2. Copy only live and visible pickup positions and identities. Use native
   validated game functions or confirmed fields to populate the knowledge
   values. `Knowledge::Unknown` is different from `Knowledge::No`.
3. For collectibles, set the run's effective Curse of the Blind state and the
   pickup's complete hidden-identity state. The latter must include forced
   blind and alternate-path question-mark artwork. A clear forced-blind byte
   alone is not sufficient.
4. For trinkets, pass `TrinketLookupId(rawSubtype)`. For pills, ask the native
   ItemPool for the actual effect for this player and check native identification
   before calling `PillLookupId(effect)`. Never look up a pill by color.
5. Call `SelectNearest(...)` with room-coordinate radius. Unknown/unsupported
   descriptions cannot win selection. The returned strings refer to static
   data. The pickup pointer refers to the caller's current-frame snapshot.
6. Render the title and description at the overlay position. Run the markup
   through `MarkupReader`; a renderer must implement font measurement, wrapping,
   symbols, color changes and conditional substitutions. The tokenizer does not
   draw these elements by itself.

The installed adapter is `native/source/program/eid_ui.cpp`, with measured
layout in `eid_ui/layout.hpp`. It supplies those visual elements using the
verified native sprite interface. Long descriptions rotate between pages rather
than silently losing their final lines. See `eid_ui/README.md` for limits and
the source-art preview. All 1,108 usable descriptions passed its layout tests.

Visibility policy follows the current upstream defaults in
[`main.lua`](https://github.com/wofsauge/External-Item-Descriptions/blob/8b59d00c6828e333294639ad2b2b6875eae821cb/main.lua)
and
[`eid_config.lua`](https://github.com/wofsauge/External-Item-Descriptions/blob/8b59d00c6828e333294639ad2b2b6875eae821cb/eid_config.lua):
collectibles respect blind state; ordinary shop and Options? cards stay hidden;
Soul Stones have the shop/obstruction exception; pills may appear in shops and
Options? choices, but their effects stay unidentified until known. Unreadable
native state is suppressed. `Touched` is not a universal identity flag.

The expanded adapter selects the nearest main co-op player and enables supported
ordinary challenges, including Cantripped's card mapping. April's Fool and
unknown alternate challenge state remain suppressed. Every PC question-mark
placeholder and object priority rule is not reproduced; see the coverage list.

## Native evidence boundary

The iOS bridge supports an ARM64 1.7.9b game and helped identify useful functions
to inspect. Its addresses must not be copied into the Switch module. A concrete
difference is the animation-layer stride: the Switch analysis finds `0xb0`,
while the iOS bridge uses `0x90`. The field at pickup `+0x56c` is used as a shop
slot in the Switch `MakeShopItem` function; its use as an Options? index has not
been established. Card selection therefore cannot treat that field as proven.
See `native-api.md` for the separately maintained Switch evidence.

## Remaining features before full EID parity

The expansion now implements Birthright, quality artwork, room and pocket
descriptions, transformation counters, basic charge information, held inventory,
Spindown results and selected owned-item/character conditions. The full PC
feature set is still incomplete: most dynamic conditional logic, the full Bag
of Crafting recipe browser, complete reminder categories, Lua/mod integrations,
configuration menus and additional languages remain. The authoritative detailed
status is [eid-coverage.md](eid-coverage.md); practical controls are in
[eid-controls.md](eid-controls.md). These are explicit subsets, not full parity.

## Rebuild and verify

Run from the project directory with Python 3.11 or newer:

```powershell
python tools/eid/import_eid.py reference/External-Item-Descriptions eid/generated
python tools/eid/test_import_eid.py
```

The Python suite passed all nine checks on 2026-10-13. The host C++17 test passed
with GCC 12.2.0, warnings treated as errors, and separately with
UndefinedBehaviorSanitizer. The latest bounded AddressSanitizer plus UBSan run
also passed. One earlier captured ASan repeat produced a non-diagnostic
`DEADLYSIGNAL` loop and was stopped; this intermittent result is retained in
`eid/generated/cpp-asan-prior-run.json`, so it is not described as a consistently
clean sanitizer history. The runner now limits both runtime and diagnostic
output. The tests check every usable table entry and consume
every markup byte, as well as invalid IDs, unused slots, hidden pickups, nearest
selection, shop/choice cards and identified/unidentified pills. The compiler
initially rejected question-mark sequences as legacy trigraph warnings; the
generator now escapes question marks while preserving their actual text.

To reproduce both C++ test builds in a host environment with GCC and Python:

```text
python3 tools/eid/run_cpp_tests.py --ubsan
python3 tools/eid/run_cpp_tests.py --sanitize
```

The runner writes `eid/generated/cpp-test-results.json` and the separate
`cpp-asan-results.json` with commands, outputs, source hashes and results.
For a single manual build:

```text
g++ -std=c++17 -O2 -Wall -Wextra -Werror -I eid eid/tests/portable_test.cpp eid/generated/descriptions.cpp -o eid/test-build/eid-portable-test
eid/test-build/eid-portable-test
```

Create `eid/test-build` first, and use `.exe` for a Windows host compiler. These
tests validate the portable library, not actual gameplay or Switch addresses.

# Flip, Glitched Crown and Tainted Cain

This adds three more kinds of useful item information to the native Switch
description display. The combined native build and offline checks pass.
These new cases have not yet been checked on the console.

## What the player sees

- **Flip:** the nearby pedestal shows the name and picture of the item on its
  other side. Hold **Map** for its full description; release Map to return.
  An empty pedestal with a valid other-side item shows that item's description
  automatically. Any local player owning Flip enables this information.
- **Glitched Crown:** a nearby cycling pedestal shows the available items.
  Press **Map** to step through their descriptions, then back to the overview.
  When the nearest player owns Spindown Dice, the page changes when Map is
  released. The selected page stays steady while the actual pedestal cycles.
- **Tainted Cain:** when he is the closest player, an ordinary pedestal shows
  what breaking it produces instead of effects he cannot collect. This includes
  Birthright, the room's guaranteed pickup, the five pickup-specific trinkets,
  Lucky Toe and Daemon's Tail. The display respects the native unlock checks for
  Bone Hearts, Cracked Keys and Rotten Hearts.

Curse of the Blind and the normal pickup visibility checks still apply before
any of these descriptions are selected. Quest items, the Death Certificate
dimension and Cantripped retain their ordinary descriptions. The preview reads
existing game values; it does not flip, reroll or break the pedestal.

When Flip and Glitched Crown share a pedestal, Crown's page controls take
priority. Flip's other-side name remains visible, but holding Map does not open
its full separate page in that combination. This is a known difference from
the PC mod's combined preview controls. Held Map near either feature also takes
priority over the general inventory reminder.

## Verified build and evidence

Target: the user's Repentance 1.7.9b NRO, SHA-256
`cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a`.
The enclosing loader accepts only this build before calling feature code.

| Behavior | Exact native evidence |
| --- | --- |
| Flip's saved item | `InitFlipState` at `0x26b6c8` stores a shared data pointer at pickup `+0x610`; `TryFlip` at `0x26b924` reads type, variant and subtype at saved data `+0/+4/+8`. The adjacent `+0x618` control pointer is never treated as item data. |
| Crown's choices | `Pickup::Update` at `0x26c9b8` rotates the alternate array at `+0x5e4`, count `+0x604`. Capacity is eight alternates plus the current item. |
| Room salvage | `SalvageCollectible` at `0x2dd374` supplies the pickup/unlock rules. It is inspected, never called. `GetPoolForRoom` at `0x3c5c7c` proves the normal/Greed tables; the Error room is excluded. |
| Owned items/trinkets | `HasCollectible` at `0x27d3f4` and `GetTrinketMultiplier` at `0x29b030` include native ownership behavior, including swallowed trinkets. |
| Saved unlocks | `PersistentGameData::Unlocked` at `0x439654`, object embedded at manager `+0x18`; unlock IDs 391, 415 and 411 gate the three pickup types. |
| Map controls | `IsActionPressed` at `0x3f9b6c`, `IsActionTriggered` at `0x3f9b7c`, action 13, controller at player `+0x19ec`. Native device IDs are 1–8 and 33 (handheld), verified by input polling at `0x4fbae8`. All validated local players are checked; mask bits use player slots, so device 33 cannot overflow a shift. |

Evidence is recorded in `evidence/eid-special/native-layout.json`, with source
hashes, symbol offsets and byte snapshots. The saved disassembly and focused
salvage review are beside that report. Flip and Crown layouts also received an
independent review by the agent handling boss hooks.

The code stores at most 32 pedestal identities and their known item IDs. It
does not retain item-object pointers for later dereferencing. Identity includes
the room, dimension, run/floor seeds and the entity's initialization seed.
Changed choice sets reset their page; a rewind or module reset clears the cache.
Input releases are accepted only for the same recently observed pedestal, so
walking away and returning cannot create a delayed false button release.

No new hook, texture, animation or shared build-file change is needed.
`native/source/program/eid_special.hpp` exposes the three calls already wired
by the EID owner: `ReadEidFlipItem`, `AugmentEidSpecial`, `ResetEidSpecial`.

## Offline checks and remaining cases

`eid_special/test-results.json` records passing optimized and undefined-behavior
sanitizer runs: **17,558 checks and 1,458 text layouts per run**. Checks cover
every known collectible's salvage/quest handling, room pools, locked-pickup
suppression, Birthright/trinkets, text bounds, invalid queue IDs/counts, queue
rotation, rerolls, cache eviction and Map press/release/rewind behavior.
`evidence/eid-special/native-layout.json` separately checks all twelve native
symbol addresses against the exact dumped game. These are offline results;
they do not verify console rendering or controller behavior.

Remaining limitations:

- Crown's page order follows the native queue when first seen, then stays
  stable. PC EID builds its order from repeated observations. The preview
  requires five distinct known items and an owner of Glitched Crown.
- The full PC held-Map Spindown preview and simultaneous Flip/Crown preview
  combinations are not reproduced. The selected item's ordinary Spindown
  results still appear, with native availability checked for that page.
- Procedurally generated negative item IDs and REPENTOGON's additional cycling
  sources are outside this standard 1.7.9b catalogue.
- The nearest player's salvage rules are used. PC EID's additional co-op
  different-effect notices and multiple simultaneous description panels are
  not added here.
- Selection is remembered for 32 recently inspected pedestals; older cached
  selections may return to the overview. No disk settings or new language
  selection controls are added.

Before a hardware pass is claimed, check a regular/empty Flip pedestal, a
Crown pedestal while cycling and rerolling, the combined items, Map from a
second player, Tainted Cain with and without Birthright/trinkets, a locked
salvage pickup, and the quest/Death Certificate/Cantripped exclusions.

## Source and attribution

The behavior and seven salvage text strings come from
[External Item Descriptions](https://github.com/wofsauge/External-Item-Descriptions/tree/8b59d00c6828e333294639ad2b2b6875eae821cb),
by wofsauge and contributors. The importer verifies both that commit and the
used source files, then reads their data without executing Lua. Fifteen room
and trinket rules and eleven quest IDs are recorded in
`eid_special/data-build.json`; quest tags come from the user's exact game dump.

`eid/THIRD_PARTY_NOTICES.md` and `eid/UPSTREAM-CREDITS.md` retain the upstream
attribution. That upstream revision contains no LICENSE or COPYING file; this
local port does not grant a new license over its source text. Nothing in this
scope was uploaded or published.

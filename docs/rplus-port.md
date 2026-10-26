# Repentance+ Rules (iteration 8)

Rule changes from the PC Repentance+ update that are **code**, not data, so
iteration 6's data-only balance pass could not include them. Every address
below was read from the user's verified 1.7.9b dump
(`Repentance.nro`, SHA-256 `cc363208…4b66a`), using its ELF symbols plus
Ghidra 12.1.4 decompilation. `tools/rplus/generate_guards.py` re-reads the
dump and pins every byte the mod depends on.

## What changes

| Rule | 1.7.9b (vanilla) | With the mod |
| --- | --- | --- |
| ??? and The Lamb | 1 in 5 chance of a Void portal | Always a portal |
| Mega Satan | 1 in 2: the run ends, otherwise a big chest plus a portal | Always the chest plus a portal; the chest still ends the run |
| Mother | No portal | A portal, once Ending 21 (Mother) has been seen |
| Boss Rush | No wave display | "Wave 3/15" under the hit counter line |

All portal rules still require The Void to be **available**, using the game's
own check (`Level::IsStageAvailable(The Void)`). That covers the unlock
(achievement 320), Greed Mode and challenges with an end floor. So a save
without The Void unlocked behaves exactly as before.

Each rule has its own switch: Mods → Repentance+ Rules → Y.

## How the game decides (1.7.9b)

All boss rewards come from `Room::SpawnClearAward` (`0x456810`, 10,836 bytes):

- The game works out an *award stage*: the floor number, plus 1 under Curse of
  the Labyrinth (curse bit 2) or challenge 44.
- **Stage 11 (Dark Room / Chest):** after the chest, it rolls
  `RNG::Random(5)` at `0x45918c`. A result of 0 (together with the challenge
  conditions) leads to the shared portal code, which also needs
  `IsStageAvailable(12, 0)`.
- **Mega Satan (room boss id 55, `room+0x1fc0`):** it rolls `RNG::Random(2)` at
  `0x4582c8`. A non-zero result calls `Game::End(8)` at `0x458594`, the Mega
  Satan ending. Zero spawns the big chest (pickup variant 340) and the portal,
  if The Void is available.
- **Mother:** this branch is chosen by room index -10 (`game+0x21558`), a boss
  room (`room+0x10 == 5`) and award stage 8. It records completion event 13 and
  spawns the big chest. It never spawns a portal. Room index -10 on award
  stage 6 is Mom in Mausoleum II instead.
- **The portal itself** is `Room::SpawnGridEntity(index, 17 /* trapdoor */,
  variant 0, seed, vardata 1)`. `GridEntity_TrapDoor::InitSubclass` loads
  `gfx/grid/VoidTrapdoor.anm2` for it. The index comes from
  `FindFreePickupSpawnPosition(…, 80, true, false)`, then
  `(x-40)/40+0.5 + width*((y-120)/40+0.5)`, then `fix_trapdoor_pos`.
- The other portal chances (Mom 1 in 20, Mom's Heart/It Lives 1 in 10,
  Isaac/Satan 15 in 100) are not changed by Repentance+ and are left alone.

`Room::TrySpawnTheVoidDoor` (`0x4723e0`) is only the door in Hush's room. It is
not involved.

## How the mod does it

- **Portal rolls.** A trampoline on `RNG::Random(unsigned)` (`0x44e3e8`) always
  calls the original first, so the seed advances exactly as in vanilla. It then
  returns 0 only when its return address is one of the two calls above
  (`0x459190` for ???/The Lamb, `0x4582cc` for Mega Satan) and that rule is on.
  Every other random number in the game is untouched.
- **Mother.** A trampoline on `Room::SpawnClearAward`:
  - before the award, it applies the same room test the game uses (room index
    -10, boss room, award stage 8), checks that The Void is available, and
    checks that cutscene 24 ("Mother", that is Ending 21) has been shown;
  - after the original runs, it spawns the portal exactly like the game's own
    portal code does.

  The cutscene counters are at `PersistentGameData + 0xe20 + id*4`, proven by
  `PersistentGameData::IncreaseCutsceneCounter`. `PersistentGameData` is at
  `Manager + 0x18`.
- **Boss Rush.** The ambush state lives at `game+0x26faa8`
  (`Room::Update` calls `Ambush::Update` there):
  - active byte at `+0x38`;
  - wave total at `+0x30`;
  - current wave at `+0x3c`, which `Ambush::Update` increments before each wave.

  Room type 17 is Boss Rush (11 is a challenge room, which also uses Ambush).

If any guard fails (a different build), the mod shows as unavailable and the
game keeps its own rules. Two trampolines are added (24 of the ~80 exlaunch
slots in use).

## Evidence and tools

- `tools/native-analysis/prepare_targets.py` produces `evidence/native-analysis/symbols.json` and `plt.tsv`.
- `tools/rplus/generate_guards.py` produces `generated/rplus_guards.hpp` and `evidence/rplus/guards.json`.
- `tools/rplus/run_tests.py` runs:
  - the rule decisions (`features/rplus/tests.cpp`);
  - the production `rplus.cpp` on a fake game (`tools/rplus/test_hook.cpp`);
  - a Switch syntax check when devkitA64 is installed.

## Not included

These are other Repentance+ code changes. They are listed by effort in
`docs/plans/iteration-8-2026-10-25/PLAN.md`:

- Mega Bean's room-wide fart;
- Rosary Bead deals;
- beggar deal chances;
- Missing Poster's Super Secret Room reveal;
- golden trinket effects;
- the item reworks;
- online features (not possible on Switch).

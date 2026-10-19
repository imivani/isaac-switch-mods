# Instant Fires and Poops

This ports the locally installed Workshop mod **2833146105, version 1.1** to the exact Switch Repentance 1.7.9b dump. In a clear room, one accepted hit puts out an ordinary orange fire. A hit that damages poop triggers the game's own destruction routine, keeping its usual drops and effects. Untouched objects, combat rooms and colored fires retain their normal behavior.

The existing game-update hook calls `UpdateInstantBreak(game)` after normal and accelerated ticks. The existing room-entry hook calls `ResetInstantBreakRoom()`. `InitializeInstantBreak(module)` verifies the target code before installing the separate NPC damage hook. No pictures, music, input shortcuts or save writes are added by this feature.

**102 focused adapter/lifecycle checks passed under AddressSanitizer and UndefinedBehaviorSanitizer**, and an isolated Switch compiler syntax check passed. These checks cover invalid pointers, replaced/regenerated objects, room changes, combat transitions and repeated calls, but do not execute the closed-source game or prove gameplay on hardware. The combined package and console test are separate steps.

The fire adaptation preserves the original damage call and only forces death after an accepted positive hit. Its callback ordering is therefore not identical to PC Lua. Tough/giant poop still uses native repeated damage on subsequent updates, as the original Lua's state loop does.

Exact source hashes, native call evidence, safety differences and full test boundaries are recorded in `evidence/instant_break/README.md`, `provenance.json`, `guards.json` and `verification.json`. Re-run `tools/instant_break/verify.py` to execute the isolated checks and refresh their source hashes.

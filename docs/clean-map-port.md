# Clean minimap and original boss-bar switching

The small minimap can now show its room boxes and icons without the dark square and thick outer frame. Turning **Clean Minimap** off brings that frame back immediately. The expanded map keeps its original drawing. This change does not replace map pictures, change map discovery, or affect Antibirth music.

The original boss bars can also return when **Enhanced Boss Bars** is turned off. The package must restore the three original boss-bar pictures; while the enhanced bars are enabled, a small wrapper skips only the original boss-bar drawing. It restores the game's stored health and bar-position values before the enhanced bars are drawn. Gideon's wave counter continues through the original game code.

These features are being integrated into the combined native pack. The automated checks below pass; a new Switch run is still needed to verify the installed result visually.

## Source identification

The requested name was “Better Overlay.” None of the nine installed Isaac Workshop mods has that name, and the bounded Steam search did not identify a page with that exact title. The primary Workshop pages that match the requested visual effect are [Better minimap by europa](https://steamcommunity.com/sharedfiles/filedetails/?id=834431160) and [[REP] Better Minimap](https://steamcommunity.com/sharedfiles/filedetails/?id=2534310479). Their descriptions discuss removing the thick border/background. Neither is installed locally.

This implementation reproduces the user's explicitly requested small-map effect using the game's existing drawing call. It does not claim to be a source port of an unidentified “Better Overlay” mod, and no Workshop image or Lua file was copied for this feature. Saved primary pages and the local texture inspection are under `evidence/iteration4/clean-map`.

## Small-map implementation

The exact supported Repentance NRO has SHA-256:

`cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a`

The native Minimap object is embedded at `game+0x24ee70`. Its small-map sprite is at `game+0x24ee88`. Within `Minimap::Render` at module offset `0x41d54c`, the sequence starting at `0x41d744` selects animation `Frame`, frame zero, then calls `ANM2::Render` at `0x41d79c`. Its original return address is `0x41d7a0`. Other room/icon and expanded-map draws have different callers.

`InitializeCleanMap(module)` verifies the original render prologue and the complete 92-byte frame setup/draw sequence. `ShouldHideCleanMapFrame(sprite, caller)` hides only that exact call on the current game's small-map sprite, and only while the setting is enabled. Unrelated sprite draws exit before reading settings or querying game memory.

The shared `ANM2::Render` callback passes its own `__builtin_return_address(0)` into the predicate before item-animation settings are checked. The hook implementation uses `B` or `LDR X17; BR X17` for entry, preserving the original link register. Capturing a return address inside the predicate itself would be incorrect. The shared callback belongs to `item_visuals.cpp`; this feature installs no second hook.

`ResetCleanMap()` owns no resource and intentionally retains its verified identity if SDK module unloading fails. A later initialization rechecks the newly mapped game code.

The original `minimap1.anm2` and `minimap2.anm2` files were parsed, and their physical PCX images were decoded and visually inspected. The small `Frame` is a separate 64-by-64 area containing the background/frame; map tiles and icons occupy other areas. The evidence PNGs are inspection outputs, not game replacements.

## Original boss-bar wrapper

`InitializeBossHudGate(module)` verifies the native health-field setter, negative-health branch, and complete early-return sequence. It installs no additional hook. Integration is:

```cpp
auto restore = isaac::BeginBossHud(hud);
Orig(hud);
isaac::EndBossHud(restore);
```

Only enable this gate together with the restored original resources. If initialization fails, mark Enhanced Boss Bars unavailable so the untouched stock bars remain visible. After the call, render enhanced bars only when their setting is enabled.

The game stores its HUD at `game+0xe8468`. The native boss-health value is `HUD+0xc53e8`; the bar-position byte is `HUD+0xc53f4`. At `0x3ae374`, the original HUD checks health. A negative value branches at `0x3ae380` to `0x3ae3b0`, which clears only that position byte and returns from the final boss-bar section. Earlier HUD drawing is already complete.

The wrapper checks current-game/HUD identity, field permissions, room metadata, and finite nonnegative health. It saves exact health bits and the complete position byte, sets the native hidden sentinel `-1.0f` during the one synchronous original draw, and restores both immediately afterward. There is no saved game pointer across frames and no health calculation or persistent game-state change. A nested call sees the hidden sentinel and does not overwrite the outer restoration token. Restoration still happens if the setting changes before the call returns.

Gideon's room boss ID `83` bypasses the wrapper. The original HUD's hidden boss IDs `55`/`88` and Beast-room test (room-config type `16`, variant `35`) also bypass it, retaining their native hiding side effects. Unknown or unreadable room data leaves original drawing alone.

The combined package must omit only these three transparent overrides from **both** resource layers, and the installer must remove their previously installed copies:

- `gfx/ui/ui_bosshealthbar.pcx`
- `gfx/ui/ui_bosshealthbar_mini.pcx`
- `gfx/ui/ui_bosshealthbar_static.pcx`

`ui_bosshealthbar_stone` is not part of that removal. Original assets then load from the user's game. No new boss-bar or minimap assets are required.

## Checks and remaining validation

- `features/clean_map/test-results.json`: **420 checks passed** with optimization and again with undefined-behavior checking. Tests compile the actual native predicate and cover live toggling, caller/object identity, expanded-map exclusion, guard mismatches, invalid memory, fresh game/module state, and unchanged game memory.
- `features/clean_map/boss-test-results.json`: **154 checks passed** in each mode. Tests compile the actual wrapper and cover original/enhanced toggling, exact restoration, Gideon/Beast exceptions, nested calls, permissions, malformed pointers, health edge cases, and guard failures.
- `evidence/iteration4/clean-map/resource-inspection.json`: exact NRO, instruction guards, original animation/texture hashes, decoded image dimensions, and generator input hashes.

Reproduce on the prepared development environment:

```text
python tools/clean_map/inspect_resources.py
wsl -d IsaacDevkit --exec /bin/bash -lc 'cd %USERPROFILE%/Desktop/sw/Isaac-switch-mods && python3 tools/clean_map/run_tests.py'
wsl -d IsaacDevkit --exec /bin/bash -lc 'cd %USERPROFILE%/Desktop/sw/Isaac-switch-mods && python3 tools/clean_map/run_boss_tests.py'
```

The host tests replace memory queries, settings, and the native original-draw boundary with bounded test versions. They do not execute the Switch game's renderer. The installed pack still needs visual checks for small/expanded maps, live toggles during an ordinary boss fight, Gideon's wave counter, and main-menu controls in both handheld and docked modes.

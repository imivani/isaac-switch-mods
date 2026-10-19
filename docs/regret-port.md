# Regret Pedestals behavior and native port

The requested mod briefly shows the actual item icon where an unchosen item disappeared. Its icon fades over 60 rendered frames and rises by 0.1 screen units per frame. It is useful for alternative-floor hidden choices, option items, voiding, and disappearing angel-deal choices. It does not keep reroll history.

## Upstream evidence

- Repository: <https://github.com/seanstertm/Isaac-Regret-Pedestals>
- Reviewed commit: `49b494d32681b20764139206527331525f3ee452`, 2022-03-05, metadata version 1.4.
- Behavior source: <https://github.com/seanstertm/Isaac-Regret-Pedestals/blob/49b494d32681b20764139206527331525f3ee452/main.lua>
- README: <https://github.com/seanstertm/Isaac-Regret-Pedestals/blob/49b494d32681b20764139206527331525f3ee452/README.md>
- No LICENSE file is present in this pinned checkout. This port implements the behavior independently; it does not include copied Lua or upstream artwork.

The source admits collectible pickups (entity type 5, variant 100) when their animation is `Idle` or `ShopIdle`. Its blind-item filter is commented out, so it tracks visible and hidden pedestals alike. After a tracked item leaves the room's collectible list, it draws the actual item art at the last item position. The afterimage uses the animation style from initial admission. Changes to the item while the same pickup remains alive do not make a separate afterimage.

The default `stopWithHourglass` setting blocks new tracking when any player's primary active item is Glowing Hourglass. It does not clear already tracked pickups. New-room callbacks clear all tracked pickups and afterimages. A disappeared pickup whose final item ID is zero is not drawn.

## Portable implementation

`features/regret/state.hpp` owns values rather than engine objects. It tracks at most 128 pickups and 128 afterimages without heap allocation. It offers complete-snapshot and explicit-removal APIs. The removal API accepts a final snapshot before the game frees the object, so collected items that have become empty can be suppressed safely. A later snapshot from the same update can correct or cancel an earlier removal image. Room changes clear state. Incomplete room scans never imply that missing entries were removed. Signed item IDs preserve the game's procedural-item identities.

The fade renders ages 0 through 59, with alpha `1 - age / 60` and upward movement `age / 10`. Simultaneous removals and expirations use compaction, so removing one entry cannot skip the next. Zero-ID entries are discarded; this intentionally avoids upstream's never-expiring zero-ID list entries. When the afterimage capacity is reached, the oldest image is replaced. Invalid coordinates cannot be drawn. Identity includes both address and generation to separate reuse of an engine allocation.

Tests run in the isolated development environment with:

```powershell
wsl -d IsaacDevkit --exec sh %USERPROFILE%/Desktop/sw/Isaac-switch-mods/tools/regret/run_tests.sh --asan
```

The ten scenario tests passed with ordinary execution, UndefinedBehaviorSanitizer, and AddressSanitizer plus UndefinedBehaviorSanitizer. They cover the 60-frame fade, final positions, collected-item suppression, duplicate removals, rerolls, admission animation, Hourglass behavior, incomplete scans, room resets, identity reuse, simultaneous expiration, capacity limits, invalid coordinates, signed procedural IDs, and final-snapshot correction. They exercise portable behavior; they are not console gameplay validation.

One earlier PIE AddressSanitizer run in this WSL image entered a repeated `DEADLYSIGNAL` loop; that test process was stopped. The final non-PIE sanitizer build passed in a single bounded rerun. The runner now limits time to 15 seconds per process and bounds sanitizer output. This records the environment issue rather than treating the failing runtime as a passing test.

## Native implementation

`native/source/program/regret.cpp` implements four guarded hooks: pickup update, pickup removal, pickup destruction, and room initialization. All four entry-point checks run before any are patched. On mismatch the feature remains disabled and draws `Regret: hook mismatch` through the shared HUD bridge. It samples a pickup before and after its update, before removal, and before its destructor; it never dereferences a destroyed pickup. Room initialization suppresses old-room cleanup notifications and clears state even when restoring or re-entering the same room. A fallback complete room scan handles missing entries, while incomplete scans cannot invent disappearance events.

The adapter checks every player's primary active item against Glowing Hourglass ID 422, verified in the dumped `rp_patch/resources/items.xml`. Pickup identity uses its address and initial seed. The room stamp additionally includes game/room addresses, room index, dimension, stage/type, run seed, and floor seed.

`native/source/program/native_sprite.hpp/.cpp` provides a reusable owned sprite wrapper. The game itself constructs/destructs its 0x158-byte animation object in fixed aligned mod storage. The object cannot be moved or copied because its constructor stores self-pointers. Internal allocations remain owned by the game's allocator. Borrowed 24-byte libc++ strings are passed only to verified const-reference calls that copy them; the mod never frees game strings.

The renderer loads the game's collectible animation, obtains the true item's graphics path from `ItemConfig::GetCollectible(id)`, replaces its item layer, selects `Idle` or `ShopIdle` frame zero, and draws with the game's world-to-screen conversion. This matches the Lua mod's actual-item art, including revealing disappeared blind items. It deliberately does not call `SetupCollectibleGraphics`, because that helper changes art for the April Fools challenge whereas the original Regret mod reads the item configuration directly. Null/invalid item configurations are skipped safely.

`ResetRegret()` destroys the owned sprite before the game module unloads. Installed-hook status is retained, so a failed SDK unload can resume without patching already-modified entry points. A successful reload calls `InitializeRegret` again and revalidates fresh code even if the module uses the same address.

## Exact-build evidence

These offsets are for the user's dumped Repentance 1.7.9b NRO, SHA-256 `cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a`. The root loader separately checks its build identity before initializing features. Local disassembly and decompilation are retained under `evidence/native-analysis` and `evidence/native-analysis/expanded`; assembly is authoritative where the decompiler inferred function arguments incorrectly.

| Call/data | NRO offset or field | Evidence |
| --- | --- | --- |
| Pickup update | `0x26c9b8` | `expanded/26c9b8.asm.txt`; first 20 bytes are stack/register operations |
| Pickup removal | `0x276960` | `expanded/276960.c`; base removal marks flags and does not free |
| Pickup destructor D1/D2 | `0x2691f8` | `expanded/2691f8.asm.txt`; fifth instruction is ADRP, supported by exlaunch's relocator |
| Room initialization | `0x45c020` | `expanded/45c020.asm.txt`; x0 room, x1 configuration, x2 descriptor, uint32 return preserved |
| ANM2 allocation size | `0x158` | `expanded/17924.asm.txt` allocates that exact size then invokes the constructor |
| ANM2 constructor/destructor | `0x6540` / `0x71a4` | `expanded/6540.*`, `expanded/71a4.*` |
| ANM2 load/replace/load graphics | `0xc970` / `0xd270` / `0xcb6c` | exported symbols and corresponding saved native analysis |
| ANM2 set frame/render | `0xa65c` / `0x9bd4` | `expanded/a65c.*`, `expanded/9bd4.*` |
| ANM2 tint | object `+0xe8`, setter `0x37848` | `expanded/65b8.c`, `expanded/37848.asm.txt` |
| World to screen | `0x489354` | `expanded/489354.asm.txt`; Vector2 travels in s0/s1 and returns in s0/s1 |
| Item configuration and art | getter `0x3c0c50`, manager `+0x36538`, returned config `+0x38` | `expanded/3c0c50.c`, `26c3ec.c` |
| Current animation name | pickup `+0x50` ANM2; animation-data pointer at ANM2 `+0x38` | `expanded/a1f4.c`, `expanded/a454.c`; comparing name also handles a stopped Idle frame |
| Pickup initial seed | entity `+0x3e8` | Entity::Init `0x597d4` saved analysis |
| Primary active item | player `+0x1964` | `expanded/27f828.c` |

The native code has been written and the portable tests have passed. The combined prototype also cross-compiled with `-Wall -Werror` and passed the root project's binary/import checks. No on-console result has been observed yet, so the build is ready for a gameplay test rather than proven in-game. The original mod's optional PC Mod Config Menu integration is not recreated; the port uses its default Hourglass setting.

## Console acceptance checks

1. Enter a room with two option pedestals. Pick one up. The unchosen item should leave its true icon fading upward for 60 render frames; the collected item should not leave one.
2. Repeat with an alternative-floor blind pedestal and with disappearing angel-deal choices. The disappeared item's actual icon should be revealed.
3. Clear several pedestals simultaneously. All afterimages should appear and finish without a skipped or permanent icon. Test a shop choice to verify `ShopIdle` positioning.
4. Reroll an existing pedestal without removing its entity. Its old item should not appear as a separate history image.
5. Hold Glowing Hourglass in any player's primary active slot before entering the room. Newly seen choices should not be tracked. Restore or re-enter the same room and verify prior-room afterimages are cleared.
6. Change floors, restart a run, return to the title screen, and relaunch. The feature should have no leftover room items or sprite state.

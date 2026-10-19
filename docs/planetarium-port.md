# Planetarium Chance port

The implementation translates the current Planetarium Chance mod's run, floor
and display behavior into C++. The native adapter calls the Switch game's own
chance calculation, then keeps that percentage fixed for the floor being played.
Picking up a chance-changing item therefore affects the next floor's display,
as in the original mod. The initial console build has the integration limits
listed below; portable test success is not evidence of a successful console run.

Source is pinned to
[`b724ac3641e89e0aaa8b43a301ccb168a07d1d6a`](https://github.com/Sectimus/isaac-planetarium-chance/tree/b724ac3641e89e0aaa8b43a301ccb168a07d1d6a),
upstream version 11.1, under GNU GPL version 3. Attribution and the full license
are in `features/planetarium/NOTICE.md` and `features/planetarium/LICENSE`.

## Chance calculation

The current upstream Lua calls `Level:GetPlanetariumChance()`; it does not
contain an independent item-by-item probability formula. The Switch executable
exports the matching native function:

```cpp
using GetPlanetariumChance = float (*)(const void* game);
// Repentance module + 0x3d58bc; Game starts with the Level object.
```

Its floating-point return is proven by ARM64 assembly. The initial decompiler
output incorrectly labeled the return `void`; that guessed C signature must not
be used. The native implementation handles skipped treasure rooms, previous
planetariums, alternate-path stage correction, Telescope Lens, Crystal Ball,
Magic 8 Ball and Sausage. It does not perform the mod's separate unlock and
initial-floor treasure-room eligibility checks.

Evidence: `evidence/native-analysis/expanded/3d58bc.asm.txt` and `.c`.
The chance getter does not call a random-number generator. Using the native
function avoids introducing a second guessed version of the game's formula.

## Portable implementation

`features/planetarium/planetarium.hpp` includes:

- A current-floor cache, with no mid-floor changes from item pickups.
- Native fraction validation, clamping to 0..100%, and unknown-state handling.
- New-run eligibility from Telescope Lens availability and a treasure room on
  the starting floor.
- Continued-run eligibility records, validated against the run identity. With
  no saved record it uses the original mod's availability fallback.
- Unlocking during a run and the ascent's forced 0% result.
- The original change-popup fade, slide timing and signed percentage change.
- HUD hide rules, including latched Dogma transition suppression and Beast room.
- Counter, co-op, Jacob/Esau, Duality, difficulty and HUD-offset layout logic.

The native adapter supplies independently verified state. A room pointer or
stage number alone is not a sufficient floor identity because a rerun or R Key
can revisit the same stage. `identity.hpp` incorporates the saved stage seed,
stage, stage type and ascent. It applies the native alternate-path seed-index
correction and rejects indices outside the game's 15-entry saved-seed table.
The portable layer never reads game memory itself.

`ChanceTracker::BeginRun` and `ObserveFloor` take the native chance sample and
state. `Render` returns draw coordinates and values. `Save` returns the small
eligibility record; persistence is a responsibility of the native adapter.

The original mod persists eligibility but recalculates chance on Continue, and
its README warns that the resumed current-floor value can differ until the next
floor. The portable implementation preserves that behavior; it does not claim
to recover historical generation probability from a changed inventory.

## Native integration

`native/source/program/planetarium.cpp` exports `InitializePlanetarium`,
`BeginPlanetariumHud`, `RenderPlanetarium` and `ResetPlanetarium` through
`planetarium.hpp`. The common loader verifies the exact NRO build before these
functions may run. The adapter:

- Samples `Level::GetPlanetariumChance` on the first accepted HUD frame and
  each changed floor identity. It retries invalid samples instead of displaying
  an invented number.
- Checks Telescope Lens through the native trinket configuration and ordinary
  `IsAvailableEx(UINT32_MAX)` test. It reads the initial stage-1 treasure-room
  list directly without invoking the room-selection function that consumes RNG.
- Detects a new game object, changed run seed or frame-counter rollback. The
  first observation on stage 1 inspects its treasure-room eligibility. Entry on
  a later stage uses the upstream fallback for Continue without saved mod data.
  An existing eligibility record is retained in memory for matching-run
  restarts on later stages.
- Calls the native ascent and Beast-room getters, and reads verified FoundHUD
  and screen-shake state.
- Loads the game's `gfx/ui/hudstats2.anm2`, `Idle` frame 8, for the real icon.
  It keeps an owned, nonmoving native sprite and destroys it before module
  unloading. If loading fails, a small `P` identifies the percentage instead.
- Uses the same font object, full-size draw call and half-opacity white tint
  as the game's ordinary stat percentages. It draws the original signed
  red/green change indicator with the ported slide and fade.
- Observes the game's actual stat-column position and appends one row. Native
  counter, difficulty, disabled-achievement, HUD-offset and character shifts
  are already included in that position; they are not applied a second time.

The common overlay already gates game HUD visibility and the NO_HUD seed.
FoundHUD is checked within this feature, after sampling, so turning off the
stats display does not by itself stop floor tracking.

Native addresses, object layouts, and their assembly/decompiler evidence are
listed in [`native-api.md`](native-api.md#planetarium-chance-floor-identity-and-options).
Screen-shake is applied once to text, while the icon remains stationary, as in
the Lua mod. Owned-sprite construction and destruction are described in the
same document.

## Stat-column appearance correction

The old adapter used the common overlay font at `manager + 0x76f90`, scaled to
0.65, and a partly reconstructed screen position. The original
`HUD::StatHUD::Render` at `module + 0x3af194` instead draws the stat percentages
with the font embedded at `manager + 0xd7080`, using `Font::DrawString` at normal
scale. The corrected adapter uses that exact font object. This carries its
native glyph shapes and outlines rather than approximating them by shrinking
the ordinary counter font. The font is owned and loaded by the game; no new
font files or textures are required.

The upstream Lua loads `luaminioutlined.fnt`, draws its chance at icon position
`(+16,+1)` with white opacity 0.5, and draws change text at `(+46,+1)` before its
slide. The native stat renderer proves those same percentage offsets and
opacity. The adapter retains them, as well as upstream's stationary icon and
text-only screen shake. It does not claim that the Switch font's source
filename is `luaminioutlined.fnt`; the important match is the actual font
already used by the other Switch stat percentages.

The observer receives `StatHUD::Render(this, const Vector2& origin)`. The native
HUD supplies its completed position. Its stat object is at
`game + 0xe8468 + 0xc5968`; first and second stat-player pointers are at `+0x158`
and `+0x228`, and the combined deal-chance flag is at `+0x2f8`. The original
function draws eight rows at 12-pixel spacing, seven with the combined deal
icon, and uses 14-pixel spacing when both stat entries are populated. The
adapter starts its extra icon exactly after those rows. This also follows the
native combined-deal spacing for Jacob/Esau instead of applying a fixed
12-pixel subtraction from the PC layout formula.

`BeginPlanetariumHud()` clears the observation immediately before the original
HUD call. `RenderPlanetarium` consumes it once after that call; it draws nothing
when the original stat column did not render in that same pass. A game-frame
counter alone would leave stale positions while paused or during hidden-HUD
transitions. The portable `StatDrawPass` tests cover this distinction.

Initialization verifies the exact 20 original bytes of both the stat renderer
and the font draw routine before installing one observer. All five overwritten
stat-renderer instructions only adjust the stack/save registers. Repeated
initialization recognizes the installed bytes; a newly mapped module at the
same address must match the original bytes again. Reset destroys the owned
icon while the module is still present and clears the draw latch. Installed
hook identity is retained if the SDK declines to unload the module.

`Font::DrawString` at `0x4ceef0` takes the nontrivial game Color object indirectly
through a pointer in the raw ARM64 call boundary. The native adapter uses that
verified ABI and the normal-scale wrapper, not a guessed by-value four-float
argument. `Font::IsLoaded` at `0x4ce898` is exactly `LDRB W0,[X0]; RET`, so the
font-loaded check requires only its verified first byte.

Evidence is saved in `evidence/planetarium-layout/native-layout.json`, including
the dump identity, matching source guards, and hashes of the original assembly,
decompiler output, upstream Lua and adapter. Reproduce that audit with
`python tools/planetarium/verify_native_layout.py`.

## Current integration limits

- This adapter observes HUD frames; it does not yet receive the exact native
  run-start or new-level event. If the common HUD gate hides the overlay across
  a transition, the chance is first captured when the HUD becomes available.
  A restart with the same seed and a higher observed frame count can also
  escape the current restart detector. These cases require lifecycle hooks
  before exact callback parity can be claimed.
- No mod data is written to the SD card. `Save()` remains available for later
  persistence integration. After a process restart on a later floor, the
  availability fallback cannot reconstruct a challenge's original treasure-room
  eligibility. First-floor continuation rescans that floor. The historical
  chance itself is never recoverable from eligibility alone, as upstream also
  notes for Continue.
- The exact Dogma death-animation guard is not connected. The build
  conservatively hides the feature throughout Home and for the rest of that
  run, while the native Beast-room guard is also active.
- The new stat-font and position correction passed local logic and exact
  binary checks. On October 14, 2026, the user confirmed normal game launch and
  matching stat appearance on the Switch. Separate display modes and co-op
  layouts have not received individual console confirmations.

## Verification

Both optimized C++17 and UndefinedBehaviorSanitizer host builds passed. The tests
exercise unchanged current-floor values after item changes, native-result
propagation, eligibility, mid-run unlocks, ascent, new/continued runs, invalid
saved records, popup lifetime, suppression, HUD positions, alternate-path seed
indexing and floor-identity changes. The new layout tests cover native row
counts, two-player row spacing, exact percentage/change offsets, text-only
screen shake, avoiding duplicate HUD offsets, one draw per observed pass,
hidden stats on paused frames, mismatched game identity, invalid coordinates
and reset recovery. Synthetic native
chance results test the port's handling; they do not execute the Switch game or
independently validate its item formula.

Run from the project directory in the `IsaacDevkit` environment:

```text
python3 tools/planetarium/run_tests.py
```

The test runner records commands, outputs and source hashes in
`features/planetarium/test-results.json`. On-console rendering and integration
remain separate checks.

The new stat observer/font correction is included in the compiled and installed
`combined-expanded` package. Its normal launch and stat appearance were confirmed
by the user in `deployments/20261014T194151Z-combined-expanded/console-tests.json`.
That visual check does not resolve the lifecycle and persistence limits above.

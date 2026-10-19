# Time Machine for the verified Switch build

This implements the continuous machine/beggar acceleration requested through
[TimeMachine[Repentance]](https://steamcommunity.com/sharedfiles/filedetails/?id=2617557401).
The original PC Lua file is not executable in this Switch game's absent Lua mod
API. The implementation is native code tied to the verified 1.7.9b NRO.

The behavior reference is tarako's linked Workshop description and the readable
[TimeMachine Fixed source](https://github.com/archibate/isaacmods/tree/abfd7005d72578cdd439b151d6dbb31f8a396796/timemachinefixed),
retained at that commit in `reference/archibate-isaacmods`. The Fixed fork is
identified explicitly; this package does not claim to execute the original
Workshop script unchanged.

## Player behavior

Hold towards a machine or beggar in a cleared room. Repeated interactions
gradually speed up, to at most six normal game steps per displayed game update.
Release the direction, move away, pause, use another control, encounter a threat,
or change room and acceleration stops. No extra action is bound to a Switch
button. The feature does not change payout probabilities or create free resources.

Supported machine variants: slot machine, blood donation machine, fortune teller,
coin beggar, devil beggar, shell game, key master, bomb bum, Greed donation machine,
battery bum, hell game, crane, confessional and rotten beggar. Ordinary donation,
restock, dressing-room and special Isaac machines are excluded.

The game itself advances its clock, invincibility and other temporary effects
during each extra step. This avoids an independent hand-edited timer diverging
from Hush/Boss Rush entrance logic. Health-consuming interactions slow to normal
speed for the final heart. An idle co-op partner touching such a machine also
blocks acceleration when health is low. Moving partners must themselves be
using a supported machine, to avoid surprising them with faster movement.

## Exact native evidence

The immutable NRO SHA-256 is
`cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a`.
Every callable helper and the single update hook has a byte guard generated from
that file. All guards must pass before the hook is installed.

| Evidence | Finding used |
| --- | --- |
| `evidence/timemachine/native/351884.c` and assembly | `Game::Update` at 0x351884 advances frame 0x24f99c and timer 0x24f9a0 through the native gameplay path |
| `native/3539dc.c` | `Game::IsPaused` covers game over, leaderboard, room/stage transitions, pause and prompts |
| `native/3505a4.c` | Both native counters are preserved by the normal save path |
| `native-fields/32f1d0.c` | Slot state/cooldown at 0x400/0x40a, actual payment resource checks, and health donation on contact even without movement |
| `native-fields/59e54.c` | Native collision radius at entity+0x344 |
| `native-fields/27f29c.c`, `27dd54.c`, `27e380.c`, `27f6fc.c` | Red/soul/bone/eternal health fields |
| `native-fields/29b6fc.c` | Controller index at player+0x19ec |
| `native-input/41ca44.c` | Map action is 13, so opening the reminder keeps Time Machine at ordinary speed |
| `native-safety/59130.c` | Enemy classification is entity types 10 through 999 |
| `evidence/boss_ui/timemachine-review/2b14b4.c` and `4f775c.c` | Movement-trigger edges can activate Mars repeatedly if replayed; fresh movement presses must be excluded |
| `evidence/boss_ui/timemachine-review/2b0d9c.asm.txt` and its caller | The native movement getter returns two floats in s0/s1, handles analog, reversed controls and mirrors, and is used only with controls enabled |

Each extra update obtains fresh game, room, player and machine state. No entity
pointer is dereferenced across an update without revalidation. Identity includes
spawn seeds, room-entry frame, run seed, room index, dimension and floor. An
unexpected frame-counter result, death, payout/removal or transition cancels the
remaining updates. A guarded nested call cannot recursively accelerate itself.

## Deliberate port differences and remaining checks

- The Switch version advances a full normal game step, keeping all players and
  timers consistent. The Lua version separately updates selected entities.
- Acceleration only runs while holding towards the target. Fresh presses and
  shooting, item, pill, bomb, drop, map or pause controls stay at normal speed.
- Live enemies, bombs, lasers, projectiles and burning fireplaces stop acceleration. Verified harmless ashes are allowed. This version
  does not kill spawned flies or defer bombs as the Fixed fork optionally does.
  Static shopkeepers are permitted. Put out room fires before using the speed-up.
- Eternal chests, third-party Lua machines, a PC Mod Config Menu and optional
  payout-pickup locks from the newer Fixed fork are not part of this adaptation.
- Host tests exercise the actual portable contact, resource, ramp and lifecycle
  policy. They do not execute the Switch game. The adapter also passed the
  combined Switch compiler check. On October 14, 2026, the user confirmed that
  repeated interactions speed up and return to normal when released. The reply
  did not identify which machine/beggar was used; individual machine variants,
  low-health cases, co-op and transition cases still lack separate console
  confirmations. See the installed build's `console-tests.json` in
  `deployments/20261014T194151Z-combined-expanded` for the exact question/reply.

Reproduce the host policy tests with `python3 tools/timemachine/run_tests.py` in
IsaacDevkit. Their report and current source hashes are in
`evidence/timemachine/test-results.json`. The machine-facing instructions are
included in the expanded package's testing file.

## October 15 repair and menu integration

A put-out fire is still a live type-33 entity in this game. Its native routine
sets state to 3, health to 1, clears both collision classes, and clears its Dead
flag. The earlier Time Machine check treated every such entity as danger. It
therefore stayed at normal speed in a cleared room containing harmless ashes.
The archived combined-extended inspector reproduces that failure. The user's
particular room state was not captured, so a new console test is still needed
before calling the whole reported regression resolved.

The new inspector accepts only the proved extinguished state for fire variants
0 through 3: state at +0xb70 equals 3, health at +0x354 equals 1, and both collision
classes at +0x1e0/+0x1e4 equal zero. Burning, relighting, white, moving, unknown and
unreadable fires still stop acceleration. The native fireplace routine is also
checked against exact-build bytes at startup. No fire data, damage, drops or
payouts are modified by Time Machine.

The native controller IDs remain 1 through 8 and 33. The archived and fixed
inspectors both accept these values in allocated memory fixtures. Zero is not
an all-controllers shortcut; that native selector is UINT_MAX. The shared update
scheduler passes with other observers before inspection, too. Neither rule was
reverted without evidence.

The actual inspector now lives in `features/timemachine/adapter.hpp`, used by the
console wrapper and host memory tests. A generated fixture retains the archived
inspector from the iteration-4 baseline, substituting only native memory and
function boundaries. It rejects harmless ashes, while the production inspector
accelerates in the same fixture and stops on release or a relighting fire. The
complete production callback source is also compiled with platform boundaries
replaced to test its guard, toggle, observer and menu control flow.

The shared input hook is Game::ProcessInput at 0x3515ec, with void(this*) ABI.
Manager::Update polls controllers before that call, so the menu can consume input
before normal game actions run. Game::Update at 0x351884 and
Game::InterpolatePositions at 0x353ab0 skip their work while the menu latch is set.
Interpolation also updates secondary player entities; blocking only Game::Update
would leave those calls active. RevertInterpolatedPositions still restores render
positions. Camera smoothing and co-op-selection visual interpolation outside these
calls are unchanged. All three entry points are guarded; the two added hooks
displace only stack/register setup instructions.

Turning Time Machine off disables only extra steps. Normal successful ticks still
notify the other mods. Menu-blocked passes reset the speed ramp without calling
the cancellation callback that would close the menu. `TimeMachineStatus()` exposes
a short reason for normal speed and retains the last gameplay reason while the
menu is open.

Validation: 258 policy/lifecycle, 154 actual-memory adapter/dispatch, and 22
production callback integration cases passed ASan/UBSan. The production file
passed the isolated Switch compiler check. Current source hashes and test results
are in `evidence/timemachine/test-results.json`; the archived regression provenance
and native evidence index are under `evidence/timemachine/iteration4/`. These are
host tests, not Switch gameplay.

Console checks remaining: use a funded slot machine or coin beggar in a cleared
room containing extinguished fire remains; confirm speed grows and returns to
normal on release. Confirm live fire blocks it, and confirm Time Machine OFF
leaves other mods working. If another case stays slow, the new status distinguishes
danger, low health, unsupported machines, controls, resources and co-op movement.

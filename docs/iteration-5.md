# Iteration 5

Three additions on top of the confirmed-working `combined-refined` package
(module SHA-256 `e7fe2778…0a98`). Only `subsdk9` changes; the launch
descriptor and all 1088 resource files are identical to the installed package.

## Machine Lock

Holding toward a machine or beggar while not lined up exactly makes Isaac
slide around its round collision until contact is lost. Machine Lock keeps the
player on the spot where contact began for as long as they keep holding toward
that machine, so Time Machine keeps working.

- It latches when Time Machine's own inspection accepts the player: a cleared
  room, touching a supported machine, pushing toward it, and able to pay.
- After every game update (normal and accelerated), the player's position is
  put back on that spot. Velocity is left alone, so the push into the machine,
  its collision and its payment work as normal.
- It lets go when the stick is released, pushed sideways or away, when another
  action is pressed (shooting, items, bombs, map, etc.), on any danger, room
  change, low health for blood/health machines, lost controls, a removed,
  broken or jammed machine, running out of money while the machine is idle, or
  a machine that stays busy for more than five seconds.
- It is its own entry in the Mods list (on by default) and works with Time
  Machine turned off. It needs the Time Machine hooks, so it is unavailable if
  those fail their guards.

Evidence: entity position is `+0x310`, velocity `+0x334`. `Entity::Update`
(`0x60040`) applies friction and adds velocity to position; see
`evidence/iteration5/native-movement/60040.c`. Teleporting entities write only
`+0x310` (`0x650e0`).

## Greed Machine Never Jams

`Entity_Slot::Update` jams the Greed Donation Machine when
`Random()*100 < Entity_Player::GetGreedDonationBreakChance()`
(`0x32e368`–`0x32e394`), then sets game state flag bit 34 (`game+0x26fa50`).
`Entity_Slot::Init` reads that bit (`0x328ee0`) and starts the machine jammed.

- The break-chance function (`0x2fd1bc`) returns 0 while the mod is on, so no
  donation can jam it. The HUD reads the same function, so it shows 0%.
- Each ordinary game tick clears bit 34. A run that jammed before installing
  this gets a working machine the next time one is created (next floor).
- Turned off, the game's own chance and flag are used unchanged.

Guards cover the function prologue, the exact compare sequence, the flag write
and the flag read. Decompiled evidence is in `evidence/iteration5/native` and
`native-greed`.

## Hit Counter

Counts the hits taken this run and on this floor, drawn in the HUD's own
pickup-counter font (`Manager+0x76f90`, the coins/bombs/keys numbers).

- Hooks `Entity_Player::TakeDamage` (`0x2a34d0`). A hit is a call that returns
  true, or a Holy Mantle-style block (returns false but starts invincibility at
  `+0x1734`). Damage flagged `DAMAGE_NO_PENALTIES` or `DAMAGE_RED_HEARTS`
  (`0x10000020`, the same mask the game uses to skip its penalty counters) and
  `DAMAGE_FAKE` are not hits: blood donations, IV Bag, self-damage items.
- Tainted Forgotten's soul forwards damage through the same function; only the
  outermost call counts.
- A different run seed, or the same seed back at its first second (seeded
  restart), starts a new count. Exiting to the menu and continuing keeps it.
  A new stage or stage type resets the floor count.
- Counting continues while the counter is hidden, so turning it back on shows
  the right number.

### Settings (Mods → Hit Counter → Y)

| Setting | Values |
| --- | --- |
| Show counter | On/off (same as A on the Mods list) |
| Position | A, then D-pad to move; tap = 1 pixel, hold to speed up; A or B when done |
| Size | 50% to 300% in 10% steps (Left/Right) |
| Opacity | 10% to 100% in 10% steps (Left/Right) |
| Floor hits | Show "Floor: n" after the run total |
| Reset to defaults | Top center, 100%, 85%, floor hits on |

The default position is top center, which the stock HUD leaves empty. Like the
other Mods choices, settings last until Isaac is fully closed: saving them to
the SD card would need an SDK import the loader does not currently use.

## Menu changes

- The Mods list now has 13 entries and pages with L/R as before.
- Y on a mod with settings opens its page; the footer shows `Y: settings`.
- Closing a settings page waits for every button to be released, so its B
  does not also close the Mods list.

## Tests (local, not the Switch)

- Time Machine: 258 policy, 177 adapter/dispatch (23 new Machine Lock cases)
  and 28 production-hook cases (5 new) under ASan/UBSan; Switch syntax check.
- Hit Counter and Greed: 59 portable cases and 39 production-hook cases
  (counting rules, mantle, soul forwarding, runs/floors, drawing, settings,
  move/clamp, greed chance and flag, guard failures); Switch syntax check.
- Item menu native callbacks: 10036 checks including the settings page on both
  the in-game and title-screen Mods lists.
- Existing clean-map, boss-bar, picker, UI and item-visual suites re-run.

None of these run the game. See `iteration-5-testing.md` for the console checks.

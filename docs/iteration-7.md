# Iteration 7: the paper mod menu

Built 2026-10-19 from the plan in `docs/plans/mod-menu-redesign-2026-10-19/PLAN.md`.
Build mode: `combined-menu` (`tools\build.ps1 -Mode combined-menu`).

## What changed

- **New menu** (see `docs/mod-menu.md`): Items, Mods, Run, Tools and Menu
  pages drawn with the game's own menu art, fonts, button pictures and
  sounds, with page-turn and paper motion (can be reduced).
- **Pause screen**: **Y** opens the menu; a "Y Mods" prompt is shown (can be
  hidden; Y still works).
- **Settings are saved** in Isaac's save data, including which mods are on.
  Before this, mods reset to on every time the game started.
- **More per-mod settings**: Time Machine top speed; EID nearby descriptions
  and Hold-Map browsing; the Hit Counter page moved into the new menu.
- **Shortcut hold time** is a setting (0.5 s / 1 s / 2 s / off).
- Includes everything from iteration 6 (the Repentance+ balance data files).

Decisions taken for the four open questions in the plan:

1. *Look*: authentic paper pages from the game's own textures, not new art.
2. *Persistence*: a separate small file in the game's save data, written only
   on close and only when something changed.
3. *Item actions*: pedestal, give, shop item; story items that break runs
   when given directly can only be placed.
4. *Scope*: every priority in the plan, including the Tools page.

## Safety

- Every game function used is checked against the exact 1.7.9b bytes before
  anything is installed. If a check fails, the menu (and only the menu)
  stays off; if only the pause-screen check fails, the other two ways to
  open the menu still work.
- Saving uses the game's own linked file-system functions; the module's
  system imports are unchanged (verified by `tools/check_build.py`).
- The menu closes instantly if the room, run, player or controller changes
  under it.

## Files

- New: `features/menu_ui/*`, `native/source/program/mod_menu.cpp/.hpp`,
  `assets/menu_ui/resources/gfx/codex_menu/menu.anm2`, `tools/menu_ui/*`,
  `tools/verify_menu.py`, `tools/install-menu-mtp.ps1`,
  `tools/tests/install-menu-mock.ps1`.
- Changed: `debug_kit.cpp` (opens the new menu), `hit_counter.cpp` (layout
  from saved settings, preview in the menu), `timemachine.cpp` (top speed),
  `eid_features.cpp` (two switches), `eid_ui.cpp` (ink on paper, text width,
  description body, single icons), `mod_settings.cpp` (save/restore mask),
  `overlay.cpp`, `features/debug_kit/policy.hpp` (hold time).
- Retired: the old item-test menu drawing and its harness
  (`tools/item_menu/run_native_tests.py`, `test_native_hooks.cpp`); its
  iteration 4 evidence is kept as a record.

## Tests (host, not console)

| Suite | Result |
| --- | --- |
| `tools/menu_ui/run_tests.py` — menu model, layout, motion, settings format | 118 checks |
| `tools/menu_ui/run_native_tests.py` — production Switch code on a fake console, -O2 and ASan/UBSan | 18,884 checks |
| `tools/item_menu/run_ui_tests.py` — EID drawing helpers incl. new paper helpers | 134,818 checks |
| `tools/hit_counter/run_tests.py` — counter, menu-driven layout, Switch syntax | 68 checks |

Plus every earlier suite re-run (see `evidence/menu-verification.json`).

## Not yet known

Nothing here has run on the console yet. Things that only the console can
show: the menu's look and motion in the real renderer, sounds, saving and
reloading on real save data, the pause-screen hook, and the shop price for
placed shop items.

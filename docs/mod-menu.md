# Mod menu (iterations 7 and 8)

A paper menu in the style of the game's own Mods, Options, Stats and
Collection pages. It replaces the old dark item-test menu and the old Mods
list.

## Opening it

| Where | How |
| --- | --- |
| In a run | Pause, then press **Y** ("Y Mods" shows in the bottom-right corner). |
| In a run, without pausing | Hold **both stick buttons + D-pad Down** in a cleared room (2 seconds by default; 0.5 s, 1 s or off under Menu). The game freezes while it is open. |
| Main menu | Open the game's **Mods** screen and press **X**. Only the Mods and Menu pages are available there. |

**L / R** change page, **B** goes back or closes, **+** closes from anywhere.
Only the controller that opened the menu can use it.

## Pages

- **Items**: every collectible, trinket, card/rune, pill and pickup, with the
  game's own pictures. **X** opens the filter card (search, type, item kind,
  quality, item pool, everything / favourites / recent / owned / not owned,
  sort). **Y** marks a favourite.
  - **RS** opens the search board: pick letters with the D-pad and **A**.
    - Capitals and punctuation don't matter, and numbers match item IDs.
    - **B** deletes, **Y** clears, **RS** or Done closes.
    - The search works with the other filters and lasts until the menu closes.
  - **LS** turns the page of a long description (it also turns by itself; `2/3` shows in the margin).
  - **A** opens the action note:
  - *Place on a pedestal* / *Drop on the floor* (next to Isaac);
  - *Give to Isaac* (not for pickups; the Broken Shovel and Knife pieces,
    Dogma and Dad's Note can only be placed);
  - *Place as a shop item* (collectibles; the room sets the price).
  The selected item's EID description is shown underneath.
- **Mods**: all 14 mods with a stamp showing on or off. **A** switches a mod,
  **Y** opens its settings where it has any:
  - *Hit Counter*: show, move (D-pad, hold to go faster), size, opacity,
    floor hits, reset;
  - *Time Machine*: top speed (2x to 6x);
  - *External Item Descriptions*: nearby descriptions, hold Map to browse;
  - *Repentance+ Rules*: Void portal after ??? and The Lamb, after Mega Satan,
    after Mother, and the Boss Rush wave counter (see `docs/rplus-port.md`).
- **Run**: seed, floor, character, time, hits, stats, item history and
  transformation progress.
- **Tools**: full health, +1 soul heart, +10 coins, +5 bombs, +5 keys, charge
  the active item, reveal the map, reroll pedestals (D6).
- **Menu**: sounds, motion (full / reduced), shortcut hold time, the pause
  prompt, which page opens first, background darkness, reset.

## Saved settings

Everything above, including which mods are on, favourites and recent items,
is saved in a small text file, `isaac_mods_menu.cfg`, inside Isaac's own save
data (the same storage as your game progress). Iteration 8 writes version 2
of the file, which adds the Repentance+ rule switches. It is written when the menu
closes and only if something changed. A missing or damaged file just means
the defaults. Deleting Isaac's save data resets it too.

## How it is built

- `features/menu_ui/`: the menu itself (pages, input, animation, layout and
  the settings format), shared by the Switch module and the tests.
- `native/source/program/mod_menu.cpp`: the Switch side. Every game function
  it calls is checked byte-for-byte against Repentance 1.7.9b first
  (`tools/menu_ui/generate_guards.py`). Saving uses the game's own already
  linked file functions, so the module needs nothing new from the system.
- `assets/menu_ui/resources/gfx/codex_menu/menu.anm2`: named crops of the
  game's own menu pictures (no artwork is copied).
- `debug_kit.cpp`: the stick-button hold and the title-screen X.

Tests: `tools/menu_ui/run_tests.py` (menu model and preview draw lists),
`tools/menu_ui/run_native_tests.py` (the real Switch code against a fake
console), `tools/item_menu/run_ui_tests.py` (EID drawing helpers),
`tools/hit_counter/run_tests.py`. Preview pictures:
`python tools/menu_ui/preview.py` → `evidence/menu/preview/`.

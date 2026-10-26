# Iteration 8: readable menu, item search, Repentance+ Rules

Built on iteration 7 (`combined-menu`), from the plan in
`docs/plans/iteration-8-2026-10-25/PLAN.md`. Build mode is still
`combined-menu` (`tools\build.ps1 -Mode combined-menu`). It now also
regenerates the Repentance+ guards.

## What changed

### Readable menu

- **Ink for EID colours.** Coloured description words were darkened to 55%,
  which turned blue, red, purple and grey into near-black. Light ones (yellow,
  lime, cyan) fell below readable contrast. Each EID colour now has its own
  ink, at least 4.5:1 on paper and still distinct from plain ink
  (`features/menu_ui/ink.hpp`). Any other colour is darkened only as far as
  needed.
- **Faint ink is readable.** Secondary text (summary line, page numbers, "OFF",
  hints) moved from about 3.4:1 to 5.4:1 contrast. Only things you can't use
  right now stay pale (`InkDisabled`).
- **The item note keeps its text on the paper.** The third description line
  used to sit on the note's dark torn edge. The name row and text moved up, and
  long text turns pages:
  - by itself every 5 seconds;
  - by hand with **LS** ("More text" prompt);
  - with a page marker such as `2/3` in the margin.
- **Text is measured, not guessed.** Tab labels, button prompts, pop-up
  messages and the hit counter move bar are laid out by the game's real font
  widths. Prompts that don't fit are dropped, least important first. Long
  messages wrap onto two lines inside their strip.
- **No more running into labels.** Setting values drop their `< >` arrows
  before overlapping the label. Every number line has a width limit.
- **Shorten, then "...".** Long names shrink up to 15% before they are cut
  with "...". The small EID-font text now ends with "..." instead of stopping
  mid-word.

### Item search

- On the Items page, press **RS** (or pick **Search** at the top of the X
  filter card) to open a paper letter board: A–Z, 0–9, Space, Delete, Done.
- The list filters as you type:
  - capitals and punctuation don't matter ("moms knife" finds Mom's Knife);
  - a number also matches the item's ID ("118" finds Brimstone);
  - names that start with the search come first, then names with a word that
    starts with it, then names that contain it anywhere.
- Search works together with the other filters. **B** deletes a letter,
  **Y** clears, **RS** or Done closes. The search lasts until the menu closes.

### Repentance+ Rules (new mod)

- Once The Void is unlocked, a Void portal is guaranteed after ???, The Lamb
  and Mega Satan.
- A portal also appears after Mother, once her ending has been seen.
- A Boss Rush wave counter.

Each rule can be switched off in its settings (Y). Details and addresses:
[rplus-port.md](rplus-port.md).

### Saved settings

The settings file is now version 2 and adds `rplus=` (the four rule switches).
A version-1 file did not know about the new mod, so the mod starts switched on.

## Files

- New:
  - `features/menu_ui/ink.hpp`;
  - `features/rplus/` (rules and tests);
  - `native/source/program/rplus.cpp/.hpp`;
  - `generated/rplus_guards.hpp`;
  - `tools/rplus/` (guard generator, native hook test, runner);
  - `docs/rplus-port.md`.
- Changed:
  - `features/menu_ui/menu.hpp`: search, paging, measured layout;
  - `paint.hpp`: inks, `Format` width, description page;
  - `host.hpp`: `TextWidth`, `DescriptionLines`;
  - `prefs.hpp`: version 2;
  - `eid_ui.cpp/.hpp`: ink palette, "...", shared layout, line count, page choice;
  - `mod_menu.cpp`: measuring, shrink-to-fit, rules;
  - `mod_settings`: 14th mod;
  - `main.cpp`, `overlay.cpp`;
  - `tools/build.ps1`;
  - the menu test harnesses;
  - `tools/menu_ui/preview.py`: real font widths, ink colours, page choice,
    readability audit;
  - `tools/menu_ui/preview_dump.cpp`: new scenes.

## Tests (host, not console)

Run on macOS with clang (the project's runners use GCC under WSL):

| Suite | Result |
| --- | --- |
| `tools/menu_ui/run_tests.py`: menu model, search, paging, fitting, ink contrast sweep | 4,392 checks |
| `tools/menu_ui/run_native_tests.py`: production `mod_menu.cpp` on a fake console, -O2 and ASan/UBSan | 19,404 checks |
| `tools/rplus/run_tests.py`: rule decisions, and production `rplus.cpp` on a fake game | 32 + 21 checks |
| `eid_ui`, `eid`, `eid-special`, `hit_counter`, `clean_map` (×2), `item_visuals` suites | all pass |
| `tools/menu_ui/preview.py` readability audit over 21 rendered scenes (real textures and fonts from the dump) | no text off screen, overlapping or under 4.5:1 |

Not run here:

- the devkitA64 build and the Switch syntax checks (no devkitA64 on macOS). A
  clang syntax pass over the changed Switch files found no errors in project
  code;
- `tools/item_menu/run_ui_tests.py`, which needs a capture from an earlier PC
  run, and whose harness runs fake code from data memory, which macOS blocks.

Console testing: [iteration-8-testing.md](iteration-8-testing.md).

# Iteration 8 plan: Repentance+ rules, item search, readable menu

Drafted 2026-10-25. Builds on iteration 7 (`combined-menu`).

> **Status (2026-10-25): all three tracks implemented and host-tested;
> not yet built for the Switch or tested on the console.** See
> [iteration 8](../../iteration-8.md) and [rplus-port.md](../../rplus-port.md).
>
> Where the result differs from this plan:
> - **Build mode** stays `combined-menu`; `build.ps1` also runs the new
>   guard generator. There is no separate `combined-rplus` mode, verifier or
>   installer.
> - **Item note:** the art is a fixed size, so the text was moved up (all three
>   lines now fit above the torn edge) instead of making the note taller. Pages
>   turn with **LS**, because **RS** is search.
> - The filter card opens on **Show**. Search is the row above it, so earlier
>   button habits still work.
> - **"Search all types"** was left out: results stay within the chosen category.
> - **Track 3** was researched in the dump (Ghidra 12.1.4). The rolls turned out
>   to be two `RNG::Random` calls inside `Room::SpawnClearAward`, so
>   ???/Lamb/Mega Satan use the planned Option A (force the game's own roll).
>   Mother uses Option B, gated on the game's own Mother-room test and on
>   cutscene 24 having been shown. The **Boss Rush wave counter** is included.
>   Hooks in use: 24 of ~80.

Three tracks, done in this order (least risky first):

1. **Menu readability fixes:** only menu code changes, no new game hooks.
2. **Item search:** only menu code changes, no new game hooks.
3. **Repentance+ rules:** new game hooks, so it needs research in the dump first.

Each track can ship on its own. Nothing in tracks 1 and 2 waits on track 3.

---

## Track 1: Menu readability

### What the code shows

These are likely causes of "text overflows / is black and hard to read".
They are confirmed from the source, not yet from console screenshots.

| # | Problem | Where | Effect on screen |
| --- | --- | --- | --- |
| 1 | Coloured EID text is darkened to 55% brightness for paper | `native/source/program/eid_ui.cpp` `InkColor` (line 34) | Blue, purple, grey and red keywords become muddy, near-black and hard to tell apart from normal ink. |
| 2 | Faint ink `0x8a7c74` on paper `0xf4ede1` is about 3.4:1 contrast | `features/menu_ui/paint.hpp` `InkFaint` | Summary line, page numbers, "OFF", "--", help notes and "N matches" are hard to read on a TV. |
| 3 | `PaintList::Format` has no width limit | `paint.hpp` `Format` | The summary/page counter, Run page stats and "x / y" counters can run past the paper edge. |
| 4 | Button prompts and toasts guess text width from character count (5.2 / 5.4 px per letter) | `menu.hpp` `PaintPrompts`, `PaintToast` | Long toasts spill out of their dark box, and prompts can overlap each other. |
| 5 | Right-aligned row values have no width limit | `menu.hpp` `PaintRowValue` | Long choice values can run into the row label. |
| 6 | The item note's description box is 280 x 38 (about 3 lines) and flips pages on its own every 5 seconds | `menu.hpp` `PaintItems`, `eid_ui.cpp` `DrawEidMenuBody` | Long descriptions look cut off, and there's no sign that more text exists. |
| 7 | EID text in the menu is cut mid-word without "..." | `eid_ui.cpp` `DrawEidMenuText` | Names look broken rather than shortened. |
| 8 | Tab widths are hard-coded | `menu.hpp` `PaintTabs` | Fragile if names or fonts change. |

### Changes

- **Ink palette for EID colours:** replace the flat 55% darkening with a
  table that maps each EID colour (`{{ColorBlue}}`, `{{ColorRainbow}}`, ...)
  to a hand-picked ink, then a fallback that darkens only until the colour
  reaches 4.5:1 contrast against paper. Keywords keep their meaning (red stays
  red) and stay readable.
- **Darker faint ink:** move `InkFaint` to about `0x6b5d55` (at least 4.5:1). Keep
  the current shade only for things meant to look disabled, as a new
  `InkDisabled`.
- **Measured text:** add `Host::TextWidth(Font, text, scale)` (the native side
  already has `NativeWidth`; the test fake returns a fixed width per letter). Use it for:
  - prompts: lay them out by real width, and drop the lowest-priority prompts if they don't fit;
  - toasts: size the box to the text, and wrap to two lines past about 300 px;
  - row values: give the value whatever space the label leaves.
- **`Format` gets a `maxWidth`** like `Text`, and every caller passes one.
- **Same "..." everywhere:** `DrawEidMenuText` adds "..." when it cuts text,
  like the menu's own text already does.
- **Description note:**
  - make the note taller when the text needs it (up to 4–5 lines);
  - show a small "1/2" marker when there is more;
  - let **RS up/down** (or ZL+D-pad) flip pages by hand, with auto-flip only as the fallback.
- **Readability check in the preview tool:** extend `tools/menu_ui/preview.py`
  (or the host test draw-list checks) to fail when a `Text` paint is wider
  than its box or below 4.5:1 contrast against the paper under it. This
  catches regressions without a console.

### Needed from you

Photos or screenshots of the worst spots (which page, which item). The
list above is from reading code. Pictures will confirm or add to it.

---

## Track 2: Item search

### Behaviour

- On the Items page, press **RS** (right stick click; free on this page today), or
  pick the new top row **Search** in the X filter card.
- A paper letter board opens: A–Z, 0–9, apostrophe, space, ⌫ and Done, laid out
  as a grid you move with the D-pad. **A** types the letter, **B** deletes one,
  **Y** clears, and **+** or Done closes.
- The list filters **live** as you type:
  - case-insensitive;
  - punctuation and spaces are ignored, so "moms knife" finds "Mom's Knife";
  - matches anywhere in the name ("sad" finds "Sad Onion" and "Sad Bombs");
  - names that start with the search come first.
- Typing a number also matches the item **ID** ("118" finds Brimstone).
- Search works together with the existing filters (type, quality, pool,
  owned...). The summary line shows `Search: "brim"` and **Y** in the board
  clears it.
- Optional toggle **Search all types:** collectibles, trinkets, cards, pills
  and pickups in one list, each with its icon.
- The search text is remembered until the menu closes. It is not saved to the config file.

### Why a letter board, not the Switch keyboard

The system keyboard is a separate applet. Calling it needs new system imports
(`swkbd`/applet services) while the game runs. `tools/check_build.py`
deliberately keeps the module's imports unchanged, and launching an applet
from inside the game's frame is a crash risk. A letter board drawn with the
menu's own art needs no new imports and matches the look.

### Code

- `features/menu_ui/menu.hpp`:
  - `search_[24]` state;
  - a `Matches(name, query)` helper that normalises both sides;
  - one more check inside `Refilter`'s `add` lambda;
  - `UpdateSearch` and `PaintSearch`;
  - prompt text.
- `features/menu_ui/tests.cpp`: matching rules (punctuation, case, ID, prefix
  ranking), search combined with filters, and an empty result.
- No native changes beyond what the menu already draws.

---

## Track 3: Repentance+ rules

A new mod entry, **Repentance+ Rules**, with settings for each rule (Mods →
Y), so each one can be turned off separately. Every hook is byte-guarded like
the existing mods. If a guard fails, only that rule turns off.

### 3a. Guaranteed Void portal (the main ask)

What Repentance+ changed, according to the wiki pages linked below:

- Once The Void is unlocked, a Void portal is **guaranteed** after
  **???**, **The Lamb** and **Mega Satan** (in Repentance: 20%, 20% and 50%).
- New: a guaranteed portal after **Mother**, once Ending 21 (the Mother
  ending) has been seen.
- The other chances (Mom, Mom's Heart/It Lives, Isaac/Satan) appear
  unchanged.

Mega Satan is the special case. In 1.7.9b, the 50% roll decides between "run ends
now" and "portal plus chest". With the rule on, it always takes the portal
plus chest path. You can still end the run by opening the chest, so nothing is lost.

**Research first** (in your Ghidra project with `tools/native-analysis`; the
NRO has ELF symbols, which makes this tractable):

1. Find the routine behind the Lua API's `Room:TrySpawnTheVoidDoor(force)`,
   the native "spawn Void door" call, and everything that calls it.
2. Find where each boss kill rolls the Void chance: the 20% for ???/Lamb,
   and the 50% branch after Mega Satan dies (likely in the boss-room clear
   or ending code). Record the RNG compare and branch addresses.
3. Confirm the unlock check:
   - the achievement getter is already known at `0x439654` (used in `eid_special.cpp`);
   - look up the achievement IDs for "The Void" and the Mother ending in the
     dump's `achievements.xml`/code. Don't guess them.
4. Find how Mother's defeat spawns its exit, to place a portal next to it.

**Implementation shape:**

- **Option A (preferred):** hook the roll for ???/Lamb/Mega Satan so it
  reports "spawn" when the rule is on and the Void is unlocked. This uses
  the game's own spawn path, so portal position, chest and save state all
  stay vanilla.
- **Option B (fallback):** after the boss room clears, call the native
  "spawn Void door (force)" ourselves if no portal exists.
- **Mother:** Option B only, gated on the ending-seen check.
- Generator `tools/rplus/generate_guards.py` writes `generated/rplus_guards.hpp`.
  Host tests in `features/rplus/` cover the decision logic:
  - which boss, unlocked or not, rule on or off;
  - Greed Mode excluded;
  - Mega Satan in a challenge or with "Mega Satan ends the run" seeds (check
    the native code for any special case like this).

### 3b. Other Repentance+ changes: pick which you want

Ranked by how practical they are on Switch. All are code changes, which is why
iteration 6 (data only) skipped them.

| Change | Effort | Notes |
| --- | --- | --- |
| Boss Rush wave counter | Low | Pure overlay, fits the existing HUD hook. Needs the wave index from the room. |
| Mega Bean sweeps the whole room (like Mama Mega), opening Boss Rush/Hush | Medium | Would also let the held-back 6-charge data change apply. |
| Rosary Bead turns Devil deals into Angel deals while held | Medium | One check in deal selection. |
| Beggars / Bomb Bums / Key Masters raise Angel/Devil chance | Medium | Needs the deal-chance function and the payout events. |
| Missing Poster: 33% chance to reveal the Super Secret Room on a new chapter | Medium | Floor-generation hook. The map reveal already exists in Tools. |
| Golden trinket effects for trinkets that had none | Medium–High | One case at a time. |
| Milk!, Abyss, Lil Portal, Delirious, Mars, Dataminer reworks | High | Full behaviour rewrites. Not recommended this iteration. |
| Online co-op, emote wheel, chat | Not possible | Needs the Repentance+ online backend. |

Recommendation for iteration 8: **3a + Boss Rush wave counter**. Add one or two
Medium items only after 3a has been checked on the console.

---

## Delivery

- Build mode `combined-rplus` (`tools/build.ps1`), with
  `tools/verify_rplus.py` and `tools/install-rplus-mtp.ps1` following the
  iteration 7 pattern.
- Hook count: track 3 adds roughly 2–4 trampolines. Iteration 7 uses 22 of
  `JitSize = 0x4000` (about 80 at 200 bytes each), so there is room.
  `tools/check_build.py` already enforces this.
- Settings: add `rplus` rule bits to `isaac_mods_menu.cfg`, with a format
  version bump. Missing keys fall back to the defaults (rules on).
- Mod count goes from 13 to 14. Check the Mods page paging (`ModRows = 10`).

## Console checks (after install)

1. Items page: every collectible's description is readable on paper; no text
   past the paper edges; toasts fit their box; prompts don't overlap.
2. Search: "moms" finds Mom's items; "118" finds Brimstone; search plus a
   quality filter works; B deletes, Y clears, closing the menu forgets the search.
3. With The Void unlocked: beat The Lamb, then ???, then Mega Satan (three runs). A Void
   portal appears every time. After Mega Satan, the chest still ends the run.
4. With the rule off: vanilla chances return. On a save without The Void
   unlocked: no portal.
5. Mother, after Ending 21 has been seen: a portal appears next to the exit.

## Sources

- [Repentance+ (wiki.gg)](https://bindingofisaacrebirth.wiki.gg/wiki/Repentance%2B)
- [The Void (wiki.gg)](https://bindingofisaacrebirth.wiki.gg/wiki/The_Void)

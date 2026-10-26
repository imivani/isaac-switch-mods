# Iteration 8: what to check on the Switch

Build on the PC with `.\tools\build.ps1 -Mode combined-menu`, fully close
Isaac, copy the `atmosphere` folder from `dist\combined-menu` to the SD card
as in [INSTALL.md](../INSTALL.md) (merge / replace), then start the game.

## Menu readability

1. Pause, **Y**, Items. Move over several items with long descriptions (for
   example Brimstone, Sacred Heart, D6):
   - all three lines are on the paper, not on the dark torn edge;
   - coloured words (red, yellow, blue, green) are clearly readable and still
     look coloured;
   - a long description shows `1/2` in the right margin and turns pages by
     itself; **LS** turns it by hand ("More text" in the prompts).
2. Look along the bottom: no button prompts overlap, and nothing is cut off at
   the right edge.
3. Mods → Hit Counter → **Y**, and Menu: values like `< Last tab >` don't
   touch their labels.
4. Pick an item and choose *Place on a pedestal* while standing in a corner,
   to get the long "No free space nearby…" message: it fits its dark strip.

## Item search

5. Items → **RS**. Type `BRI`: Brimstone is first. **B** deletes a letter,
   **Y** clears.
6. Type `118`: Brimstone. Type `MOMS K`: Mom's Knife is listed.
7. **RS** closes the board: the list stays filtered and the top line shows
   `"MOMS K" - …`. **X** → **Up** onto Search → **Left** clears it.
8. Close and reopen the menu: the search is gone.

## Repentance+ Rules (needs a save with The Void unlocked)

9. Mods → Repentance+ Rules → **Y**: four switches, all on.
10. Beat **The Lamb**, then **???** (separate runs, or use the item menu to
    reach them quickly): a Void portal appears every time, next to the chest.
11. Beat **Mega Satan**: the run does not end by itself. A big chest and a
    Void portal appear. Opening the chest still ends the run.
12. Beat **Mother** on a save that has already seen her ending: a Void portal
    appears near the big chest. On a save that hasn't, there's no portal (that
    run plays the ending).
13. **Boss Rush**: "Wave 1/15" (and so on) appears under the hit counter while
    the waves run, and disappears when it ends.
14. Switch **Mega Satan** off and beat him again: half the time the run ends
    right away, as in vanilla.
15. On a save **without** The Void unlocked: nothing changes, no portals.

If something is wrong, note what you did and what you saw. To go back, copy
the previous release's `atmosphere` folder over it. Older builds read the
version-2 settings file and ignore the new line.

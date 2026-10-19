# Iteration 5: what to check on the Switch

Fully close Isaac before installing. After installing, launch it normally.

## 1. Mods list

1. Choose a save, then press **X** on the main menu.
2. The list has 13 entries, including **Hit Counter**, **Machine Lock** and
   **Greed Machine Never Jams**, all **ON**.
3. Select Hit Counter: the footer says `A: toggle  Y: settings  B: back`.

## 2. Machine Lock + Time Machine

1. In a cleared room with a beggar or slot machine and some coins, walk into it
   from slightly off-center (for example straight down onto its side).
2. Isaac should stay on the spot instead of sliding around it, and Time
   Machine should speed up as before.
3. Release the stick, push away, or shoot: you can move freely again.
4. Turn Machine Lock **OFF** in Mods and try again: the old sliding returns.

## 3. Greed Donation Machine

1. In Greed mode, donate many coins to the Greed Donation Machine.
2. It should never jam. If the HUD shows a jam chance, it reads 0%.
3. If you load a run where it had already jammed, it should work from the next
   floor.

## 4. Hit Counter

1. Start a run. `Hits: 0  Floor: 0` appears at the top center in the same font
   as the coin counter.
2. Take a hit from an enemy: both numbers go up by one.
3. Blood donation machine / IV Bag damage does not count. A Holy Mantle shield
   breaking does count.
4. Go to the next floor: Floor resets to 0, Hits keeps its total.
5. Hold both sticks + D-pad Down in a cleared room → X for Mods → Hit Counter →
   **Y**:
   - Position → **A**, move with the D-pad (hold to go faster), **A** to finish.
   - Size and Opacity → Left/Right.
   - Floor hits → **A** to hide the floor number.
   - Reset to defaults → **A**.
6. Settings last until Isaac is fully closed.

If anything is wrong, note what you did and what you saw. The previous working
build is backed up on the SD card by the installer, with instructions in the
deployment record's `ROLLBACK.md`.

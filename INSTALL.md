# Installing Isaac Switch Mods

## Before you start

- Your Switch runs **Atmosphère**.
- Isaac (**Afterbirth+ with the Repentance DLC**) is **updated to 1.7.9b**,
  the latest version. Start the game once after updating.
- **Fully close Isaac** before copying files: highlight it on the Home menu,
  press X, then Close.

## Install

1. Download `IsaacSwitchMods-vX.Y.Z.zip` from the
   [latest release](../../releases/latest) and unzip it on your computer.
2. Put your SD card in your computer, or connect the Switch by USB with
   **DBI → Run MTP responder** (or Hekate's USB tools).
3. Copy the unzipped **`atmosphere`** folder onto the **root** of the SD card.
   When asked, choose **merge / replace**. It only adds files inside
   `atmosphere/contents/010021C000B6A000` (Isaac's folder).
4. **Only if you use SaltyNX** (for example with FPSLocker or ReverseNX):
   open `SaltySD/exceptions.txt` on the SD card and add this line, otherwise
   the mods will not load:
   ```
   X010021C000B6A000
   ```
5. Put the card back (or close DBI) and start Isaac.

On the main menu, open the game's **Mods** screen and press **X**: the mod
menu should open. In a run, pause and press **Y**.

### Optional: Repentance+ balance changes

Unzip `IsaacSwitchMods-vX.Y.Z-balance-addon.zip` and copy its `atmosphere`
folder to the SD card the same way. It adds four data files
(`items.xml`, `items_metadata.xml`, `itempools.xml`, `pocketitems.xml`).
Finish or abandon your current run first and start a new one afterwards.
The full list of changes is in [docs/balance-changes.md](docs/balance-changes.md).

## Updating

Copy the new release's `atmosphere` folder over the old one (replace all).

## Uninstalling

Delete these from the SD card (Isaac's own game files are never changed):

- `atmosphere/contents/010021C000B6A000/exefs/subsdk9`
- `atmosphere/contents/010021C000B6A000/exefs/main.npdm`
- `atmosphere/contents/010021C000B6A000/romfs` (this mod's pictures, sounds
  and, if installed, the balance files)

If you have **no other mods** for Isaac, you can simply delete the whole
`atmosphere/contents/010021C000B6A000` folder.

Your saves are not affected. The mod menu keeps its settings in a small file,
`isaac_mods_menu.cfg`, inside Isaac's save data; the game ignores it without
the mod.

## Troubleshooting

**The game closes with "The software was closed because an error occurred."**
- Check that the game is updated to 1.7.9b and that you copied the whole
  `atmosphere` folder.
- If you use **SaltyNX**, check the `X010021C000B6A000` line from step 4.
- If it still happens, open an issue and attach the newest
  `atmosphere/crash_reports/*_010021c000b6a000.log` from your SD card.

**Nothing changed in the game.** The files are probably in the wrong place.
The path must be exactly
`atmosphere/contents/010021C000B6A000/exefs/subsdk9` on the SD card.

**Other Isaac mods.** Another mod that also uses `exefs/subsdk9` or
`exefs/main.npdm` cannot be used at the same time.

**A mod shows "unavailable".** Your game version is not 1.7.9b, so that mod
switched itself off for safety.

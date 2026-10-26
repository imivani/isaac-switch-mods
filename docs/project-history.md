# Isaac Switch mod pack

A native Switch add-on for this console's dumped **Repentance 1.7.9b**, title
`010021C000B6A000`. It recreates selected PC mod behavior using this version's
game functions and retained upstream artwork/data.

**Iteration 5 (`combined-plus`) is installed and confirmed working by the user
(2026-10-19).** It adds Machine Lock, Greed Machine Never Jams and a Hit
Counter on top of iteration 4 (`combined-refined`, kept as the backup), and
replaced only `subsdk9` (see [iteration 5](docs/iteration-5.md)).
Iteration 6 (`combined-balance`) is built and verified locally but **not yet
installed**: data-only Repentance+ item buffs/nerfs (quality, charges, tags,
pools, prices) from the PC v1.9.7.17 data, with EID and Specialist updated to
match. See [iteration 6](docs/iteration-6.md) and the full
[change list](docs/balance-changes.md). Install with
`tools\install-balance-mtp.ps1` while DBI's MTP responder runs.
**Iteration 7 (`combined-menu`) is installed (2026-10-19, record
`deployments/20261019T194355Z-combined-menu`; two earlier modules crashed:
exlaunch's hook pool was too small, then the settings code called a lazily
bound import slot directly) and awaiting console checks**: a new paper-style mod menu (Items, Mods, Run, Tools, Menu pages;
Y on the pause screen; saved settings) plus everything in iteration 6. See
[iteration 7](docs/iteration-7.md) and [the menu](docs/mod-menu.md). It can be
installed over iteration 5 or 6 with `tools\install-menu-mtp.ps1`.
Local test results are separate from console confirmation.
**Iteration 8** (2026-10-25, same `combined-menu` build mode) is written and
host-tested but **not yet built for the Switch or installed**:
- readable menu text (an ink palette for EID colours, measured layout, item
  notes that page instead of running onto the torn edge);
- item search;
- the new **Repentance+ Rules** mod (guaranteed Void portals after
  ???/Lamb/Mega Satan/Mother, Boss Rush wave counter).

See [iteration 8](docs/iteration-8.md), the [console checks](docs/iteration-8-testing.md)
and [how the rules work](docs/rplus-port.md).

## Included features

- External Item Descriptions, including supported room descriptions and the
  hold-map character/inventory browser.
- Enhanced Boss Bars, Planetarium Chance, and Regret Pedestals.
- Time Machine, Instant Fires and Poops, and Specialist for Good Items.
- Iteration 5: Machine Lock (stay against a machine or beggar while holding
  toward it), Greed Machine Never Jams, and a Hit Counter with position, size,
  opacity and floor-count settings (Mods → Hit Counter → Y).
- Animated Items and Unique Birthright pictures.
- Clean Minimap: hide the normal small-map frame.
- An item test menu with all 721 actual items in this version, descriptions,
  alphabetical navigation and type filters.
- A Mods list to turn the features on/off (13 in iteration 5). Choices last
  for the current Isaac session and reset after fully closing the game.
- Iteration 7 replaces the last two with the paper mod menu: every item,
  trinket, card, pill and pickup with filters and favourites, run stats and
  history, tools, per-mod settings, and choices saved in the save data.

This does not load the original PC Lua mods. Supported behavior is implemented
separately for this exact Switch release; full upstream feature parity is not
claimed. The existing Antibirth music and retired picture-description backups
remain outside this update's replacement operation.

## Controls

Iteration 7: pause and press **Y**, hold both stick buttons + D-pad Down in a
cleared room, or press **X** on the game's Mods screen. Full controls are in
[the menu guide](docs/mod-menu.md). Up to iteration 6:

After choosing a save, press **X** at the main menu for Mods.

In an unpaused, cleared room, hold **both stick buttons + D-pad Down for two
seconds**, then release. The item picker pauses gameplay while it is open.
Use the D-pad to select, L/R to change pages, ZL/ZR to jump letters, and Y to
filter. **A** places one selected item on a free pedestal; **B** closes the
menu. **X** switches between Items and Mods. In Mods, **A** toggles a feature.

See [full controls](docs/item-menu.md), [iteration 4 changes](docs/iteration-4.md),
[console checks](docs/iteration-4-testing.md), and
[installation and recovery](docs/refined-installer.md).

## Build and verify

From PowerShell in this folder, using the installed `IsaacDevkit` environment:

```powershell
.\tools\build.ps1 -Mode combined-refined
python .\tools\verify_refined.py
```

Iteration 5:

```powershell
.\tools\build.ps1 -Mode combined-plus
python .\tools\verify_plus.py
.\tools\install-plus-mtp.ps1   # with DBI's MTP responder running
```

Iteration 7:

```powershell
.\tools\build.ps1 -Mode combined-menu
python .\tools\verify_menu.py
.\tools\install-menu-mtp.ps1   # with DBI's MTP responder running
```

The output is `dist/combined-refined`. Its verification checks the original game
identity, compiled code, fresh local tests, every packaged resource, and the
archived installed baseline. The final report is `verification.json` in that
folder. The new installer is `tools/install-refined-mtp.ps1`; it requires the
passing reports and the exact installed baseline before changing the Switch.

Earlier package folders and deployment records are retained as history and
recovery copies. Install one version at a time and fully close Isaac first.
The source game dump is kept unchanged and is not part of a distributable pack.

## Credits

Original mods:

- [Enhanced Boss Bars](https://github.com/wofsauge/Enhanced-Boss-Bars) by Wofsauge
  and its contributors.
- [External Item Descriptions](https://github.com/wofsauge/External-Item-Descriptions)
  by Wofsauge and its contributors.
- [Planetarium Chance](https://github.com/Sectimus/isaac-planetarium-chance)
  by Sectimus and its contributors.
- [Regret Pedestals](https://github.com/seanstertm/Isaac-Regret-Pedestals)
  by seanstertm.

The native loader is based on [exlaunch](https://github.com/shadowninja108/exlaunch).
Its license is retained in `native/LICENSE`. The Planetarium Chance license
and source attribution are retained in `features/planetarium/`. Other upstream
credits and source snapshots are retained locally. The user's dumped game files
are input evidence, not files to distribute with the add-on.


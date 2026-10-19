# Third-party notices

This project re-implements PC mods for the Switch. The behaviour was rewritten
in C++, but descriptions, artwork, animations and music come from the original
mods and remain the work of their authors. Their projects do not state a
license unless noted. They are credited here; **authors who want their work
removed or credited differently can open an issue** and it will be handled.

| Component | Authors | Source | What is used here | License |
| --- | --- | --- | --- | --- |
| External Item Descriptions | Wofsauge and contributors | [GitHub](https://github.com/wofsauge/External-Item-Descriptions), [Workshop 836319872](https://steamcommunity.com/sharedfiles/filedetails/?id=836319872) | English description text (compiled into the module), inline icons and font | none stated |
| Enhanced Boss Bars | Wofsauge and contributors | [GitHub](https://github.com/wofsauge/Enhanced-Boss-Bars) | Boss bar artwork and layout data | none stated |
| Planetarium Chance | Sectimus and contributors | [GitHub](https://github.com/Sectimus/isaac-planetarium-chance) | Behaviour (re-implemented) | GPL-3.0 (`features/planetarium/LICENSE`) |
| Regret Pedestals | seanstertm | [GitHub](https://github.com/seanstertm/Isaac-Regret-Pedestals) | Behaviour (re-implemented) | none stated |
| The Specialist for Good Items v1.5.2 | Jiftoo (programming), Devector (graphics); original Pog mod authors; MMD skeleton by SAB64 | [Workshop 2575911103](https://steamcommunity.com/sharedfiles/filedetails/?id=2575911103) | Dance animations and four songs | metadata permits modifying/fixing |
| Animated Items | Workshop author | [Workshop 2570913695](https://steamcommunity.com/sharedfiles/filedetails/?id=2570913695) | 98 item animations | none stated |
| Unique Birthrights | Workshop author | [Workshop 2690434875](https://steamcommunity.com/sharedfiles/filedetails/?id=2690434875) | 41 Birthright pictures | none stated |
| Instant Fires and Poops v1.1 | Workshop author | [Workshop 2833146105](https://steamcommunity.com/sharedfiles/filedetails/?id=2833146105) | Behaviour (re-implemented) | none stated |
| exlaunch | shadowninja108 and contributors | [GitHub](https://github.com/shadowninja108/exlaunch) | Native loader and hooking library | GPL-2.0 (`native/LICENSE`); some files carry MIT notices |

## The game

The Binding of Isaac: Rebirth, Afterbirth+ and Repentance are © Nicalis, Inc.
and Edmund McMillen. No game executable, dump or audio is included.

- The mod menu's artwork file (`gfx/codex_menu/menu.anm2`) only points at the
  game's own menu pictures on the player's console.
- The small byte patterns and item names in the generated headers identify the
  exact game version and the game's own content, so the mods can check
  compatibility.
- `exefs/main.npdm` is the game's launch descriptor with four extra system
  permissions that the loader needs.
- The optional **balance add-on** contains the game's `items.xml`,
  `items_metadata.xml`, `itempools.xml` and `pocketitems.xml` with values
  changed to match the PC Repentance+ update.

# Installed UI polish

The user confirmed the combined prototype worked, then requested removal of
the old item pictures with text printed on them and a finish closer to the PC
mods. The replacement was installed and read back successfully on 2026-10-13
local time. The user then confirmed: "Normal item pictures and the new
description look good." The old printed picture text is gone and the polished
ordinary item description is readable by user report. In response to the next
boss-fight check, the user reported: "It all works and looks perfect." The
new boss portrait, bar appearance and health movement therefore also passed
on the Switch by user report. The requested visual-polish pass is complete.

## What changed

- Item descriptions use EID's original outlined lettering, purple item names,
  item pictures, colored symbols and measured, wrapped bullet points. Long
  descriptions rotate through pages. The large test panel is gone.
- Boss bars use Enhanced Boss Bars' original portraits, textured frames and
  supported special boss artwork. Several bosses fit into compact bottom rows.
  The ordinary built-in bars are hidden so they do not overlap. Gideon's wave
  counter is preserved.
- The permanent four-mod test banner is removed.
- Planetarium Chance and Regret Pedestals remain in the combined add-on. Their
  existing behavior was retained during this visual pass.
- The old picture-description overlays and their ON/OFF menu are inactive.
  Antibirth music remains installed.

The native ports still do not contain every option or behavior of the PC Lua
mods. EID's extra state-dependent interactions, cards/pills and settings remain
outside the implemented scope. Some boss-body grouping, champion colors and
status symbols remain incomplete. The user confirmed the prototype's ordinary
behavior; detailed encounter checks remain in `console-testing.md`.

## Old picture-description backup

Record: `deployments/20261014T011046Z-legacy-eid-removal/`.

All 910 description overlays were backed up from the Switch, then compared
against the known original mod files. No active old overlay path remained after
the following exact folders were renamed:

| Original SD location | Preserved inactive location |
|---|---|
| `atmosphere/contents/010021C000B6B001/romfs` | Same parent, `codex-legacy-romfs-20261014T011046Z` |
| `atmosphere/contents/010021C000B6A000/romfs/rp_patch` | Same parent, `codex-legacy-rp_patch-20261014T011046Z` |
| `mods` containing only the verified four old description-menu files | `codex-legacy-description-menu-20261014T011046Z` at the SD root |

An empty `mods` directory was recreated. SimpleModManager itself was retained.
The old overlays are also preserved under the record's `before/` directory.
All 161 music paths and byte counts remained unchanged. That is a path/size
comparison, not a fresh hash of the full music collection. No music directory
was a mutation target.

## Polished package verification and installation

Package: `dist/combined-polished/`.
Record: `deployments/20261014T012319Z-combined-polished/`.

The new 193044-byte add-on has SHA-256
`60269b3dcf93891408f8455794ce3fd39cdd6a15c180a24e3b8705754b513b38`.
Its launch descriptor is identical to the working prototype. The Isaac-only
SaltyNX exception remains present.

The package includes 222 unique art/animation assets in each of the game's two
resource layers, making 444 transferred files. Every file was read back from
the Switch and matched its local hash. The Switch texture copies preserve the
source pixels and use the game's RGBA PCX format. Every animation's required
sheet was checked. The old base `resources/gfx` contained only empty folders;
it was verified empty and archived before the new folder was installed.

The first install attempt, `20261014T011904Z-combined-polished`, stopped on that
empty-folder guard before changing any Switch files. The later record above is
the successful installation.

Host checks include every usable EID description (1,108), measured wrapping,
icon references, all packaged resource hashes, and 14 boss-renderer boundary
scenarios with memory/error checks. These checks do not establish physical
console appearance or every gameplay interaction.

## Returning to the previous working version

Fully close Isaac before changing installed files. Follow the successful
deployment's `ROLLBACK.md`: archive only the new `romfs/resources/gfx` and
`romfs/rp_patch` folders, then restore its saved prototype `subsdk9`.
The tested prototype is preserved in that record's `before/subsdk9` and on the
Switch as `exefs/codex-prototype-backup-20261014T012319Z`.

Do not remove the title's whole directory because it also contains the music.
The old picture-description mod can remain inactive when rolling back the new
visuals. Restoring it is a separate action using the removal record; do not
merge the retired `rp_patch` over the new UI folder.

Credits, detailed behavior and previews:
`eid_ui/README.md`, `eid_ui/NOTICE.md`, and `evidence/boss_ui/README.md`.

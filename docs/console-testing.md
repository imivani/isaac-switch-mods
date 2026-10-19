# Console checks

Status: **the polished build is installed. Normal item pictures, the new
description appearance, boss portraits and boss health movement passed by user
report. The requested visual-polish pass is complete**.
On 2026-10-13, the two loader-test files were copied to the Switch through DBI.
Both were read back and their SHA-256 hashes matched the checked local package.
Neither destination file nor the exefs folder existed before installation.
Existing music, description resources, cheats and resource metadata were not
changed during that installation. The combined feature build was subsequently
installed as recorded below.

Deployment record:
`deployments/20261014T002425Z-loader-test/deployment.json` (timestamp in UTC).
Computer checks and successful transfer cannot replace the gameplay checks
below.

## First attempt and scoped loading-conflict repair

The user reported that `Native mod test: LOADED` did not appear. A fresh DBI
readback confirmed both installed test files still matched the checked package.
That initial attempt did not pass the hardware test.

The Switch's `saltynx_core.log` identifies SaltyNX Core 2.1.2 and records two
redirected imports of `_ZN2nn2ro10LoadModuleEPNS0_6ModuleEPKvPvmi`, including
the added module's import at offset `0x20490`. Our startup code checks the
original SDK bytes through this pointer before installing hooks. The redirected
pointer is a concrete conflict consistent with the silent guard failure.
The subsequent retry displayed the indicator after this conflict was removed.

Evidence: `deployments/20261014T003427Z-missing-marker/`, especially
`saltynx_core.log:387-393`. The existing exceptions file did not contain Isaac.

Added exactly `X010021C000B6A000` to `sdmc:/SaltySD/exceptions.txt`.
SaltyNX 2.1.2's [exception handler](https://github.com/masagrator/SaltyNX/blob/2.1.2/saltysd_proc/source/hijack.c#L188-L199)
reads this setting on each process launch. The `X` disables its entire
bootstrap for Isaac; a plain title ID would leave Core's import changes active.
Fully closing and relaunching Isaac is sufficient to apply this exception.
SaltyNX-based FPS/graphics adjustments are disabled for Isaac during this test.

The original 51-byte file was preserved exactly in the deployment's `before/`
folder and as `sdmc:/SaltySD/exceptions.before-isaac-20261014.txt`. Direct MTP
overwrite left it unchanged, so the original was renamed before copying the
updated file. Both the updated file and remote backup were read back and their
SHA-256 hashes verified. The new file is 69 bytes, SHA-256
`fd2da4c81d51b4704773a446894baddc5a52629e8f6a0fe0e009e636afb22fa8`.

Repair record and rollback instructions:
`deployments/20261014T003759Z-saltynx-isaac-exception/`.
The loader binary, game resources, music, cheats and save data were not changed
by this repair. The user then reported **"The green message appears"** in
response to the active Repentance run check. The loader test passed on the
physical Switch by user report; the four features have not yet been tested.
The result and exact package identity are recorded in
`evidence/console-tests.json`. The combined build remains uninstalled at this
point in the record.

If this loading problem recurs, collect fresh SaltyNX logs. The process log
should confirm that Isaac is forced in `exceptions.txt` and that bootstrap was
aborted. Do not remove the binary identity guards to force an unverified hook.

## Combined build transfer

After the user reported the green loader indicator, they closed Isaac and
reopened DBI's MTP responder. `tools/install-combined-prototype-mtp.ps1`
verified the installed loader and launch descriptor, backed them up, checked
that the Isaac-only SaltyNX exclusion was still present, and installed the
checked combined module. The launch descriptor was identical and stayed in
place. Both active files were copied back from the Switch and verified:

- `subsdk9`: 163330 bytes, SHA-256
  `ab7a15007f93c7eb035ec84c4a3f9221ee073880be59fade87ff3cb38084b54b`.
- `main.npdm`: 1576 bytes, SHA-256
  `796396f15c01ea4ffd3d5cd6d265a833675ceb3d59b218dbc6d2984d620b4824`.

Deployment record and rollback instructions:
`deployments/20261014T004959Z-combined-prototype/`.
The working loader-only module is backed up locally under `before/subsdk9`
and in the Switch's same exefs folder under the unreserved name
`codex-loader-backup-20261014T004959Z`. No original game module was replaced.
Existing resources, music, cheats, saves and the SaltyNX exception were not
modified by this transfer.

The user relaunched and reported **"The four-mod message appears"** in response
to the `Isaac: four-mod test build` check. This confirms the combined module
reaches its in-game indicator. They did not separately report whether a
`Regret: hook mismatch` warning appears, so absence of that warning is not yet
established by that message alone. In response to the nearby collectible check,
the user then reported that it works, described the test as successful, and
asked for polished UI and removal of the earlier picture-description mod.
This establishes the ordinary EID interaction by user report. Their broader
"all works" report does not independently verify every edge case below.

## Polished build and old picture-description removal

The old 910 overlay files were copied to the computer and their hashes checked
before their containing directories were renamed out of the active game paths.
The four old ON/OFF menu files were also backed up and retired. The 161 music
files kept the same paths and sizes; no music directory was changed.
The separate removal record is
`deployments/20261014T011046Z-legacy-eid-removal/`.

The polished package was installed through DBI on 2026-10-13 local time
(2026-10-14 UTC). All 444 resource files were copied back and matched their
checked local hashes. The add-on and unchanged launch descriptor were also
read back and verified. Deployment and rollback record:
`deployments/20261014T012319Z-combined-polished/`.

The installed add-on is 193044 bytes, SHA-256
`60269b3dcf93891408f8455794ce3fd39cdd6a15c180a24e3b8705754b513b38`.
The previous working prototype was retained both locally and on the Switch.
This version deliberately removes the permanent test banner. It uses original
EID lettering/symbols and Enhanced Boss Bars frames/portraits. The user then
reported **"Normal item pictures and the new description look good"** in
response to the ordinary item appearance check. This confirms that the old
printed picture text is gone and the new description is readable. In response
to the boss portrait, bar appearance and damage-response check, the user then
reported **"It all works and looks perfect"**. This establishes the ordinary
boss visual and health movement check by user report. It does not individually
establish every special encounter or extended-play condition below.

The requested appearance checks are complete. No further routine test is
requested for this polish pass. See [the polish record](ui-polish.md).

## Historical first-install procedure

The following loader/prototype steps have already been completed on this
Switch. Use the polished-build check above for the current installation.

1. Fully close Isaac from the Switch HOME menu. Open DBI's MTP responder and
   connect the Switch to the computer.
2. Inspect the existing folder at
   `atmosphere/contents/010021C000B6A000/exefs`. Back up any `subsdk9` and
   `main.npdm` already there before adding these files. Leave music and other
   existing resource files in place.
3. Install the **loader-test** package first. Start Isaac, enter Repentance and
   start or continue a run. Look for **Native mod test: LOADED** at the bottom.
4. If the label appears and the game behaves normally, close Isaac completely
   and replace those two files with the **combined-prototype** package.
5. Start a run and look for **Isaac: four-mod test build**. A visible
   **Regret: hook mismatch** message means that feature did not initialize and
   must be investigated before claiming the combined build works.

The exact Switch dump is required. Other updates, regions and game editions
have not been supported by this build.

## Combined behavior

| Check | Expected result | Observed result |
|---|---|---|
| Ordinary boss | Its original portrait and live health bar appear; damage lowers the bar | Polished appearance and health movement passed by user report |
| Several bosses | Independent health values; note layout and any missing bosses | Not run |
| Friendly boss | No enemy health bar for it | Not run |
| Nearby collectible | Correct item name/text appears while nearby and hides with distance | Prototype interaction and polished ordinary item appearance passed by user report |
| Nearby trinket | Description matches the trinket | Not run |
| Hidden item / Curse of the Blind | Item descriptions do not reveal hidden identities | Not run |
| Planetarium unlocked | Percentage is shown and agrees with the game's state | Not run |
| Chance-changing item | Current floor stays fixed; a later floor updates | Not run |
| Planetarium locked / challenge | Eligibility follows the port's stated rules | Not run |
| Choice pedestal disappears | Correct item icon briefly appears, rises and fades | Not run |
| Shop choice disappears | Correct shop item picture and placement | Not run |
| Empty pedestal | No invented item icon | Not run |
| Glowing Hourglass held | New Regret tracking is suppressed, matching the default original setting | Not run |
| Enter another room | Old disappearing-item pictures are cleared | Not run |
| Pause / continue / new run | No stale chance or item pictures leak between runs | Not run |
| Antibirth music | Existing music still plays | Not run |
| Extended play | No crashes, visible stutters, or growing resource use | Not run |

Regret's original code reveals the actual item that disappeared, including
blind choice items. The description panel independently protects hidden items.

## Removing the test

Close Isaac completely. Restore the exact backed-up `subsdk9` and `main.npdm`,
or remove only those two files if neither existed before the test. Do not delete
the title's whole folder: it also contains the existing music and other mods.

If a test crashes, preserve the Atmosphere crash report and record whether the
loader-only test worked, the first missing label, and the in-game action that
triggered it. Do not mark a feature passed just because the game reached its
title screen.

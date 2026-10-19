# Installing the refined update

The new installer keeps a verified copy of the working mod before replacing it. It copies the new files into inactive folders, reads them back to check every file, then activates the completed folders. It replaces the mod program last. If an operation fails, it attempts to restore every folder independently and reports any part it could not verify.

The implementation is `tools/install-refined-mtp.ps1`. The previous extended installer and its tests are unchanged. The new script targets `dist/combined-refined` and requires passing build, resource and final verification reports before connecting to the Switch.

## Exact starting point

The required working baseline is the user's installed extended pack:

- Module SHA-256: `b0139e3bee342c43191250b506ccc8ef318d57cdba0678fc194280e15c431959`, 336,958 bytes.
- Original resource count: 1,094 files.
- Baseline package: `deployments/20261014T205717Z-combined-extended/confirmed-working-package.zip`.
- Package SHA-256: `0cde894b9f2473fab04e79af27cf464176322c03053aa629327d21314582468f`.
- The verified archive is extracted locally under `deployments/20261015T113822-iteration4-baseline/installed-package`.
- Resource-manifest SHA-256: `3bc8e96052a902acb7dd5e0dc4214033ee69babe673b728ab89419a1194ff3a5`.

The installer requires all four existing resource folders: `gfx` and `codex_specialist` inside both `romfs/resources` and `romfs/rp_patch/resources`, under title `010021C000B6A000`. Before any change it checks the live module, launch descriptor, Isaac-only SaltyNX exclusion, exact resource inventory, and a hash-verified local backup of every original resource.

## Allowed update

The new package contains 1,088 resources. Exactly 196 files change: the base and patch copies of the 98 corrected Animated Items ANM2 files. Exactly six files leave the active resource tree: `gfx/ui/ui_bosshealthbar.pcx`, `gfx/ui/ui_bosshealthbar_mini.pcx` and `gfx/ui/ui_bosshealthbar_static.pcx`, in both resource layers. Removing those transparent replacements allows the original game's boss bar to appear when Enhanced Boss Bars is switched off. The other 892 files are unchanged. Any other addition, removal or changed resource is rejected.

The two Specialist folders are present in the baseline and are backed up and verified. They are skipped during copying when unchanged. Every replaced folder is renamed to a timestamped `codex-extended-*` backup; the installer does not delete the old files. Antibirth music, saves, the launch descriptor and the SaltyNX exclusion remain outside the replacement operation.

Every asynchronous upload targets a unique `codex-staged-*` name. Transfer limits allow 256 KiB/s, one second per file and two minutes of overhead, with a five-minute minimum and one-hour maximum. Progress is printed every 15 seconds during a pending transfer. A timed-out copy may finish later, but its destination remains inactive. The active inventory is checked again immediately before each folder is activated. Duplicate inventory entries, missing files, extra files, wrong sizes and failed hashes are rejected.

Each run writes its backup/readback evidence and `deployment.json` under a timestamped `deployments/*-combined-refined` folder. A successful run also writes `ROLLBACK.md`. On failure, independent restoration attempts cover the module and every changed resource folder, including the two already-existing Specialist folders. A failed restoration or failed readback is recorded as `TRANSFER_FAILED_RECOVERY_INCOMPLETE`; it is never reported as a confirmed recovery.

## Local verification

`pwsh -NoProfile -File tools/tests/install-refined-mock.ps1` passed **93 local cases**, with zero COM objects and zero device operations. The runner executes the actual installer's parsed functions and complete top-level transaction against simulated folders and transfers. It covers all four existing folders, module-last activation, backup retention, failure before/after renames, independent recovery, missing/extra/wrong-size stages, failed hashes, changed active inventories, delayed and timed-out transfers, duplicate inventories, exact local package contents, baseline guards and the permitted resource changes.

The actual current 1,088-file package and the 1,094-file baseline also passed local file-size/hash checks. The longest staged/readback path is 188 characters, below the installer's 250-character limit. Results and source hashes are in `evidence/installer-refined-mock/report.json`; baseline provenance and unchanged prior-tool hashes are in `baseline-provenance.json` beside it.

These checks do not exercise the live Windows file-transfer provider or the Switch. The parent task owns the final build verification, device transfer and console test. No Switch installation was performed while preparing this installer.

# Iteration 3: inventory browsing and additional Workshop mods

This build continues the exact Repentance 1.7.9b Switch port. It is a native adaptation of selected original Workshop code and artwork; it does not run arbitrary PC Lua mods.

Status: installed, file-verified, and confirmed working by the user on the Switch. The successful installation record is `deployments/20261014T205717Z-combined-extended`; its `console-tests.json` records the live checks separately.

The final module is 336,958 bytes, SHA256 `b0139e3bee342c43191250b506ccc8ef318d57cdba0678fc194280e15c431959`. Verification checked 279 compiled source inputs, 1,094 packaged resource files (547 unique assets), and 19 current test, review and asset reports. The original game files and all 444 resources from the installed pack remain unchanged. The machine-readable local-check result is `dist/combined-extended/verification.json`. It retains its original pre-install `NOT_RUN` console field; subsequent live confirmation is recorded separately in the deployment's `console-tests.json`.

## Requested changes

- EID Map-held inventory and character browsing: fix the controller-ID rejection found in the installed build, including handheld device 33 and controller 8. The same mistaken cap was removed from special EID input and Time Machine.
- Instant Fires and Poops (2833146105): original default behavior, after a hit in a cleared room. Ordinary orange fireplaces and poop only; native game destruction preserves drops.
- Animated Items (2570913695): original animations for the 98 supported vanilla collectibles, with original artwork.
- Unique Birthright Sprites (2690434875): original pictures for 41 vanilla character types, selected from the relevant player.
- The Specialist for Good Items (2575911103): original character dance costumes and music when a visible qualifying item appears. Existing music resumes after the effect.
- A deliberate testing shortcut: both stick clicks plus D-pad Down for two seconds in a cleared room, spawning six normal free item pedestals.

## Baseline and preservation

The previous installed module was SHA256 `9b127d356d4bf7ede680348f0baa76ab18891c3d7313370e534b8bbc0cf99be7`.
Its complete package and starting source are retained under `deployments/20261014T200957Z-iteration3-baseline`.
The game dump is an immutable analysis input. Saves, the original game installation, Antibirth music files and the Isaac-only SaltyNX exception are outside the new installer changes. The launch descriptor remains byte-identical to the installed one.

The prior four console checks passed: launch/Planetarium, sacrifice rewards, one Pin bar and Time Machine acceleration/release. Map browsing was subsequently reported broken and is recorded as a failure in that deployment's test log. For this build, the user subsequently confirmed: "Everything works perfect. I love it." This is a general gameplay confirmation; the detailed edge cases were not separately enumerated.

The initial USB transfer reached the old 60-second limit before all larger artwork files arrived. The installer restored the previous resource folder, and a separate readback verified all 222 restored files. The retry used the revised installer, which passed 61 local checks: it uploads and hash-checks inactive folders before activation, allows time based on package size, and keeps Windows paths short. All 1,094 resource files and the new module then verified successfully. The previous module and resource folders remain backed up on the Switch and locally. The launch descriptor and SaltyNX exception file were checked again and remain unchanged.

## Source provenance

The four Workshop folders were found locally under the Steam Workshop directory for app 250900. They are read-only inputs; source snapshots, hashes and namespaced resource imports are retained in this project. No Steam files or original game textures are modified by the importer.

Distribution and complete upstream parity have not been claimed. Optional PC mod configuration menus, third-party character integrations and later PC APIs are outside these vanilla Switch adaptations. See the individual port notes for exact supported behavior and console checks.

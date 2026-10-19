# Requested second iteration

Scope: TimeMachine[Repentance] (Workshop 2617557401), native Planetarium Chance
stat styling, EID room/pocket/character features, and segmented boss health bars.

The known-working installed package is backed up at
`deployments/20261014T184751Z-iteration2-baseline`. Its module SHA-256 is
`60269b3dcf93891408f8455794ce3fd39cdd6a15c180a24e3b8705754b513b38`.
The new package is `dist/combined-expanded`; the prior package is retained.

## Work and evidence

- Boss ownership: independent worker; native parent links and upstream rules,
  tests for one segmented boss, multiple separate bosses, deaths and splits.
- Planetarium: independent worker; actual stat font and observed native layout.
- EID: independent worker; source-backed compatibility inventory and expanded
  description selection. Full upstream parity must not be inferred from the
  number of catalogue entries or from a successful compilation.
- Time Machine: capped native update acceleration while using supported machines;
  original game timers and temporary effects advance together. See the separate
  Time Machine port notes for deliberate platform differences.
- Combined integration: new build mode, guarded hooks, offline checks, isolated
  package and a console test checklist.

## Build and verification result

The combined Switch build and package checks passed on October 14, 2026.
The module is 294,605 bytes, SHA-256
`9b127d356d4bf7ede680348f0baa76ab18891c3d7313370e534b8bbc0cf99be7`.
The package contains 444 verified resource files across the two game layers.
The launch descriptor is identical to the installed polished package.

`evidence/expanded-verification.json` checks the packaged files, all 249 recorded
source snapshot files, nine current test reports, the untouched original dump,
and the retained known-working backup. The original item picture replacements
stay retired; installation is limited to the native mod file and its two known
graphics folders. The music, save data and Isaac-only launch exception are
outside the write scope.

Notable local checks include 55 boss renderer cases, 125 Time Machine cases,
3,557 contextual EID descriptions and 7,322 measured layouts, 17,547 special EID
checks and 1,458 layouts, and 13 simulated installer/recovery cases. These
exercise the port code and packaging, not the running Switch game.

See `eid-controls.md` for player controls, `eid-coverage.md` for the exact EID
feature checklist, and `eid-special-port.md` for Flip/Crown interaction rules.
Full crafting searches, some dynamic interactions and reminder categories,
settings/language selection and the PC Lua extension API remain outside this
build. It must not be described as complete upstream EID parity.

Status: installed through DBI on October 14, 2026. The installed module and all
444 resource files were copied back and matched the package. The launch
descriptor was retained and verified. Installation, backups and restoration
instructions are recorded in `deployments/20261014T194151Z-combined-expanded`.
The user confirmed normal game launch and matching Planetarium stat appearance
with "Works perfectly" in response to that specific check on October 14, 2026.
The user also confirmed that sacrifice-room descriptions appear and update to
the next reward after use. A single Pin was confirmed to show one health bar
that works correctly, including damage and diving/reappearance. The user also
confirmed that Time Machine speeds up repeated interactions and returns to
normal when released. These four primary console checks passed. The other EID
feature checks and broader regression cases remain pending.
Exact observations and their scope are recorded in that deployment's
`console-tests.json`; these confirmations do not establish all features.

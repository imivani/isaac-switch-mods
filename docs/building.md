# Building from source

Players do not need this; download a release instead.

## What you need

- Windows 10/11 with PowerShell, and WSL with a Linux distribution that has
  **devkitPro devkitA64** installed at `/opt/devkitpro` (the build uses a
  distribution named `IsaacDevkit` by default; set `ISAAC_WSL_DISTRO` to use
  another).
- Python 3.11+ with `Pillow` and `lz4` (`pip install pillow lz4`). Set
  `ISAAC_PYTHON` if `python` on your PATH is not the right one.
- **Your own dump** of Isaac 1.7.9b (`010021C000B6A000`, update v524288) in a
  folder named `Isaac-dump` next to this repository, with `exefs/main.npdm`,
  `romfs/.nro/Repentance.nro` and the `romfs` resources. The build checks the
  exact file hashes and derives `main.npdm` from your copy. No game files are
  in this repository.

## Build

```powershell
.\tools\build.ps1 -Mode combined-menu
```

The package appears in `dist/combined-menu`. The compiled module alone can
also be built with `make` inside `native/` from WSL, since the generated
headers are checked in.

## Layout

| Folder | Contents |
| --- | --- |
| `native/` | exlaunch-based Switch module (`subsdk9`); `source/program/` holds the mods |
| `features/` | Portable logic for each mod, shared with the host tests |
| `eid/`, `eid_ui/`, `eid_special/` | Item descriptions: data, layout and drawing |
| `assets/` | Artwork and animations packaged into `romfs` |
| `tools/` | Generators, packaging, checks and test runners |
| `docs/` | Notes on each port |

Every game function the mods call is matched byte-for-byte against 1.7.9b
before anything is hooked (`native/source/program/generated/*_guards.hpp`).

## Tests

The test runners (`tools/*/run_tests.py`, `tools/menu_ui/run_native_tests.py`,
...) compile the production code against fake game boundaries with GCC on
Linux/WSL, including AddressSanitizer and UndefinedBehaviorSanitizer runs.
Some generators and checks need files from your own dump or the PC game, which
are not part of this repository.

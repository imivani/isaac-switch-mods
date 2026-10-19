# Native Switch injection research

Date: 2026-10-13. Scope: read-only source and dumped-file research for Isaac 1.7.9b, program ID `010021C000B6A000`. No console files, original dump files, or Ghidra project were modified by this investigation.

## Decision

Use **exlaunch in Module mode**, with a new `subsdk9`. Keep the original `rtld`, `main`, `subsdk0`, `subsdk1`, and `sdk`. Derive the descriptor from the user's original `main.npdm`; do not install exlaunch's sample descriptor. Resource mods, including Antibirth music, can remain in the existing `romfs` overlay.

This is a source-supported implementation route, not a claim of on-console compatibility. It still needs a successful build, load test, and game test.

The cloned upstream reference is `reference/framework-research/exlaunch` at commit `f9f4b0dd07b68f97958cb9c79228bbca22ca80d5`. Its GPL-2.0 license applies to framework reuse.

## Why this framework

| Choice | Source observations | Decision |
|---|---|---|
| exlaunch | General C/C++ game injection; `LOAD_KIND=Module` emits `subsdk9`; provides trampolines and inline hooks; source exposes SDK imports through ordinary linking. | Best fit for a title-specific mod. |
| SaltyNX | General sysmodule with plugin support and existing FPS/graphics features. Current source intercepts `nn::ro::LoadModule` and updates replacements in dynamically loaded NROs. README explicitly recommends exlaunch for title-specific code injection. | Technically possible, but adds dependence on a shared system injector and its own loader hook. |
| Skyline | Main upstream is a Smash-focused environment. Its reversed `nn::ro` declarations are useful ABI references. | Do not presume its complete runtime is a drop-in Isaac loader. |

SaltyNX reference commit: `72557bdf5a9cd1a17e85f9535e512e3904754aaf`. Older claims that SaltyNX cannot handle NROs are stale: current `saltysd_dynamic.c` has a LoadModule hook. However, its normal symbol lists do not automatically become a complete game-specific dynamic-module API. Its own LoadModule interception is also a potential interaction to test if SaltyNX is already active.

Sources:

- [exlaunch Makefile](https://github.com/shadowninja108/exlaunch/blob/f9f4b0dd07b68f97958cb9c79228bbca22ca80d5/Makefile)
- [exlaunch hook example](https://github.com/shadowninja108/exlaunch/blob/f9f4b0dd07b68f97958cb9c79228bbca22ca80d5/source/program/main.cpp)
- [SaltyNX README](https://github.com/masagrator/SaltyNX/blob/72557bdf5a9cd1a17e85f9535e512e3904754aaf/README.md)
- [SaltyNX NRO loader interception](https://github.com/masagrator/SaltyNX/blob/72557bdf5a9cd1a17e85f9535e512e3904754aaf/saltysd_core/source/saltysd_dynamic.c)
- [Atmosphere per-title ExeFS override](https://github.com/Atmosphere-NX/Atmosphere/blob/master/docs/components/modules/loader.md)

## Build requirements

The checked exlaunch revision uses devkitPro `libnx/switch_rules`, devkitA64, GNU make and a Bash shell, Python 3, `elf2nso`, and `npdmtool`. It compiles C++23, targets ARMv8-A/Cortex-A57, and expects a `DEVKITPRO` directory. The standard devkitPro Windows installer offers a Switch Development group (`switch-dev`) and MSYS2. Its installer source is the primary reference for the Windows bundle. No Java8 or x86 compiler is involved in compiling this mod.

Use a build path without spaces, or verify every upstream shell recipe's quoting. Do not run the upstream FTP/deploy targets blindly; their example destination is unrelated to this game. The build emits a sample `main.npdm` alongside `subsdk9`, and that sample must be excluded from installation.

- [Build flags and tool dependencies](https://github.com/shadowninja108/exlaunch/blob/f9f4b0dd07b68f97958cb9c79228bbca22ca80d5/misc/mk/common.mk)
- [devkitPro Windows installer](https://github.com/devkitPro/installer/blob/master/nsis/devkitPro.nsi)

## Dynamic Repentance module

The user's dump shows that `main` is a launcher; the real Repentance code is in `rom:/.nro/Repentance.nro`. Consequently, `InstallAtOffset()` is inappropriate for a Repentance function because exlaunch interprets it relative to the static main module. Its built-in memory inventory finds static NSOs, not later NROs.

Recommended sequence:

1. Install a trampoline on the SDK's five-argument `nn::ro::LoadModule`.
2. Before calling the original, inspect the input NRO header and retain whether its size and build ID match the supported Repentance dump. Do not save an unbounded pointer to its temporary input buffer.
3. Call the original with all arguments unchanged. Install no game hooks if it fails.
4. Resolve known Repentance exports with `LookupModuleSymbol` on the returned module. Validate the loaded function bytes and derive a base using a confirmed function offset if needed.
5. Use absolute function pointers or `InstallAtPtr` for the game hooks.
6. Keep original calls and avoid out-of-band reads of entity pointers that can be destroyed. Account for module unloading if the implementation retains global pointers beyond hook callbacks.

Confirmed SDK dump strings:

```text
_ZN2nn2ro10LoadModuleEPNS0_6ModuleEPKvPvmi
_ZN2nn2ro10LoadModuleEPNS0_6ModuleEPKvPvmib
_ZN2nn2ro18LookupModuleSymbolEPmPKNS0_6ModuleEPKc
```

The five-argument form is the launcher import. These community-reversed ABI declarations match its mangled name:

```cpp
namespace nn::ro {
    class Module;
    Result LoadModule(Module*, const void* image, void* bss, size_t bssSize, int flags);
    Result LookupModuleSymbol(uintptr_t* outAddress, const Module*, const char* name);
}
```

Use an opaque `Module` and the exported lookup function to avoid assuming internal SDK structure offsets. In particular, the input `image` pointer is not guaranteed to be the final executable mapping.

The exlaunch hook implementation supports targets farther than the ARM64 relative-branch range. It writes an absolute `LDR X17; BR X17` sequence, taking four or five instructions. Validate at least the first 20 bytes and sufficient function length. The eight-byte `IsBoss` functions are unsuitable for this kind of far trampoline. The current `RwPages` alignment helper also assumes a hook write does not straddle a page unless its size is adjusted; verify `(address & 0xfff) + 20 <= 0x1000` for selected sites or fix the range calculation.

- [Dynamic module ABI](https://github.com/skyline-dev/skyline/blob/master/include/nn/ro.h)
- [Static module discovery](https://github.com/shadowninja108/exlaunch/blob/f9f4b0dd07b68f97958cb9c79228bbca22ca80d5/source/lib/util/sys/mem_layout.cpp)
- [Absolute branch and trampoline implementation](https://github.com/shadowninja108/exlaunch/blob/f9f4b0dd07b68f97958cb9c79228bbca22ca80d5/source/lib/hook/nx64/hook_impl.cpp)

## Minimum descriptor changes

Original source: `../Isaac-dump/exefs/main.npdm`, 1572 bytes, SHA256 `d325e6e0fdec64053c0efd7e2bf1cd679215b10772c410a91247e1d41bae4c5e`.

Original ACID is at `0x80`, size `0x3d4`; original ACI0 is at `0x460`, size `0x1c4`. Both KAC lists contain nine descriptors. Both filesystem masks are `0x4000000000000000`; service lists match; main priority is 44, main CPU is 0, stack size is `0x100000`. Permitted thread-priority values include 28 through 59, and CPU IDs 0 through 2. Existing debug permission is enabled. Those fields need no change for the framework.

The ordinary exlaunch module init, memory inspection, logger, and hook paths already have their required SVCs except:

- `0x74` MapProcessMemory, used to make a writable alias of executable memory.
- `0x75` UnmapProcessMemory, used to release that alias.

The current Atmosphere path obtains the current-process handle using existing `svcGetInfo`. exlaunch also contains an older-system fallback using IPC. To preserve that unmodified fallback, add:

- `0x40` CreateSession.
- `0x43` ReplyAndReceive.

The fallback's thread priority 32 and CPU2 already fit the original restrictions. Cache flushes in this implementation use ARM instructions; CreateCodeMemory/ControlCodeMemory are not needed. Secure-monitor helpers exist in the source but are not used by this Module-mode path.

The exact four-SVC-only modification plan is:

| File offset | Old value | New value | Purpose |
|---|---:|---:|---|
| `0x43c` | `0x40000f8f` | `0x41200f8f` | ACID existing SVC group2, add40 and43 |
| `0x60c` | `0x40000f8f` | `0x41200f8f` | ACI0 existing SVC group2, add40 and43 |
| `0x454` | zero padding | `0x8600000f` | ACID new SVC group4, add74 and75 |
| `0x2b4` | 36 | 40 | ACID KAC byte count |
| `0x7c` | 980 | 984 | META ACID size |
| `0x284` | `0x2d4` | `0x2d8` | ACID inner size |
| `0x494` | 36 | 40 | ACI0 KAC byte count |
| `0x74` | 452 | 456 | META ACI0 size |
| `0x624` (append) | end of file | `0x8600000f` | ACI0 new SVC group4, add74 and75 |

This fits the ACID's existing 12-byte padding before ACI0. The resulting file is 1576 bytes. An in-memory-only check confirmed that the resulting SVC sets add exactly `{0x40, 0x43, 0x74, 0x75}` to both blocks and remove none. No patched descriptor was written by this research task. Implementation should assert the input hash and layout before generating a separate derived file, then verify all original FS/service blocks and unrelated bytes remain unchanged. The modified descriptor relies on Atmosphere's per-title override behavior and is not a newly Nintendo-signed descriptor.

- [SVC use in exlaunch writable aliases](https://github.com/shadowninja108/exlaunch/blob/f9f4b0dd07b68f97958cb9c79228bbca22ca80d5/source/lib/util/sys/rw_pages.cpp)
- [Current-process handle and fallback](https://github.com/shadowninja108/exlaunch/blob/f9f4b0dd07b68f97958cb9c79228bbca22ca80d5/source/lib/util/sys/cur_proc_handle.cpp)
- [Descriptor format and generation](https://github.com/switchbrew/switch-tools/blob/master/src/npdmtool.c)
- [Descriptor decoding](https://github.com/SciresM/hactool/blob/master/npdm.c)
- [Atmosphere descriptor override validation](https://github.com/Atmosphere-NX/Atmosphere/blob/master/stratosphere/loader/source/ldr_meta.cpp)

## Native EID precedent and search boundary

[IsaacExternalItemDescriptionsiOS](https://github.com/emp0ry/IsaacExternalItemDescriptionsiOS) is a useful native ARM64 reference. Its README identifies iOS1.4, embedded Repentance1.7.9b, version-specific layout guards, bounded entity snapshots, and an importer that combines upstream EID `ab+` base tables with `rep` overrides. Its loader and UIKit renderer are iOS-specific, and none of its offsets should be copied into the Switch build without independent dump validation.

A bounded GitHub-oriented web search for native Switch Isaac/EID/exlaunch/subsdk9 ports did not identify a ready-made native Switch EID port. This is not evidence that no such port exists. No claim about the presence or absence of Lua inside the Switch binary is made by this framework investigation.

## Remaining runtime questions

- Does this exact game load the extra `subsdk9` and modified descriptor cleanly on the user's current Atmosphere setup?
- Does exlaunch's mapped-memory alias work for the dynamically loaded Repentance module on this firmware?
- Does existing SaltyNX interception interact with the new SDK function trampoline?
- Are the selected HUD and pickup function signatures, object offsets, coordinate units, and render timing correct in gameplay?
- Is the first overlay readable in handheld and docked modes, and are Antibirth music and save behavior preserved in a real run?

These need the build and console test. Source review and file validation alone cannot answer them.

## Toolchain download fallback verified 2026-10-13

Live HEAD requests to both `pkg.devkitpro.org` and `downloads.devkitpro.org` returned HTTP403 for the package databases and the MSYS bootstrap archive. Current devkitPro/buildscripts GitHub releases exposed no binary assets. No working official native-Windows mirror was established by this check.

A Docker-engine-independent route was verified at the registry/API level:

1. Obtain Google's official `crane.exe` release from [go-containerregistry v0.22.1](https://github.com/google/go-containerregistry/releases/tag/v0.22.1).
2. Use `crane export` to flatten devkitPro's official Linux image into a root filesystem tar.
3. Import that tar into a new isolated WSL2 distribution. This runs the real devkitPro Linux compiler and patched linker locally, without Docker Desktop's engine.

Verified crane asset:

```text
https://github.com/google/go-containerregistry/releases/download/v0.22.1/go-containerregistry_Windows_x86_64.tar.gz
Bytes: 16978582
SHA256: 0e073ea8192c3b8442ec8aaf44d53c1050a09084669fae3a6ceb0f2026cf8b21
Checksum source: https://github.com/google/go-containerregistry/releases/download/v0.22.1/checksums.txt
```

Verified devkitPro image:

```text
devkitpro/devkita64@sha256:82575ea78651b530b2e232bb3799cfd1fe331514e053d5f724bb4b28191fb79d
Platform: linux/amd64
Created: 2026-02-19T17:58:02.285642469Z
Compressed layer total: 704671158 bytes (approximately 672 MiB)
```

Its official Dockerfile installs `switch-dev`, `switch-portlibs`, devkitARM, dkp-toolchain-vars, and hactool. The live registry manifest contains four layers. No image layer was downloaded or installed by this read-only research task.

Example commands for the parent implementation task:

```powershell
crane.exe export --platform linux/amd64 devkitpro/devkita64@sha256:82575ea78651b530b2e232bb3799cfd1fe331514e053d5f724bb4b28191fb79d C:\path\devkita64-rootfs.tar
wsl --import IsaacDevkit C:\CodexTools\ReverseEngineering\IsaacDevkit C:\path\devkita64-rootfs.tar --version 2
```

Image environment variables live in image metadata and are not automatically recreated by extracting its filesystem. For the WSL build, explicitly set `DEVKITPRO=/opt/devkitpro` and include `/opt/devkitpro/tools/bin` and `/opt/devkitpro/devkitA64/bin` in `PATH`. Imported WSL distributions default to root; no interactive user-account setup is necessary for a bounded build command. The C++23 compiler version and `--nx-module-name` linker support still need an actual smoke test after import.

- [Official devkitPro image definition](https://github.com/devkitPro/docker/blob/master/devkita64/Dockerfile)
- [Crane filesystem export](https://github.com/google/go-containerregistry/blob/main/cmd/crane/doc/crane_export.md)
- [Microsoft WSL tar import](https://learn.microsoft.com/en-us/windows/wsl/use-custom-distro)

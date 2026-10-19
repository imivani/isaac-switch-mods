# The Specialist for Good Items — Switch adaptation

The character performs the original Specialist dance when a visible high-quality item is in the room. This includes the original character-specific dance pictures, the Binky animation, and the four original songs. Taking the last qualifying item or leaving the room stops the dance. The normal room music then resumes through the game's own music selection, which keeps the existing Antibirth music replacement available.

The implementation uses the installed **The Specialist for Good Items v1.5.2**, Steam Workshop item **2575911103**. It is a native adaptation of that mod's default behavior for the exact Switch Repentance 1.7.9b build. It does not load the PC Lua script.

## Included behavior

- Quality-four collectibles trigger the dance. The original special list also includes Cricket's Head, Magic Mushroom, Brimstone, and Sacred Heart.
- All 41 native character types have their original costume mapping. Binky adds its separate animation.
- The original reduced-flashing default is enabled: Tainted Eden wears regular Eden's dance costume while keeping the Tainted Eden song.
- Classic characters, tainted characters, Tainted Cain, and Tainted Eden select the original four songs. In co-op, the source's player order and secondary-body exclusions determine the song; every supported player can dance.
- Curse of the Blind, forced-blind pedestals, unknown item pictures, and invisible items do not reveal a good item's identity through the dance. Alternative-path treasure rooms retain the source's visible-pedestal rules.
- Rerolls and same-count item changes are checked on ordinary game updates. A continuing dance does not restart its animation each frame.
- A room entry starts a new music interval even when the game reuses the same room object. This includes the case where one good-item room follows another and the game already changed the background song.
- If another part of the game replaces the dance music during the same room visit, this feature does not take it back or overwrite it when stopping.

There is no new button to press. The PC configuration menu, custom character extension API, user-edited quality thresholds, and custom song aliases are not included. The original default settings are compiled into this build. No on-console visual or audio result is claimed by the automated checks below.

## Original files and isolated resources

`assets/specialist/manifest.json` records the exact installed source files and their SHA-256 hashes. The source folder is `C:\Program Files (x86)\Steam\steamapps\workshop\content\250900\2575911103`.

The prepared resources contain:

| Resource | Count | Prepared location |
| --- | ---: | --- |
| Character and Binky animation files | 42 | `gfx/codex_specialist/` |
| Original referenced pictures | 42 | `gfx/codex_specialist/` |
| Original songs, unchanged bytes | 4 | `codex_specialist/music/` |
| Costume and music registration files | 2 | `codex_specialist/` |

The shared packager converts the pictures into the Switch texture format and places the same resources in the base and Repentance resource layers. No existing costume file, music entry, or Antibirth track is replaced.

The source XML contains one unused reference to `specialist_t_samson2.anm2`, which is absent from the installed mod and is not used by its character mapping. It is excluded rather than invented. The original four referenced songs are retained; two other unreferenced OGG files are not needed. The source music XML has a malformed comment, so preparation removes comment text before parsing the actual four track definitions.

The original animation files also contain **1,024 crop rectangles extending below their original picture sizes**, among 19,883 total frame rectangles. These are inherited source data, including head and walking animations. The checker verifies that the entire animation data and picture bytes match the original, records the inherited exceptions, and preserves them. It does not silently stretch or pad the artwork. Their actual rendering remains part of the console check.

Credits and the source's modification permission are retained in `assets/specialist/credits.txt` and the hashed source metadata. This local adaptation does not imply a new license for the original music or art.

## Registration and lifetime evidence

All native addresses below apply only to the immutable NRO with SHA-256 `cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a`. Thirteen exact function-entry byte guards are generated from that file. A mismatch disables this feature.

| Evidence | Verified behavior used here |
| --- | --- |
| `evidence/iteration3/native-core/3b9448.c` and `.asm.txt` | `ItemConfig::LoadCostumes(config, path, ModEntry*)`: a null third argument selects existing-ID replacement. A genuinely constructed non-null object selects appending for `type="none"`. |
| Same function, `0x3b9e40..0x3ba084` | The append branch allocates independent item objects, copies the costume path and flags into them, and does not retain the temporary ModEntry or its maps. |
| Same function, `0x3b96cc` and `0x3ba1fc` | Failure returns zero and success returns one in `w0`; the adapter uses a Boolean result. |
| `evidence/specialist/native/420d18.*`, `41f8dc.*`, and `evidence/specialist/allocation/41fb0c.asm.txt` | The real ModEntry constructor/destructor are available. The native allocation at `0x41fcec` is exactly `0x12b8` bytes. The adapter constructs aligned storage and destroys it immediately after the independent item copies are registered. |
| `evidence/iteration3/native-core/425218.*` | `Music::LoadConfig(music, path, true)` appends tracks and assigns their IDs from the current count. False would clear the existing registry and is never used. The missing-file branch at `0x425254` returns a zero pointer/register value; success at `0x426a38` sets `w0=1`. |
| `evidence/specialist/paths/41fb04.asm.txt` and `evidence/native-analysis/plt.tsv` | Switch path redirection copies the path string: the tail target `0x670140` is the native `std::string` copy constructor. Namespaced paths remain usable. |
| `evidence/iteration3/native-core/427238.*` and `451834.*`; `evidence/specialist/native/426c38.*` and `427908.*` | The adapter plays a registered song, applies the user's music volume, checks the current track, and restores music through `Room::PlayMusic`. The music object belongs to the manager at `+0x36068`. |
| Existing native room-update evidence under `evidence/timemachine/native/46d788.c` | Room `+0x191c` is the entry-frame stamp; game `+0x24f99c` is the ongoing game frame. The adapter uses both to detect reused-room entries and avoid duplicate update work. |

Costume IDs and music IDs are discovered after appending; none are hard-coded into the original registries. The adapter checks that all original costume pointers remain in their previous positions and that exactly 42 costumes and four songs were added. Subsequent initialization can reuse the namespaced entries. Partial or malformed registration disables dancing rather than applying an uncertain ID.

`ResetSpecialist()` removes only the tracked dance costumes and restores music only while the feature still owns the current track. It runs before an attempted native module unload. Registration bookkeeping remains valid if that unload fails. `OnSpecialistModuleUnloaded()` clears bookkeeping only after a successful unload and makes no native calls. The temporary loader object has already been destroyed, so no native destructor is deferred until after its code is gone.

## Checks and remaining console verification

`features/specialist/test-results.json` records **PASS: 86,266 checks across 98 actual adapter frames in each of two modes**, optimized and undefined-behavior-sanitized. The test compiles the real `specialist.cpp`; game calls and memory are replaced by bounded test fixtures. It covers registration and original-entry preservation, all 41 character mappings, Binky, reduced flashing, co-op, animation continuity, music ownership, room transitions including a reused pointer with changed entry/music, failed unload recovery, hidden items, bad data, and exact-code guards. This is not a console gameplay test.

`assets/specialist/validation.json` records **PASS** for the complete prepared resource tree: 42 costumes, 42 unchanged pictures, 247 source-identical animations, 19,883 frame rectangles, and four unchanged Ogg Vorbis songs. It checks all file references and hashes, records the inherited crop exceptions, and includes the checker and input hashes for freshness verification.

Reproduce the checks with:

```powershell
& '%USERPROFILE%\AppData\Local\Programs\Python\Python311\python.exe' tools/specialist/check_assets.py
wsl -d IsaacDevkit --exec /bin/bash -lc 'cd %USERPROFILE%/Desktop/sw/Isaac-switch-mods && python3 tools/specialist/run_tests.py'
```

On the console, verify the original dance and audio with a visible quality-four pedestal; taking it should stop the dance and restore the current room's music. Then check a second good-item room, a tainted character, Binky, co-op, and a blind pedestal. Console-specific texture loading, audio playback, and the inherited source crop behavior remain the important live checks.

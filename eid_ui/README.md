# EID display for the native Switch mod

This replaces the large description panel with compact, floating text that uses
the original PC mod's outlined pixel font, purple item name, small item picture,
inline symbols, colors, and aligned bullet points. It changes how descriptions
look; the caller still decides which nearby item is visible and safe to describe.

## Integration

Include `native/source/program/eid_ui.hpp` and call:

```cpp
isaac::RenderEidDescription(frame, category, lookup_id, description);
```

The call accepts the existing `FeatureFrame` and `isaac_eid::Description` types.
Use it only after selection, hidden-item, Curse of the Blind, and HUD visibility
checks accept an item. The expanded native adapter uses
`RenderEidContext(frame, selection, contextDescription)` for room, card, pill and
state-aware descriptions. It hashes text contents, so a reused mutable buffer
updates correctly when the sacrifice count or player state changes. The parent
overlay owns the shared HUD checks; `eid_features.cpp` owns contextual selection.

Call `isaac::ResetEidUi()` **before** unloading the game module, while its sprite
functions remain callable. It releases owned sprites and clears cached layouts.
Do not copy or move the owned `NativeSprite` objects. The parent integration also
guards the exact supported game version before calling this renderer.

The implementation lives in `native/source/program/eid_ui.cpp`; the portable
layout code is `layout.hpp`. Text is measured with the real bundled font's
character widths and drawn through verified game sprites. It does not call an
unverified native font interface.

## Assets

`assets-manifest.json` records the pinned source revision, source checksums,
resource checksums, and game-relative paths. `resources/gfx/codex_eid_ui/` has
12 files: the two original font sheets, their generated glyph animation, four
original icon sheets with their animations, and a generated item-picture
animation. There are99 font glyphs and281 static icon mappings.

The common packaging step converts these PNG files into the game's PCX texture
format and stages the folder into both `resources` and `rp_patch/resources`.
Animation files retain logical `.png` names, matching the original game's
resource convention. Controller icons use the game's existing
`gfx/ui/buttons.anm2`; item pictures come from the game's item configuration at
runtime. Extracted game pictures are not added to this resource bundle.

Regenerate source assets from the pinned checkout with:

```powershell
python eid_ui/generate_assets.py
```

The script refuses a different upstream revision. See `NOTICE.md` and the
unchanged upstream credits for attribution.

## Verification

The saved results are `test-results.json` and `asset-check-results.json`.
The reports record their exact run time and input hashes. Both the optimized
build and the undefined-behavior check passed with GCC12.2.0 and warnings treated
as errors. The separate context suite checks the expanded descriptions and
state changes; see `../docs/eid-coverage.md`.

The tests cover all 1,108 usable descriptions in the imported catalogue:

- Every actual character has a font glyph, and every markup token is classified.
- Wrapping stays inside the measured width; continuation lines align with text.
- Colors, reset tags, item icons, malformed input, Unicode, and long words are
  checked explicitly.
- No raw markup is exposed. No current description needs a variable-containing
  paragraph omitted.
- The largest description uses 17 lines and 274 drawing cells. Every item name
  fits within the three-line title limit.

The asset check confirms all 99 glyph frames match the original font metrics
and all 120 static icon references have valid frames and image crop bounds.

Run the checks from the project root:

```powershell
wsl -d IsaacDevkit --exec /bin/bash -lc 'cd %USERPROFILE%/Desktop/sw/Isaac-switch-mods && python3 eid_ui/run_tests.py'
& '%USERPROFILE%\AppData\Local\Programs\Python\Python311\python.exe' eid_ui/check_assets.py
```

`preview.png` renders the actual C++ layout with the original font and icon
pixels. It shows The Sad Onion, Sacred Heart, Book of Virtues, and Cancer. It is
a desktop preview, not a Switch screenshot. Its Book of Virtues item thumbnail
is unavailable in the local preview's image lookup; the game renderer instead
uses the running game's configuration. Rebuild the preview with:

```powershell
& '%USERPROFILE%\AppData\Local\Programs\Python\Python311\python.exe' eid_ui/preview.py
```

## Current limits

The current renderer supports the expanded room and pickup contexts, preserving
the same original EID font and wrapping. The complete PC set of conditions,
menus, languages and selection behavior is not implemented; coverage is tracked
explicitly in `../docs/eid-coverage.md`.

Long descriptions automatically rotate through pages, with a small page number.
There is no new controller shortcut for page changes. A description can cache up
to 16 distinct item pictures; unavailable or excess pictures use a generic item
symbol. Future unsupported formatting is safely omitted instead of shown as
debug labels. Unknown-variable paragraphs would be omitted; none occur in the
current tested catalogue.

The text starts beside the left HUD and reserves space above the bottom boss
display. Its exact placement, readability, and rendering cost still need to be
checked with this polished build running on the physical console. Portable
tests and a desktop preview cannot establish that hardware result.

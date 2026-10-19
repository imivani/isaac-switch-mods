# Animated Items and Unique Birthrights native port

This adds moving artwork to the 98 items supported by the supplied Animated Items mod. Birthright uses the supplied picture for the nearest living player's character, including all 41 vanilla character/body variants. The pedestal, price, sparkles, item descriptions and regret markers continue to use the existing game and mod drawing. This is native behavior with the original animation files, not a global replacement of every item's picture.

The October 15 repair has passed local source, drawing-adapter and memory-handling checks. The user reported invisible Breakfast and Psy Fly pictures in the previous installed pack. The repaired pack still needs console confirmation; the combined build and SD deployment are owned by the main task.

## October 15 missing-picture repair

The animation files were looking for their pictures inside a duplicated directory. The game resolves each sheet relative to its animation file's directory. Our relocated files repeated that directory in the sheet name. For example, `animated/25.anm2` now names `sheets/25_0.png`; it previously repeated the full `isaac_mods/item_visuals/animated` subtree. All **98 animations and 99 sheet references** are corrected, including the three stock-image dependencies. Birthright's 41 character pictures remain unchanged.

The original installed PCX textures are intact: all 137 decode to exactly the source pixels, and every visible Breakfast/Psy Fly keyframe contains nontransparent pixels within its sheet. The fix leaves the encoder, texture pixels, frame crops and tint unchanged. `evidence/iteration4/item-visuals/runtime-path-validation.json` records the old missing paths, corrected paths, texture hashes and native evidence.

Replacement drawing also requires a successfully loaded texture for every animation layer. If an image is missing, the normal item picture remains visible and an error notice appears. Birthright checks its replacement head texture before exchanging it. This is a fallback for an unavailable image; the path correction is what makes the intended animations reachable.

Animated Items and Unique Birthright now obey their separate in-game mod settings. Turning either off immediately resumes that feature's normal game drawing. The Clean Map predicate runs first in the existing full-sprite callback and remains independent of both settings; its exact caller/sprite checks belong to `clean_map.cpp`.

## Source and scope

Read-only source snapshots are under `reference/iteration3/2570913695` and `reference/iteration3/2690434875`. Original Steam Workshop files were not edited.

| Source | Supplied version | main.lua SHA-256 |
|---|---|---|
| [Animated Items](https://steamcommunity.com/sharedfiles/filedetails/?id=2570913695) | 1.11 | `950355e75b2fb8fcc238e91e87336428c21a4bf98bf2c20a073612f3081078c7` |
| [Unique Birthrights](https://steamcommunity.com/sharedfiles/filedetails/?id=2690434875) | 1.4 | `0ddab4fb7b4e30650f2682d63ee0fd407f16db57c86d70a33e3900f25b7b7ffd` |

The complete per-file sizes and hashes are in `features/item_visuals/asset-build.json`; that manifest is authoritative. Animated Items' metadata credits KingBobson for the main behavior, Beezy64 for toggles, ValentinL for the extension API and the original artists. Unique Birthrights credits Gouchnox, with sprite-changing code based on Stewartisme's Custom Mr Dollys. No standalone LICENSE/COPYING file was present in the supplied mod directories. This local port preserves the source/credit evidence and does not assert permission for public redistribution.

All 98 default Animated Items mappings resolve uniquely to this dump's `rp_patch/resources/items.xml`. All animations have `Fps=30`, the looping `AnimatedPedestal` animation, and no event triggers. The port preserves their 3,160 layer keyframes, layer order, interpolation, colors, delays and art. It reproduces the source's pedestal bob/scale steps, fixed shop offset and three held-pickup height phases. Motion advances from game updates, so additional render calls cannot speed it up and pause does not advance it.

Unique Birthrights changes only the visible head picture during the native item draw. Unlike upstream's fixed player-one choice, this port deliberately selects the nearest valid player, with stable first-player tie breaking. This was the requested co-op behavior. Unknown character types keep the normal artwork.

The PC configuration menu, per-item PC saved toggles, third-party AnimatedItemsAPI registrations, custom character integrations and optional alternate/custom Birthright folders are outside this native vanilla-default port. Inventory icons remain the game's normal icons. Birthright's held pickup uses the actual picking player's picture: the source's changed pickup picture also follows into that animation, because native `Pickup::Update` calls `AnimatePickup(const ANM2&,bool,...)` to copy it. An animation begins when its item first renders; after more than two game updates without a matching visible draw it restarts. That restart rule can differ from an offscreen PC effect's animation phase, while preserving its actual frame sequence and speed.

## Resources and packaging

Package the complete **235-file** tree at `assets/item_visuals/resources`, as enumerated by `features/item_visuals/asset-build.json:resource_paths`:

- 98 namespaced ANM2 files under `gfx/isaac_mods/item_visuals/animated`.
- 96 unchanged source PNG files under its `sheets` folder.
- 41 unchanged 32×32 Birthright PNG files under `gfx/isaac_mods/item_visuals/birthright`.

Only ANM2 image paths are rewritten, relative to each relocated ANM2 directory. Custom sheets use `sheets/<item>_<sheet>.png`. The common packager converts source PNG pixels to the verified four-plane RGBA PCX format; ANM2 references retain the game's logical `.png` names. It must not replace the stock collectible ANM2 or any stock collectible picture. No dumped game artwork is included in this resource tree.

Three source image dependencies resolve to exact stock logical paths below `gfx`: `familiar/familiar_fruityplum.png`, `familiar/familiar_orbitals_03_guardianangel.png` and `DarkPrincesCrown.png`. The relocated ANM2s reach these with `../../../` references. The latter two have verified PCX files in the dump. Fruity Plum's exact path is verified by the dumped `rp_patch/resources/gfx/003.225_fruity plum.anm2`; its physical sheet is absent from this local base/update extraction and is expected from the game's mounted content. That dependency needs console confirmation; it is not a reason to copy or invent a replacement game image.

The supplied animations include 54 crop frames extending beyond their source sheets: item 11 (12), 469 (29), 563 (12), and 689 (1). These coordinates are already present in the original mod and remain unchanged. `asset-validation.json` records each one. This is an explicit source-art exception, not a claim that all source crop rectangles are inside their images.

## Native integration and exact evidence

Entry points in `native/source/program/item_visuals.hpp`:

```cpp
bool InitializeItemVisuals(std::uintptr_t module);
void ResetItemVisuals();                         // before SDK NRO unload
void RenderItemVisualsStatus(const FeatureFrame& frame); // error notice only
```

Artwork draws inside the game's entity pass, at the entity's normal depth. It does not depend on a HUD text overlay. Initial loading requires the combined module's exact NRO guard and then validates all four hook prologues before installing any hook:

| Function | NRO offset | Raw ABI |
|---|---:|---|
| `Entity_Pickup::Render` | `0x270c34` | `void(Pickup*, const Vec2*)` |
| `Entity_Player::render_player_sprite` | `0x2d9398` | `void(Player*, const Vec2*, bool renderHeld)` |
| `ANM2::RenderLayer` | `0x9d70` | `void(ANM2*, unsigned, const Vec2*, const Vec2*, const Vec2*)` |
| `ANM2::Render` | `0x9bd4` | `void(ANM2*, const Vec2*, const Vec2*, const Vec2*)` |

Each overwritten 20-byte prologue consists only of stack/register save instructions. Exact bytes, export names, NRO hash and evidence-file hashes are in `features/item_visuals/native-layout.json`. The held-player hook is narrower than outer `Player::Render`; its false `renderHeld` calls are afterimages and do not activate replacement. Nested rendering restores the previous context, including recursive second-body rendering.

The global layer hook admits only layer 1 of the exact current pickup's original sprite (`pickup+0x50`). The one native linked-item branch additionally admits its verified second pickup (`parent+0x5d8`, with child's reverse link `+0x5d0`). It does not intercept the Flip ghost sprite, a mod-owned sprite, EID or Regret. For held items, the full-sprite hook admits only the current player's exact held ANM2 (`player+0x12d8`), and validates the queued ItemConfig pointer (`player+0x1c38`) against native `GetCollectible` using its ID at `ItemConfig+4`.

Force-blind pickups and question-mark/blank sheets always retain original drawing. Animated ground items use their actual visible image, as the source question-mark test does. Birthright and held Animated Items additionally respect Curse of the Blind, as their source callbacks specify. Normal collectible layers, price display, sparkle layers and native clipping are retained. Held substitution temporarily clears only head-layer visibility; it restores that byte after the original sprite draws.

Birthright uses **cached owned textures**, not per-frame reloads. One lazily constructed native ANM2 per encountered character holds its 32×32 picture. `NativeSprite::ExchangeLayerTexture` exchanges the complete data/control pair at `LayerState+0xa0/+0xa8` with the game's head layer for one native draw; `TextureDrawScope` restores it even across an early return. Both owning objects remain alive. Reference counts, path strings, layer transforms, colors, filters, wrapping and blend state stay intact. `SetSpriteSheet` at `0x751c`, `ANM2::Reset` at `0x65b8` and `RenderFrame` at `0x87dc` prove this paired layout and its use at draw time. Original geometry and 32×32 crop coordinates therefore continue to work with the replacement.

Source `SpriteOffset` uses world units. Verified `GetRenderDistance` at `0x35ae0` multiplies X/Y by 0.65. Ground replacements compensate for the pickup's original SpriteOffset while retaining native dual-item/Flip displacement. The new animation's scale is the source step curve. Native ANM2 rendering supplies normal global color/shader/screen-shake handling.

Ownership is bounded to 98 lazy animation sprites, 41 lazy Birthright holders and 256 numeric timing records. Timing keys include entity address, initialization seed, birth frame and item ID; room/run changes and clock rewinds clear them. Negative special-room indices are retained as identities, never used to index memory. No game entity pointer is retained for later dereference. Native objects are destroyed before module unload. A failed unload can recreate them using the still-installed hooks; a real reload, including at the same address, must match the original prologues again.

Failed hook verification or missing animation data displays a small error notice. `NativeSprite::HasLayerTexture` additionally requires readable layer storage, the image-valid byte at `LayerState+0x38`, and live data/control pointers at `+0xa0/+0xa8`. Native `ANM2::load_graphics` at `0xd2cc` establishes this validity byte only after the loaded image passes its validity check. Failed image/identity validation falls back to the game's drawing. Repaired texture presence and visual timing still need a console run.

The texture-path evidence is exact-build assembly at `Preload2+0x5e8` (`0xff54`), which calls `GetAbsolutePathFromBasePath` at `0x361a0` through PLT `0x6707d0`. That helper normalizes slash separators, removes the ANM2 filename, processes `..` components and appends the sheet path. The complete helper decompile and assembly are saved under `evidence/iteration4/item-visuals/path-resolution`. The larger 20,880-byte Preload2 decompile reached its time limit; its saved assembly establishes the call and its arguments. No inferred texture-path change depends on that failed decompile.

## Verification

Commands run locally:

```text
python tools/item_visuals/build_assets.py
python tools/item_visuals/verify_sources.py
python tools/item_visuals/verify_runtime_paths.py
wsl -d IsaacDevkit --cd %USERPROFILE%/Desktop/sw/Isaac-switch-mods -- python3 tools/item_visuals/run_tests.py
```

- `asset-build.json`: 98 animations, 41 character pictures, 235 resources; all source files copied with hashes, no Lua executed.
- `asset-validation.json`: every PNG unchanged; every ANM2 attribute/keyframe unchanged except private sheet paths; 3,160 keyframes compared; exact resource-tree closure and every sheet's native relative-path resolution.
- `native-layout.json`: exact NRO, symbol sizes/ABIs and all four runtime prologue arrays match the dump.
- `test-results.json`: **76,025 behavior checks, 3,121 actual shared NativeSprite checks, and 1,502 actual item-visual callback checks**, each passing optimized and undefined-behavior-sanitized builds. Tests exercise looping, animation timing and resets, reused entity addresses, rerolls back to the same item, bounded cache replacement, shop/held poses, hidden paths, nearest co-op selection and all 41 character values. The actual shared `native_sprite.cpp` is compiled against a bounded read-only memory-query mock; tests verify texture readiness, full handle-pair swapping, unchanged unrelated bytes and reference-count storage, 1,000 scoped draw/restores, null/bounds/permission rejection and no mutation on failure. The actual `item_visuals.cpp` callbacks use fake game services and a fake graphics backend to verify all 98 ground/held selections, each missing Breakfast/Psy Fly layer, load/frame failures, held visibility restoration, independent settings, Birthright and the Clean Map early predicate. No game executable is run in these tests.
- `runtime-path-validation.json`: the frozen installed package reproduces all 99 missing resolved paths; all corrected paths resolve to the intended source/stock dependencies; all 137 installed textures decode identically to their source pixels; reported items have no empty visible frame crop.

The reports record current source hashes and explicitly state `hardware_test: false`. They do not substitute for checking several animated ground/shop/held items, a hidden pedestal, nearby co-op Birthright selection, pause/unpause, room changes, and the Fruity Plum stock dependency on the console.

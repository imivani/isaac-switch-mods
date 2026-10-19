# Switch Repentance 1.7.9b native overlay evidence

This file records observations from the user's verified Switch dump. These offsets belong to **Repentance.nro**, loaded separately by the small ExeFS `main` launcher. They are not offsets from `main`, and `0x7100000000` is only Ghidra's chosen analysis base. No entry here constitutes an on-console test.

The generated `.c` files are approximate pseudocode. Ghidra did not infer all argument registers correctly: use the assembly and the raw ABI declarations below. Original dump files were never edited.

## Exact build anchors

- Application: `010021C000B6A000`; update `010021C000B6A800`, version `524288`, displayed `1.7.9b`.
- NRO file size: `11210752` (`0xab1000`).
- SHA-256: `cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a`.
- NRO build ID (32 bytes): `91c73fdd575061318d68886316afeac72388b2ab000000000000000000000000`.
- NRO text offset/size: `0/0x68d000`; rodata `0x68d000/0x3a7000`; data `0xa34000/0x7d000`; BSS `0xc60000` bytes.
- NRO header and machine-code anchors are saved in [build-anchors.json](../evidence/native-analysis/build-anchors.json).

`HUD::Render` begins at module `+0x3ac71c`. Its first 20 bytes are:

```text
ef 3b b6 6d ed 33 01 6d eb 2b 02 6d e9 23 03 6d fd 7b 04 a9
```

They decode to:

```asm
stp d15,d14,[sp,#-0xa0]!
stp d13,d12,[sp,#0x10]
stp d11,d10,[sp,#0x20]
stp d9,d8,[sp,#0x30]
stp x29,x30,[sp,#0x40]
```

These instructions have no PC-relative operand. A normal trampoline must preserve/replay every overwritten instruction and preserve the original stack effect. Validate the module build ID, readable header, and these bytes before installing a hook. A mismatch must leave the game untouched.

## Rendering entry point and order

| Function/object | Module offset | Evidence |
|---|---:|---|
| `HUD::Render()` | `0x3ac71c`, 9480 bytes | [assembly](../evidence/native-analysis/3ac71c.asm.txt), [pseudocode](../evidence/native-analysis/3ac71c.c) |
| `Game::Render()` | `0x35231c` | [pseudocode](../evidence/native-analysis/35231c.c) |
| `g_Game` pointer variable | `0xabb448` | Defined ELF symbol; load pointer from this address |
| `g_Manager` pointer variable | `0xabcce0` | Defined ELF symbol; load pointer from this address |
| `KAGE::Graphics::g_ShapeRenderer` object | `0xb00860` | Defined ELF symbol, 16-byte object; pass this address directly |
| `IsaacRepentance::WIDTH` float | `0xab1360` | HUD uses this for horizontal layout |
| `IsaacRepentance::HEIGHT` float | `0xab1364` | HUD uses this for vertical layout |

`Game::Render` calls `HUD::Render(game + 0xe8468)`, then stage transition, item overlay, boss overlay, pause screen and game-over rendering. Calling the original HUD first and then drawing the custom overlay at this point keeps those subsequent screens above it. It also avoids moving the overlay outside the game's render-target/post-processing sequence. Runtime correctness still needs a console test.

The HUD first checks a visible/enabled byte at `HUD + 0xc53fa`, then returns if special seed effect ID `10` (NoHUD) is present. The seed-effect tree root pointer is at `game+0x25da0`; each node has left/right pointers at `+0/+8` and signed effect ID at `+0x1c`. A bounded, read-only binary-tree lookup can mirror this guard. A malformed/unreadable tree should suppress the overlay. Merely checking the HUD byte does not honor the NoHUD seed.

Read WIDTH/HEIGHT at runtime: they live in the NRO's zero-initialized memory, so their final values are not stored in the dumped file. They are the native HUD's logical dimensions, which need not be the Switch's physical display resolution.

### Text

The vanilla HUD draws counters with a `Font` embedded at `*g_Manager + 0x76f90`.

| Function | Module offset |
|---|---:|
| `Font::DrawString(const char*,float,float,Color,unsigned,bool) const` | `0x4ceef0` |
| `Font::DrawStringScaled(const char*,float,float,float,float,Color,unsigned,bool) const` | `0x4cef6c` |

`Color` is four floats, red/green/blue/alpha at `+0/+4/+8/+0xc`; its constructor confirms this. Although the original C++ signature says by-value `Color`, the nontrivial copy constructor means the machine ABI passes its address. Do not declare a trivial float aggregate by value at this boundary.

```cpp
struct ColorRaw { float r, g, b, a; };
using DrawStringRaw = void (*)(void* font, const char* text,
    float x, float y, const ColorRaw* color, unsigned boxWidth, bool center);
using DrawStringScaledRaw = void (*)(void* font, const char* text,
    float x, float y, float scaleX, float scaleY,
    const ColorRaw* color, unsigned boxWidth, bool center);
```

Registers are `x0=font`, `x1=text`, `x2=color pointer`, `w3=boxWidth`, `w4=center`; text coordinates/scales occupy `s0..s3`. Use a local color copy whose lifetime spans the call. A `boxWidth` of zero and `center=false` match ordinary vanilla counter calls. DrawString is the byte-string path; use ASCII for the initial renderer until UTF-8 drawing has been tested.

Evidence: [DrawString assembly](../evidence/native-analysis/4ceef0.asm.txt), [DrawStringScaled assembly](../evidence/native-analysis/4cef6c.asm.txt), [Color constructor](../evidence/native-analysis/4ef130.asm.txt).

**Do not use `Game::DebugText` for this overlay.** That function spawns an `Entity_Text`, consumes the game's random-number generator and modifies game state; it is not a direct drawing primitive. See [0x359e34](../evidence/native-analysis/359e34.c).

### Filled rectangles

`ShapeRenderer::FillQuad(float,float,float,float,const Color&)` is module `+0x4f2350`.

```cpp
using FillQuadRaw = void (*)(void* shapeRenderer,
    float left, float top, float right, float bottom, const ColorRaw* color);
```

Registers: `x0=moduleBase+0xb00860`, `x1=color pointer`, `s0..s3=left,top,right,bottom`. These are **corner coordinates**, not width and height. This is proven by the `DestinationQuad` constructor called inside it. The function selects the native shape shader and delegates to the existing renderer.

Evidence: [FillQuad assembly](../evidence/native-analysis/4f2350.asm.txt), [quad constructor](../evidence/native-analysis/4efb78.c).

## Read-only room/entity snapshot

The fields below are independently checked against this Switch binary. They were not copied from iOS offsets.

| Read | Address relative to object | Type / meaning |
|---|---:|---|
| Current room | `game + 0x21550` | `Room*` |
| Embedded entity list | `room + 0x1950` | `EntityList` object |
| Active entity array | `EntityList + 0x78` = `room + 0x19c8` | `Entity**` |
| Active entity count | `EntityList + 0x84` = `room + 0x19d4` | `uint32_t` |
| Render entity array | `EntityList + 0x90` = `room + 0x19e0` | `Entity**`, ordered by render depth |
| Render entity count | `EntityList + 0x9c` = `room + 0x19ec` | `uint32_t` |
| Player vector begin/end | `game + 0x25c50 / +0x25c58` | `Entity_Player**` |
| Entity type/variant/subtype | `entity + 0x38 / +0x3c / +0x40` | 32-bit integers |
| Visible | `entity + 0x1c1` | byte |
| Exists | `entity + 0x1c2` | byte |
| Dead | `entity + 0x1c3` | byte |
| Entity flags | `entity + 0x1b8` | `uint64_t` |
| Position x/y | `entity + 0x310 / +0x314` | floats |
| Current/max hit points | `entity + 0x354 / +0x358` | floats |
| Spawn frame | `entity + 0x2f4` | int32 |
| Game frame counter | `game + 0x24f99c` | int32 |

Use a bounded snapshot on the HUD/render thread, and do not keep entity pointers across frames. Reject null objects, unreasonable counts and non-finite health/position values. `EntityList::QueryType` fills and caches mutable scratch lists; direct iteration of its active array avoids those side effects.

**Do not rely on finding players in the active array.** `EntityList::Add` only appends there when entity flag bit 3 is clear. Bit 3 is `RENDER_FLOOR`, not `PERSISTENT` (the latter is bit 37); the previous persistent label was incorrect. Obtain players from the separate game player vector. The render array includes newly added entities excluded from the active array, subject to the same bounds/existence checks. See [EntityList::Add](../evidence/native-analysis/75c64.c).

`Game::GetNearestPlayer` at `0x35a270` has raw ABI `void* (*)(void* game, const Vec2* position)`, where `Vec2` is two floats and the result is a player pointer. It reads the vector and compares squared position distances. Its empty-vector path logs but then still dereferences the first entry, so a bounded local loop over begin/end is safer for an overlay. Compute count only after validating pointer order/alignment and cap the number of player pointers read.

Evidence: [Room::AddEntity](../evidence/native-analysis/468a7c.asm.txt), [EntityList::QueryType](../evidence/native-analysis/79718.c), [EntityList::Update](../evidence/native-analysis/767c0.c), [Game::GetNearestPlayer](../evidence/native-analysis/35a270.c), [Entity::Render](../evidence/native-analysis/624fc.c), [Entity::Kill](../evidence/native-analysis/5c8e0.c), [Entity::Remove](../evidence/native-analysis/5d3e0.c), [HUD::RenderHealthBar](../evidence/native-analysis/3b2454.c).

### Boss predicate and vanilla health calculation

`Entity::IsBoss()` is virtual at **vtable byte offset `0x98`**. `EntityList::Update` uses this slot. Both the Entity and Entity_NPC [vtable relocation entries](../evidence/native-analysis/entity-vtables.json) agree. The base implementation is module `+0x67580`, returning false; the NPC implementation is `+0x124b20`, returning the byte at NPC `+0xcc9`. Prefer the virtual method instead of guessing an entity type range or reading the NPC field on other classes.

```cpp
using IsBossRaw = bool (*)(const void* entity);
auto vtable = *reinterpret_cast<const uintptr_t* const*>(entity);
bool boss = reinterpret_cast<IsBossRaw>(vtable[0x98 / 8])(entity);
```

Vanilla aggregates boss HP in `EntityList::Update`: it requires existence, virtual `CanShutDoors` (`vtable+0x90`), excludes type `0x362`, checks `IsBoss`, excludes entity flag bit 20, and only sums HP once frame age is positive and flag bit 31 is clear. Current HP is clamped at zero. It divides summed current HP by a retained maximum-health total and calls `HUD::SetBossHealth`. A basic per-entity overlay does not yet reproduce all vanilla grouping or the upstream Enhanced Boss Bars special cases.

## EID identity and spoiler guards

- Collectible pickups: type `5`, variant `100`; subtype is collectible ID.
- Trinket pickups: type `5`, variant `350`; vanilla masks subtype with `0x7fff` to remove the golden-trinket bit before config lookup.
- Pickup shop price is signed integer `pickup + 0x564`. `MakeShopItem` uses `+0x56c` for its shop-slot argument; it is **not established here as an options-pickup index**.
- Pickup forced blindness byte is `pickup + 0x562`, proven by `SetForceBlind` at `0x26c3b8`.
- `Level::GetCurses()` is module `+0x3d57cc`, machine signature `uint32_t (*)(const void* game)`. The Level object is the Game prefix, as verified by the call in `Entity_Pickup::ReloadGraphics`.
- Its result combines `game+0xc` with special-seed permanent and banned curses. Use this getter instead of only reading the raw field. Mask `0x40` is the collectible-blindness condition used by the game.
- `ReloadGraphics` suppresses that blindness during explicit reveal calls and one room state (`game+0x21560 == 2`). Initially suppressing the description whenever curse/forced blindness is active is conservative; full parity should inspect the actually displayed sprite.

Evidence: [GetCurses](../evidence/native-analysis/3d57cc.c), [SetForceBlind](../evidence/native-analysis/26c3b8.c), [ReloadGraphics](../evidence/native-analysis/26a97c.c), [MakeShopItem](../evidence/native-analysis/270fc4.c).

### Displayed question-mark sheet

The game uses `gfx/Items/Collectibles/questionmark.png` when the hidden flag is passed into `SetupCollectibleGraphics`. This gives an additional check against revealing an identity whose displayed picture is hidden.

The entity's ANM2 starts at `entity+0x50`. `ANM2::GetLayer(int)` at `0xa034` reads a state-array pointer at `ANM2+0xa8` and int count at `ANM2+0xb0`. Therefore these are `entity+0xf8` and `entity+0x100`. **Switch LayerState stride is `0xb0`**, and the path is a native libc++ string at **LayerState+0x10**, as shown by `ReplaceSpritesheet` at `0xd270`. The corresponding iOS project's stride `0x90` and string offset `+0x8` are different and must not be used.

For collectible layer 1: validate count at least 2, state pointer, and string bounds before reading `states + 0xb0 + 0x10`. Native libc++ string uses byte0 bit0 for long representation, pointer at +0x10 and size at +8 in that case; short representation has size byte0>>1 and bytes at +1. Bound any read, reject malformed string state, and suppress text for `questionmark.png` (case-insensitive match). The string layout is visible in `ANM2::GetLayer` and native string construction/destruction paths. Do not call mutating spritesheet APIs to inspect it.

Evidence: [GetLayer](../evidence/native-analysis/a034.c), [ReplaceSpritesheet](../evidence/native-analysis/d270.c), [SetupCollectibleGraphics](../evidence/native-analysis/26c3ec.c).

Pill colors are not effect IDs. No pill-effect/identification integration has been validated by this work. Skip pill descriptions until that mapping and knowledge policy are implemented.

`Game::Start` stores the challenge ID at signed int `game+0x26fa88`. The alternate challenge parameter selector is at `game+0x26fb10`. Until special challenge descriptions are implemented, either being nonzero is a conservative reason to suppress EID. Evidence: [Game::Start](../evidence/native-analysis/review/34e518.c), [challenge parameter getter](../evidence/native-analysis/review/34ea68.c).

## Planetarium chance, floor identity and options

`Level::GetPlanetariumChance()` at `0x3d58bc` is a native, read-only chance calculation with raw ABI **`float (*)(const void* game)`**. The result is a fraction in `s0`, clamped to `0..1`; the decompiler's inferred `void` return is wrong. It queries current trinket/collectible ownership, stage and missed treasure-room state. It does not draw or consume RNG. The upstream mod calls this on run/floor initialization and retains that value for the floor; repeatedly recalculating after item pickups would change that behavior. The getter does not itself establish the mod's unlock/treasure-room eligibility.

`Level::IsBackwardsPath()` at `0x3e1d98` has raw ABI `bool (*)(const void* game)`. It checks stage `1..6` and bit zero of byte `*g_Game+0x26fa56`; use the getter to preserve both checks.

| Read | Address | Evidence |
|---|---|---|
| Stage / stage type | signed ints `game+0`, `game+4` | `Level::SetStage` writes both |
| Start seed | u32 `game+0x25d44` | Seeds at `game+0x25d40`; `SetStartSeed` writes Seeds+4; `Game::Start` copies it |
| Saved stage seed | u32 `game+0x25d58+4*effectiveStage` | `Level::Init` uses this to seed level RNG |
| Effective stage | `stage + (stageType == 4 || stageType == 5 ? 1 : 0)` | Exact `Level::Init` branch |
| Current room index / dimension | signed ints `game+0x21558`, `game+0x21560` | `GetCurrentRoomDesc` passes them to `GetRoomByIdx` |
| Room descriptors | embedded at `game+0x18`, stride `0x100` | `GetRoomByIdx` and `QueryRoomTypeIndex` |
| Descriptor count | signed int `game+0x21510` | Native room iteration bound |
| Descriptor data / dimension | pointer `descriptor+0x10`, int `descriptor+0xc` | Native room iteration |
| Room type | int `descriptor.data+8` | `QueryRoomTypeIndex` comparison |
| FoundHUD | byte `*g_Manager+0x3650a` | `StatHUD::Render` guard and Options `FoundHUD` key |
| HUD offset | float `*g_Manager+0x36514` | Options `HudOffset` key; HUD uses `1-value` |
| Boss HP bottom preference | byte `*g_Manager+0x3652f` | Options `BossHpOnBottom` key |

Use the complete run/floor/room tuple, not merely a Room pointer or numeric stage, for short-lived feature caches. `Seeds::ForgetStageSeed` changes one saved stage seed through a seeded RNG. `SetStartSeed` initializes 15 entries; require effective-stage index `0..14` before reading. These identities still require lifecycle checks for restarting a run with the same start seed or regenerating an identical floor; hooks or frame-counter rollback can supplement them.

To find a treasure room without changing game state, scan at most 507 embedded descriptors, require valid non-null data, and compare type to `4`. The native room-name table maps `4` to `treasure` (relocation `0xa3a6c8` to string `0x8a5086`). `QueryRoomTypeIndex` is not a read-only replacement: it builds/shuffles candidates with the game's RNG.

ItemConfig is embedded at `*g_Manager+0x36538`. `GetTrinket(int)` at `0x3c0c9c` has raw ABI `const void* (*)(const void* config, int id)`, masks the ID with `0x7fff` and returns null outside the vector. `Item::IsAvailableEx(unsigned)` at `0x3c101c` has raw ABI `bool (*)(const void* item, uint32_t flags)`. Native `ItemPool::ResetTrinkets` uses flags `0xffffffff`; this supplies the normal eligibility test for Telescope Lens, ID `152`, including unlock/mode restrictions. Do not infer Telescope eligibility from the chance getter alone.

`Room::IsBeastRoom` at `0x452438`, raw `bool (*)(const void* room)`, follows `room+8` to its descriptor and `descriptor+0x10` to data, then requires data type `16` and variant `35`. See [assembly](../evidence/native-analysis/expanded/452438.asm.txt). Difficulty is signed int `game+0x2f0028`, written by `Game::Start`.

Evidence: [chance assembly](../evidence/native-analysis/expanded/3d58bc.asm.txt), [chance pseudocode](../evidence/native-analysis/expanded/3d58bc.c), [ascent getter](../evidence/native-analysis/expanded/3e1d98.asm.txt), [Level::Init](../evidence/native-analysis/expanded/3e0688.c), [SetStage](../evidence/native-analysis/expanded/3e420c.c), [SetStartSeed](../evidence/native-analysis/expanded/4a1114.c), [ForgetStageSeed](../evidence/native-analysis/expanded/4a1228.c), [room lookup](../evidence/native-analysis/expanded/3dc3b0.c), [room type query](../evidence/native-analysis/expanded/3e605c.c), [StatHUD](../evidence/native-analysis/expanded/3af194.c), [options loading](../evidence/native-analysis/expanded/4301f0.c), [trinket availability call](../evidence/native-analysis/expanded/3c78e0.asm.txt).

## Owned sprites and item icons

Native `ANM2` size is **`0x158` bytes**, independently proven by `AnmCache::CreateGlobalAnim`: it allocates `0x158` and calls the constructor. Use suitably aligned fixed storage. The constructor stores pointers back to the object at `+0x30` and `+0x60`, so raw copying or moving a constructed sprite is invalid.

| Function | Module offset | Raw machine declaration |
|---|---:|---|
| ANM2 constructor | `0x6540` | `void (*)(void* sprite)` |
| ANM2 destructor D1/D2 | `0x71a4` | `void (*)(void* sprite)` |
| Load | `0xc970` | `void (*)(void* sprite, const void* nativeString24, bool loadGraphics)` |
| ReplaceSpritesheet | `0xd270` | `void (*)(void* sprite, unsigned layer, const void* nativeString24)` |
| LoadGraphics | `0xcb6c` | `void (*)(void* sprite)` |
| SetFrame(name, index) | `0xa65c` | `void (*)(void* sprite, const char* animation, int frame)` |
| Render | `0x9bd4` | `void (*)(void* sprite, const Vec2* position, const Vec2* cropTopLeft, const Vec2* cropBottomRight)` |
| ColorMod::SetTint | `0x37848` | `void (*)(void* colorMod, float r, float g, float b, float a)` |
| Room::WorldToScreenPosition | `0x489354` | `Vec2 (*)(const void* room, Vec2 worldPosition)` |

WorldToScreenPosition uses `x0=room`, the input/output vector in `s0/s1`, and includes room render offset and game screen shake. This ABI is different from ANM2::Render's pointer arguments. For the latter, two zero vectors provide no cropping, matching native calls.

The sprite's `ColorMod` is at `sprite+0xe8`; its first four floats are RGBA. Sprite scale is at `+0xdc/+0xe0`, offset at `+0xd4/+0xd8`. Setting tint on owned sprite storage avoids the static global `PushColorOverride` stack.

To validate animation names before SetFrame, ANM2 holds the AnimationData array pointer at `+0x98` and u32 count at `+0xa0`. Each entry is `0x150` bytes and starts with its native string name. These values are independently visible in [SetFrame's search loop](../evidence/native-analysis/expanded/a65c.c).

Load and ReplaceSpritesheet copy their const-reference native string input into game-owned storage. A borrowed 24-byte libc++ string descriptor can therefore be used during the call, but never invoke the native string destructor on that borrowed descriptor. Native `Load`/sprite destructor manage the real cache references and allocation. Keep all native-owned sprite construction, texture changes, rendering and teardown on the game's render thread. Destroy sprites **before** the NRO unloads; never call a destructor through an unloaded function address.

For actual collectible artwork, `ItemConfig::GetCollectible(int)` at `0x3c0c50` has raw ABI `const void* (*)(const void* config, int id)`. It range-checks nonnegative IDs; negative IDs delegate to the game's procedural-item manager. Check the returned pointer; its graphic path is a native libc++ string at `item+0x38`. Load `gfx/005.100_collectible.anm2`, replace layer 1 with that path, call LoadGraphics, then SetFrame to `Idle` or `ShopIdle` frame zero. This follows Regret Pedestals' actual-item-art behavior.

`Entity_Pickup::SetupCollectibleGraphics` at `0x26c3ec` is **static** despite its class name: raw `void (*)(void* sprite, unsigned layer, int itemId, uint32_t seed, bool blind)`. It accepts an ANM2 as its first argument, not an Entity. Its challenge-specific art substitution makes it less faithful than direct config artwork for the Regret mod.

Evidence: [size allocation](../evidence/native-analysis/expanded/17924.asm.txt), [constructor](../evidence/native-analysis/expanded/6540.asm.txt), [destructor](../evidence/native-analysis/expanded/71a4.c), [Load](../evidence/native-analysis/expanded/c970.c), [ReplaceSpritesheet](../evidence/native-analysis/d270.c), [Render](../evidence/native-analysis/expanded/9bd4.asm.txt), [tint](../evidence/native-analysis/expanded/37848.asm.txt), [WorldToScreenPosition](../evidence/native-analysis/expanded/489354.asm.txt), [GetCollectible](../evidence/native-analysis/expanded/3c0c50.c), [item artwork selection](../evidence/native-analysis/26c3ec.c).

## Pickup lifecycle for Regret Pedestals

| Read | Address | Evidence |
|---|---|---|
| InitSeed | u32 `entity+0x3e8` | `Entity::Init` stores its seed argument here |
| Creation frame | int `entity+0x2f4` | `Entity::Init` copies game frame counter |
| Primary active item ID | int `player+0x1964` | `GetActiveMaxCharge` uses slot stride `0x1c` |
| Current animation | `*(ANM2+0x38)` points to AnimationData; its leading member is the native string name | `ANM2::IsPlaying` compares this string |

`ANM2::IsPlaying` also requires its playback-state flag, so it is not equivalent to querying the current animation after SetFrame has stopped playback. Inspecting the validated AnimationData name is appropriate when matching the upstream `GetAnimation()==Idle/ShopIdle` rule. Glowing Hourglass's item ID is `422` in the game's items XML.

The following methods all use `void (*)(void* pickup)`:

| Method | Offset / size | First 20 bytes |
|---|---:|---|
| Pickup::Update | `0x26c9b8` / 11860 | `eb2bb86de923016dfd7b02a9fd830091fc6f03a9` |
| Pickup::Remove | `0x276960` / 52 | `fd7bbea9f30b00f9fd030091f30300aa00e442f9` |
| Pickup destructor D1/D2 | `0x2691f8` / 888 | `fd7bbda9f50b00f9fd030091f44f02a9084200f0` |

Update and Remove's first five instructions have no PC-relative operand. Destructor's first four instructions do not either, but the fifth is **ADRP** and requires correct relocation if the hook overwrites 20 bytes. Its first 16 bytes are `fd7bbda9f50b00f9fd030091f44f02a9`.

Pickup::Remove removes an optional linked entity, then tail-calls base Entity::Remove (`0x5d3e0`). Base Remove only clears `exists` and sets `dead`; it does not free the object. Update can set subtype to zero. Sampling only at HUD time can therefore retain a stale item ID and mistakenly ghost a collected item. Observe final subtype after Update and immediately before Remove/destruction; cache values, never a pointer for later dereference. Do not read the object after the destructor. A pre-destructor observer is useful as a final cleanup path when deletion bypasses Pickup::Remove. Reset cached room state before carrying ghosts across room/floor changes.

For a room-entry boundary that also covers reloading an identical room tuple, `Room::Init(const RoomConfig::Room*,RoomDescriptor*)` is at `0x45c020`, size 23964. Its raw signature is `uint32_t (*)(void* room, const void* roomConfig, void* roomDescriptor)`; preserve its `w0` result, which is explicitly set to 1 at the normal epilogue. Its first 20 bytes are `eb2bb86de923016dfd7b02a9fd830091fc6f03a9`, all stack/register instructions. The first three integer argument registers are `x0=room`, `x1=config`, `x2=descriptor`; this function's generated C signature is misinferred. `Level::load_room` calls it at `0x3d7104`. Suppress/reset Regret observations around the original Room::Init so teardown notifications for the old room cannot become afterimages in the new one. Then synchronize the new room and resume tracking. See [Room::Init assembly](../evidence/native-analysis/expanded/45c020.asm.txt).

Evidence: [Entity::Init](../evidence/native-analysis/expanded/597d4.c), [active item lookup](../evidence/native-analysis/expanded/27f828.c), [animation check](../evidence/native-analysis/expanded/a454.c), [Pickup::Update](../evidence/native-analysis/expanded/26c9b8.asm.txt), [Pickup::Remove](../evidence/native-analysis/expanded/276960.asm.txt), [base Remove](../evidence/native-analysis/expanded/5d3e0.asm.txt), [pickup destructor](../evidence/native-analysis/expanded/2691f8.asm.txt).

## Reproducing the inspection

`tools/native-analysis/prepare_targets.py` parses the immutable NRO's symbols, dynamic table and PLT relocations, writing `symbols.json`, `dynamic.json`, `plt.json`, `plt.tsv` and `targets.tsv`. `InspectNativeTargets.java` disassembles only listed functions, resolves their immediate PLT callees from that evidence, and exports approximate C plus assembly through the existing Ghidra project. Full game auto-analysis was not required.

The selected-function work establishes a credible native rendering and data-reading path. It does not yet establish that a built hook loads correctly, that UI placement is good at runtime, or that all Enhanced Boss Bars / External Item Descriptions behavior is implemented.

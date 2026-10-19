# Verified native EID context inputs

Exact target: Repentance1.7.9b, `Repentance.nro` SHA-256
`cc363208c220f65547edda6d8268b3b6c0f7373b14c91ae6ffb378018de4b66a`.
Offsets below are relative to the already-validated module or named live object.
The core adapter does not write these fields, invoke pickups, consume pills,
generate loot, roll recipe randomness or change the game's saved state.

Read-only Ghidra batches used a private project copy under
`evidence/eid-analysis-project`; the immutable original project and dump were
not changed. Each function's machine instructions and decompiler view are saved
under `evidence/native-analysis/{eid-expansion,eid-expansion-more,eid-final-fields,eid-small-getters}`.
Function declarations in generated decompiler text can be imperfect; arguments
and return conventions were checked against the instructions and callers.

| Input | Verified location | Evidence |
|---|---|---|
| Room grid dimensions / embedded pointers | Room+0x14,+0x18 / Room+0x30 | GetGridEntity0x45bfdc; bounded to448 cells. |
| Grid type / variant / completed payout count | Grid+8,+0xc,+0x1c | Spikes::Update0x3941e4 and GridEntity::hurt_func0x37e57c; the latter increments+0x1c on an actual accepted sacrifice. |
| Room type | Room+0x10 | hurt_func checks13 for sacrifice; Sanguine variant>=100 has its separate branch. |
| Grid world position |0x454d38, two-float return | `Vec2(const void*room,int index)`; actual origin y120, not an assumed iOS offset. |
| World position to grid index |0x459e3c | `int(const void*room,const Vec2*)`; native bounds checks. |
| Grid collision |0x452654 | `int(const void*room,int index)`; direct grid/path cases. Active-spike path999 is additionally blocked. |
| Flying |0x5c744 | `bool(const void*entity)`; reads entity grid collision class+0x1e0. No guessed player flight byte. |
| Options? pickup index | Pickup+0x55c | SetNewOptionsPickupIndex0x277674 writes it. This differs from earlier iOS guesses. |
| Shop price / forced blindness | Pickup+0x564 /+0x562 | Existing native-api evidence, MakeShopItem0x270fc4 and SetForceBlind0x26c3b8. |
| Pill pool | Game+0x242c0 | Player::UsePill0x2dfc04 caller. |
| Pill identification | Game+0x24d28+color | UsePill writes one byte for the consumed color; golden color14 takes a separate reroll branch. Color is masked with0x7ff. |
| Resolved pill effect |0x3c857c | `int(pool,color,player)`; read-only resolution applies the actual player's pill conversion/health rules. Called only after knowledge permits disclosure; horse flag0x800 retained. |
| Player character | Player+0x1738 | PlayerManager::FirstPlayerByType0x441e38. |
| Inventory count table | pointer Player+0x1ab8 | HasCollectible0x27d3f4; bounds-checked copy of733 integers. Native accessor used for the named modifier items. |
| Transformation counters | Player+0x1c54,15 integers | HasPlayerForm0x298510 and increment_player_form_counter0x29813c. |
| Pocket slots | Player+0x1c10, four8-byte pairs | GetCard0x2e8d58/GetPill0x2e8d80 and HUD::PlayerHUD::Update0x3a0d04; id,type; type1card,type0pill,type2active. |
| Active slots | Player+0x1964,stride0x1c | Existing HUD/active-slot evidence; four supported slots. Metadata validates active IDs. |
| Equipped trinkets | Player+0x1ab0 and+0x1ab4 | GetTrinket0x2f1c20; golden bit0x8000 stripped for lookup. |
| Crafting contents / cached output | Player+0x2690..0x2697 /+0x2698 | TryAddToBagOfCrafting0x2ff160 and HUD::RenderCraftingTable0x3a7080. The recipe function is deliberately not called. |
| Crane prize | Slot+0x570 | SetPrizeCollectible0x32e5c0; variant16 renders that item into sprite layer2. |
| Current animation comparison |0xa454 | `bool(const ANM2*,const char*)`; ANM2+0x38 points to current animation, playing byte+0x54. Borrowed animation name is validated first. |
| Input controller / Map action | Player+0x19ec / action13 | SetControllerIndex0x29b6fc, Minimap::Update0x41ca44; parent time-machine input evidence. |
| Shoot-left/right browsing | actions4/5 | Player::get_shooting_input0x2b0ffc subtracts/adds the action values from x; `evidence/boss_ui/timemachine-review/details/2b0ffc.asm.txt`. |
| Input query |0x3f9b6c | `bool(manager,unsigned action,unsigned controller,const void*player)`; native wrapper dispatches the actual input manager. |
| Stage/type / Greed mode | Game+0,+4 / difficulty+0x2f0028 | Existing Level/Game::Start evidence. Difficulty2/3 selects Greed conditions. |

The shared profile already verifies the game/manager globals, player list,
entity positions/existence/visibility, frame counter, curse function and
challenge fields. The core now permits supported ordinary challenges; April's
Fool32 and unknown alternate challenge state remain suppressed. Card challenge43
uses the target's Cantripped mapping.

The adapter refreshes selection/state every six gameplay frames, immediately on
per-player Map or browsing-input changes. It draws the cached measured layout
each render. Null entity arrays are valid when the count is zero, allowing
grid-only room descriptions and reminders. Every array has a fixed bound;
unknown/malformed identities fail closed. The current grid route check is a
conservative approximation of the source path finder, not a claim of identical
physics behavior in every room.

Integration is `RenderEidFeatures(frame,entities,count)`. `ResetEidFeatures()`
resets special-feature state and releases EID sprites before module unload.
No new persistent storage is required for sacrifice progress because the game
already owns that counter. Native calls and hardware rendering still require
the parent's combined build and console verification.

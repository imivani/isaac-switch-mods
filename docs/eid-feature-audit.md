# EID feature coverage audit

This is an independent source audit of the Switch port against EID 5.25 at
`8b59d00c6828e333294639ad2b2b6875eae821cb`. It separates a working description
display from full EID behavior. The user has confirmed the installed ordinary
collectible/trinket display and restored item pictures. That confirmation does
not test newly added room descriptions, pockets, contextual effects or controls.

## Current expansion

The new portable `eid/context.hpp` and its native adapter now implement sacrifice, dice-room,
Sanguine Bond and Card Reading text; card/pill/horse-pill descriptions; quality,
charge and transformation metadata; Birthright; golden/Mom's Box trinkets;
Tarot Cloth, Car Battery, selected pill modifiers, Binge Eater and Spindown.
These paths are connected to native game data. The core owner reports 3,557
portable cases and 7,322 layouts passing optimized and undefined-behavior
sanitizer checks. The newly expanded paths still require console testing;
offline checks are not a hardware pass.

The adapter also implements reachable/known
card and pill selection, crane prizes, Broken Shovel, nearest-player context,
held-Map reminders and the already cached result of a full Bag of Crafting.
A cached full-bag result is a crafting preview, not the full recipe browser.

Flip, Glitched Crown and Tainted Cain salvage are implemented in the isolated
special adapter, with 17,558 portable checks and 1,458 layouts passing in both
optimized and undefined-behavior sanitizer runs. Exact-build symbols and
pointer layouts were checked against the dump. `eid-special-port.md` states
the supported controls and combined Flip/Crown limitations. Their new native
paths pass the combined native build and remain unverified on hardware.

The current character/Greed conditional import covers 53 upstream literal
registrations, comprising 233 operations. This is a useful subset rather than
all of EID's conditional logic. Nearest-player Lost/Keeper/no-red-health
context is connected; advanced history-based effects remain below.

## Major remaining families to account for

| Family | Why a player notices it | Source and practical priority |
| --- | --- | --- |
| Full Bag of Crafting | Available room/floor ingredients, possible recipes, sorting, search, locking and cycling results | `features/eid_bagofcrafting.lua`, both `eid_bagofcrafting_search*.lua`, config `DisplayBagOfCrafting`/`BagOfCraftingDisplayRecipesMode`; substantial work beyond the cached current recipe |
| Character and inventory conditionals | Health-ups change for soul-only/no-health characters; proc chance changes with luck; many item combinations alter the text | `features/eid_conditionals*.lua`, `eid_modifiers.lua:316` and `:336`, modular description files; high priority after room/pocket coverage |
| Held-Map reminder completeness | Native EID has overview, character, actives, pockets, held and gulped trinkets, passive history, Lemegeton wisps and Tainted Blue Baby spells | `features/eid_holdmapdesc.lua:29`; enumerate the implemented categories instead of calling a smaller reminder the full feature |
| Advanced contextual results | Pandora's Box by floor, Book of Virtues wisps, Wild Card's last effect, The Stars? oldest item, Void's absorbed items and Spindown, changing D Infinity state | `features/eid_modifiers.lua:195`, `:1225`, `:1281` and `eid_holdmapdesc.lua`; many require persistent event history, not just an owned-item bit |
| Co-op behavior | Multiple players' nearby descriptions and different effects/identified pills can differ | `main.lua:1410`, `eid_modifiers.lua` co-op callbacks; choosing the nearest player's context is useful but does not reproduce all simultaneous panels |
| Settings, language and display controls | Hide/show binding, scale, position, optional indicators, local display, languages and persistent preferences | `eid_config.lua`, `eid_mcm.lua`, `eid_language_manager.lua`; plain-English status should state which settings exist on Switch |
| Mod/API integration | Third-party Lua mods register custom descriptions and callbacks | `features/eid_api.lua`; importing vanilla tables does not provide the Lua extension API |

The upstream default-on room/pocket and contextual features should be completed
before spending effort on optional RNG prediction modes. Mouse controls, PC
configuration interfaces, Repentance+ additions and REPENTOGON-only features
are separate compatibility work for this standard 1.7.9b target. In particular,
detailed TMTRAINER effects and donation-machine achievement descriptions rely
on APIs absent from the current Switch bridge.

## Audit checks and useful verified leads

- The earlier native selector only admitted pickup variants 100 and 350, and
  `RenderEidDescription` accepted only those categories. The expansion must
  replace both restrictions; imported card/pill data alone cannot enable them.
- Existing `eid_ui/test-results.json` records zero omitted variable paragraphs
  in the baseline catalogue. The renderer deliberately omits unresolved
  variable paragraphs, however, so newly imported conditional templates must
  be substituted before rendering. Their absence must not be mistaken for
  complete conditional support.
- The transformation builder emits `ColorEIDTransform`; the UI owner has
  updated the named-color path and general-context rendering. Its current
  asset inventory includes 281 icons. The broader offline layout checks pass.
- `Entity_Pickup::InitFlipState` is at `0x26b6c8`, `SetFlipState` at `0x26b874`,
  and `TryFlip` at `0x26b924`. Direct assembly proves alternate saved state at
  pickup `+0x610` with type/variant/subtype at `+0/+4/+8`. Reading it does not
  require invoking a flip or generating a new item.
- `Pickup::Update` at `0x26df3c` rotates the alternate collectible queue from
  pickup `+0x5e4`, count `+0x604`, into the current subtype and appends the old
  subtype. This provides bounded read-only cycle information.
- `Entity_Player::SalvageCollectible` at `0x2dd374` changes the game and must
  never be called to preview salvage. Upstream's display instead uses player
  type, room pool, quest tags, Birthright and held/gulped trinkets.
- `VoidHasCollectible` is exported at `0x2a1d68`; its ABI still needs a focused
  check before connecting absorbed-item context. An owned-item-only Spindown
  branch is not the absorbed-Spindown behavior in upstream `eid_modifiers.lua`.

No Switch files were changed by this audit. New native features require their
own build checks and actual console cases before being described as verified.

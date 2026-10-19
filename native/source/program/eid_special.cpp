#include "eid_special.hpp"
#include "generated/build_profile.hpp"
#include "../../../eid_special/special.hpp"

namespace isaac {
namespace {
using namespace feature_memory;
using namespace isaac_eid;
using namespace isaac_eid_special;

CycleCache s_Cycles;
MapLatch s_Map;
std::uintptr_t s_Module{}, s_Game{};
int s_LastFrame = -1;

bool Pointer(std::uintptr_t pointer) { return pointer > 0x10000 && !(pointer & 7); }
bool Live(std::uintptr_t entity, unsigned type, unsigned variant, std::size_t size) {
    return Pointer(entity) && Readable(entity, size) &&
           Read<unsigned>(entity + 0x38) == type && Read<unsigned>(entity + 0x3c) == variant &&
           Read<unsigned char>(entity + 0x1c1) && Read<unsigned char>(entity + 0x1c2) &&
           !Read<unsigned char>(entity + 0x1c3);
}
bool KnownId(unsigned id) {
    Description description;
    return id <= 732 && EnglishRepentance().Find(Category::Collectible, id, description);
}
bool Has(const FeatureFrame& frame, std::uintptr_t player, unsigned id) {
    return reinterpret_cast<bool (*)(const void*, unsigned, bool)>(frame.module + 0x27d3f4)(
        reinterpret_cast<const void*>(player), id, false);
}

struct PartyInput { bool flip{}, crown{}, triggered{}; unsigned held_mask{}; };
PartyInput ReadParty(const FeatureFrame& frame, std::uintptr_t manager) {
    PartyInput result;
    const auto begin = Read<std::uintptr_t>(frame.game + profile::PlayersBegin);
    const auto end = Read<std::uintptr_t>(frame.game + profile::PlayersEnd);
    if (!Pointer(begin) || end <= begin || end - begin > 8 * sizeof(std::uintptr_t) ||
        (end - begin) % sizeof(std::uintptr_t) || !Readable(begin, end - begin)) return result;
    const auto pressed = reinterpret_cast<bool (*)(const void*, unsigned, unsigned, const void*)>(frame.module + 0x3f9b6c);
    const auto triggered = reinterpret_cast<bool (*)(const void*, unsigned, unsigned, const void*)>(frame.module + 0x3f9b7c);
    for (auto at = begin; at < end; at += sizeof(std::uintptr_t)) {
        const auto player = Read<std::uintptr_t>(at);
        if (!Live(player, 1, 0, 0x2850)) continue;
        result.flip = result.flip || Has(frame, player, 711);
        result.crown = result.crown || Has(frame, player, 689);
        const auto controller = Read<unsigned>(player + 0x19ec);
        if (!ValidNativeController(controller)) continue;
        if (pressed(reinterpret_cast<const void*>(manager), 13, controller, reinterpret_cast<const void*>(player))) {
            const auto slot = static_cast<unsigned>((at - begin) / sizeof(std::uintptr_t));
            result.held_mask |= 1u << slot;
            result.triggered = result.triggered || triggered(reinterpret_cast<const void*>(manager), 13, controller,
                                                             reinterpret_cast<const void*>(player));
        }
    }
    return result;
}

bool ReadKey(const FeatureFrame& frame, std::uintptr_t entity, CycleKey& key) {
    int stage{}, stage_type{};
    key = {}; key.entity = entity; key.game = frame.game;
    if (!TryRead(frame.game, stage) || !TryRead(frame.game + 4, stage_type) || stage < 0 || stage > 14 ||
        stage_type < 0 || stage_type > 5) return false;
    const int seed_index = stage + ((stage_type == 4 || stage_type == 5) ? 1 : 0);
    if (seed_index > 14) return false;
    return TryRead(entity + 0x3e8, key.init_seed) && TryRead(frame.game + 0x25d44, key.run_seed) &&
           TryRead(frame.game + 0x25d58 + static_cast<unsigned>(seed_index) * 4, key.floor_seed) &&
           TryRead(frame.game + 0x21558, key.room) && TryRead(frame.game + 0x21560, key.dimension) &&
           key.dimension >= 0 && key.dimension <= 2;
}
bool ReadCycle(std::uintptr_t entity, unsigned current, CycleSnapshot& cycle) {
    unsigned count{};
    if (!TryRead(entity + 0x604, count) || count < 1 || count > 8 || !Readable(entity + 0x5e4, count * 4)) return false;
    unsigned alternates[8]{};
    std::memcpy(alternates, reinterpret_cast<const void*>(entity + 0x5e4), count * 4);
    return CollectCycle(current, alternates, count, EnglishRepentance(), cycle);
}
SalvageFacts ReadSalvage(const FeatureFrame& frame, std::uintptr_t manager, std::uintptr_t player,
                         const ContextState& state) {
    SalvageFacts facts;
    if (state.player.type != 23 || !TryRead(frame.room + 0x10, facts.room_type) ||
        !TryRead(frame.game + 0x21560, facts.dimension) ||
        !TryRead(frame.game + profile::ChallengeId, facts.challenge) || facts.room_type < 0 || facts.room_type >= 30)
        return facts;
    // GetPoolForRoom reads these tables for every non-Error room. Reading the
    // same entry avoids constructing/calling an RNG just to obtain a room pool.
    if (facts.room_type != 3 && !TryRead(frame.game + 0x242c0 + (state.greed ? 0x998 : 0x920) +
                                       static_cast<unsigned>(facts.room_type) * 4, facts.pool)) return facts;
    const auto multiplier = reinterpret_cast<unsigned (*)(const void*, unsigned)>(frame.module + 0x29b030);
    constexpr unsigned trinkets[] = {22, 34, 36, 41, 42, 44, 45};
    for (unsigned id : trinkets) facts.trinkets[id] = multiplier(reinterpret_cast<const void*>(player), id) > 0;
    unsigned achievement = 0;
    if (facts.pool == 5 || facts.pool == 22) achievement = 391;
    else if (facts.pool == 9) achievement = 415;
    else if (facts.pool == 12 || facts.pool == 21) achievement = 411;
    // SalvageCollectible checks these unlocks before the guaranteed first
    // pickup. Suppress that line when its getter cannot safely be queried.
    facts.guarantee_available = !achievement;
    if (achievement && Readable(manager + 0x18, 0x2a8))
        facts.guarantee_available = reinterpret_cast<bool (*)(const void*, unsigned)>(frame.module + 0x439654)(
            reinterpret_cast<const void*>(manager + 0x18), achievement);
    facts.valid = true;
    return facts;
}
void ReadPreviewAvailability(const FeatureFrame& frame, std::uintptr_t manager, unsigned id, ContextState& state) {
    if (!state.player.owned[723]) return;
    for (unsigned index = 0; index < 3 && id && id != 668; ++index) {
        id = SpindownNext(id);
        if (state.player.owned[356] && id != 668) id = SpindownNext(id);
        if (!id) break;
        const auto item = reinterpret_cast<std::uintptr_t (*)(const void*, int)>(frame.module + 0x3c0c50)(
            reinterpret_cast<const void*>(manager + 0x36538), static_cast<int>(id));
        state.spindown_available[id] = Knowledge::Unknown;
        if (!Readable(item, 0x90)) continue;
        const bool available = reinterpret_cast<bool (*)(const void*, unsigned)>(frame.module + 0x3c101c)(
            reinterpret_cast<const void*>(item), 0xffffffff);
        state.spindown_available[id] = available ? Knowledge::Yes : Knowledge::No;
    }
}
void AppendFlip(unsigned alternate, bool hint, ContextDescription& output) {
    Description description;
    if (!alternate || !EnglishRepentance().Find(Category::Collectible, alternate, description)) return;
    output.body.Line("{{Collectible711}} -> "); output.body.Icon("Collectible", alternate);
    output.body.Add(" "); output.body.Add(description.name);
    if (hint) output.body.Line("Hold Map to preview the other item");
}
} // namespace

unsigned ReadEidFlipItem(const FeatureFrame& frame, std::uintptr_t entity) {
    if (!frame.module || !Live(entity, 5, 100, 0x620) || Read<unsigned char>(entity + 0x562)) return 0;
    // This is a shared-pointer's DATA pointer, not an inline save-state object.
    const auto saved = Read<std::uintptr_t>(entity + 0x610);
    if (!Pointer(saved) || !Readable(saved, 12) || Read<unsigned>(saved) != 5 || Read<unsigned>(saved + 4) != 100) return 0;
    const auto alternate = Read<unsigned>(saved + 8);
    return KnownId(alternate) ? alternate : 0;
}

bool AugmentEidSpecial(const FeatureFrame& frame, std::uintptr_t entity, std::uintptr_t player,
                       const ContextObject& object, const ContextState& state, ContextDescription& output) {
    if (!frame.module || !frame.game || !frame.room || object.kind != ContextKind::Pickup ||
        object.category != Category::Collectible || !Live(entity, 5, 100, 0x620) ||
        Read<unsigned char>(entity + 0x562) || !Live(player, 1, 0, 0x2850)) return false;
    std::uintptr_t manager{};
    int now{};
    if (!TryRead(frame.module + profile::ManagerGlobal, manager) || !Pointer(manager) ||
        !Readable(manager + 0x36538, 0x28) || !TryRead(frame.game + 0x24f99c, now) || now < 0) return false;
    if (s_Module != frame.module || s_Game != frame.game || now < s_LastFrame) ResetEidSpecial();
    s_Module = frame.module; s_Game = frame.game; s_LastFrame = now;

    const auto party = ReadParty(frame, manager);
    const unsigned alternate = party.flip ? ReadEidFlipItem(frame, entity) : 0;
    CycleKey key;
    CycleSnapshot cycle;
    const bool keyed = ReadKey(frame, entity, key);
    const bool cycling = party.crown && keyed && ReadCycle(entity, object.id, cycle);
    MapInput input;
    if (keyed) input = s_Map.Update(key, party.held_mask, party.triggered, now);
    else { s_Map.Reset(); input.held = party.held_mask != 0; }

    // Work on a bounded copy so a different preview's Spindown availability
    // cannot overwrite the main selection's context.
    ContextState preview_state = state;
    unsigned displayed = object.id;
    CycleEntry* entry = nullptr;
    bool flip_preview = false;
    if (cycling) {
        entry = &s_Cycles.Touch(key, cycle);
        CycleCache::Advance(*entry, state.player.owned[723] ? input.released : input.pressed, now);
        displayed = entry->selection ? entry->cycle.ids[entry->selection - 1] : 689;
        ReadPreviewAvailability(frame, manager, displayed, preview_state);
        if (!BuildCycleDescription(*entry, preview_state, EnglishRepentance(), output)) return false;
    } else if (alternate && (!object.id || input.held)) {
        displayed = alternate;
        ContextObject preview; preview.id = alternate;
        ReadPreviewAvailability(frame, manager, alternate, preview_state);
        if (!BuildContextDescription(preview, preview_state, EnglishRepentance(), output)) return false;
        flip_preview = true;
    }
    if (!output.valid) return false;
    const auto salvage = ReadSalvage(frame, manager, player, preview_state);
    ApplySalvage(displayed, preview_state, salvage, EnglishRepentance(), output);
    if (flip_preview) output.body.Line(object.id ? "{{Collectible711}} Flip preview - release Map to return" : "{{Collectible711}} Item on the other side");
    else AppendFlip(alternate, !cycling, output);
    if (entry) {
        output.body.Line(state.player.owned[723] ? "Release Map to browse: " : "Press Map to browse: ");
        const auto next = entry->selection < entry->cycle.count ? entry->cycle.ids[entry->selection] : 689;
        output.body.Icon("Collectible", next);
    }
    output.valid = !output.name.truncated && !output.body.truncated;
    // Crown's page controls take priority when it and Flip share a pedestal.
    // The alternate item's name remains visible on every Crown page.
    return output.valid && input.held && (cycling || alternate != 0);
}

void ResetEidSpecial() {
    s_Cycles.Reset(); s_Map.Reset(); s_Module = s_Game = 0; s_LastFrame = -1;
}
} // namespace isaac

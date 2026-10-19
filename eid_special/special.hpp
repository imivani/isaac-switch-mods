#pragma once
#include "../eid/context.hpp"
#include "generated.hpp"

namespace isaac_eid_special {
using namespace isaac_eid;

struct SalvageFacts {
    int room_type = 0, pool = -1, dimension = -1, challenge = -1;
    bool trinkets[190]{};
    bool guarantee_available = false;
    bool valid = false;
};

inline bool QuestItem(unsigned id) {
    for (const auto value : data::QuestItems) if (value == id) return true;
    return false;
}

template<std::size_t N> void Substitute(TextBuffer<N>& output, const char* source,
                                       const char* replacement, bool vowel = false) {
    while (*source && !output.truncated) {
        if (std::strncmp(source, "{1}", 3) == 0) { output.Add(replacement); source += 3; }
        else if (std::strncmp(source, "{n}", 3) == 0) { if (vowel) output.Add("n"); source += 3; }
        else { output.Add(source, 1); ++source; }
    }
}

inline bool ApplySalvage(unsigned id, const ContextState& state, const SalvageFacts& facts,
                         const Catalogue& catalogue, ContextDescription& output) {
    if (!output.valid || !facts.valid || state.player.type != 23 || facts.challenge == 43 ||
        facts.dimension == 2 || !id || QuestItem(id)) return false;
    // Transformation and ordinary collectible effects are no longer relevant:
    // this character breaks a non-quest pedestal instead of acquiring it.
    output.body.Clear();
    output.body.Add("{{Player23}} ");
    const bool birthright = state.player.owned[619];
    output.body.Add(birthright ? data::TaintedCainPedestalBaseBirthright : data::TaintedCainPedestalBase);
    if (facts.room_type != 3 && facts.guarantee_available) for (const auto& rule : data::SalvageRoomTypes) {
        if (static_cast<int>(rule.id) != facts.pool) continue;
        const char* name = rule.name;
        Description card;
        if (rule.card && catalogue.Find(Category::Card, rule.card, card)) name = card.name;
        if (!name || !*name) break;
        output.body.Line("");
        Substitute(output.body, data::TaintedCainPedestalGuaranteed, name, rule.vowel);
        break;
    }
    for (const auto& rule : data::SalvageTrinkets) if (rule.id < 190 && facts.trinkets[rule.id]) {
        output.body.Line(""); output.body.Icon("Trinket", rule.id); output.body.Add(" ");
        Substitute(output.body, data::TaintedCainPedestalBonus, rule.name);
    }
    if (facts.trinkets[42]) {
        output.body.Line("{{Trinket42}} ");
        output.body.Add(birthright ? data::TaintedCainPedestalLuckyToeBirthright : data::TaintedCainPedestalLuckyToe);
    }
    if (facts.trinkets[22]) {
        output.body.Line("{{Trinket22}} "); output.body.Add(data::TaintedCainPedestalDaemonsTail);
    }
    output.valid = !output.name.truncated && !output.body.truncated;
    return output.valid;
}

struct CycleSnapshot {
    unsigned ids[9]{};
    unsigned count = 0;
};

inline bool CollectCycle(unsigned current, const unsigned* alternates, unsigned count,
                          const Catalogue& catalogue, CycleSnapshot& output) {
    output = {};
    if (!alternates || count < 1 || count > 8) return false;
    for (unsigned index = 0; index <= count; ++index) {
        const auto id = index == 0 ? current : alternates[index - 1];
        Description description;
        if (!id || !catalogue.Find(Category::Collectible, id, description)) { output = {}; return false; }
        bool duplicate = false;
        for (unsigned i = 0; i < output.count; ++i) duplicate = duplicate || output.ids[i] == id;
        if (!duplicate) output.ids[output.count++] = id;
    }
    // Vanilla EID's Glitched Crown summary begins with five distinct choices.
    return output.count >= 5;
}

struct CycleKey {
    std::uintptr_t entity = 0, game = 0;
    std::uint32_t init_seed = 0, run_seed = 0, floor_seed = 0;
    int room = 0, dimension = 0;
    bool Same(const CycleKey& other) const {
        return entity == other.entity && game == other.game && init_seed == other.init_seed &&
               run_seed == other.run_seed && floor_seed == other.floor_seed &&
               room == other.room && dimension == other.dimension;
    }
};

struct CycleEntry {
    CycleKey key;
    CycleSnapshot cycle;
    unsigned selection = 0;
    std::uint64_t used = 0;
    int last_input_frame = -1;
};

struct MapInput { bool held = false, pressed = false, released = false; };
constexpr bool ValidNativeController(unsigned device) {
    return (device >= 1 && device <= 8) || device == 33;
}
class MapLatch {
public:
    MapInput Update(const CycleKey& key, unsigned held_mask, bool triggered, int frame) {
        // The caller refreshes at least every six frames while a pedestal is
        // selected, and immediately when any player's Map state changes.
        // Never invent a release after a gap or a change of selected pedestal.
        const bool continuous = valid_ && key_.Same(key) && frame >= frame_ && frame - frame_ <= 6;
        MapInput input{held_mask != 0, triggered && held_mask != 0,
                       continuous && (mask_ & ~held_mask) != 0};
        key_ = key; mask_ = held_mask; frame_ = frame; valid_ = frame >= 0;
        return input;
    }
    void Reset() { *this = MapLatch{}; }
private:
    CycleKey key_{};
    unsigned mask_ = 0;
    int frame_ = -1;
    bool valid_ = false;
};

class CycleCache {
public:
    void Reset() { *this = CycleCache{}; }
    CycleEntry& Touch(const CycleKey& key, const CycleSnapshot& cycle) {
        unsigned slot = 0;
        for (unsigned i = 0; i < Capacity; ++i) {
            if (entries_[i].used && entries_[i].key.Same(key)) { slot = i; break; }
            if (!entries_[i].used || entries_[i].used < entries_[slot].used) slot = i;
        }
        auto& entry = entries_[slot];
        bool same = entry.used && entry.key.Same(key) && entry.cycle.count == cycle.count;
        if (same) for (unsigned i = 0; i < cycle.count; ++i) {
            bool found = false;
            for (unsigned j = 0; j < entry.cycle.count; ++j) found = found || cycle.ids[i] == entry.cycle.ids[j];
            same = same && found;
        }
        if (!same) { entry = {}; entry.key = key; entry.cycle = cycle; }
        entry.used = ++serial_;
        return entry;
    }
    static void Advance(CycleEntry& entry, bool triggered, int frame) {
        if (!triggered || frame < 0 || frame == entry.last_input_frame) return;
        entry.last_input_frame = frame;
        entry.selection = (entry.selection + 1) % (entry.cycle.count + 1);
    }
    static constexpr unsigned Capacity = 32;
private:
    CycleEntry entries_[Capacity]{};
    std::uint64_t serial_ = 0;
};

inline bool BuildCycleDescription(const CycleEntry& entry, const ContextState& state,
                                  const Catalogue& catalogue, ContextDescription& output) {
    if (entry.cycle.count < 5 || entry.cycle.count > 9 || entry.selection > entry.cycle.count) return false;
    ContextObject object;
    object.id = entry.selection ? entry.cycle.ids[entry.selection - 1] : 689;
    if (!BuildContextDescription(object, state, catalogue, output)) return false;
    if (!entry.selection) {
        output.body.Clear();
        for (unsigned i = 0; i < entry.cycle.count; ++i) {
            Description description;
            if (!catalogue.Find(Category::Collectible, entry.cycle.ids[i], description)) return false;
            output.body.Line(""); output.body.Icon("Collectible", entry.cycle.ids[i]); output.body.Add(" ");
            output.body.Add(description.name);
        }
    }
    output.valid = !output.name.truncated && !output.body.truncated;
    return output.valid;
}
} // namespace isaac_eid_special

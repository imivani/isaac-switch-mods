#include "special.hpp"
#include "../eid_ui/layout.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace isaac_eid;
using namespace isaac_eid_special;
static ContextDescription output;
static ContextState state;
static isaac_eid_ui::Builder builder;
static isaac_eid_ui::Layout layout;
static unsigned checks = 0, layouts = 0;

static void Check(bool condition) { ++checks; assert(condition); }
static bool Contains(const char* text) { return std::strstr(output.body.data, text) != nullptr; }
static bool Build(unsigned id) { ContextObject object; object.id = id; return BuildContextDescription(object, state, EnglishRepentance(), output); }
static void Layout(const char* text, bool body) {
    builder.Build(text, body ? 142 : 122, body, layout);
    Check(!layout.truncated && layout.unknown_markup == 0 && layout.omitted_variable_paragraphs == 0);
    for (unsigned i = 0; i < layout.line_count; ++i) Check(layout.lines[i].width <= (body ? 142 : 122));
    ++layouts;
}
static void LayoutOutput() { Layout(output.name.data, false); Layout(output.body.data, true); }

int main() {
    const auto catalogue = EnglishRepentance();
    SalvageFacts facts;
    state.player.type = 23;
    facts.valid = facts.guarantee_available = true;
    facts.dimension = 0; facts.challenge = 0; facts.room_type = 1;
    Check(Build(1)); Check(ApplySalvage(1, state, facts, catalogue, output));
    Check(Contains("3-8 random pickups")); Check(!Contains("Tears")); LayoutOutput();
    state.player.owned[619] = true;
    for (const auto& rule : data::SalvageTrinkets) facts.trinkets[rule.id] = true;
    facts.trinkets[42] = facts.trinkets[22] = true;
    for (const auto& rule : data::SalvageRoomTypes) {
        facts.pool = static_cast<int>(rule.id);
        Check(Build(1)); Check(ApplySalvage(1, state, facts, catalogue, output));
        Check(Contains("6-16")); Check(Contains("Guaranteed")); Check(Contains("Hearts are 80%"));
        Check(Contains("66% chance for {{BlinkBirthright}}2"));
        for (const auto& bonus : data::SalvageTrinkets) Check(Contains(bonus.name));
        if (rule.id == 4) Check(Contains("an Eternal Heart"));
        if (rule.id == 9) Check(Contains("Cracked Key"));
        LayoutOutput();
    }
    facts.guarantee_available = false;
    Check(Build(1)); Check(ApplySalvage(1, state, facts, catalogue, output)); Check(!Contains("Guaranteed"));
    facts.guarantee_available = true; facts.room_type = 3;
    Check(Build(1)); Check(ApplySalvage(1, state, facts, catalogue, output)); Check(!Contains("Guaranteed"));
    facts.room_type = 1;
    for (int mode = 0; mode < 5; ++mode) {
        facts.valid = mode != 0; state.player.type = mode == 1 ? 0 : 23;
        facts.dimension = mode == 2 ? 2 : 0; facts.challenge = mode == 3 ? 43 : 0;
        Check(Build(1)); const auto before = output;
        Check(!ApplySalvage(mode == 4 ? 0 : 1, state, facts, catalogue, output));
        Check(std::strcmp(before.body.data, output.body.data) == 0);
    }
    facts.valid = true; facts.challenge = 0;
    for (unsigned id : data::QuestItems) {
        Check(Build(id)); const auto before = output;
        Check(!ApplySalvage(id, state, facts, catalogue, output));
        Check(std::strcmp(before.body.data, output.body.data) == 0);
    }
    // Check every known collectible: quest exceptions keep their real text;
    // all others replace ordinary effects, while retaining the name/art ID.
    for (unsigned id = 1; id <= 732; ++id) {
        if (!Build(id)) continue;
        const unsigned icon = output.icon_id;
        Check(ApplySalvage(id, state, facts, catalogue, output) == !QuestItem(id));
        Check(output.icon_id == icon);
        if (!QuestItem(id)) { Check(Contains("random pickups")); LayoutOutput(); }
    }

    state = {};
    unsigned alternates[8] = {2,3,4,5,6,7,8,9};
    CycleSnapshot cycle;
    Check(!CollectCycle(1, nullptr, 4, catalogue, cycle));
    Check(!CollectCycle(1, alternates, 0, catalogue, cycle));
    Check(!CollectCycle(1, alternates, 9, catalogue, cycle));
    Check(!CollectCycle(1, alternates, 3, catalogue, cycle));
    Check(CollectCycle(1, alternates, 4, catalogue, cycle)); Check(cycle.count == 5);
    Check(CollectCycle(1, alternates, 8, catalogue, cycle)); Check(cycle.count == 9);
    alternates[4] = 1;
    Check(CollectCycle(1, alternates, 5, catalogue, cycle)); Check(cycle.count == 5);
    alternates[1] = 65535;
    Check(!CollectCycle(1, alternates, 4, catalogue, cycle)); Check(cycle.count == 0);
    alternates[1] = 3; alternates[4] = 6;
    Check(CollectCycle(1, alternates, 4, catalogue, cycle));
    CycleKey key; key.entity = 0x10008; key.game = 0x100000; key.init_seed = 10; key.run_seed = 20; key.floor_seed = 30;
    CycleCache cache;
    auto& first = cache.Touch(key, cycle);
    Check(first.selection == 0); Check(BuildCycleDescription(first, state, catalogue, output));
    Check(output.icon_id == 689); Check(Contains("{{Collectible1}}")); LayoutOutput();
    CycleCache::Advance(first, true, 10); Check(first.selection == 1);
    CycleCache::Advance(first, true, 10); Check(first.selection == 1);
    CycleCache::Advance(first, false, 11); Check(first.selection == 1);
    CycleSnapshot rotated{{3,4,5,1,2},5};
    auto& same = cache.Touch(key, rotated);
    Check(same.selection == 1); Check(same.cycle.ids[0] == 1);
    Check(BuildCycleDescription(same, state, catalogue, output)); Check(output.icon_id == 1); LayoutOutput();
    for (int frame = 11; frame <= 15; ++frame) CycleCache::Advance(same, true, frame);
    Check(same.selection == 0);
    CycleSnapshot rerolled{{6,7,8,9,10},5};
    auto& changed = cache.Touch(key, rerolled);
    Check(changed.selection == 0 && changed.cycle.ids[0] == 6);
    CycleCache::Advance(changed, true, 20);
    auto other_key = key; ++other_key.init_seed;
    Check(cache.Touch(other_key, cycle).selection == 0);
    Check(cache.Touch(key, rerolled).selection == 1);
    for (unsigned i = 0; i < CycleCache::Capacity; ++i) {
        other_key.entity = 0x20000 + i * 8;
        cache.Touch(other_key, cycle);
    }
    Check(cache.Touch(key, cycle).selection == 0); // LRU eviction.
    cache.Reset(); Check(cache.Touch(key, rerolled).selection == 0);
    for (unsigned selection = 0; selection <= cycle.count; ++selection) {
        auto& entry = cache.Touch(key, cycle); entry.selection = selection;
        Check(BuildCycleDescription(entry, state, catalogue, output)); LayoutOutput();
    }

    MapLatch input;
    Check(!ValidNativeController(0));
    for (unsigned device = 1; device <= 8; ++device) Check(ValidNativeController(device));
    Check(ValidNativeController(33));
    Check(!ValidNativeController(9) && !ValidNativeController(32) && !ValidNativeController(34));
    auto edge = input.Update(key, 1, true, 10); Check(edge.held && edge.pressed && !edge.released);
    edge = input.Update(key, 1, false, 16); Check(edge.held && !edge.pressed && !edge.released);
    edge = input.Update(key, 0, false, 17); Check(!edge.held && !edge.pressed && edge.released);
    edge = input.Update(key, 3, true, 18); Check(edge.pressed);
    edge = input.Update(key, 1, false, 19); Check(edge.held && edge.released); // Co-op controller release.
    edge = input.Update(key, 0, false, 30); Check(!edge.released); // Lost selection must not fabricate release.
    input.Update(key, 1, true, 31);
    edge = input.Update(other_key, 0, false, 32); Check(!edge.released);
    input.Update(key, 1, true, 33);
    edge = input.Update(key, 0, false, 1); Check(!edge.released); // Rewind.
    input.Reset(); edge = input.Update(key, 0, false, 2); Check(!edge.released);
    TextBuffer<8> tiny; Substitute(tiny, "Guaranteed {1}", "Black Heart"); Check(tiny.truncated);
    std::printf("PASS: %u EID special checks; %u layouts; no game process used\n", checks, layouts);
}

#include "context.hpp"
#include "../eid_ui/layout.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <initializer_list>

using namespace isaac_eid;
static ContextState state;
static ContextDescription output;
static isaac_eid_ui::Builder builder;
static isaac_eid_ui::Layout layout;
static unsigned layouts = 0;

static void CheckLayout(const char* text, bool bullets) {
    builder.Build(text, bullets ? 142 : 122, bullets, layout);
    assert(!layout.truncated && layout.unknown_markup == 0 && layout.omitted_variable_paragraphs == 0);
    for (unsigned line = 0; line < layout.line_count; ++line) {
        const auto& row = layout.lines[line];
        assert(row.width <= (bullets ? 142 : 122));
        for (unsigned i = row.first; i < row.first + row.count; ++i)
            assert(layout.cells[i].x + layout.cells[i].width <= (bullets ? 142 : 122));
    }
    ++layouts;
}
static bool Build(ContextObject object) {
    const bool result = BuildContextDescription(object, state, EnglishRepentance(), output);
    if (result) { CheckLayout(output.name.data, false); CheckLayout(output.body.data, true); }
    return result;
}
static bool Contains(const char* text) { return std::strstr(output.body.data, text); }

int main() {
    state = {};
    // A known empty pedestal may carry a verified Flip alternate. Identity0 is
    // permitted at this policy layer; native selection separately requires Flip.
    PickupSnapshot empty;
    empty.active_and_visible = true; empty.blind = Knowledge::No;
    RunSnapshot live;
    live.in_play = true; live.curse_of_blind = Knowledge::No;
    assert(DecidePresentation(empty,live) == Presentation::Description);
    empty.blind = Knowledge::Yes;
    assert(DecidePresentation(empty,live) == Presentation::Hidden);
    empty.blind = Knowledge::No; live.curse_of_blind = Knowledge::Yes;
    assert(DecidePresentation(empty,live) == Presentation::Hidden);
    for (unsigned paid = 0; paid < 32; ++paid) {
        assert(Build({ContextKind::Sacrifice, Category::Collectible, paid}));
        const unsigned next = NextSacrifice(paid);
        assert(next >= 1 && next <= 12);
        assert(std::strcmp(output.body.data, ById(context_data::sacrifice, next)->markup) == 0);
    }
    assert(NextSacrifice(std::numeric_limits<unsigned>::max()) == 12);
    state.stage = 2;
    assert(Build({ContextKind::Sacrifice, Category::Collectible, 0}));
    assert(!Contains("bomb") && Contains("50%"));
    for (unsigned dice = 1; dice <= 6; ++dice) assert(Build({ContextKind::Dice, Category::Collectible, dice}));
    assert(!Build({ContextKind::Dice, Category::Collectible, 7}));
    for (unsigned portal = 0; portal < 3; ++portal) {
        assert(Build({ContextKind::Portal, Category::Collectible, portal}));
        assert(!std::strchr(output.body.data, '#'));
    }
    assert(Build({ContextKind::Sanguine})); assert(Contains("%"));
    for (unsigned character = 1; character <= 41; ++character) assert(Build({ContextKind::Character, Category::Collectible, character}));
    ContextObject pill{ContextKind::Pickup, Category::Pill, 3, 5, false, false};
    assert(Build(pill)); assert(output.body.size == 0 && std::strcmp(output.name.data,"Unidentified pill")==0);
    assert(output.icon_id == 5); // The visible color never becomes the effect ID.
    pill.identified = true;
    assert(Build(pill)); assert(Contains("Soul Heart"));
    pill.category = Category::HorsePill; pill.subtype = 0x805;
    assert(Build(pill)); assert(Contains("+4 Soul Hearts") && output.icon_id == 5);
    pill.id = 9999;
    assert(Build(pill)); assert(Contains("Random horse pill effect"));
    state.player.owned[654] = true;
    pill.id = 48;
    assert(Build(pill)); assert(Contains("+1.2 Damage"));
    pill.id = 43;
    assert(Build(pill)); assert(Contains("Black Heart"));
    state = {};
    assert(Build({ContextKind::Pickup, Category::Trinket, 2, 0x8002, true}));
    assert(Contains("60"));
    state.player.owned[439] = true;
    assert(Build({ContextKind::Pickup, Category::Trinket, 2, 0x8002, true}));
    assert(Contains("70")); // Upstream uses custom1.4 multiplier, not blanket3x.
    state.player.owned[451] = true;
    assert(Build({ContextKind::Pickup, Category::Card, 3})); assert(Contains("second foot"));
    state.player.owned[263] = true;
    assert(Build({ContextKind::Pickup, Category::Card, 33})); assert(Contains("Clear Rune: 12"));
    state.player.owned[356] = true;
    assert(Build({ContextKind::Pickup, Category::Collectible, 34})); assert(Contains("{{ColorYellow}}3"));
    state.player.owned[723] = true;
    assert(Build({ContextKind::Pickup, Category::Collectible, 733-1})); assert(Contains("{{Collectible723}}"));
    assert(SpindownNext(1) == 0 && SpindownNext(668) == 668 && SpindownNext(2) == 1);
    state = {}; state.player.type = 10;
    assert(Build({ContextKind::Pickup, Category::Collectible, 108})); assert(Contains("No effect for The Lost"));
    state.player.type = 31;
    assert(Build({ContextKind::Pickup, Category::Collectible, 108})); assert(Contains("No effect for The Lost"));
    state.player.type = 0;
    assert(Build({ContextKind::Pickup, Category::Collectible, 108})); assert(!Contains("No effect for"));
    state.greed = true;
    assert(Build({ContextKind::Pickup, Category::Card, 19})); assert(Contains("first room"));
    state.greed = false;
    assert(Build({ContextKind::Pickup, Category::Card, 19})); assert(!Contains("first room"));
    state = {}; state.player.type = 0;
    assert(Build({ContextKind::Pickup, Category::Collectible, 619})); assert(Contains(ById(context_data::birthright,1)->markup));
    state.player.forms_known = true; state.player.forms[0] = 2;
    assert(Build({ContextKind::Pickup, Category::Collectible, 145})); assert(Contains("Guppy (2/3)"));
    assert(Build({ContextKind::Pickup, Category::Collectible, 164})); assert(Contains("Recharges over time") && !Contains("110 seconds"));
    bool walkable[15]; for (bool& tile : walkable) tile = true;
    bool reached[15]{};
    for (unsigned row = 0; row < 3; ++row) walkable[row*5+2] = false;
    assert(BuildReachability(walkable,5,3,0,reached));
    assert(reached[11] && !reached[3] && !reached[14]); // Wall isolates the other half.
    walkable[7] = true;
    assert(BuildReachability(walkable,5,3,0,reached) && reached[14]);
    assert(!BuildReachability(walkable,100,100,0,reached));
    assert(!BuildReachability(walkable,5,3,15,reached));
    TextBuffer<12> bounded;
    bounded.Add("abc room"); assert(bounded.ReplaceFirst("room","wave") && std::strcmp(bounded.data,"abc wave")==0);
    assert(!bounded.ReplaceFirst("wave","a very long value") && bounded.truncated);
    assert(std::strcmp(bounded.data,"abc wave")==0); // Failed edits preserve valid data.
    unsigned contexts = 0;
    const auto catalogue = EnglishRepentance();
    for (int mode = 0; mode < 3; ++mode) {
        state = {}; state.player.type = mode == 1 ? 10 : 0; state.greed = mode == 2;
        state.player.forms_known = true;
        if (mode == 2) for (unsigned id : {356u,439u,451u,654u,664u,723u}) state.player.owned[id] = true;
        for (std::size_t i = 0; i < catalogue.count; ++i) {
            const auto& row = catalogue.entries[i];
            if (row.placeholder) continue;
            ContextObject object{ContextKind::Pickup, static_cast<Category>(row.category), row.lookup_id, 1, mode==2};
            assert(Build(object)); ++contexts;
        }
    }
    for (const auto& condition : context_data::conditions) {
        state = {}; state.greed = condition.greed;
        for (unsigned character = 0; character < 41; ++character) if (condition.players & (std::uint64_t{1} << character)) { state.player.type = static_cast<int>(character); break; }
        assert(Build({ContextKind::Pickup,static_cast<Category>(condition.category),condition.id})); ++contexts;
    }
    std::printf("PASS: %u context descriptions, %u measured layouts; sacrifice, dice, pills, modifiers, character/Greed conditions, metadata, hidden identity and bounded reachability checks.\n", contexts, layouts);
}

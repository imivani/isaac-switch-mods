#include "catalogue.hpp"
#include "markup.hpp"
#include "pickup_policy.hpp"
#include <cassert>
#include <cstring>
#include <cstdio>

using namespace isaac_eid;

int main() {
    const auto catalogue = EnglishRepentance();
    Description found;
    assert(catalogue.count == 1120);
    assert(catalogue.Find(Category::Collectible, 1, found));
    assert(std::strcmp(found.name, "The Sad Onion") == 0);
    assert(std::strstr(found.markup, "{{Tears}} +0.7 Tears"));
    assert(catalogue.Find(Category::Collectible, 5, found));
    assert(std::strstr(found.markup, "+1.5 Damage"));
    assert(!catalogue.Find(Category::Collectible, 718, found));
    assert(!found.name && !found.markup);
    assert(!catalogue.Find(Category::Collectible, 0, found));
    assert(!catalogue.Find(Category::Collectible, 65537, found));
    assert(catalogue.Find(Category::Pill, 4, found));
    assert(std::strcmp(found.name, "Bombs are Key") == 0);
    assert(catalogue.Find(Category::HorsePill, 3, found));
    assert(std::strstr(found.markup, "+4 Soul Hearts"));
    assert(catalogue.Find(Category::Pill, 9999, found));
    assert(catalogue.Find(Category::Trinket, 57, found));
    assert(std::strcmp(found.name, "??" "?'s Soul") == 0);
    assert(TrinketLookupId(0x8001) == 1);
    assert(PillLookupId(0) == 1 && PillLookupId(49) == 50 && PillLookupId(-1) == 0);

    // Every offset lands on the expected strings; all non-placeholder records
    // can be found, including the final record in each binary-search range.
    std::size_t valid = 0;
    for (std::size_t i = 0; i < catalogue.count; ++i) {
        const auto& entry = catalogue.entries[i];
        const bool success = catalogue.Find(static_cast<Category>(entry.category), entry.lookup_id, found);
        assert(success == !entry.placeholder);
        if (success) {
            assert(found.name == catalogue.strings + entry.name_offset);
            assert(found.markup == catalogue.strings + entry.markup_offset);
            const auto bytes = std::strlen(found.markup);
            MarkupReader all_markup(found.markup, bytes);
            Token part{};
            std::size_t covered = 0, iterations = 0;
            while (all_markup.Next(part)) {
                assert(++iterations <= bytes + 1);
                assert(part.text >= found.markup && part.text + part.size <= found.markup + bytes);
                covered += part.size + (part.kind == TokenKind::Markup ? 4 : part.kind == TokenKind::Variable ? 2 : 0);
            }
            assert(covered == bytes);
            ++valid;
        }
    }
    assert(valid == 1108);

    const char markup[] = "A{{Tears}}+1#{{ColorRed}}B{{CR}} {1} {oops";
    MarkupReader reader(markup, sizeof(markup) - 1);
    Token token{};
    int symbols = 0, breaks = 0, variables = 0, texts = 0;
    while (reader.Next(token)) {
        assert(token.text >= markup && token.text + token.size <= markup + sizeof(markup) - 1);
        if (token.kind == TokenKind::Markup) ++symbols;
        if (token.kind == TokenKind::LineBreak) ++breaks;
        if (token.kind == TokenKind::Variable) ++variables;
        if (token.kind == TokenKind::Text) ++texts;
    }
    assert(symbols == 3 && breaks == 1 && variables == 1 && texts == 6);
    MarkupReader empty(nullptr, 100);
    assert(!empty.Next(token));
    const char malformed[] = "{{not-closed#still-text";
    MarkupReader bad(malformed, sizeof(malformed) - 1);
    int iterations = 0;
    while (bad.Next(token)) assert(++iterations < 10);

    RunSnapshot run;
    run.in_play = true;
    run.curse_of_blind = Knowledge::No;
    PickupSnapshot pickups[3]{};
    for (auto& pickup : pickups) {
        pickup.active_and_visible = true;
        pickup.blind = Knowledge::No;
        pickup.lookup_id = 1;
    }
    pickups[0].x = 30;
    pickups[1].x = 10;
    pickups[1].lookup_id = 5;
    pickups[2].x = 3;
    pickups[2].blind = Knowledge::Yes;
    auto selection = SelectNearest(catalogue, pickups, 3, run, 100);
    assert(selection.pickup == &pickups[1]);
    assert(std::strcmp(selection.description.name, "My Reflection") == 0);
    run.curse_of_blind = Knowledge::Yes;
    assert(!SelectNearest(catalogue, pickups, 3, run, 100).pickup);
    run.curse_of_blind = Knowledge::Unknown;
    assert(!SelectNearest(catalogue, pickups, 3, run, 100).pickup);
    run.curse_of_blind = Knowledge::No;
    run.in_play = false;
    assert(!SelectNearest(catalogue, pickups, 3, run, 100).pickup);
    run.in_play = true;
    run.special_challenge = true;
    assert(!SelectNearest(catalogue, pickups, 3, run, 100).pickup);
    run.special_challenge = false;
    assert(!SelectNearest(catalogue, pickups, 4097, run, 100).pickup);
    assert(!SelectNearest(catalogue, pickups, 3, run, -1).pickup);

    PickupSnapshot card;
    card.category = Category::Card;
    card.lookup_id = 1;
    card.active_and_visible = true;
    card.reachable = Knowledge::Yes;
    card.shop_item = Knowledge::No;
    card.options_choice = Knowledge::No;
    assert(DecidePresentation(card, run) == Presentation::Description);
    card.shop_item = Knowledge::Yes;
    assert(DecidePresentation(card, run) == Presentation::Hidden);
    card.lookup_id = 81;
    card.reachable = Knowledge::No;
    assert(DecidePresentation(card, run) == Presentation::Description);
    card.options_choice = Knowledge::Yes;
    assert(DecidePresentation(card, run) == Presentation::Hidden);
    card.options_choice = Knowledge::Unknown;
    assert(DecidePresentation(card, run) == Presentation::Hidden);

    PickupSnapshot pill;
    pill.category = Category::Pill;
    pill.lookup_id = PillLookupId(3);
    pill.active_and_visible = true;
    pill.reachable = Knowledge::Yes;
    pill.pill_identified = Knowledge::No;
    selection = SelectNearest(catalogue, &pill, 1, run, 100);
    assert(selection.presentation == Presentation::UnidentifiedPill);
    assert(std::strcmp(selection.description.name, "Unidentified Pill") == 0);
    pill.pill_identified = Knowledge::Yes;
    selection = SelectNearest(catalogue, &pill, 1, run, 100);
    assert(selection.presentation == Presentation::Description);
    assert(std::strcmp(selection.description.name, "Bombs are Key") == 0);
    pill.reachable = Knowledge::Unknown;
    assert(DecidePresentation(pill, run) == Presentation::Hidden);
    pill.reachable = Knowledge::Yes;
    pill.pill_identified = Knowledge::No;
    pill.golden_pill = true;
    assert(DecidePresentation(pill, run) == Presentation::Hidden);
    pill.lookup_id = 9999;
    assert(DecidePresentation(pill, run) == Presentation::Description);

    std::puts("PASS: EID catalogue, markup and pickup-knowledge regressions");
    return 0;
}

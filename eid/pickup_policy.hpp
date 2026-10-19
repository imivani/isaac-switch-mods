#pragma once
#include "catalogue.hpp"

namespace isaac_eid {

enum class Knowledge : std::uint8_t { Unknown, No, Yes };
enum class Presentation : std::uint8_t { Hidden, Description, UnidentifiedPill };

struct PickupSnapshot {
    Category category = Category::Collectible;
    std::uint32_t lookup_id = 0;
    float x = 0, y = 0;
    // The Switch adapter must take these values from the current room on the
    // game's own update/render thread. Defaults suppress unknown information.
    bool active_and_visible = false;
    Knowledge blind = Knowledge::Unknown;
    Knowledge shop_item = Knowledge::Unknown;
    Knowledge options_choice = Knowledge::Unknown;
    Knowledge reachable = Knowledge::Unknown;
    Knowledge pill_identified = Knowledge::Unknown;
    bool golden_pill = false;
};

struct RunSnapshot {
    bool in_play = false;
    bool special_challenge = false; // Until challenge-specific rules are ported.
    Knowledge curse_of_blind = Knowledge::Unknown;
    float player_x = 0, player_y = 0;
};

// Mirrors the standard EID visibility defaults, with unreadable state hidden.
// Pickup::Touched is deliberately not treated as an identity-knowledge flag.
inline Presentation DecidePresentation(const PickupSnapshot& pickup, const RunSnapshot& run) {
    if (!run.in_play || run.special_challenge || !pickup.active_and_visible) return Presentation::Hidden;
    switch (pickup.category) {
    case Category::Collectible:
        return run.curse_of_blind == Knowledge::No && pickup.blind == Knowledge::No
            ? Presentation::Description : Presentation::Hidden;
    case Category::Trinket:
        return Presentation::Description;
    case Category::Card: {
        const bool soul_stone = pickup.lookup_id >= 81 && pickup.lookup_id <= 97;
        if (pickup.options_choice != Knowledge::No || pickup.shop_item == Knowledge::Unknown)
            return Presentation::Hidden;
        if (pickup.shop_item == Knowledge::Yes && !soul_stone) return Presentation::Hidden;
        return soul_stone || pickup.reachable == Knowledge::Yes
            ? Presentation::Description : Presentation::Hidden;
    }
    case Category::Pill:
    case Category::HorsePill:
        if (pickup.reachable != Knowledge::Yes) return Presentation::Hidden;
        if (pickup.golden_pill) return pickup.lookup_id == 9999 ? Presentation::Description : Presentation::Hidden;
        return pickup.pill_identified == Knowledge::Yes
            ? Presentation::Description : Presentation::UnidentifiedPill;
    }
    return Presentation::Hidden;
}

struct Selection {
    const PickupSnapshot* pickup = nullptr;
    Presentation presentation = Presentation::Hidden;
    Description description{};
};

inline bool ValidCoordinate(float value) {
    // Also rejects NaN and infinities, without depending on a C math runtime.
    return value >= -1000000.f && value <= 1000000.f;
}

inline Selection SelectNearest(const Catalogue& catalogue, const PickupSnapshot* pickups,
                               std::size_t count, const RunSnapshot& run, float radius) {
    Selection selection;
    if (!pickups || count > 4096 || !run.in_play || !ValidCoordinate(run.player_x) ||
        !ValidCoordinate(run.player_y) || !(radius >= 0 && radius <= 1000000.f)) return selection;
    float best = radius * radius;
    for (std::size_t index = 0; index < count; ++index) {
        const auto& pickup = pickups[index];
        const auto presentation = DecidePresentation(pickup, run);
        if (presentation == Presentation::Hidden || !ValidCoordinate(pickup.x) || !ValidCoordinate(pickup.y)) continue;
        const float dx = pickup.x - run.player_x, dy = pickup.y - run.player_y;
        const float distance = dx * dx + dy * dy;
        if (distance > best || (selection.pickup && distance == best)) continue;
        Description description;
        if (presentation == Presentation::UnidentifiedPill) {
            description = {"Unidentified Pill", ""};
        } else if (!catalogue.Find(pickup.category, pickup.lookup_id, description)) {
            continue;
        }
        best = distance;
        selection = {&pickup, presentation, description};
    }
    return selection;
}

// These helpers normalize identity only; they do not determine knowledge.
inline std::uint32_t TrinketLookupId(std::uint32_t subtype) { return subtype & 0x7fffu; }
inline std::uint32_t PillLookupId(std::int32_t resolved_effect) {
    return resolved_effect >= 0 && resolved_effect < 50 ? static_cast<std::uint32_t>(resolved_effect + 1) : 0;
}

} // namespace isaac_eid

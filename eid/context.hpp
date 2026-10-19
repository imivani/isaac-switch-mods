#pragma once
#include "catalogue.hpp"
#include "pickup_policy.hpp"
#include "generated/context.hpp"
#include <cstdio>
#include <cstring>

namespace isaac_eid {

template<std::size_t N> struct TextBuffer {
    char data[N]{};
    std::size_t size = 0;
    bool truncated = false;
    void Clear() { size = 0; data[0] = 0; truncated = false; }
    bool Add(const char* value, std::size_t length) {
        if (!value || length >= N - size) { truncated = true; return false; }
        std::memcpy(data + size, value, length); size += length; data[size] = 0; return true;
    }
    bool Add(const char* value) { return value && Add(value, std::strlen(value)); }
    void Number(int value) { char buffer[24]; std::snprintf(buffer, sizeof(buffer), "%d", value); Add(buffer); }
    void Line(const char* value) { if (size) Add("#"); Add(value); }
    void Icon(const char* prefix, unsigned value) { Add("{{"); Add(prefix); Number(static_cast<int>(value)); Add("}}"); }
    bool ReplaceFirst(const char* old_text, const char* new_text) {
        if (!old_text || !new_text || !old_text[0]) return false;
        auto* position = std::strstr(data, old_text);
        if (!position) return false;
        const auto old_size = std::strlen(old_text), new_size = std::strlen(new_text);
        if (new_size >= N || new_size > old_size + N - size - 1) { truncated = true; return false; }
        std::memmove(position + new_size, position + old_size, size - static_cast<std::size_t>(position - data) - old_size + 1);
        std::memcpy(position, new_text, new_size);
        size = size - old_size + new_size;
        return true;
    }
};

template<class T, std::size_t N> const T* ById(const T (&entries)[N], unsigned id) {
    std::size_t first = 0, last = N;
    while (first < last) { const auto middle = first + (last - first) / 2; if (entries[middle].id < id) first = middle + 1; else last = middle; }
    return first < N && entries[first].id == id ? &entries[first] : nullptr;
}
inline const context_data::Item* ItemMetadata(unsigned id) { return ById(context_data::items, id); }
inline const context_data::Pocket* PocketMetadata(Category category, unsigned id) {
    const auto value = category == Category::HorsePill ? 70u : static_cast<unsigned>(category);
    for (const auto& row : context_data::pockets) if (row.category == value && row.id == id) return &row;
    return nullptr;
}

enum class ContextKind : std::uint8_t { Pickup, Sacrifice, Dice, Sanguine, Portal, Crafting, Character };
struct ContextObject {
    ContextKind kind = ContextKind::Pickup;
    Category category = Category::Collectible;
    unsigned id = 0, subtype = 0;
    bool golden = false, identified = true;
};
struct PlayerContext {
    int type = -1;
    bool owned[733]{};
    int forms[15]{};
    bool forms_known = false;
    bool can_fly = false;
    int red_hearts = -1, soul_hearts = -1, bone_hearts = -1;
};
struct ContextState {
    PlayerContext player;
    unsigned player_types[8]{};
    unsigned player_count = 0;
    int stage = 0, stage_type = 0;
    bool greed = false;
    // Native adapter fills availability only for the at-most-66 preceding IDs
    // needed by Spindown. Unknown is shown as a question mark, never asserted.
    Knowledge spindown_available[733]{};
};
struct ContextDescription {
    TextBuffer<256> name;
    TextBuffer<8192> body;
    int icon_variant = 0;
    unsigned icon_id = 0;
    const char* static_icon = nullptr;
    bool valid = false;
    void Clear() { name.Clear(); body.Clear(); icon_variant = 0; icon_id = 0; static_icon = nullptr; valid = false; }
    Description View() const { return {name.data, body.data}; }
};

inline unsigned NextSacrifice(unsigned completed) { return completed >= 11 ? 12 : completed + 1; }
inline unsigned SpindownNext(unsigned id) {
    if (id == 668) return id;
    for (unsigned attempts = 0; id > 0 && attempts <= 10; ++attempts) {
        --id;
        const auto* item = ItemMetadata(id);
        if (item && !item->hidden) return id;
    }
    return 0;
}

inline void AppendSpindown(unsigned id, const ContextState& state, ContextDescription& output) {
    if (id == 668 || !state.player.owned[723]) return;
    output.body.Line("{{Collectible723}} ");
    for (unsigned i = 0; i < 3; ++i) {
        id = SpindownNext(id);
        if (state.player.owned[356] && id != 668) id = SpindownNext(id);
        if (!id) { output.body.Add("Item disappears"); break; }
        output.body.Icon("Collectible", id);
        if (state.spindown_available[id] != Knowledge::Yes) output.body.Add("?");
        if (id == 668) break;
        if (i < 2) output.body.Add(" -> ");
    }
    if (state.player.owned[356]) output.body.Add(" ({{Collectible356}})");
}

inline void AppendMetadata(unsigned id, const ContextState& state, ContextDescription& output) {
    const auto* item = ItemMetadata(id);
    if (!item) return;
    output.name.Add(" "); output.name.Icon("Quality", item->quality);
    if (item->type == 3) {
        output.body.Line("{{Battery}} ");
        if (item->charge_type == 2 || id == 263 || id == 286 || id == 348 || id == 489) output.body.Add("Special recharge");
        else if (item->charge_type == 1 && item->charges >= 30 && item->charges % 30 == 0) {
            // Timed charges count 30 per second (Butter Bean: 90 = 3 s, 60 = 2 s).
            output.body.Add("Recharges in "); output.body.Number(static_cast<int>(item->charges / 30));
            output.body.Add(item->charges == 30 ? " second" : " seconds");
        }
        else if (item->charge_type == 1) output.body.Add("Recharges over time");
        else if (item->charges == 0) output.body.Add("No recharge");
        else { output.body.Number(static_cast<int>(item->charges)); output.body.Add(" room charges"); }
    }
    static constexpr const char* names[13] = {"Guppy","Beelzebub","Fun Guy","Seraphim","Bob","Spun","Yes Mother?","Conjoined","Leviathan","Oh Crap","Bookworm","Adult","Spider Baby"};
    static constexpr const char* icons[13] = {"Guppy","Beelzebub","FunGuy","Seraphim","Bob","Spun","Mom","Conjoined","Leviathan","OhCrap","Bookworm","Adult","SpiderBaby"};
    for (unsigned form = 0; form < 13; ++form) if (item->forms & (1u << form)) {
        output.body.Line("{{ColorEIDTransform}}{{"); output.body.Add(icons[form]); output.body.Add("}} "); output.body.Add(names[form]);
        if (state.player.forms_known) {
            output.body.Add(" ("); output.body.Number(state.player.forms[form] > 3 ? 3 : state.player.forms[form]); output.body.Add("/3)");
        }
        output.body.Add("{{CR}}");
    }
}

inline void AppendPillModifiers(const ContextObject& object, const ContextState& state, ContextDescription& output) {
    if (!object.identified || object.id == 9999) return;
    const auto* pocket = PocketMetadata(object.category, object.id);
    const bool horse = object.category == Category::HorsePill;
    if (state.player.owned[654] && pocket) {
        const auto* klass = pocket->classification;
        const bool negative = std::strchr(klass, '-') != nullptr;
        const bool damage = (negative && std::strchr(klass, '3')) || object.id == 48;
        const bool heart = (std::strchr(klass, '1') || std::strchr(klass, '2')) && (negative || object.id == 43);
        if (damage) output.body.Line(horse ? "{{Collectible654}} {{Damage}} +1.2 Damage" : "{{Collectible654}} {{Damage}} +0.6 Damage");
        else if (heart) output.body.Line("{{Collectible654}} Spawns 1 {{BlackHeart}} Black Heart");
    }
    if (object.id == 50) {
        const bool positive = state.player.owned[75] || state.player.owned[46] || state.player.owned[303];
        if (state.player.owned[654]) {
            if (!positive) output.body.Line("{{Collectible654}} No random stat increase");
            output.body.Line(horse ? "{{Collectible654}} {{Damage}} +1.2 Damage" : "{{Collectible654}} {{Damage}} +0.6 Damage");
        } else if (positive) output.body.Line("No stat decrease");
    }
    if (state.player.owned[348] && pocket && pocket->mimic >= 0) {
        output.body.Line("{{Collectible348}} Placebo: "); output.body.Number(pocket->mimic); output.body.Add(" {{Battery}} charges");
    }
}

inline void ApplyContextConditions(const ContextObject& object, const ContextState& state, ContextDescription& output) {
    const auto category = object.category == Category::HorsePill ? 70u : static_cast<unsigned>(object.category);
    for (const auto& condition : context_data::conditions) {
        if (condition.category != category || condition.id != object.id) continue;
        if (condition.greed ? !state.greed : state.player.type < 0 || state.player.type >= 41 ||
            !(condition.players & (std::uint64_t{1} << state.player.type))) continue;
        if (condition.operation == 0) output.body.Line(condition.new_text);
        else if (condition.operation == 1) { output.body.Clear(); output.body.Add(condition.new_text); }
        else output.body.ReplaceFirst(condition.old_text, condition.new_text);
    }
}

inline bool BuildContextDescription(const ContextObject& object, const ContextState& state,
                                    const Catalogue& catalogue, ContextDescription& output) {
    output.Clear();
    if (object.kind == ContextKind::Sacrifice) {
        const auto next = NextSacrifice(object.id);
        const auto* row = ById(context_data::sacrifice, next);
        if (!row) return false;
        output.name.Add("Next sacrifice ("); output.name.Number(static_cast<int>(next)); output.name.Add("/12)");
        const char* end = next <= 2 && state.stage > 1 ? std::strchr(row->markup, '#') : nullptr;
        output.body.Add(row->markup, end ? static_cast<std::size_t>(end - row->markup) : std::strlen(row->markup));
        output.static_icon = "SacrificeRoom";
    } else if (object.kind == ContextKind::Dice) {
        const auto* row = ById(context_data::dice, object.id);
        if (!row) return false;
        output.name.Add("Dice room: "); output.name.Number(static_cast<int>(object.id));
        output.body.Add(row->markup); output.static_icon = "DiceRoom";
    } else if (object.kind == ContextKind::Character) {
        const auto* row = ById(context_data::character, object.id);
        if (!row) return false;
        output.name.Add(row->name); output.body.Add(row->markup); output.icon_variant = 1; output.icon_id = object.id - 1;
    } else if (object.kind == ContextKind::Portal) {
        static constexpr unsigned cards[] = {18, 5, 19};
        if (object.id >= 3) return false;
        Description card;
        if (!catalogue.Find(Category::Card, cards[object.id], card)) return false;
        output.name.Add("Card Reading");
        const auto* end = std::strchr(card.markup, '#');
        output.body.Add(card.markup, end ? static_cast<std::size_t>(end - card.markup) : std::strlen(card.markup));
        output.icon_variant = 100; output.icon_id = 660;
    } else if (object.kind == ContextKind::Sanguine) {
        Description description;
        if (!catalogue.Find(Category::Collectible, 692, description)) return false;
        output.name.Add(description.name); output.icon_variant = 100; output.icon_id = 692;
        const char* start = description.markup;
        while (*start) {
            const char* end = start;
            bool percentage = false;
            while (*end && *end != '#' && *end != ';') { if (*end == '%') percentage = true; ++end; }
            if (percentage) { if (output.body.size) output.body.Add("#"); output.body.Add(start, static_cast<std::size_t>(end - start)); }
            start = *end ? end + 1 : end;
        }
    } else {
        if ((object.category == Category::Pill || object.category == Category::HorsePill) && !object.identified) {
            output.name.Add("Unidentified pill"); output.icon_variant = 70; output.icon_id = object.subtype & 0x7ff; output.valid = true; return true;
        }
        Description description;
        if (!catalogue.Find(object.category, object.id, description)) return false;
        output.name.Add(description.name);
        output.icon_variant = static_cast<int>(object.category); output.icon_id = object.id;
        if (object.category == Category::Pill || object.category == Category::HorsePill) {
            output.icon_variant = 70; output.icon_id = object.subtype & 0x7ff;
        }
        const char* text = description.markup;
        if (object.category == Category::Trinket) {
            const context_data::Text* variant = object.golden ?
                (state.player.owned[439] ? ById(context_data::goldenbox, object.id) : ById(context_data::golden, object.id)) :
                state.player.owned[439] ? ById(context_data::momsbox, object.id) : nullptr;
            if (variant) text = variant->markup;
        } else if (object.category == Category::Card && state.player.owned[451]) {
            const auto* variant = ById(context_data::tarot, object.id); if (variant) text = variant->markup;
        } else if (object.category == Category::Collectible && state.player.owned[356]) {
            const auto* variant = ById(context_data::carbattery, object.id); if (variant) text = variant->markup;
        }
        if (object.kind == ContextKind::Crafting) output.body.Add("Ready to craft#");
        if (object.category == Category::Collectible && object.id == 619 && state.player.type >= 0) {
            // The nearest player's actual character effect replaces the generic text.
            const auto* row = ById(context_data::birthright, static_cast<unsigned>(state.player.type + 1));
            if (row) { output.body.Add(row->name); output.body.Add("#"); text = row->markup; }
        }
        output.body.Add(text);
        ApplyContextConditions(object, state, output);
        if (object.category == Category::Collectible) {
            AppendMetadata(object.id, state, output);
            AppendSpindown(object.id, state, output);
            if (state.player.owned[664]) {
                const auto* row = ById(context_data::bingeEaterBuffs, object.id);
                if (row) { output.body.Line("{{Collectible664}} "); output.body.Add(row->markup); }
            }
        } else if (object.category == Category::Card) {
            const auto* pocket = PocketMetadata(object.category, object.id);
            const unsigned mimic = pocket && pocket->rune ? 263 : 286;
            if (pocket && pocket->mimic >= 0 && state.player.owned[mimic]) {
                output.body.Line(""); output.body.Icon("Collectible", mimic); output.body.Add(" ");
                output.body.Add(mimic == 263 ? "Clear Rune: " : "Blank Card: "); output.body.Number(pocket->mimic); output.body.Add(" {{Battery}} charges");
                if (state.player.owned[451] && mimic == 286) {
                    const auto* row = ById(context_data::tarotClothBlankCardBuffs, object.id);
                    if (row) { output.body.Line("{{Collectible286}} "); output.body.Add(row->markup); }
                }
            }
        } else if (object.category == Category::Pill || object.category == Category::HorsePill) AppendPillModifiers(object, state, output);
    }
    output.valid = output.name.size > 0 && !output.name.truncated && !output.body.truncated;
    return output.valid;
}

// Conservative bounded grid search for visible cards/pills. The native adapter
// supplies collision/active-spike facts; this function never queries/mutates RNG.
inline bool BuildReachability(const bool* walkable, unsigned width, unsigned height,
                              unsigned start, bool* reachable) {
    if (!walkable || !reachable || !width || !height || width > 28 || height > 28 || width * height > 448 || start >= width * height) return false;
    const unsigned count = width * height;
    for (unsigned i = 0; i < count; ++i) reachable[i] = false;
    unsigned queue[448], head = 0, tail = 0;
    queue[tail++] = start; reachable[start] = true;
    while (head < tail) {
        const auto index = queue[head++];
        const unsigned neighbors[] = {index ? index - 1 : count, index + 1, index >= width ? index - width : count, index + width};
        for (unsigned edge = 0; edge < 4; ++edge) {
            const auto next = neighbors[edge];
            if (next >= count || reachable[next] || !walkable[next]) continue;
            if ((edge == 0 && index % width == 0) || (edge == 1 && index % width == width - 1)) continue;
            reachable[next] = true; queue[tail++] = next;
        }
    }
    return true;
}
} // namespace isaac_eid

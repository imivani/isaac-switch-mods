#include "eid_features.hpp"
#include "eid_ui.hpp"
#include "eid_special.hpp"
#include "native_sprite.hpp"
#include "generated/build_profile.hpp"
#include "../../../eid/context.hpp"
#include "../../../eid/native_adapter_calls.hpp"
#include <cstring>

namespace isaac {
namespace {
using namespace isaac_eid;
using namespace feature_memory;
constexpr unsigned MaxPlayers = 8, MaxGrid = 448, MaxReminder = 800;
constexpr float RadiusSquared = 200.f * 200.f;
struct Player { std::uintptr_t address; float x, y; bool map, left, right; };
struct Candidate { ContextObject object; std::uintptr_t entity{}; unsigned player{}; float distance = RadiusSquared; };
struct GridSnapshot {
    bool valid{}, reachable[MaxPlayers][MaxGrid]{};
    unsigned width{}, height{}, count{};
    std::uintptr_t cells[MaxGrid]{};
};
ContextState s_State;
ContextDescription s_Output;
GridSnapshot s_Grid;
ContextObject s_Reminders[MaxReminder];
std::uintptr_t s_Module{}, s_Selection{}, s_ReminderPlayer{}, s_MapPlayer{}, s_Room{};
int s_LastRefresh = -1, s_LastFrame = -1, s_MapReleased = -1000;
unsigned s_ReminderIndex{}, s_ReminderCount{};
bool s_ShowNearby = true, s_ShowMap = true;
unsigned s_MapMask{};
bool s_Map{}, s_Left{}, s_Right{};

bool Pointer(std::uintptr_t value) { return value > 0x10000 && !(value & 7); }
bool Live(std::uintptr_t entity) {
    return Pointer(entity) && Readable(entity, 0x360) && Read<unsigned char>(entity + 0x1c2) &&
           Read<unsigned char>(entity + 0x1c1) && !Read<unsigned char>(entity + 0x1c3);
}
bool AnimationPlaying(const FeatureFrame& frame, std::uintptr_t entity, const char* name) {
    const auto animation = Read<std::uintptr_t>(entity + 0x50 + 0x38);
    char checked[128];
    if (!CopyNativeString(animation, checked, sizeof(checked))) return false;
    return eid_native::Function<bool (*)(const void*, const char*)>(frame.module, 0xa454)(
        reinterpret_cast<const void*>(entity + 0x50), name);
}
bool KnownGraphic(std::uintptr_t entity, unsigned layer) {
    const auto states = Read<std::uintptr_t>(entity + 0xf8);
    const auto count = Read<unsigned>(entity + 0x100);
    if (!Pointer(states) || count <= layer || count > 64) return false;
    char path[512];
    if (!CopyNativeString(states + layer * 0xb0 + 0x10, path, sizeof(path)) || !path[0]) return false;
    for (auto* cursor = path; *cursor; ++cursor) if (*cursor >= 'A' && *cursor <= 'Z') *cursor += 'a' - 'A';
    return !std::strstr(path, "questionmark.");
}
bool Has(const FeatureFrame& frame, std::uintptr_t player, unsigned id) {
    if (!id || id > 732) return false;
    return eid_native::Function<bool (*)(const void*, unsigned, bool)>(frame.module, 0x27d3f4)(
        reinterpret_cast<const void*>(player), id, false);
}
unsigned ReadPlayers(const FeatureFrame& frame, std::uintptr_t manager, Player* players) {
    const auto begin = Read<std::uintptr_t>(frame.game + profile::PlayersBegin);
    const auto end = Read<std::uintptr_t>(frame.game + profile::PlayersEnd);
    if (!Pointer(begin) || end <= begin || end - begin > MaxPlayers * 8 || (end - begin) % 8 || !Readable(begin, end - begin)) return 0;
    unsigned count = 0;
    for (auto pointer = begin; pointer < end; pointer += 8) {
        const auto player = Read<std::uintptr_t>(pointer);
        if (!Live(player) || !Readable(player, 0x2850) || Read<unsigned>(player + 0x38) != 1 || Read<unsigned>(player + 0x3c) != 0) continue;
        auto& row = players[count];
        row.address = player; row.x = Read<float>(player + 0x310); row.y = Read<float>(player + 0x314);
        if (!ValidCoordinate(row.x) || !ValidCoordinate(row.y)) continue;
        const auto controller = Read<unsigned>(player + 0x19ec);
        row.map = eid_native::ControllerDevice(controller) && eid_native::Function<bool (*)(const void*, unsigned, unsigned, const void*)>(frame.module, 0x3f9b6c)(
            reinterpret_cast<const void*>(manager), 13, controller, reinterpret_cast<const void*>(player));
        if (row.map) {
            const auto pressed = eid_native::Function<bool (*)(const void*, unsigned, unsigned, const void*)>(frame.module, 0x3f9b6c);
            row.left = pressed(reinterpret_cast<const void*>(manager), 4, controller, reinterpret_cast<const void*>(player));
            row.right = pressed(reinterpret_cast<const void*>(manager), 5, controller, reinterpret_cast<const void*>(player));
        }
        ++count;
    }
    return count;
}
unsigned NearestPlayer(const Player* players, unsigned count, float x, float y, float& distance) {
    unsigned selected = 0;
    distance = 1.e20f;
    for (unsigned i = 0; i < count; ++i) {
        const float dx = x - players[i].x, dy = y - players[i].y;
        const float candidate = dx * dx + dy * dy;
        if (candidate < distance) { distance = candidate; selected = i; }
    }
    return selected;
}
int GridIndex(const FeatureFrame& frame, float x, float y) {
    const NativeVec2 point{x, y};
    return eid_native::Function<int (*)(const void*, const NativeVec2*)>(frame.module, 0x459e3c)(
        reinterpret_cast<const void*>(frame.room), &point);
}
void ReadGrid(const FeatureFrame& frame, const Player* players, unsigned player_count) {
    s_Grid.valid = false;
    const auto width = Read<unsigned>(frame.room + 0x14), height = Read<unsigned>(frame.room + 0x18);
    if (!width || !height || width > 28 || height > 28 || width * height > MaxGrid || !Readable(frame.room + 0x30, width * height * 8)) return;
    s_Grid.width = width; s_Grid.height = height; s_Grid.count = width * height;
    std::memcpy(s_Grid.cells, reinterpret_cast<const void*>(frame.room + 0x30), s_Grid.count * 8);
    bool walkable[MaxGrid]{};
    for (unsigned i = 0; i < s_Grid.count; ++i) {
        if (s_Grid.cells[i] && !Readable(s_Grid.cells[i], 0x50)) return;
        const int collision = eid_native::Function<int (*)(const void*, int)>(frame.module, 0x452654)(
            reinterpret_cast<const void*>(frame.room), static_cast<int>(i));
        walkable[i] = collision == 0;
        if (s_Grid.cells[i]) {
            const auto type = Read<unsigned>(s_Grid.cells[i] + 8);
            // Active spikes are unsafe for walking-path identity discovery.
            if ((type == 8 || type == 9) && Read<int>(frame.room + 0xe98 + i * 4) == 999) walkable[i] = false;
        }
    }
    for (unsigned i = 0; i < player_count; ++i) {
        const bool flying = eid_native::Function<bool (*)(const void*)>(frame.module, 0x5c744)(reinterpret_cast<const void*>(players[i].address));
        if (flying) { for (unsigned cell = 0; cell < s_Grid.count; ++cell) s_Grid.reachable[i][cell] = true; }
        else {
            const int start = GridIndex(frame, players[i].x, players[i].y);
            if (start < 0 || !BuildReachability(walkable, width, height, static_cast<unsigned>(start), s_Grid.reachable[i])) return;
        }
    }
    s_Grid.valid = true;
}
Knowledge Reachable(const FeatureFrame& frame, unsigned player, float x, float y) {
    if (!s_Grid.valid || player >= MaxPlayers) return Knowledge::Unknown;
    const int index = GridIndex(frame, x, y);
    return index >= 0 && static_cast<unsigned>(index) < s_Grid.count && s_Grid.reachable[player][index] ? Knowledge::Yes : Knowledge::No;
}
bool Pill(const FeatureFrame& frame, std::uintptr_t player, unsigned raw_color, ContextObject& object) {
    const unsigned color = raw_color & 0x7ff;
    if (!color || color > 14 || (raw_color & ~0xfffu)) return false;
    object.category = (raw_color & 0x800) ? Category::HorsePill : Category::Pill;
    object.subtype = raw_color;
    if (color == 14) { object.id = 9999; object.identified = true; return true; }
    unsigned char identified{};
    if (!TryRead(frame.game + 0x24d28 + color, identified)) return false;
    object.identified = identified != 0;
    if (!object.identified) { object.id = 0; return true; }
    const int effect = eid_native::Function<int (*)(const void*, unsigned, const void*)>(frame.module, 0x3c857c)(
        reinterpret_cast<const void*>(frame.game + 0x242c0), raw_color, reinterpret_cast<const void*>(player));
    object.id = PillLookupId(effect);
    return object.id != 0;
}
void ReadContext(const FeatureFrame& frame, const Player* players, unsigned count, unsigned selected) {
    s_State = ContextState{};
    const auto player = players[selected].address;
    s_State.player.type = Read<int>(player + 0x1738);
    const auto inventory = Read<std::uintptr_t>(player + 0x1ab8);
    if (Readable(inventory, 733 * 4)) {
        // One validated bounded copy avoids hundreds of kernel memory queries.
        int counts[733];
        std::memcpy(counts, reinterpret_cast<const void*>(inventory), sizeof(counts));
        for (unsigned id = 1; id <= 732; ++id) s_State.player.owned[id] = counts[id] > 0;
    }
    // The native accessor also accounts for character-held equivalents.
    constexpr unsigned modifiers[] = {46,75,263,286,303,348,356,439,451,619,654,664,710,711,723};
    for (unsigned id : modifiers) s_State.player.owned[id] = Has(frame, player, id);
    s_State.player.forms_known = Readable(player + 0x1c54, sizeof(s_State.player.forms));
    if (s_State.player.forms_known) {
        std::memcpy(s_State.player.forms, reinterpret_cast<const void*>(player + 0x1c54), sizeof(s_State.player.forms));
        for (int value : s_State.player.forms) if (value < 0 || value > 10000) s_State.player.forms_known = false;
    }
    s_State.player.red_hearts = Read<int>(player + 0x16bc);
    s_State.player.soul_hearts = Read<int>(player + 0x16c4);
    s_State.player.bone_hearts = Read<int>(player + 0x2450);
    s_State.stage = Read<int>(frame.game); s_State.stage_type = Read<int>(frame.game + 4);
    const auto difficulty = Read<int>(frame.game + 0x2f0028); s_State.greed = difficulty == 2 || difficulty == 3;
    for (unsigned i = 0; i < count; ++i) s_State.player_types[s_State.player_count++] = Read<unsigned>(players[i].address + 0x1738);
}
void SpindownAvailability(const FeatureFrame& frame, std::uintptr_t manager, unsigned selected) {
    if (!s_State.player.owned[723] || selected > 732) return;
    const unsigned first = selected > 66 ? selected - 66 : 1;
    for (unsigned id = first; id < selected; ++id) {
        const auto item = eid_native::Function<std::uintptr_t (*)(const void*, int)>(frame.module, 0x3c0c50)(
            reinterpret_cast<const void*>(manager + 0x36538), static_cast<int>(id));
        if (!Readable(item, 0x90)) continue;
        const bool available = eid_native::Function<bool (*)(const void*, unsigned)>(frame.module, 0x3c101c)(reinterpret_cast<const void*>(item), 0xffffffff);
        s_State.spindown_available[id] = available ? Knowledge::Yes : Knowledge::No;
    }
}
Candidate Select(const FeatureFrame& frame, const std::uintptr_t* entities, unsigned count,
                 const Player* players, unsigned player_count, bool blind, unsigned challenge) {
    Candidate best;
    for (unsigned index = 0; index < count; ++index) {
        const auto entity = entities[index];
        if (!Live(entity)) continue;
        const auto type = Read<unsigned>(entity + 0x38), variant = Read<unsigned>(entity + 0x3c), subtype = Read<unsigned>(entity + 0x40);
        if (type != 5 && type != 6 && type != 1000) continue;
        const float x = Read<float>(entity + 0x310), y = Read<float>(entity + 0x314);
        if (!ValidCoordinate(x) || !ValidCoordinate(y)) continue;
        float distance{};
        const auto player = NearestPlayer(players, player_count, x, y, distance);
        if (distance >= best.distance) continue;
        ContextObject object;
        bool accepted = false;
        if (type == 5 && (variant == 100 || variant == 350 || variant == 300 || variant == 70)) {
            if (!Readable(entity, 0x568)) continue;
            object.category = static_cast<Category>(variant); object.id = subtype; object.subtype = subtype;
            PickupSnapshot pickup;
            pickup.category = object.category; pickup.lookup_id = subtype; pickup.active_and_visible = true;
            pickup.x = x; pickup.y = y;
            RunSnapshot run;
            run.in_play = true; run.curse_of_blind = blind ? Knowledge::Yes : Knowledge::No;
            if (variant == 100) pickup.blind = Read<unsigned char>(entity + 0x562) || !KnownGraphic(entity, 1) ? Knowledge::Yes : Knowledge::No;
            else if (variant == 350) { object.id = TrinketLookupId(subtype); object.golden = (subtype & 0x8000) != 0; pickup.lookup_id = object.id; }
            else if (variant == 300) {
                pickup.options_choice = Read<int>(entity + 0x55c) > 0 ? Knowledge::Yes : Knowledge::No;
                pickup.shop_item = Read<int>(entity + 0x564) != 0 ? Knowledge::Yes : Knowledge::No;
                pickup.reachable = Reachable(frame, player, x, y);
                if (challenge == 43 && subtype > 32768 && subtype <= 32768 + 732) {
                    object.category = Category::Collectible; object.id = subtype - 32768;
                    // The visible card face is its identity in Cantripped.
                    accepted = pickup.options_choice == Knowledge::No && pickup.shop_item == Knowledge::No && pickup.reachable == Knowledge::Yes;
                }
            } else {
                if (!Pill(frame, players[player].address, subtype, object)) continue;
                pickup.category = object.category; pickup.lookup_id = object.id;
                pickup.pill_identified = object.identified ? Knowledge::Yes : Knowledge::No;
                pickup.golden_pill = object.id == 9999; pickup.reachable = Reachable(frame, player, x, y);
            }
            if (!accepted) accepted = DecidePresentation(pickup, run) != Presentation::Hidden;
            if (variant == 100 && !subtype) {
                bool flip_owned = false;
                for (unsigned owner = 0; owner < player_count; ++owner) if (Has(frame, players[owner].address, 711)) { flip_owned = true; break; }
                accepted = accepted && flip_owned && ReadEidFlipItem(frame, entity) != 0;
            }
        } else if (type == 1000 && variant == 76 && subtype < 6) {
            object.kind = ContextKind::Dice; object.id = subtype + 1; accepted = true;
        } else if (type == 1000 && variant == 161 && subtype < 3) {
            object.kind = ContextKind::Portal; object.id = subtype; accepted = true;
        } else if (type == 5 && variant == 110 && AnimationPlaying(frame, entity, "Idle")) {
            object.id = 550; accepted = true;
        } else if (type == 6 && variant == 16 && !blind && Readable(entity, 0x574) &&
                   !AnimationPlaying(frame, entity, "Broken") && !AnimationPlaying(frame, entity, "OutOfPrizes") && KnownGraphic(entity, 2)) {
            object.id = Read<unsigned>(entity + 0x570); accepted = object.id > 0 && object.id <= 732;
        }
        if (accepted) best = {object, entity, player, distance};
    }
    const auto room_type = Read<unsigned>(frame.room + 0x10);
    if (s_Grid.valid) for (unsigned index = 0; index < s_Grid.count; ++index) {
        const auto grid = s_Grid.cells[index];
        if (!grid || Read<unsigned>(grid + 8) != 8) continue;
        const bool sanguine = Read<unsigned>(grid + 0xc) >= 100;
        if (!sanguine && room_type != 13) continue;
        const auto position = eid_native::Function<NativeVec2 (*)(const void*, int)>(frame.module, 0x454d38)(
            reinterpret_cast<const void*>(frame.room), static_cast<int>(index));
        float distance{};
        const auto player = NearestPlayer(players, player_count, position.x, position.y, distance);
        if (distance >= best.distance) continue;
        ContextObject object;
        object.kind = sanguine ? ContextKind::Sanguine : ContextKind::Sacrifice;
        object.id = Read<unsigned>(grid + 0x1c);
        best = {object, grid, player, distance};
    }
    return best;
}
void AddReminder(ContextObject object) {
    if (s_ReminderCount < MaxReminder) s_Reminders[s_ReminderCount++] = object;
}
void BuildReminders(const FeatureFrame& frame, std::uintptr_t player) {
    s_ReminderCount = 0;
    // This exact output is already calculated by the game when the bag changes.
    // Reading it cannot consume recipe randomness or alter the inventory.
    if (s_State.player.owned[710]) {
        bool full = true;
        for (unsigned i = 0; i < 8; ++i) if (!Read<unsigned char>(player + 0x2690 + i)) full = false;
        const auto result = Read<unsigned>(player + 0x2698);
        if (full && result && result <= 732) AddReminder({ContextKind::Crafting, Category::Collectible, result});
    }
    if (s_State.player.type >= 0 && s_State.player.type < 41) AddReminder({ContextKind::Character, Category::Collectible, static_cast<unsigned>(s_State.player.type + 1)});
    for (unsigned slot = 0; slot < 4; ++slot) {
        const auto id = Read<unsigned>(player + 0x1964 + slot * 0x1c);
        const auto* item = ItemMetadata(id);
        if (item && item->type == 3) AddReminder({ContextKind::Pickup, Category::Collectible, id});
    }
    for (unsigned slot = 0; slot < 4; ++slot) {
        const auto id = Read<unsigned>(player + 0x1c10 + slot * 8), type = Read<unsigned>(player + 0x1c14 + slot * 8);
        if (!id) continue;
        ContextObject object;
        if (type == 1 && id <= 97) { object.category = Category::Card; object.id = id; }
        else if (type == 0) { if (!Pill(frame, player, id, object)) continue; }
        else continue;
        AddReminder(object);
    }
    for (unsigned slot = 0; slot < 2; ++slot) {
        const auto subtype = Read<unsigned>(player + 0x1ab0 + slot * 4), id = TrinketLookupId(subtype);
        if (id && id <= 189) AddReminder({ContextKind::Pickup, Category::Trinket, id, subtype, (subtype & 0x8000) != 0});
    }
    for (unsigned id = 1; id <= 732; ++id) if (s_State.player.owned[id]) {
        const auto* metadata = ItemMetadata(id);
        if (metadata && metadata->type != 3 && !metadata->hidden) AddReminder({ContextKind::Pickup, Category::Collectible, id});
    }
    if (s_ReminderIndex >= s_ReminderCount) s_ReminderIndex = 0;
}
} // namespace

void RenderEidFeatures(const FeatureFrame& frame, const std::uintptr_t* entities, unsigned count) {
    if (!frame.module || !frame.game || !frame.room || (!entities && count) || count > 4096 || (s_Module && s_Module != frame.module)) return;
    std::uintptr_t manager{};
    int now{}, challenge{};
    unsigned alternate{};
    if (!TryRead(frame.module + profile::ManagerGlobal, manager) || !Readable(manager + 0x36538, 0x28) ||
        !TryRead(frame.game + 0x24f99c, now) || now < 0 || !TryRead(frame.game + profile::ChallengeId, challenge) ||
        !TryRead(frame.game + profile::AlternateChallenge, alternate)) return;
    // April's Fool replaces item effects with random ones; ordinary identities
    // would be misleading. Unknown custom challenge variants remain suppressed.
    if (challenge == 32 || alternate != 0) { s_Output.valid = false; return; }
    Player players[MaxPlayers]{};
    const auto player_count = ReadPlayers(frame, manager, players);
    if (!player_count) { s_Output.valid = false; return; }
    unsigned map_player = 0, map_mask = 0; bool map = false;
    for (unsigned i = 0; i < player_count; ++i) if (players[i].map) {
        map_mask |= 1u << i;
        if (!map) { map = true; map_player = i; }
    }
    const bool left = map && players[map_player].left, right = map && players[map_player].right;
    const auto map_owner = map ? players[map_player].address : 0;
    const bool owner_changed = map && s_Map && map_owner != s_MapPlayer;
    const bool reset = now < s_LastFrame || s_Room != frame.room;
    const bool edge = map_mask != s_MapMask || owner_changed || left != s_Left || right != s_Right;
    if (reset) { s_LastRefresh = -1; s_ReminderIndex = 0; }
    if (!map && s_Map) s_MapReleased = now;
    if (map && (!s_Map || owner_changed)) {
        if (!reset && !owner_changed && s_ReminderPlayer == map_owner && now - s_MapReleased < 180) ++s_ReminderIndex;
        else s_ReminderIndex = 0;
    }
    const bool next_page = right && !left && (!s_Right || owner_changed);
    const bool previous_page = left && !right && (!s_Left || owner_changed);
    s_Module = frame.module; s_Room = frame.room; s_LastFrame = now; s_Map = map;
    s_MapPlayer = map_owner;
    s_MapMask = map_mask; s_Left = left; s_Right = right;
    if (s_LastRefresh < 0 || now - s_LastRefresh >= 6 || edge) {
        s_LastRefresh = now;
        s_Output.Clear(); s_Selection = 0;
        ReadGrid(frame, players, player_count);
        const auto curses = eid_native::Function<unsigned (*)(const void*)>(frame.module, profile::GetCurses)(reinterpret_cast<const void*>(frame.game));
        const auto best = Select(frame, entities, count, players, player_count, (curses & 0x40) != 0, static_cast<unsigned>(challenge));
        bool special_map = false;
        if (best.entity && s_ShowNearby) {
            ReadContext(frame, players, player_count, best.player);
            if (best.object.category == Category::Collectible) SpindownAvailability(frame, manager, best.object.id);
            BuildContextDescription(best.object, s_State, EnglishRepentance(), s_Output);
            if (best.object.kind == ContextKind::Pickup && best.object.category == Category::Collectible)
                special_map = AugmentEidSpecial(frame, best.entity, players[best.player].address, best.object, s_State, s_Output);
            s_Selection = best.entity;
        }
        if (map && !special_map && s_ShowMap) {
            ReadContext(frame, players, player_count, map_player);
            s_ReminderPlayer = players[map_player].address;
            BuildReminders(frame, s_ReminderPlayer);
            if (s_ReminderCount) {
                // Use this player's current list when wrapping, including a
                // left press on the same frame that Map first opens.
                if (next_page) s_ReminderIndex = (s_ReminderIndex + 1) % s_ReminderCount;
                if (previous_page) s_ReminderIndex = s_ReminderIndex ? s_ReminderIndex - 1 : s_ReminderCount - 1;
                const auto& object = s_Reminders[s_ReminderIndex];
                BuildContextDescription(object, s_State, EnglishRepentance(), s_Output);
                s_Output.body.Line("{{ColorSilver}}Held: "); s_Output.body.Number(static_cast<int>(s_ReminderIndex + 1));
                s_Output.body.Add("/"); s_Output.body.Number(static_cast<int>(s_ReminderCount));
                s_Output.body.Add("#Hold Map + shoot left/right to browse{{CR}}");
                s_Selection = s_ReminderPlayer ^ (static_cast<std::uintptr_t>(s_ReminderIndex + 1) << 32);
            }
        }
    }
    RenderEidContext(frame, s_Selection, s_Output);
}
void SetEidOptions(bool nearby, bool map) { s_ShowNearby = nearby; s_ShowMap = map; }
void ResetEidFeatures() {
    ResetEidSpecial(); ResetEidUi();
    s_Module = s_Selection = s_ReminderPlayer = s_MapPlayer = s_Room = 0;
    s_LastRefresh = s_LastFrame = -1; s_MapReleased = -1000;
    s_ReminderIndex = s_ReminderCount = s_MapMask = 0; s_Map = s_Left = s_Right = false;
    s_Output.Clear(); s_Grid.valid = false;
}
} // namespace isaac

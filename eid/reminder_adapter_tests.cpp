#include "../native/source/program/eid_features.hpp"
#include "../native/source/program/eid_ui.hpp"
#include "../native/source/program/eid_special.hpp"
#include "../native/source/program/native_sprite.hpp"
#include "../native/source/program/generated/build_profile.hpp"
#include "native_adapter_calls.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {
unsigned checks = 0, frames = 0;
void Check(bool value, const char* message) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
struct Block {
    std::vector<std::uint64_t> storage;
    explicit Block(std::size_t size) : storage((size + 7) / 8) {}
    std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(storage.data()); }
    std::size_t Size() const { return storage.size() * 8; }
    template<class T> void Put(std::size_t offset, T value) {
        Check(offset + sizeof(value) <= Size(), "fixture write is bounded");
        std::memcpy(reinterpret_cast<void*>(Address() + offset), &value, sizeof(value));
    }
    void Clear() { std::memset(storage.data(), 0, Size()); }
};
std::vector<const Block*> memory;
struct InputCall { unsigned action, device; std::uintptr_t player; };
struct Harness {
    Block module{0xac0000}, game{0x300000}, manager{0x40000}, room{0x2000};
    Block players{8 * 8}, player0{0x3000}, player1{0x3000}, inventory0{733 * 4}, inventory1{733 * 4};
    Block pickup{0x700};
    isaac::FeatureFrame frame{};
    std::array<std::array<bool, 14>, 34> input{};
    std::vector<InputCall> calls;
    isaac_eid::ContextDescription rendered{};
    std::uintptr_t selection{};
    unsigned curses{}, rendered_count{}, player_count = 1;
    int tick = 0;
    Harness() {
        memory = {&module, &game, &manager, &room, &players, &player0, &player1, &inventory0, &inventory1, &pickup};
        frame = {module.Address(), game.Address(), room.Address(), 640, 360, {nullptr, nullptr, nullptr}};
        Reset();
    }
    void Player(Block& player, Block& inventory, unsigned device, unsigned type = 0) {
        player.Clear(); inventory.Clear();
        player.Put<unsigned>(0x38, 1); player.Put<unsigned char>(0x1c1, 1); player.Put<unsigned char>(0x1c2, 1);
        player.Put<float>(0x310, 100); player.Put<float>(0x314, 100);
        player.Put<unsigned>(0x19ec, device); player.Put<unsigned>(0x1738, type);
        player.Put<std::uintptr_t>(0x1ab8, inventory.Address());
    }
    void Reset(unsigned device = 33) {
        isaac::ResetEidFeatures();
        game.Clear(); room.Clear(); module.Clear(); manager.Clear(); pickup.Clear();
        input = {}; calls.clear(); rendered.Clear(); rendered_count = 0; selection = 0; curses = 0; tick = 0;
        Player(player0, inventory0, device); Player(player1, inventory1, 2, 1);
        module.Put<std::uintptr_t>(isaac::profile::ManagerGlobal, manager.Address());
        players.Put<std::uintptr_t>(0, player0.Address()); players.Put<std::uintptr_t>(8, player1.Address());
        game.Put<std::uintptr_t>(isaac::profile::PlayersBegin, players.Address());
        SetPlayers(1);
        game.Put<int>(0, 1);
    }
    void SetPlayers(unsigned count) {
        player_count = count;
        game.Put<std::uintptr_t>(isaac::profile::PlayersEnd, players.Address() + count * 8);
    }
    void Step(const std::uintptr_t* entities = nullptr, unsigned count = 0) {
        ++tick; ++frames;
        game.Put<int>(0x24f99c, tick);
        rendered.Clear(); rendered_count = 0; calls.clear(); selection = 0;
        isaac::RenderEidFeatures(frame, entities, count);
    }
    bool HasText(const char* text) const { return std::strstr(rendered.body.data, text) != nullptr; }
    void Owned(unsigned id, int count = 1) { inventory0.Put<int>(id * 4, count); }
    void Reminder(unsigned index, unsigned count) {
        const auto expected = std::string("Held: ") + std::to_string(index) + "/" + std::to_string(count);
        Check(rendered.valid, "held Map produces a real rendered description");
        Check(HasText(expected.c_str()), "rendered reminder has the expected page index and count");
    }
};
Harness* current;

bool ActionPressed(const void* manager, unsigned action, unsigned device, const void* entity) {
    Check(manager == reinterpret_cast<const void*>(current->manager.Address()), "action ABI receives game manager");
    Check(action == 13 || action == 4 || action == 5, "reminder never polls movement/use/drop/pause actions");
    const auto player = reinterpret_cast<std::uintptr_t>(entity);
    Check(player == current->player0.Address() || player == current->player1.Address(), "action ABI receives selected player");
    unsigned actual_device = 0;
    std::memcpy(&actual_device, reinterpret_cast<const void*>(player + 0x19ec), sizeof(actual_device));
    Check(device == actual_device, "action dispatch preserves the exact KAGE device ID");
    Check(device < current->input.size(), "fake device is in the supported input table");
    current->calls.push_back({action, device, player});
    return current->input[device][action];
}
bool HasCollectible(const void* player, unsigned id, bool ignore_modifiers) {
    Check(!ignore_modifiers && id >= 1 && id <= 732, "HasCollectible ABI and bounded item ID");
    const auto address = reinterpret_cast<std::uintptr_t>(player);
    const auto& inventory = address == current->player0.Address() ? current->inventory0 : current->inventory1;
    int count = 0;
    std::memcpy(&count, reinterpret_cast<const void*>(inventory.Address() + id * 4), sizeof(count));
    return count > 0;
}
unsigned Curses(const void* game) {
    Check(game == reinterpret_cast<const void*>(current->game.Address()), "GetCurses receives game");
    return current->curses;
}
int PillEffect(const void* pool, unsigned color, const void* player) {
    Check(pool == reinterpret_cast<const void*>(current->game.Address() + 0x242c0), "pill query receives correct item pool");
    Check(player == reinterpret_cast<const void*>(current->player0.Address()), "pill query receives reminder owner");
    Check((color & 0x7ff) == 1, "pill identity is queried only for identified fixture color");
    return 0;
}
}

extern "C" std::uint32_t svcQueryMemory(MemoryInfo* info, std::uint32_t* page, std::uint64_t address) {
    *page = 0;
    for (const auto* block : memory) if (address >= block->Address() && address < block->Address() + block->Size()) {
        *info = {block->Address(), block->Size(), Perm_R}; return 0;
    }
    *info = {}; return 1;
}

namespace isaac::eid_native {
std::uintptr_t ResolveTestFunction(std::uintptr_t module, std::uintptr_t offset) {
    Check(module == current->module.Address(), "native call uses verified mock module");
    switch (offset) {
        case 0x3f9b6c: return reinterpret_cast<std::uintptr_t>(&ActionPressed);
        case 0x27d3f4: return reinterpret_cast<std::uintptr_t>(&HasCollectible);
        case isaac::profile::GetCurses: return reinterpret_cast<std::uintptr_t>(&Curses);
        case 0x3c857c: return reinterpret_cast<std::uintptr_t>(&PillEffect);
        default: std::fprintf(stderr, "Unexpected native call %zx\n", static_cast<std::size_t>(offset)); std::exit(1);
    }
}
}
namespace isaac {
void RenderEidContext(const FeatureFrame&, std::uintptr_t selection, const isaac_eid::ContextDescription& description) {
    current->rendered = description; current->selection = selection; ++current->rendered_count;
}
void ResetEidUi() {}
void ResetEidSpecial() {}
bool CopyNativeString(std::uintptr_t, char*, std::size_t) { return false; }
unsigned ReadEidFlipItem(const FeatureFrame&, std::uintptr_t) { return 0; }
bool AugmentEidSpecial(const FeatureFrame&, std::uintptr_t, std::uintptr_t,
                       const isaac_eid::ContextObject&, const isaac_eid::ContextState&,
                       isaac_eid::ContextDescription&) { return false; }
}

int main() {
    Harness h; current = &h;
    for (unsigned device : {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u, 33u}) {
        h.Reset(device); h.input[device][13] = true; h.Step(); h.Reminder(1, 1);
        Check(h.rendered.icon_variant == 1 && h.rendered.icon_id == 0, "first reminder is Isaac character information");
        Check(h.calls.size() == 3 && h.calls.front().action == 13, "Map and both shooting directions are sampled");
        Check(h.calls.front().device == device, "every real Switch device ID reaches native input");
        h.Step(); h.Reminder(1, 1);
        h.input[device][13] = false; h.Step();
        Check(!h.rendered.valid, "Map release closes reminder immediately without nearby objects");
    }
    for (unsigned device : {0u, 9u, 32u, 34u, std::numeric_limits<unsigned>::max()}) {
        h.Reset(device); h.Step();
        Check(h.calls.empty() && !h.rendered.valid, "invalid controller IDs do not query all devices or open a reminder");
    }

    h.Reset(); h.Owned(1); h.Owned(2); h.input[33][13] = true;
    h.Step(); h.Reminder(1, 3);
    h.input[33][5] = true; h.Step(); h.Reminder(2, 3);
    Check(h.rendered.icon_id == 1 && h.rendered.icon_variant == 100, "right page shows an actually owned item");
    for (unsigned i = 0; i < 15; ++i) { h.Step(); h.Reminder(2, 3); }
    h.input[33][5] = false; h.Step(); h.input[33][5] = true; h.Step(); h.Reminder(3, 3);
    h.input[33][5] = false; h.Step(); h.input[33][5] = true; h.Step(); h.Reminder(1, 3);
    h.input[33][5] = false; h.Step(); h.input[33][4] = true; h.Step(); h.Reminder(3, 3);
    h.input[33][5] = true; h.Step(); h.Reminder(3, 3);
    Check(h.HasText("Hold Map + shoot left/right"), "reminder explains the non-movement controls");

    h.Reset(); h.Owned(1); h.input[33][13] = true; h.input[33][4] = true;
    h.Step(); h.Reminder(2, 2);
    Check(h.rendered.icon_id == 1 && h.rendered.icon_variant == 100, "opening with left wraps using current inventory, not stale count");

    h.Reset(); h.Owned(1); h.input[33][13] = true; h.Step(); h.Reminder(1, 2);
    h.input[33][13] = false; h.Step(); h.input[33][13] = true; h.Step(); h.Reminder(2, 2);
    h.input[33][13] = false; h.Step(); h.tick += 180; h.input[33][13] = true; h.Step(); h.Reminder(1, 2);

    h.Reset(); h.Owned(1); h.SetPlayers(2); h.input[33][13] = true; h.Step(); h.Reminder(1, 2);
    h.input[33][5] = true; h.Step(); h.Reminder(2, 2);
    h.input[2][13] = true; h.Step(); h.Reminder(2, 2);
    h.input[33][13] = false; h.Step(); h.Reminder(1, 1);
    Check(h.rendered.icon_variant == 1 && h.rendered.icon_id == 1, "co-op Map handover opens the second player's character page");

    h.Reset(); h.Owned(1); h.input[33][13] = true; h.input[33][5] = true; h.Step(); h.Reminder(2, 2);
    h.Owned(1, 0); h.input[33][5] = false; h.Step(); h.Reminder(1, 1);
    Check(h.rendered.icon_variant == 1, "lost items disappear and page index remains valid");

    h.Reset(); h.curses = 0x40; h.Owned(1); h.input[33][13] = true; h.input[33][5] = true;
    h.Step(); h.Reminder(2, 2);
    Check(h.rendered.icon_id == 1, "Curse of the Blind does not hide known held inventory");

    h.Reset(); h.player0.Put<unsigned>(0x1964, 105); h.player0.Put<unsigned>(0x1c10, 1);
    h.player0.Put<unsigned>(0x1c14, 1); h.player0.Put<unsigned>(0x1c18, 1);
    h.player0.Put<unsigned>(0x1c1c, 0); h.player0.Put<unsigned>(0x1ab0, 0x8001);
    h.Owned(1); h.input[33][13] = true;
    h.Step(); h.Reminder(1, 6);
    const unsigned icons[] = {100, 300, 70, 350, 100};
    for (unsigned index = 0; index < 5; ++index) {
        h.input[33][5] = true; h.Step(); h.Reminder(index + 2, 6);
        Check(h.rendered.icon_variant == static_cast<int>(icons[index]), "reminder traverses active/card/pill/trinket/passive in order");
        if (index == 2) Check(std::strstr(h.rendered.name.data, "Unidentified") != nullptr, "held unidentified pill remains unknown");
        h.input[33][5] = false; h.Step();
    }

    h.Reset(); h.player0.Put<unsigned>(0x1c10, 0x801); h.game.Put<unsigned char>(0x24d29, 1);
    h.input[33][13] = true; h.input[33][5] = true; h.Step(); h.Reminder(2, 2);
    isaac_eid::Description horse;
    Check(isaac_eid::EnglishRepentance().Find(isaac_eid::Category::HorsePill, 1, horse), "horse effect exists in catalogue");
    Check(h.rendered.icon_variant == 70 && h.rendered.icon_id == 1 && h.HasText(horse.markup), "identified held horse pill uses its native effect and horse text");

    h.Reset(); h.pickup.Put<unsigned>(0x38, 5); h.pickup.Put<unsigned>(0x3c, 350); h.pickup.Put<unsigned>(0x40, 1);
    h.pickup.Put<unsigned char>(0x1c1, 1); h.pickup.Put<unsigned char>(0x1c2, 1);
    h.pickup.Put<float>(0x310, 100); h.pickup.Put<float>(0x314, 100);
    const std::uintptr_t entities[] = {h.pickup.Address()};
    h.Step(entities, 1); Check(h.rendered.valid && h.rendered.icon_variant == 350, "ordinary nearby description still renders");
    h.input[33][13] = true; h.Step(entities, 1); h.Reminder(1, 1);
    h.input[33][13] = false; h.Step(entities, 1);
    Check(h.rendered.valid && h.rendered.icon_variant == 350, "release restores nearby description in the same frame");

    h.Reset(); h.input[33][13] = true; h.player0.Put<unsigned char>(0x1c3, 1); h.Step();
    Check(!h.rendered.valid && h.calls.empty(), "dead player is never read as reminder owner");
    h.Reset(); h.input[33][13] = true; h.game.Put<std::uintptr_t>(isaac::profile::PlayersBegin, 0x12345000); h.Step();
    Check(!h.rendered.valid && h.calls.empty(), "unreadable player vector fails closed");
    h.Reset(); h.input[33][13] = true; h.game.Put<int>(isaac::profile::ChallengeId, 32); h.Step();
    Check(!h.rendered.valid && h.calls.empty(), "existing challenge restriction remains intact");
    std::printf("PASS: %u checks across %u production-adapter frames (mock native memory/input/render boundaries).\n", checks, frames);
    return 0;
}

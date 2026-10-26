// Shared fake console for the mod menu's native tests (tools/menu_ui/run_native_tests.py).
// Two translation units use it: test_native_menu.cpp (debug_kit.cpp, runtime_extras.cpp,
// mod_settings.cpp and the test cases) and native_menu_boundary.cpp (mod_menu.cpp plus
// the fake game functions it calls through its single Native<>/Fs<> boundary).
#pragma once
#include "../../native/source/program/feature_api.hpp"
#include "../../native/source/program/generated/build_profile.hpp"
#include "../../features/menu_ui/host.hpp"
#include "../../features/menu_ui/menu.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <vector>

extern unsigned g_Checks;
#define CHECK(...) do { ++g_Checks; if (!(__VA_ARGS__)) { std::fprintf(stderr, "CHECK %s:%d: %s\n", __FILE__, __LINE__, #__VA_ARGS__); std::abort(); } } while (false)

struct MBlock {
    std::vector<std::uint64_t> data;
    explicit MBlock(std::size_t size) : data((size + 7) / 8) {}
    std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(data.data()); }
    std::size_t Size() const { return data.size() * 8; }
    template<class T> void Put(std::size_t offset, T value) {
        assert(offset + sizeof(value) <= Size());
        std::memcpy(reinterpret_cast<void*>(Address() + offset), &value, sizeof(value));
    }
    template<class T> T Get(std::size_t offset) const {
        T value{}; std::memcpy(&value, reinterpret_cast<const void*>(Address() + offset), sizeof(value)); return value;
    }
    void Bytes(std::size_t offset, const void* bytes, std::size_t n) {
        assert(offset + n <= Size()); std::memcpy(reinterpret_cast<void*>(Address() + offset), bytes, n);
    }
    // libc++ short string (size << 1 in byte 0, text from byte 1).
    void ShortString(std::size_t offset, const char* text) {
        const auto n = std::strlen(text); assert(n < 23);
        Put<std::uint8_t>(offset, static_cast<std::uint8_t>(n << 1)); Bytes(offset + 1, text, n + 1);
    }
};

struct Call { std::string name{}; std::vector<long long> args{}; std::string text{}; float x{}, y{}, f{}; };
struct SpriteState { std::string file, sheet, animation; int frame{}; float sx{1}, sy{1}, a{1}; bool created{}, loaded{}; };

enum class HookKind { ModsMenuUpdate, ModsMenuRender, PauseInput, PauseRender };

struct Host {
    MBlock module{0xabce00}, game{0x347d50}, manager{0x138000}, room{0x19d8}, descriptor{0x58}, players{64},
        entities{64}, p1{0x2850}, p2{0x2850}, input{0x100}, menuManager{0x100}, pause{0x40}, pickup{0x600},
        config{0x100}, history{0x1c * 8};
    std::vector<MBlock*> blocks{&module, &game, &manager, &room, &descriptor, &players, &entities, &p1, &p2, &input,
        &menuManager, &pause, &pickup, &config, &history};
    std::uint64_t now{1000};
    std::array<std::uint32_t, 34> buttons{};
    std::array<bool, 34> connected{};
    bool paused{}, collectible{true}, noSpace{}, spawnFails{}, chargeFails{}, mounted{true}, fsWriteFails{};
    int pillColor{5};
    unsigned installs{}, nativeMenuUpdates{}, nativeMenuRenders{}, pauseInputs{}, pauseRenders{}, titleRenders{};
    unsigned instantTicks{}, specialistTicks{}, instantResets{}, specialistResets{}, greedTicks{};
    unsigned fsReads{}, fsWrites{}, fsCommits{}, fsCreates{}, shaders{};
    std::vector<Call> calls;           // game functions called by the menu backend
    std::vector<Call> draws;           // text, rectangles, sprites and EID helpers
    std::map<const void*, SpriteState> sprites;
    unsigned spriteLoads{}, spriteRenders{};
    // Layout pushed to the features.
    int hitX{-2}, hitY{-2}; unsigned hitSize{}, hitOpacity{}, tmSteps{}; bool hitFloor{}, eidNearby{}, eidMap{};
    std::uint8_t rplusRules{0xff};

    Host() {
        namespace p = isaac::profile;
        module.Put(p::GameGlobal, game.Address()); module.Put(p::ManagerGlobal, manager.Address());
        module.Put<float>(p::ScreenWidth, 480); module.Put<float>(p::ScreenHeight, 270);
        manager.Put<int>(8, 2);
        for (std::uintptr_t font : {p::ManagerFont, std::uintptr_t{0x117120}, std::uintptr_t{0x137170}}) manager.Put<std::uint8_t>(font, 1);
        game.Put(p::GameRoom, room.Address()); room.Put(8, descriptor.Address()); descriptor.Put<std::uint8_t>(0x50, 1);
        room.Put<int>(0x14, 13); room.Put<int>(0x18, 7);
        game.Put<unsigned>(0x24f99c, 100); room.Put<unsigned>(0x191c, 10); game.Put<unsigned>(0x25d44, 123);
        game.Put<int>(0, 3); game.Put<int>(4, 1);
        Player(p1, 11, 1); Player(p2, 12, 33);
        p1.Put<float>(0x310, 280); p1.Put<float>(0x314, 280);
        SetPlayers({p1.Address()}); SetEntities({p1.Address()});
        for (unsigned id : {1, 2, 3, 4, 5, 6, 7, 8, 33}) connected[id] = true;
        config.Put<int>(0x74, 6); config.ShortString(0x38, "gfx/items/c001.png");
    }
    static void Player(MBlock& b, unsigned seed, unsigned controller) {
        b.Put<unsigned>(0x38, 1); b.Put<unsigned>(0x3c, 0); b.Put<unsigned>(0x3e8, seed);
        b.Put<std::uint8_t>(0x1c1, 1); b.Put<std::uint8_t>(0x1c2, 1); b.Put<std::uint8_t>(0x1c3, 0);
        b.Put<unsigned>(0x19ec, controller); b.Put<std::uint8_t>(0x400, 1); b.Put<int>(0x404, 0);
    }
    void SetPlayers(std::initializer_list<std::uintptr_t> values) {
        unsigned i = 0; for (auto p : values) players.Put(i++ * 8, p);
        game.Put(isaac::profile::PlayersBegin, players.Address()); game.Put(isaac::profile::PlayersEnd, players.Address() + i * 8);
    }
    void SetEntities(std::initializer_list<std::uintptr_t> values) {
        unsigned i = 0; for (auto p : values) entities.Put(i++ * 8, p);
        room.Put(isaac::profile::RoomEntities, entities.Address()); room.Put<unsigned>(isaac::profile::RoomEntityCount, i);
    }
    bool Readable(std::uintptr_t p, std::size_t size) const {
        if (!size || p + size < p) return false;
        for (const auto* b : blocks) if (p >= b->Address() && p - b->Address() <= b->Size() && size <= b->Size() - (p - b->Address())) return true;
        return false;
    }
    template<std::size_t N> void Guard(std::uintptr_t offset, const std::uint8_t (&bytes)[N]) { module.Bytes(offset, bytes, N); }
    void Record(const char* name, std::initializer_list<long long> args = {}) { calls.push_back({name, args}); }
    unsigned Count(const char* name) const { unsigned n{}; for (const auto& c : calls) n += c.name == name; return n; }
    const Call* Last(const char* name) const {
        for (auto i = calls.rbegin(); i != calls.rend(); ++i) if (i->name == name) return &*i;
        return nullptr;
    }
    unsigned Drawn(const char* name) const { unsigned n{}; for (const auto& d : draws) n += d.name == name; return n; }
    bool DrewText(const std::string& text) const {
        for (const auto& d : draws) if ((d.name == "text" || d.name == "small") && d.text.find(text) != std::string::npos) return true;
        return false;
    }
};
extern std::unique_ptr<Host> host;
// The save file outlives a fake module reload, like the console's save data.
extern std::map<std::string, std::string> g_Files;

template<HookKind kind> struct HookStub {
    static void Orig(void*) {
        if (kind == HookKind::ModsMenuUpdate) ++host->nativeMenuUpdates;
        else if (kind == HookKind::ModsMenuRender) ++host->nativeMenuRenders;
        else if (kind == HookKind::PauseInput) ++host->pauseInputs;
        else ++host->pauseRenders;
    }
    static void InstallAtPtr(std::uintptr_t address) {
        CHECK(host->Readable(address, 20)); std::memset(reinterpret_cast<void*>(address), 0xcc, 20); ++host->installs;
    }
};
#define HOOK_DEFINE_TRAMPOLINE(name) struct name : HookStub<HookKind::name>

// Test access to mod_menu.cpp internals (defined in native_menu_boundary.cpp).
namespace menu_test {
const isaac::menu_ui::Menu& Menu();
isaac::menu_ui::Prefs& Prefs();
isaac::menu_ui::Host& MenuHost();
bool Dirty();
void ForgetPreferences();   // as if the game process restarted
void PauseInput(void* pause);
void PauseRender(void* pause);
}

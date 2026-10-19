#include "../../native/source/program/specialist.hpp"
#include "../../native/source/program/feature_api.hpp"
#include "../../native/source/program/generated/build_profile.hpp"
#include "policy.hpp"
#include "generated/native.hpp"
#include "native_calls.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace {
namespace sp = isaac::specialist;
namespace api = isaac::specialist::native;
unsigned checks{}, frames{};
void Check(bool value, const char* message) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
struct Block {
    std::vector<std::uint64_t> bytes;
    explicit Block(std::size_t size) : bytes((size + 7) / 8) {}
    std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(bytes.data()); }
    std::size_t Size() const { return bytes.size() * 8; }
    void Clear() { std::memset(bytes.data(), 0, Size()); }
    template<class T> void Put(std::size_t at, T value) {
        Check(at + sizeof(value) <= Size(), "bounded fixture field");
        std::memcpy(reinterpret_cast<void*>(Address() + at), &value, sizeof(value));
    }
};
std::vector<const Block*> memory;
template<class T> T Read(std::uintptr_t p) { T v{}; std::memcpy(&v, reinterpret_cast<const void*>(p), sizeof(v)); return v; }
struct Harness {
    Block module{0xac0000}, manager{0x40000}, game{0x270000}, room{0x8000}, next_room{0x8000};
    Block descriptor{0x100}, player_pointers{16}, player0{0x3000}, player1{0x3000};
    Block entity_pointers{16}, pickup0{0x700}, pickup1{0x700}, layers0{0x160}, layers1{0x160};
    Block null_pointers{4096 * 8}, items{4096 * 0x100}, tracks{4096 * 0x70}, strings{0x100000};
    unsigned string_used{}, tick{}, curses{}, costume_loads{}, music_loads{}, constructs{}, destructs{}, adds{}, removes{}, plays{}, restores{}, volumes{};
    unsigned null_count{}, track_count{}, player_count = 1;
    bool binky0{}, binky1{}, fail_costumes{}, fail_music{}, malformed_costume{};
    void* context{};
    std::array<std::set<int>, 2> worn;
    Harness() { memory = {&module,&manager,&game,&room,&next_room,&descriptor,&player_pointers,&player0,&player1,&entity_pointers,&pickup0,&pickup1,&layers0,&layers1,&null_pointers,&items,&tracks,&strings}; }
    std::uintptr_t Config() const { return manager.Address() + 0x36538; }
    std::uintptr_t Music() const { return manager.Address() + 0x36068; }
    void String(std::uintptr_t target, const std::string& text) {
        Check(string_used + text.size() + 1 <= strings.Size(), "bounded fixture string pool");
        const auto data = strings.Address() + string_used;
        std::memcpy(reinterpret_cast<void*>(data), text.c_str(), text.size() + 1);
        const std::uint64_t value[] = {static_cast<std::uint64_t>((text.size() + 16) & ~15u) | 1u, text.size(), data};
        std::memcpy(reinterpret_cast<void*>(target), value, sizeof(value));
        string_used += static_cast<unsigned>(text.size() + 1);
    }
    void NullItem(unsigned id, const char* path) {
        const auto item = items.Address() + id * 0x100;
        items.Put<unsigned>(id * 0x100, 0); items.Put<unsigned>(id * 0x100 + 4, id); items.Put<unsigned>(id * 0x100 + 0x80, id);
        String(item + 0x88, path); null_pointers.Put<std::uintptr_t>(id * 8, item);
    }
    void SetNullCount(unsigned count) {
        null_count = count;
        manager.Put<std::uintptr_t>(0x36538 + 0x30, null_pointers.Address());
        manager.Put<std::uintptr_t>(0x36538 + 0x38, null_pointers.Address() + count * 8);
    }
    void Track(unsigned id, const char* name, const char* path) {
        tracks.Put<unsigned>(id * 0x70, id); String(tracks.Address() + id * 0x70 + 8, name); String(tracks.Address() + id * 0x70 + 0x38, path);
    }
    void SetTrackCount(unsigned count) {
        track_count = count;
        manager.Put<std::uintptr_t>(0x36068 + 0x3d8, tracks.Address());
        manager.Put<std::uintptr_t>(0x36068 + 0x3e0, tracks.Address() + count * 0x70);
    }
    void Player(Block& player, unsigned seed, int type) {
        player.Put<unsigned>(0x38, 1); player.Put<unsigned char>(0x1c1, 1); player.Put<unsigned char>(0x1c2, 1);
        player.Put<unsigned>(0x3e8, seed); player.Put<int>(0x1738, type);
    }
    void Pickup(Block& pickup, Block& layers, unsigned id, float x = 200, float y = 200) {
        pickup.Put<unsigned>(0x38, 5); pickup.Put<unsigned>(0x3c, 100); pickup.Put<unsigned>(0x40, id);
        pickup.Put<unsigned char>(0x1c1, 1); pickup.Put<unsigned char>(0x1c2, 1);
        pickup.Put<float>(0x310, x); pickup.Put<float>(0x314, y);
        pickup.Put<std::uintptr_t>(0xf8, layers.Address()); pickup.Put<unsigned>(0x100, 2);
        String(layers.Address() + 0xb0 + 0x10, "gfx/items/collectibles/visible.png");
    }
    void SetPlayers(unsigned count) {
        player_count = count;
        game.Put<std::uintptr_t>(isaac::profile::PlayersBegin, player_pointers.Address());
        game.Put<std::uintptr_t>(isaac::profile::PlayersEnd, player_pointers.Address() + count * 8);
    }
    void SetEntities(unsigned count) {
        room.Put<std::uintptr_t>(isaac::profile::RoomEntities, entity_pointers.Address()); room.Put<unsigned>(isaac::profile::RoomEntityCount, count);
        next_room.Put<std::uintptr_t>(isaac::profile::RoomEntities, entity_pointers.Address()); next_room.Put<unsigned>(isaac::profile::RoomEntityCount, count);
    }
    void Reset() {
        isaac::ResetSpecialist(); isaac::OnSpecialistModuleUnloaded();
        for (const auto* block : memory) const_cast<Block*>(block)->Clear();
        string_used = tick = curses = costume_loads = music_loads = constructs = destructs = adds = removes = plays = restores = volumes = 0;
        binky0 = binky1 = fail_costumes = fail_music = malformed_costume = false; context = nullptr; worn = {};
        for (const auto& guard : api::Guards) std::memcpy(reinterpret_cast<void*>(module.Address() + guard.offset), guard.bytes, sizeof(guard.bytes));
        module.Put<std::uintptr_t>(isaac::profile::ManagerGlobal, manager.Address()); module.Put<std::uintptr_t>(isaac::profile::GameGlobal, game.Address());
        manager.Put<int>(8, 2); manager.Put<int>(0x36068 + 0x3c8, 5);
        game.Put<std::uintptr_t>(isaac::profile::GameRoom, room.Address()); game.Put<unsigned>(0x25d44, 12345);
        room.Put<std::uintptr_t>(8, descriptor.Address()); next_room.Put<std::uintptr_t>(8, descriptor.Address()); room.Put<unsigned>(0x10, 1);
        Player(player0, 101, 0); Player(player1, 102, 23);
        player_pointers.Put<std::uintptr_t>(0, player0.Address()); player_pointers.Put<std::uintptr_t>(8, player1.Address()); SetPlayers(1);
        Pickup(pickup0, layers0, 4); Pickup(pickup1, layers1, 1);
        entity_pointers.Put<std::uintptr_t>(0, pickup0.Address()); entity_pointers.Put<std::uintptr_t>(8, pickup1.Address()); SetEntities(1);
        for (unsigned i = 0; i < 128; ++i) NullItem(i, ("gfx/old/" + std::to_string(i) + ".anm2").c_str());
        SetNullCount(128);
        for (unsigned i = 0; i < 119; ++i) Track(i, ("original_" + std::to_string(i)).c_str(), ("antibirth/" + std::to_string(i) + ".ogg").c_str());
        SetTrackCount(119);
        Check(isaac::InitializeSpecialist(module.Address()), "exact native prologues enable Specialist");
    }
    void Step() { game.Put<int>(0x24f99c, static_cast<int>(++tick)); ++frames; isaac::UpdateSpecialist(game.Address()); }
    int Current() const { return Read<int>(Music() + 0x3c8); }
    int Costume(int type) const { return 128 + static_cast<int>(sp::CostumeFor(type)); }
};
Harness* h;
void Construct(void* object) {
    Check(!h->context && (reinterpret_cast<std::uintptr_t>(object) & 7) == 0, "real aligned loader context constructed once");
    h->context = object; ++h->constructs; std::memset(object, 0x5a, api::ModEntrySize);
}
void Destroy(void* object) {
    Check(object == h->context && reinterpret_cast<unsigned char*>(object)[api::ModEntrySize-1] == 0x5a, "same full-size loader context destroyed while native code is available");
    h->context = nullptr; ++h->destructs;
}
bool LoadCostumes(void* config, const char* path, void* mod) {
    Check(config == reinterpret_cast<void*>(h->Config()) && std::strcmp(path, sp::assets::CostumeConfig) == 0, "costumes load into game registry from isolated config");
    Check(mod && mod == h->context, "non-null native ModEntry selects append instead of existing-ID replacement");
    ++h->costume_loads;
    if (h->fail_costumes) return false;
    const auto first = h->null_count;
    for (unsigned i = 0; i < 42; ++i) h->NullItem(first + i, sp::assets::Costumes[i]);
    h->SetNullCount(first + 42);
    if (h->malformed_costume) h->items.Put<unsigned>(first * 0x100 + 4, 999);
    return true;
}
bool LoadMusic(void* music, const char* path, bool append) {
    Check(music == reinterpret_cast<void*>(h->Music()) && std::strcmp(path, sp::assets::MusicConfig) == 0, "music loads into manager-owned music registry");
    Check(append, "music is always appended; original Antibirth/base tracks cannot be replaced");
    ++h->music_loads;
    if (h->fail_music) return false;
    const auto first = h->track_count;
    for (unsigned i = 0; i < 4; ++i) h->Track(first+i, sp::assets::MusicNames[i], ("codex_specialist/music/track" + std::to_string(i) + ".ogg").c_str());
    h->SetTrackCount(first + 4); return true;
}
std::uintptr_t GetNull(const void* config, int id) {
    Check(config == reinterpret_cast<const void*>(h->Config()) && id >= 0 && static_cast<unsigned>(id) < h->null_count, "null costume lookup is bounded and targets correct config");
    return Read<std::uintptr_t>(h->null_pointers.Address() + static_cast<unsigned>(id) * 8);
}
unsigned PlayerIndex(const void* player) {
    Check(player == reinterpret_cast<const void*>(h->player0.Address()) || player == reinterpret_cast<const void*>(h->player1.Address()), "costume operation addresses a live validated player");
    return player == reinterpret_cast<const void*>(h->player1.Address()) ? 1 : 0;
}
void Add(void* player, int id) {
    const auto index = PlayerIndex(player);
    Check(id >= 128 && id < 170, "only newly registered costumes are added");
    Check(h->worn[index].insert(id).second, "dance animation is not restarted by duplicate per-frame AddNullCostume"); ++h->adds;
}
void Remove(void* player, int id) {
    const auto index = PlayerIndex(player);
    Check(id >= 128 && id < 170, "only Specialist costumes are removed");
    h->worn[index].erase(id); ++h->removes;
}
bool Has(const void* player, unsigned item, bool flag) {
    Check(item == sp::Binky && !flag, "native Binky ownership check has correct ABI");
    return PlayerIndex(player) ? h->binky1 : h->binky0;
}
unsigned Curses(const void* game) { Check(game == reinterpret_cast<const void*>(h->game.Address()), "curse query uses game"); return h->curses; }
int Current(const void* music) { Check(music == reinterpret_cast<const void*>(h->Music()), "music query uses manager music object"); return h->Current(); }
void Play(void* music, int id, float volume) {
    Check(music == reinterpret_cast<void*>(h->Music()) && id >= 119 && id < 123 && volume == 1.f, "dance music uses registered IDs and float volume ABI");
    h->manager.Put<int>(0x36068 + 0x3c8, id); ++h->plays;
}
void Volume(void* music) { Check(music == reinterpret_cast<void*>(h->Music()), "native volume applies user setting"); ++h->volumes; }
void Restore(void* room) {
    Check(room == reinterpret_cast<void*>(h->room.Address()) || room == reinterpret_cast<void*>(h->next_room.Address()), "music restores through current native room");
    h->manager.Put<int>(0x36068 + 0x3c8, 5); ++h->restores;
}
}

extern "C" std::uint32_t svcQueryMemory(MemoryInfo* info, std::uint32_t* page, std::uint64_t address) {
    *page = 0;
    for (const auto* block : memory) if (address >= block->Address() && address < block->Address() + block->Size()) {
        *info = {block->Address(), block->Size(), Perm_R}; return 0;
    }
    *info = {}; return 1;
}
namespace isaac {
bool CopyNativeString(std::uintptr_t string, char* output, std::size_t capacity) {
    std::uint8_t data[24]{};
    if (!output || !capacity || !feature_memory::Readable(string, sizeof(data))) return false;
    std::memcpy(data, reinterpret_cast<const void*>(string), sizeof(data));
    std::uint64_t size = data[0] >> 1; std::uintptr_t bytes = string + 1;
    if (data[0] & 1) { std::memcpy(&size, data + 8, 8); std::memcpy(&bytes, data + 16, 8); }
    else if (size > 22) return false;
    if (size >= capacity || !feature_memory::Readable(bytes, size + 1)) return false;
    std::memcpy(output, reinterpret_cast<const void*>(bytes), size); output[size] = 0; return true;
}
}
namespace isaac::specialist::native {
std::uintptr_t ResolveTestFunction(std::uintptr_t module, std::uintptr_t offset) {
    Check(module == h->module.Address(), "all Specialist calls use verified module");
    switch(offset) {
        case ModEntryConstruct: return reinterpret_cast<std::uintptr_t>(&Construct);
        case ModEntryDestroy: return reinterpret_cast<std::uintptr_t>(&Destroy);
        case LoadCostumes: return reinterpret_cast<std::uintptr_t>(&::LoadCostumes);
        case LoadMusic: return reinterpret_cast<std::uintptr_t>(&::LoadMusic);
        case GetNullItem: return reinterpret_cast<std::uintptr_t>(&GetNull);
        case AddNullCostume: return reinterpret_cast<std::uintptr_t>(&Add);
        case RemoveNullCostume: return reinterpret_cast<std::uintptr_t>(&Remove);
        case HasCollectible: return reinterpret_cast<std::uintptr_t>(&Has);
        case GetCurses: return reinterpret_cast<std::uintptr_t>(&Curses);
        case CurrentMusic: return reinterpret_cast<std::uintptr_t>(&Current);
        case PlayMusic: return reinterpret_cast<std::uintptr_t>(&Play);
        case UpdateVolume: return reinterpret_cast<std::uintptr_t>(&Volume);
        case RoomPlayMusic: return reinterpret_cast<std::uintptr_t>(&Restore);
        default: std::fprintf(stderr,"Unexpected Specialist native offset %zx\n",static_cast<std::size_t>(offset)); std::exit(1);
    }
}
}

int main() {
    Harness fixture; h = &fixture;
    h->Reset();
    const auto original_items = h->null_pointers.bytes;
    std::array<unsigned char, 119 * 0x70> original_tracks{};
    std::memcpy(original_tracks.data(), reinterpret_cast<const void*>(h->tracks.Address()), original_tracks.size());
    h->Step();
    Check(h->null_count == 170 && h->track_count == 123 && h->constructs == 1 && h->destructs == 1 && !h->context, "42 costumes and four songs append with balanced loader lifetime");
    Check(std::memcmp(original_items.data(), h->null_pointers.bytes.data(), 128 * 8) == 0, "original null costume pointers are preserved");
    Check(std::memcmp(original_tracks.data(), h->tracks.bytes.data(), original_tracks.size()) == 0, "original music entries remain intact");
    Check(h->worn[0].count(h->Costume(0)) == 1 && h->Current() == 119 && h->volumes == 1, "quality-four visible item starts original Isaac dance and classic song");
    for (unsigned i = 0; i < 20; ++i) h->Step();
    Check(h->adds == 1 && h->plays == 1 && h->costume_loads == 1 && h->music_loads == 1, "continued dance neither restarts animation nor reloads assets/music");
    h->SetEntities(0); h->Step(); Check(h->worn[0].empty() && h->restores == 1 && h->Current() == 5, "taking item removes dance and restores current room music");

    for (int type = 0; type < 41; ++type) {
        h->Reset(); h->player0.Put<int>(0x1738, type); h->Step();
        Check(h->worn[0].count(h->Costume(type)) == 1, "every native character has its original dance costume");
        const int music = sp::MusicFor(type);
        Check(music < 0 ? h->plays == 0 : h->Current() == 119 + music, "classic/tainted/Cain/Eden music follows source character rules");
    }
    Check(sp::CostumeFor(30) == sp::CostumeFor(9), "Tainted Eden defaults to reduced flashing");
    Check(sp::CostumeFor(-1) == sp::CostumeFor(0) && sp::CostumeFor(99) == sp::CostumeFor(0), "unknown character uses original Isaac fallback");

    h->Reset(); h->binky0 = true; h->Step();
    Check(h->worn[0].size() == 2 && h->worn[0].count(128 + sp::assets::BinkyCostume), "Binky uses its additional original animation");
    h->binky0 = false; h->Step(); Check(h->worn[0].size() == 1, "lost Binky costume is removed without touching other costumes");
    h->player0.Put<int>(0x1738, 30); h->Step(); Check(h->worn[0].count(h->Costume(30)) && h->Current() == 122, "character change replaces owned costume and chooses matching music");

    h->Reset(); h->SetPlayers(2); h->Step();
    Check(h->worn[0].size() == 1 && h->worn[1].size() == 1 && h->Current() == 121, "co-op dances for both players and source-order primary character chooses music");
    h->SetPlayers(1); h->Step(); Check(h->worn[1].empty(), "departed live co-op player is cleaned up");

    h->Reset(); h->Step(); h->manager.Put<int>(0x36068 + 0x3c8, 42); h->Step(); h->SetEntities(0); h->Step();
    Check(h->plays == 1 && h->restores == 0 && h->Current() == 42, "game-owned music change is neither reclaimed nor overwritten on stop");

    h->Reset(); h->Step(); h->SetEntities(0); h->game.Put<std::uintptr_t>(isaac::profile::GameRoom, h->next_room.Address()); h->Step();
    Check(h->worn[0].empty() && h->restores == 1, "room transition restores through the new current room");

    h->Reset(); h->Step();
    h->room.Put<unsigned>(0x191c, 1); h->manager.Put<int>(0x36068 + 0x3c8, 42); h->Step();
    Check(h->plays == 2 && h->Current() == 119 && h->adds == 2 && h->removes == 1 && h->restores == 0,
          "reused room pointer with new entry and native music restarts the new room's dance and song");
    h->room.Put<unsigned>(0x191c, 2); h->manager.Put<int>(0x36068 + 0x3c8, 43);
    ++frames; isaac::UpdateSpecialist(h->game.Address());
    Check(h->plays == 3 && h->Current() == 119 && h->restores == 0,
          "same game frame still processes a changed native room entry stamp");
    h->Step(); Check(h->plays == 3 && h->adds == 3, "unchanged new room does not restart the dance each update");

    h->Reset(); h->Step(); isaac::ResetSpecialist();
    Check(h->worn[0].empty() && h->restores == 1 && !h->context, "pre-unload reset releases dance and leaves no native loader allocations");
    h->Step(); Check(h->worn[0].size() == 1 && h->costume_loads == 1 && h->music_loads == 1, "failed unload can resume without duplicate registration");
    isaac::ResetSpecialist(); isaac::OnSpecialistModuleUnloaded();
    Check(isaac::InitializeSpecialist(h->module.Address()), "new module lifetime can initialize"); h->Step();
    Check(h->costume_loads == 1 && h->music_loads == 1, "already-present isolated entries are reused after initialization");

    for (unsigned mode = 0; mode < 8; ++mode) {
        h->Reset();
        if (mode == 0) h->curses = 0x40;
        if (mode == 1) h->pickup0.Put<unsigned char>(0x562, 1);
        if (mode == 2) h->String(h->layers0.Address() + 0xc0, "gfx/items/QUESTIONMARK.png");
        if (mode == 3) h->pickup0.Put<unsigned char>(0x1c1, 0);
        if (mode == 4) h->pickup0.Put<unsigned>(0x40, 1);
        if (mode == 5) h->pickup0.Put<float>(0x310, 9999.f);
        if (mode == 6) h->pickup0.Put<unsigned>(0x100, 0);
        if (mode == 7) h->player0.Put<unsigned char>(0x1c3, 1);
        h->Step(); Check(h->adds == 0 && h->plays == 0, "blind/unknown/invisible/low-quality/distant/dead cases do not reveal or trigger dance");
    }
    h->Reset(); h->room.Put<unsigned>(0x10, 4); h->game.Put<int>(4, 4); h->SetEntities(2); h->Step();
    Check(h->adds == 1, "visible left alternative-path treasure follows upstream rule");
    h->pickup0.Put<float>(0x310, 400); h->Step(); Check(h->worn[0].empty(), "right alternative-path blind pedestal does not trigger dance");

    for (unsigned mode = 0; mode < 3; ++mode) {
        h->Reset(); h->fail_costumes = mode == 0; h->fail_music = mode == 1; h->malformed_costume = mode == 2;
        h->Step(); h->Step(); Check(h->adds == 0 && h->plays == 0 && h->constructs == h->destructs, "failed or malformed registration fails closed and cleans loader object");
        Check(h->costume_loads == 1 && h->music_loads <= 1, "failed registration is not retried every frame");
    }
    h->Reset(); h->Step(); h->SetNullCount(128); h->Step();
    Check(h->removes == 0 && h->restores == 1, "registry reset never dereferences old costume IDs");
    h->Reset(); isaac::ResetSpecialist(); isaac::OnSpecialistModuleUnloaded(); h->module.Put<unsigned char>(api::LoadMusic, 0);
    Check(!isaac::InitializeSpecialist(h->module.Address()), "mismatched native function guard disables feature");
    std::printf("PASS: %u checks across %u actual Specialist adapter frames; original assets selected, native mutations mocked.\n", checks, frames);
    return 0;
}

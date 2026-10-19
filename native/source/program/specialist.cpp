#include "specialist.hpp"
#include "feature_api.hpp"
#include "native_sprite.hpp"
#include "generated/build_profile.hpp"
#include "../../../features/specialist/policy.hpp"
#include "../../../features/specialist/generated/native.hpp"
#include "../../../features/specialist/native_calls.hpp"
#include <cstring>

namespace isaac {
namespace {
using namespace feature_memory;
namespace sp = specialist;
namespace api = specialist::native;
constexpr unsigned CostumeCount = sizeof(sp::assets::Costumes) / sizeof(sp::assets::Costumes[0]);
constexpr unsigned MusicCount = 4, MaxPlayers = 8, MaxRegistry = 4096;
constexpr std::uintptr_t ItemConfigOffset = 0x36538, MusicOffset = 0x36068;
struct OwnedPlayer { std::uintptr_t address{}; unsigned seed{}; int costume = -1, binky = -1; };
struct Player { std::uintptr_t address{}; unsigned seed{}; int type{}; bool binky{}; };
struct Registry { std::uintptr_t begin{}, end{}; unsigned count{}; };
std::uintptr_t s_Module{}, s_Manager{}, s_Game{}, s_Room{};
int s_Costumes[CostumeCount]{}, s_Music[MusicCount]{};
OwnedPlayer s_Players[MaxPlayers];
bool s_Guarded{}, s_Attempted{}, s_Registered{}, s_Dancing{}, s_OwnsMusic{};
int s_LastFrame = -1, s_PlayingTrack = -1, s_ChosenMusic = -1;
unsigned s_RunSeed{}, s_RoomEntry{};
// Exact allocation size proved by ModManager::ListMods at 0x41fcec.
// Never moved or copied: its native containers/ANM2s contain self pointers.
alignas(8) std::byte s_LoaderContext[api::ModEntrySize]{};
std::uintptr_t s_PreviousCostumes[MaxRegistry]{};

bool Pointer(std::uintptr_t value) { return value > 0x10000 && !(value & 7); }
bool Alive(std::uintptr_t entity, std::size_t size) {
    return Pointer(entity) && Readable(entity, size) && Read<unsigned char>(entity + 0x1c2) && !Read<unsigned char>(entity + 0x1c3);
}
bool RegistryAt(std::uintptr_t owner, std::uintptr_t offset, unsigned stride, Registry& result) {
    if (!TryRead(owner + offset, result.begin) || !TryRead(owner + offset + 8, result.end) ||
        !Pointer(result.begin) || result.end < result.begin || (result.end - result.begin) % stride ||
        (result.end - result.begin) / stride > MaxRegistry) return false;
    result.count = static_cast<unsigned>((result.end - result.begin) / stride);
    return result.count && Readable(result.begin, result.end - result.begin);
}
bool StringEquals(std::uintptr_t source, const char* expected) {
    char actual[512];
    return CopyNativeString(source, actual, sizeof(actual)) && std::strcmp(actual, expected) == 0;
}
unsigned FindCostumes(std::uintptr_t config, const Registry& registry) {
    unsigned found = 0;
    for (unsigned wanted = 0; wanted < CostumeCount; ++wanted) {
        s_Costumes[wanted] = -1;
        for (unsigned index = 0; index < registry.count; ++index) {
            const auto item = Read<std::uintptr_t>(registry.begin + index * 8);
            if (!Readable(item, 0xf8) || !StringEquals(item + 0x88, sp::assets::Costumes[wanted])) continue;
            if (Read<unsigned>(item) != 0 || Read<unsigned>(item + 4) != index ||
                Read<unsigned>(item + 0x80) != index ||
                api::Function<std::uintptr_t (*)(const void*, int)>(s_Module, api::GetNullItem)(reinterpret_cast<const void*>(config), static_cast<int>(index)) != item)
                return CostumeCount + 1;
            s_Costumes[wanted] = static_cast<int>(index); ++found; break;
        }
    }
    return found;
}
unsigned FindMusic(const Registry& registry) {
    unsigned found = 0;
    for (unsigned wanted = 0; wanted < MusicCount; ++wanted) {
        s_Music[wanted] = -1;
        for (unsigned index = 0; index < registry.count; ++index) {
            const auto entry = registry.begin + index * 0x70;
            if (Read<unsigned>(entry) != index || !StringEquals(entry + 8, sp::assets::MusicNames[wanted])) continue;
            char path[512];
            constexpr char prefix[] = "codex_specialist/music/";
            if (!CopyNativeString(entry + 0x38, path, sizeof(path)) || std::strncmp(path, prefix, sizeof(prefix) - 1) != 0) return MusicCount + 1;
            s_Music[wanted] = static_cast<int>(index); ++found; break;
        }
    }
    return found;
}
bool RegisterResources(std::uintptr_t manager) {
    const auto config = manager + ItemConfigOffset, music = manager + MusicOffset;
    Registry costumes, tracks;
    if (!RegistryAt(config, 0x30, 8, costumes) || !RegistryAt(music, 0x3d8, 0x70, tracks) || tracks.count < 119) return false;
    if (s_Attempted) {
        // A registry may be reset in-place when returning through a menu. Never
        // apply remembered IDs to a different or truncated registry.
        if (!s_Registered) return false;
        const unsigned endpoints[] = {0, CostumeCount - 1};
        for (unsigned index : endpoints) {
            if (s_Costumes[index] < 0 || static_cast<unsigned>(s_Costumes[index]) >= costumes.count) return false;
            const auto item = Read<std::uintptr_t>(costumes.begin + static_cast<unsigned>(s_Costumes[index]) * 8);
            if (!Readable(item, 0xf8) || !StringEquals(item + 0x88, sp::assets::Costumes[index])) return false;
        }
        return s_Music[MusicCount - 1] >= 0 && static_cast<unsigned>(s_Music[MusicCount - 1]) < tracks.count &&
               StringEquals(tracks.begin + static_cast<unsigned>(s_Music[0]) * 0x70 + 8, sp::assets::MusicNames[0]);
    }
    s_Attempted = true;
    unsigned costume_matches = FindCostumes(config, costumes);
    if (costume_matches && costume_matches != CostumeCount) return false;
    if (!costume_matches) {
        const auto original_count = costumes.count;
        if (original_count + CostumeCount > MaxRegistry) return false;
        std::memcpy(s_PreviousCostumes, reinterpret_cast<const void*>(costumes.begin), original_count * 8);
        // A null ModEntry selects existing-ID replacement. A real, constructed
        // object selects the game's append path for type='none' costumes.
        std::memset(s_LoaderContext, 0, sizeof(s_LoaderContext));
        api::Function<void (*)(void*)>(s_Module, api::ModEntryConstruct)(s_LoaderContext);
        const bool loaded = api::Function<bool (*)(void*, const char*, void*)>(s_Module, api::LoadCostumes)(
            reinterpret_cast<void*>(config), sp::assets::CostumeConfig, s_LoaderContext);
        // The type='none' branch deep-copies the path/flags into independent
        // Item objects and never stores this context or either of its maps.
        api::Function<void (*)(void*)>(s_Module, api::ModEntryDestroy)(s_LoaderContext);
        if (!loaded || !RegistryAt(config, 0x30, 8, costumes) || costumes.count != original_count + CostumeCount ||
            std::memcmp(s_PreviousCostumes, reinterpret_cast<const void*>(costumes.begin), original_count * 8) != 0) return false;
        costume_matches = FindCostumes(config, costumes);
    }
    if (costume_matches != CostumeCount) return false;
    unsigned music_matches = FindMusic(tracks);
    if (music_matches && music_matches != MusicCount) return false;
    if (!music_matches) {
        const auto original_count = tracks.count;
        if (original_count + MusicCount > MaxRegistry) return false;
        // true is append mode; false destroys/replaces the existing music.
        const bool loaded = api::Function<bool (*)(void*, const char*, bool)>(s_Module, api::LoadMusic)(
            reinterpret_cast<void*>(music), sp::assets::MusicConfig, true);
        if (!loaded || !RegistryAt(music, 0x3d8, 0x70, tracks) || tracks.count != original_count + MusicCount) return false;
        music_matches = FindMusic(tracks);
    }
    s_Registered = music_matches == MusicCount;
    return s_Registered;
}
bool SamePlayer(const OwnedPlayer& player) {
    return Alive(player.address, 0x2850) && Read<unsigned>(player.address + 0x38) == 1 &&
           Read<unsigned>(player.address + 0x3e8) == player.seed;
}
bool RegisteredCostume(int id) {
    if (id < 0) return false;
    Registry registry;
    if (!RegistryAt(s_Manager + ItemConfigOffset, 0x30, 8, registry) || static_cast<unsigned>(id) >= registry.count) return false;
    const auto item = Read<std::uintptr_t>(registry.begin + static_cast<unsigned>(id) * 8);
    if (!Readable(item, 0xf8)) return false;
    for (unsigned index = 0; index < CostumeCount; ++index)
        if (s_Costumes[index] == id) return StringEquals(item + 0x88, sp::assets::Costumes[index]);
    return false;
}
void RemoveOwned(OwnedPlayer& player) {
    if (SamePlayer(player)) {
        const auto remove = api::Function<void (*)(void*, int)>(s_Module, api::RemoveNullCostume);
        if (RegisteredCostume(player.costume)) remove(reinterpret_cast<void*>(player.address), player.costume);
        if (RegisteredCostume(player.binky)) remove(reinterpret_cast<void*>(player.address), player.binky);
    }
    player = OwnedPlayer{};
}
int CurrentMusic() {
    if (!s_Module || !Readable(s_Manager + MusicOffset, 0x3f0)) return -1;
    return api::Function<int (*)(const void*)>(s_Module, api::CurrentMusic)(reinterpret_cast<const void*>(s_Manager + MusicOffset));
}
void Stop(std::uintptr_t room) {
    for (auto& player : s_Players) RemoveOwned(player);
    if (s_OwnsMusic && CurrentMusic() == s_PlayingTrack && Readable(room, 0x7900) && Readable(Read<std::uintptr_t>(room + 8), 0x58))
        api::Function<void (*)(void*)>(s_Module, api::RoomPlayMusic)(reinterpret_cast<void*>(room));
    s_OwnsMusic = false; s_Dancing = false; s_PlayingTrack = s_ChosenMusic = -1;
}
bool KnownGraphic(std::uintptr_t entity) {
    if (Read<unsigned char>(entity + 0x562)) return false;
    const auto states = Read<std::uintptr_t>(entity + 0xf8);
    const auto count = Read<unsigned>(entity + 0x100);
    if (count < 2 || count > 64 || !Readable(states, count * 0xb0)) return false;
    char path[512];
    if (!CopyNativeString(states + 0xb0 + 0x10, path, sizeof(path)) || !path[0]) return false;
    for (auto* cursor = path; *cursor; ++cursor) if (*cursor >= 'A' && *cursor <= 'Z') *cursor += 'a' - 'A';
    return std::strstr(path, "questionmark.") == nullptr;
}
bool GoodRoom(std::uintptr_t game, std::uintptr_t room) {
    if (api::Function<unsigned (*)(const void*)>(s_Module, api::GetCurses)(reinterpret_cast<const void*>(game)) & 0x40) return false;
    const auto array = Read<std::uintptr_t>(room + profile::RoomEntities);
    const auto count = Read<unsigned>(room + profile::RoomEntityCount);
    if (!count || count > 4096 || !Readable(array, count * 8)) return false;
    unsigned item_count = 0;
    for (unsigned i = 0; i < count; ++i) {
        const auto entity = Read<std::uintptr_t>(array + i * 8);
        if (Alive(entity, 0x568) && Read<unsigned>(entity + 0x38) == 5 && Read<unsigned>(entity + 0x3c) == 100 &&
            Read<unsigned>(entity + 0x40) && sp::ScanRadius(Read<float>(entity + 0x310), Read<float>(entity + 0x314))) ++item_count;
    }
    const auto type = Read<unsigned>(room + 0x10);
    const auto stage_type = Read<int>(game + 4);
    for (unsigned i = 0; i < count; ++i) {
        const auto entity = Read<std::uintptr_t>(array + i * 8);
        if (!Alive(entity, 0x568) || !Read<unsigned char>(entity + 0x1c1) || Read<unsigned>(entity + 0x38) != 5 ||
            Read<unsigned>(entity + 0x3c) != 100 || !sp::GoodItem(Read<unsigned>(entity + 0x40))) continue;
        const auto x = Read<float>(entity + 0x310), y = Read<float>(entity + 0x314);
        if (sp::ScanRadius(x, y) && sp::AltPathVisible(type, stage_type, item_count, x, y) && KnownGraphic(entity)) return true;
    }
    return false;
}
unsigned Players(std::uintptr_t game, Player* output) {
    const auto begin = Read<std::uintptr_t>(game + profile::PlayersBegin), end = Read<std::uintptr_t>(game + profile::PlayersEnd);
    if (!Pointer(begin) || end <= begin || (end - begin) % 8 || end - begin > MaxPlayers * 8 || !Readable(begin, end - begin)) return 0;
    unsigned count = 0;
    for (auto cursor = begin; cursor < end; cursor += 8) {
        const auto address = Read<std::uintptr_t>(cursor);
        if (!Alive(address, 0x2850) || Read<unsigned>(address + 0x38) != 1 || Read<unsigned>(address + 0x3c) != 0 ||
            Read<unsigned char>(address + 0x284c)) continue;
        output[count++] = {address, Read<unsigned>(address + 0x3e8), Read<int>(address + 0x1738),
            api::Function<bool (*)(const void*, unsigned, bool)>(s_Module, api::HasCollectible)(reinterpret_cast<const void*>(address), sp::Binky, false)};
    }
    return count;
}
void Dance(const Player* players, unsigned count) {
    // Remove only this module's tracked costumes. Native AddNullCostume would
    // restart an existing animation, so it runs on state changes, not each tick.
    for (auto& owned : s_Players) if (owned.address) {
        bool present = false;
        for (unsigned i = 0; i < count; ++i) if (owned.address == players[i].address && owned.seed == players[i].seed) present = true;
        if (!present) RemoveOwned(owned);
    }
    int desired_music = -1;
    for (unsigned i = 0; i < count; ++i) {
        const auto& player = players[i];
        auto* slot = &s_Players[0];
        for (auto& candidate : s_Players) if (candidate.address == player.address || !candidate.address) { slot = &candidate; if (candidate.address) break; }
        const int desired = s_Costumes[sp::CostumeFor(player.type)];
        const int binky = player.binky ? s_Costumes[sp::assets::BinkyCostume] : -1;
        if (slot->address != player.address || slot->seed != player.seed || slot->costume != desired || slot->binky != binky) {
            RemoveOwned(*slot);
            api::Function<void (*)(void*, int)>(s_Module, api::AddNullCostume)(reinterpret_cast<void*>(player.address), desired);
            if (binky >= 0) api::Function<void (*)(void*, int)>(s_Module, api::AddNullCostume)(reinterpret_cast<void*>(player.address), binky);
            *slot = {player.address, player.seed, desired, binky};
        }
        const int song = sp::MusicFor(player.type);
        if (song >= 0) desired_music = song;
    }
    if (s_OwnsMusic && CurrentMusic() != s_PlayingTrack) s_OwnsMusic = false;
    if (desired_music >= 0 && (!s_Dancing || (s_OwnsMusic && desired_music != s_ChosenMusic))) {
        const int track = s_Music[desired_music];
        if (CurrentMusic() != track) {
            api::Function<void (*)(void*, int, float)>(s_Module, api::PlayMusic)(reinterpret_cast<void*>(s_Manager + MusicOffset), track, 1.f);
            api::Function<void (*)(void*)>(s_Module, api::UpdateVolume)(reinterpret_cast<void*>(s_Manager + MusicOffset));
            s_OwnsMusic = CurrentMusic() == track;
        }
        s_PlayingTrack = track;
    }
    s_ChosenMusic = desired_music; s_Dancing = true;
}
}

bool InitializeSpecialist(std::uintptr_t module) {
    if (s_Module) return s_Module == module && s_Guarded;
    if (!module) return false;
    for (const auto& guard : api::Guards)
        if (!Readable(module + guard.offset, sizeof(guard.bytes)) ||
            std::memcmp(reinterpret_cast<const void*>(module + guard.offset), guard.bytes, sizeof(guard.bytes)) != 0) return false;
    s_Module = module; s_Guarded = true;
    return true;
}
void UpdateSpecialist(std::uintptr_t game) {
    std::uintptr_t manager{}, current_game{}, room{};
    if (!s_Guarded || !s_Module || !TryRead(s_Module + profile::ManagerGlobal, manager) || !Readable(manager, 0x36600) ||
        !TryRead(s_Module + profile::GameGlobal, current_game) || game != current_game || !Readable(game, 0x26fa90) ||
        Read<int>(manager + 8) != 2 || !TryRead(game + profile::GameRoom, room) || !Readable(room, 0x19d8)) return;
    const auto now = Read<int>(game + 0x24f99c);
    if (now < 0) return;
    if (s_Manager && manager != s_Manager) {
        // A new manager owns independent registries; old pointers are no longer
        // safe to call through. Do not remove anything from recycled storage.
        for (auto& player : s_Players) player = OwnedPlayer{};
        s_Attempted = s_Registered = s_Dancing = s_OwnsMusic = false;
        s_PlayingTrack = s_ChosenMusic = -1;
    }
    s_Manager = manager;
    if (!RegisterResources(manager)) { if (s_Dancing) Stop(room); return; }
    const auto run_seed = Read<unsigned>(game + 0x25d44);
    const auto entered = Read<unsigned>(room + 0x191c);
    // The engine reuses its Room allocation. The native entry stamp, rather
    // than the pointer alone, starts a new dance/music ownership interval.
    const bool changed = s_Game != game || s_Room != room || s_RoomEntry != entered || s_RunSeed != run_seed || now < s_LastFrame;
    if (changed) Stop(room);
    if (!changed && now == s_LastFrame) return;
    s_Game = game; s_Room = room; s_RoomEntry = entered; s_RunSeed = run_seed; s_LastFrame = now;
    Player players[MaxPlayers];
    const auto count = Players(game, players);
    if (!count || !GoodRoom(game, room)) { if (s_Dancing) Stop(room); return; }
    Dance(players, count);
}
void ResetSpecialist() {
    if (!s_Module || !s_Guarded) return;
    std::uintptr_t manager{}, game{}, room{};
    if (TryRead(s_Module + profile::ManagerGlobal, manager) && manager == s_Manager &&
        TryRead(s_Module + profile::GameGlobal, game) && Readable(game, profile::GameRoom + 8)) {
        TryRead(game + profile::GameRoom, room);
        Stop(room);
    } else {
        for (auto& player : s_Players) player = OwnedPlayer{};
        s_Dancing = s_OwnsMusic = false;
    }
    s_Game = s_Room = 0; s_RoomEntry = 0; s_LastFrame = -1;
    // Loader context was already destroyed after deep-copy registration. Its
    // allocations cannot leak or dangle if the pending native unload fails.
}
void OnSpecialistModuleUnloaded() {
    s_Module = s_Manager = s_Game = s_Room = 0;
    s_Guarded = s_Attempted = s_Registered = s_Dancing = s_OwnsMusic = false;
    s_LastFrame = s_PlayingTrack = s_ChosenMusic = -1; s_RunSeed = s_RoomEntry = 0;
    for (auto& player : s_Players) player = OwnedPlayer{};
}
}

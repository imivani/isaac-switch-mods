#include "rplus.hpp"
#include "feature_api.hpp"
#include "lib.hpp"
#include "mod_settings.hpp"
#include "generated/build_profile.hpp"
#include "generated/rplus_guards.hpp"
#include "../../../features/rplus/policy.hpp"
#include <cstring>

namespace isaac {
namespace {
using feature_memory::Readable;
using feature_memory::TryRead;
namespace rp = rplus;
namespace api = rplus::native;
struct Vec2 { float x, y; };

std::uintptr_t s_Module{}, s_InstalledModule{};
bool s_Ready{};
std::uint8_t s_RandomPrologue[20]{}, s_AwardPrologue[20]{};
std::uint8_t s_Rules{rp::AllRules};

template<std::size_t N> bool Matches(std::uintptr_t p, const std::uint8_t (&bytes)[N]) {
    return Readable(p, N) && std::memcmp(reinterpret_cast<const void*>(p), bytes, N) == 0;
}
template<class F> F Native(std::uintptr_t offset) { return reinterpret_cast<F>(s_Module + offset); }
bool Enabled() { return s_Ready && IsModEnabled(ModFeature::RepentancePlus); }
std::uintptr_t Game() { std::uintptr_t g{}; TryRead(s_Module + profile::GameGlobal, g); return g; }

// Every RNG::Random call passes through here; only the two portal rolls in
// Room::SpawnClearAward are changed. They are told apart by return address.
HOOK_DEFINE_TRAMPOLINE(PortalRoll) {
    static unsigned Callback(void* rng, unsigned range) {
        const unsigned value = Orig(rng, range);
        if (!Enabled()) return value;
        const auto from = reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)) - s_Module;
        return rp::RollResult(rp::RollFor(from, api::ChestRollReturn, api::MegaSatanRollReturn), value, true, s_Rules);
    }
};

bool ReadMotherFacts(std::uintptr_t game, std::uintptr_t room, rp::MotherFacts& facts) {
    std::uintptr_t manager{};
    int stage{}, challenge{};
    if (!TryRead(s_Module + profile::ManagerGlobal, manager) || !TryRead(game + api::GameRoomIndex, facts.roomIndex) ||
        !TryRead(room + api::RoomType, facts.roomType) || !TryRead(game + api::GameStage, stage) ||
        !TryRead(game + profile::ChallengeId, challenge)) return false;
    const auto curses = Native<unsigned (*)(const void*)>(api::GetCurses)(reinterpret_cast<const void*>(game));
    facts.awardStage = rp::AwardStage(stage, curses, challenge);
    if (!rp::IsMotherRoom(facts)) return true;
    facts.voidAvailable = Native<bool (*)(const void*, int, int)>(api::IsStageAvailable)(reinterpret_cast<const void*>(game), api::VoidStage, 0);
    int seen{};
    facts.endingSeen = TryRead(manager + api::ManagerPersistentData + api::CutsceneCounters + api::MotherCutscene * 4, seen) && seen > 0;
    return true;
}

// The portal the game itself places after Mega Satan: a trapdoor with
// vardata 1, at a free spot near the room centre, adjusted like any trapdoor.
void SpawnPortal(std::uintptr_t room) {
    auto* r = reinterpret_cast<void*>(room);
    const auto centre = Native<Vec2 (*)(const void*)>(api::GetCenterPos)(r);
    const auto free = Native<Vec2 (*)(const void*, const Vec2*, float, bool, bool)>(api::FindFreePickupSpawnPosition)(r, &centre, 80.f, true, false);
    int width{}, height{};
    if (!TryRead(room + api::RoomWidth, width) || !TryRead(room + api::RoomHeight, height)) return;
    int index = rp::GridIndex(free.x, free.y, width, height);
    Native<void (*)(void*, int*)>(api::FixTrapdoorPos)(r, &index);
    if (index < 0 || index >= width * height) return;
    const unsigned seed = Native<unsigned (*)()>(api::RandomU32)();
    Native<bool (*)(void*, int, int, unsigned, unsigned, int)>(api::SpawnGridEntity)(r, index, static_cast<int>(api::GridTrapdoor), 0, seed, 1);
}

HOOK_DEFINE_TRAMPOLINE(MotherAward) {
    static void Callback(void* room) {
        // Decide before the award: it is the same room test the game uses.
        rp::MotherFacts facts;
        const auto game = Enabled() ? Game() : 0;
        const bool add = game && ReadMotherFacts(game, reinterpret_cast<std::uintptr_t>(room), facts) && rp::AddMotherPortal(facts, true, s_Rules);
        Orig(room);
        if (add) SpawnPortal(reinterpret_cast<std::uintptr_t>(room));
    }
};
}

bool InitializeRepentancePlus(std::uintptr_t module) {
    if (s_Ready && module == s_InstalledModule && Matches(module + api::RngRandom, s_RandomPrologue) &&
        Matches(module + api::SpawnClearAward, s_AwardPrologue)) return true;
    s_Ready = false;
    s_Module = module;
#define RP_GUARD(name) Matches(module + api::name, api::name##Guard)
    if (!module || !RP_GUARD(RngRandom) || !RP_GUARD(SpawnClearAward) || !RP_GUARD(SpawnGridEntity) ||
        !RP_GUARD(GetCenterPos) || !RP_GUARD(FindFreePickupSpawnPosition) || !RP_GUARD(FixTrapdoorPos) ||
        !RP_GUARD(RandomU32) || !RP_GUARD(IsStageAvailable) || !RP_GUARD(GetCurses) ||
        !RP_GUARD(IncreaseCutsceneCounter) || !RP_GUARD(MegaSatanRoll) || !RP_GUARD(MegaSatanEnd) ||
        !RP_GUARD(ChestRoll) || !RP_GUARD(MotherRoomCheck) || !RP_GUARD(AmbushCall) || !RP_GUARD(AmbushFields)) return false;
#undef RP_GUARD
    PortalRoll::InstallAtPtr(module + api::RngRandom);
    MotherAward::InstallAtPtr(module + api::SpawnClearAward);
    std::memcpy(s_RandomPrologue, reinterpret_cast<const void*>(module + api::RngRandom), sizeof(s_RandomPrologue));
    std::memcpy(s_AwardPrologue, reinterpret_cast<const void*>(module + api::SpawnClearAward), sizeof(s_AwardPrologue));
    s_InstalledModule = module;
    s_Ready = true;
    return true;
}
void ResetRepentancePlus() {
    // A refused SDK unload leaves the hooks active; keep their verified mapping.
}
void SetRepentancePlusRules(std::uint8_t rules) { s_Rules = rules & rp::AllRules; }
void RenderBossRushWave(const FeatureFrame& frame) {
    if (!Enabled() || !frame.game || !frame.room) return;
    int roomType{}, wave{}, total{};
    std::uint8_t active{};
    const auto ambush = frame.game + api::GameAmbush;
    if (!TryRead(frame.room + api::RoomType, roomType) || !TryRead(ambush + api::AmbushActive, active) ||
        !TryRead(ambush + api::AmbushWave, wave) || !TryRead(ambush + api::AmbushWaveTotal, total)) return;
    char label[24];
    if (!rp::WaveLabel(true, s_Rules, roomType, active != 0, wave, total, label, sizeof(label))) return;
    // Under the hit counter's default top-centre line.
    const float x = frame.width * .5f - static_cast<float>(std::strlen(label)) * 3.f;
    frame.draw.Text(label, x + 1, 23, {0, 0, 0, .6f}, .8f);
    frame.draw.Text(label, x, 22, {1, 1, 1, .95f}, .8f);
}
}

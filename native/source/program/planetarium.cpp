// SPDX-License-Identifier: GPL-3.0-only
// Based on Sectimus/isaac-planetarium-chance at b724ac3641e89e0aaa8b43a301ccb168a07d1d6a.
#include "planetarium.hpp"
#include "lib.hpp"
#include "native_sprite.hpp"
#include "generated/build_profile.hpp"
#include "../../../features/planetarium/planetarium.hpp"
#include "../../../features/planetarium/identity.hpp"
#include "../../../features/planetarium/stat_layout.hpp"

#include <cstdio>

namespace isaac {
namespace {
using namespace feature_memory;
using isaac_planetarium::ChanceTracker;
using isaac_planetarium::FloorSample;

// All addresses are independently verified for the guarded Switch 1.7.9b NRO.
// See docs/native-api.md and docs/planetarium-port.md for source evidence.
constexpr std::uintptr_t GetChance = 0x3d58bc;
constexpr std::uintptr_t IsAscent = 0x3e1d98;
constexpr std::uintptr_t GetTrinket = 0x3c0c9c;
constexpr std::uintptr_t ItemIsAvailable = 0x3c101c;
constexpr std::uintptr_t IsBeastRoom = 0x452438;
constexpr std::uintptr_t ManagerItemConfig = 0x36538;
constexpr std::uintptr_t FoundHud = 0x3650a;
constexpr std::uintptr_t StatHudRender = 0x3af194;
constexpr std::uintptr_t GameStatHud = 0xe8468 + 0xc5968;
constexpr std::uintptr_t StatFont = 0xd7080;
constexpr std::uintptr_t DrawString = 0x4ceef0;
constexpr std::uintptr_t FrameCount = 0x24f99c;
constexpr std::uintptr_t ScreenShake = 0x24f9b0;
constexpr std::uintptr_t RunSeed = 0x25d44;
constexpr std::uintptr_t StageSeeds = 0x25d58;
constexpr std::uintptr_t RoomCount = 0x21510;
constexpr std::uintptr_t RoomDescriptors = 0x18;
constexpr std::uintptr_t RoomDescriptorStride = 0x100;
constexpr int TelescopeLens = 152;
constexpr int TreasureRoom = 4;

// Each of the five observed instructions only adjusts the stack/saves
// registers. The hook never relocates an unverified PC-relative instruction.
constexpr std::uint8_t StatHudPrologue[] = {
    0xff,0xc3,0x04,0xd1,0xef,0x3b,0x09,0x6d,0xed,0x33,0x0a,0x6d,
    0xeb,0x2b,0x0b,0x6d,0xe9,0x23,0x0c,0x6d
};
constexpr std::uint8_t DrawStringPrologue[] = {
    0xff,0x43,0x01,0xd1,0xe9,0x23,0x01,0x6d,0xfd,0x7b,0x02,0xa9,
    0xfd,0x83,0x00,0x91,0xf6,0x57,0x03,0xa9
};

struct FloorStamp {
    std::int32_t stage{}, stage_type{}, frame_count{};
    std::uint32_t run_seed{}, floor_seed{};
    bool ascent{};
    std::uint64_t Key() const {
        return isaac_planetarium::FloorKey(floor_seed, stage, stage_type, ascent);
    }
};

constinit ChanceTracker s_Tracker;
constinit NativeSprite s_Icon;
constinit isaac_planetarium::StatDrawPass s_StatPass;
std::uintptr_t s_Module{}, s_Game{};
std::int32_t s_LastFrame{};
bool s_IconReady{}, s_IconAttempted{};
bool s_HookReady{};
std::uint8_t s_InstalledPrologue[sizeof(StatHudPrologue)]{};

template<std::size_t N> bool Matches(std::uintptr_t address, const std::uint8_t (&bytes)[N]) {
    return (address & 0xfff) <= 0x1000 - N && Readable(address, N) &&
           std::memcmp(reinterpret_cast<const void*>(address), bytes, N) == 0;
}

bool ReadStatLayout(std::uintptr_t stat, const NativeVec2* origin,
                    std::uintptr_t& game, isaac_planetarium::NativeStatLayout& layout) {
    std::uintptr_t manager{}, first{}, second{};
    std::uint8_t found{}, combined{};
    NativeVec2 position;
    if (!s_HookReady || !TryRead(s_Module + profile::GameGlobal, game) || !game ||
        stat != game + GameStatHud ||
        !TryRead(s_Module + profile::ManagerGlobal, manager) || !manager ||
        !TryRead(manager + FoundHud, found) || !found ||
        !TryRead(stat + 0x158, first) || !TryRead(stat + 0x228, second) ||
        !TryRead(stat + 0x2f8, combined) ||
        !TryRead(reinterpret_cast<std::uintptr_t>(origin), position)) return false;
    layout = {position.x, position.y, first != 0, second != 0, combined != 0};
    return first || second;
}

HOOK_DEFINE_TRAMPOLINE(ObservePlanetariumStats) {
    static void Callback(void* stat, const NativeVec2* origin) {
        std::uintptr_t game{};
        isaac_planetarium::NativeStatLayout layout;
        const bool drawn = ReadStatLayout(reinterpret_cast<std::uintptr_t>(stat), origin, game, layout);
        Orig(stat, origin);
        // Copy values only. The origin can be a temporary on HUD::Render's
        // stack, and no game-owned object is retained by the draw-pass latch.
        if (drawn) s_StatPass.Observe(game, layout);
    }
};

bool StatFontReady(std::uintptr_t manager) {
    std::uint8_t loaded{};
    // Font::IsLoaded at 0x4ce898 is exactly LDRB W0,[X0]; RET.
    // The original stat renderer has just used this same font in this pass.
    return TryRead(manager + StatFont, loaded) && loaded != 0;
}

void DrawStatText(std::uintptr_t manager, const char* text, float x, float y, FeatureColor color) {
    // Color has a nontrivial copy constructor in the game's C++ ABI: its raw
    // call boundary receives a pointer, not four additional float arguments.
    using Draw = void (*)(const void*, const char*, float, float, const FeatureColor*, unsigned, bool);
    reinterpret_cast<Draw>(s_Module + DrawString)(reinterpret_cast<const void*>(manager + StatFont),
                                                 text, x, y, &color, 0, false);
}

bool ReadFloor(const FeatureFrame& frame, FloorStamp& stamp) {
    unsigned seed_index{};
    if (!TryRead(frame.game, stamp.stage) ||
        !TryRead(frame.game + 4, stamp.stage_type) ||
        !TryRead(frame.game + RunSeed, stamp.run_seed) ||
        !TryRead(frame.game + FrameCount, stamp.frame_count) || stamp.frame_count < 0 ||
        !isaac_planetarium::EffectiveStage(stamp.stage, stamp.stage_type, seed_index) ||
        !TryRead(frame.game + StageSeeds + seed_index * 4, stamp.floor_seed)) return false;
    // A nonempty level is required before sampling; do not freeze a value while
    // its room list is still being constructed during a stage transition.
    std::int32_t room_count{};
    if (!TryRead(frame.game + RoomCount, room_count) || room_count < 1 || room_count > 507) return false;
    stamp.ascent = reinterpret_cast<bool (*)(const void*)>(frame.module + IsAscent)(
        reinterpret_cast<const void*>(frame.game));
    return true;
}

bool TelescopeAvailable(std::uintptr_t module, std::uintptr_t manager, bool& available) {
    const auto config = manager + ManagerItemConfig;
    std::uintptr_t begin{}, end{};
    if (!TryRead(config + 0x18, begin) || !TryRead(config + 0x20, end) ||
        !begin || end < begin || end - begin > 4096 * sizeof(std::uintptr_t) ||
        (end - begin) % sizeof(std::uintptr_t) ||
        end - begin <= TelescopeLens * sizeof(std::uintptr_t) ||
        !Readable(begin, end - begin)) return false;
    const auto item = reinterpret_cast<std::uintptr_t (*)(const void*, int)>(module + GetTrinket)(
        reinterpret_cast<const void*>(config), TelescopeLens);
    if (!Readable(item, 0xf0)) return false;
    // Native ItemPool::ResetTrinkets uses this exact all-flags availability call.
    available = reinterpret_cast<bool (*)(const void*, std::uint32_t)>(module + ItemIsAvailable)(
        reinterpret_cast<const void*>(item), UINT32_MAX);
    return true;
}

bool HasTreasureRoom(std::uintptr_t game, bool& present) {
    std::int32_t count{};
    if (!TryRead(game + RoomCount, count) || count < 1 || count > 507) return false;
    present = false;
    for (std::int32_t index = 0; index < count; ++index) {
        std::uintptr_t data{};
        std::int32_t type{};
        const auto descriptor = game + RoomDescriptors + index * RoomDescriptorStride;
        if (!TryRead(descriptor + 0x10, data) || !data || !TryRead(data + 8, type)) return false;
        if (type == TreasureRoom) { present = true; return true; }
    }
    return true;
}

FloorSample Sample(const FeatureFrame& frame, const FloorStamp& stamp, bool available) {
    const float chance = reinterpret_cast<float (*)(const void*)>(frame.module + GetChance)(
        reinterpret_cast<const void*>(frame.game));
    return {chance, true, available, stamp.ascent};
}

bool Synchronize(const FeatureFrame& frame, std::uintptr_t manager, const FloorStamp& stamp) {
    const bool restart = !s_Tracker.Initialized() || s_Game != frame.game ||
                         s_Tracker.RunKey() != stamp.run_seed || stamp.frame_count < s_LastFrame;
    if (restart || s_Tracker.FloorKey() != stamp.Key() || !s_Tracker.ChanceKnown()) {
        bool available{};
        if (!TelescopeAvailable(frame.module, manager, available)) return false;
        if (restart) {
            isaac_planetarium::RunStart start;
            start.run_key = stamp.run_seed;
            start.floor_key = stamp.Key();
            start.telescope_available = available;
            // There is no verified game-start callback/persistent save bridge
            // in this build. First observation on stage 1 can inspect its
            // treasure-room eligibility; later entry uses upstream's no-save
            // Continue fallback. No historical chance is invented.
            start.continued = stamp.stage != 1 || stamp.ascent;
            const auto in_memory_eligibility = s_Tracker.Save();
            if (start.continued && s_Game == frame.game &&
                in_memory_eligibility.ValidFor(stamp.run_seed)) start.saved = &in_memory_eligibility;
            if (!start.continued && available &&
                !HasTreasureRoom(frame.game, start.initial_floor_has_treasure)) return false;
            s_Tracker.BeginRun(start, Sample(frame, stamp, available));
        } else {
            s_Tracker.ObserveFloor(stamp.Key(), Sample(frame, stamp, available));
        }
    }
    s_Game = frame.game;
    s_LastFrame = stamp.frame_count;
    return true;
}

void PrepareIcon(std::uintptr_t module) {
    if (s_IconAttempted) return;
    s_IconAttempted = true;
    s_IconReady = s_Icon.Create(module) && s_Icon.Load("gfx/ui/hudstats2.anm2") &&
                  s_Icon.SetFrame("Idle", 8);
    if (s_IconReady) s_Icon.SetTint(1.f, 1.f, 1.f, .5f);
}

bool ReadBeastRoom(const FeatureFrame& frame, bool& beast) {
    std::uintptr_t descriptor{}, data{};
    if (!TryRead(frame.room + 8, descriptor) || !descriptor ||
        !TryRead(descriptor + 0x10, data) || !Readable(data, 12)) return false;
    beast = reinterpret_cast<bool (*)(const void*)>(frame.module + IsBeastRoom)(
        reinterpret_cast<const void*>(frame.room));
    return true;
}
} // namespace

bool InitializePlanetarium(std::uintptr_t module) {
    // A failed SDK unload leaves this same hook mapped. A duplicate init must
    // not wrap our own branch. After a successful unload/reload (even at the
    // same address), the original prologue is restored and all guards run.
    if (module && module == s_Module && s_HookReady &&
        Matches(module + StatHudRender, s_InstalledPrologue) &&
        Matches(module + DrawString, DrawStringPrologue)) return true;
    s_Module = module;
    s_HookReady = false;
    s_StatPass.Reset();
    if (!module || !Matches(module + StatHudRender, StatHudPrologue) ||
        !Matches(module + DrawString, DrawStringPrologue)) return false;
    ObservePlanetariumStats::InstallAtPtr(module + StatHudRender);
    std::memcpy(s_InstalledPrologue, reinterpret_cast<const void*>(module + StatHudRender),
                sizeof(s_InstalledPrologue));
    s_HookReady = true;
    return true;
}

void BeginPlanetariumHud() {
    s_StatPass.Begin();
}

void RenderPlanetarium(const FeatureFrame& frame) {
    if (!frame.module || !frame.game || !frame.room) return;
    if (!s_HookReady || s_Module != frame.module) {
        if (frame.draw.text) frame.draw.Text("Planetarium: hook mismatch", 12, frame.height - 40,
                                             {1,.6f,.3f,1}, .65f);
        return;
    }
    isaac_planetarium::HudLayout layout;
    const bool stats_drawn = s_StatPass.Consume(frame.game, layout);
    std::uintptr_t manager{};
    FloorStamp stamp;
    if (!TryRead(frame.module + profile::ManagerGlobal, manager) || !manager ||
        !ReadFloor(frame, stamp) || !Synchronize(frame, manager, stamp)) return;

    std::uint8_t found_hud{};
    bool beast{};
    if (!TryRead(manager + FoundHud, found_hud) ||
        !ReadBeastRoom(frame, beast)) return;
    isaac_planetarium::HudVisibility visibility;
    visibility.found_hud = found_hud != 0 && stats_drawn && StatFontReady(manager);
    // General HUD visibility and NO_HUD seed handling are parent guards.
    visibility.beast_room = beast;
    // The exact Dogma animation guard is not yet verified. Hide throughout Home
    // in this build, including the transition that upstream suppresses later.
    visibility.dogma_death_past_frame_80 = stamp.stage == 13;
    if (!TryRead(frame.game + ScreenShake, layout.screen_shake_x) ||
        !TryRead(frame.game + ScreenShake + 4, layout.screen_shake_y)) return;
    const auto output = s_Tracker.Render(visibility, layout);
    if (!output.visible) return;

    PrepareIcon(frame.module);
    if (s_IconReady) s_Icon.Render({output.icon_x, output.icon_y});
    else DrawStatText(manager, "P", output.icon_x + 3.f, output.icon_y + 1.f, {1,1,1,.5f});
    char value[24];
    if (output.chance_known) std::snprintf(value, sizeof(value), "%.1f%%", output.percent);
    else std::snprintf(value, sizeof(value), "?%%");
    DrawStatText(manager, value, output.text_x, output.text_y, {1,1,1,.5f});
    if (output.difference_alpha > 0 && output.difference_percent != 0) {
        std::snprintf(value, sizeof(value), "%+.1f%%", output.difference_percent);
        const FeatureColor color = output.difference_percent > 0
            ? FeatureColor{0,1,0,output.difference_alpha} : FeatureColor{1,0,0,output.difference_alpha};
        DrawStatText(manager, value, output.difference_x, output.difference_y, color);
    }
}

void ResetPlanetarium() {
    s_Icon.Destroy();
    s_Tracker.Reset();
    s_StatPass.Reset();
    s_Game = 0;
    s_LastFrame = 0;
    s_IconReady = false;
    s_IconAttempted = false;
    // Preserve the installed-hook identity if the SDK refuses the unload.
    // A newly mapped module must still pass InitializePlanetarium's guards.
}
} // namespace isaac

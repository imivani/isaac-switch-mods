#include "timemachine.hpp"
#include "feature_api.hpp"
#include "runtime_extras.hpp"
#include "mod_settings.hpp"
#include "generated/build_profile.hpp"
#include "generated/features.hpp"
#include "generated/timemachine_guards.hpp"
#include "../../../features/timemachine/adapter.hpp"
#include "../../../features/timemachine/dispatch.hpp"
#include "lib.hpp"
#include <cstring>

namespace isaac {
namespace {
using feature_memory::Readable;
using feature_memory::TryRead;
namespace tm = timemachine;
namespace api = timemachine::native;
std::uintptr_t s_Module{}, s_InstalledModule{};
bool s_Ready{}, s_Inside{};
constinit tm::Ramp s_Ramp;
constinit tm::Lock s_Lock;
tm::BlockReason s_Reason = tm::BlockReason::Unavailable;
std::uint8_t s_InstalledPrologue[20]{}, s_InstalledInput[20]{}, s_InstalledInterpolation[20]{};

struct Engine {
    bool Readable(std::uintptr_t address, std::size_t bytes) {
        return feature_memory::Readable(address,bytes);
    }
    template<class T> bool TryRead(std::uintptr_t address, T& value) {
        return feature_memory::TryRead(address,value);
    }
    bool Paused(std::uintptr_t game) {
        return reinterpret_cast<bool (*)(void*)>(s_Module+api::IsPaused)(reinterpret_cast<void*>(game));
    }
    bool Pressed(std::uintptr_t manager, unsigned action, unsigned controller, std::uintptr_t player) {
        return reinterpret_cast<bool (*)(void*,unsigned,unsigned,void*)>(s_Module+api::IsActionPressed)(
            reinterpret_cast<void*>(manager),action,controller,reinterpret_cast<void*>(player));
    }
    bool Triggered(std::uintptr_t manager, unsigned action, unsigned controller, std::uintptr_t player) {
        return reinterpret_cast<bool (*)(void*,unsigned,unsigned,void*)>(s_Module+api::IsActionTriggered)(
            reinterpret_cast<void*>(manager),action,controller,reinterpret_cast<void*>(player));
    }
    template<class T> void Write(std::uintptr_t address, T value) {
        if (feature_memory::Readable(address,sizeof(value)))
            std::memcpy(reinterpret_cast<void*>(address),&value,sizeof(value));
    }
    tm::Direction Movement(std::uintptr_t player) {
        // Exact native getter: two-float HFA return in s0/s1. This applies the
        // game's analog/deadzone, reversed-control, axis and mirror rules.
        return reinterpret_cast<tm::Direction (*)(void*)>(s_Module+api::MovementInput)(reinterpret_cast<void*>(player));
    }
};
// Called after every native update (ordinary and accelerated), so Machine
// Lock re-pins the player before each inspection and before rendering.
tm::Snapshot Inspect(std::uintptr_t game) {
    if (!s_Ready) { s_Reason=tm::BlockReason::Unavailable; s_Lock.Reset(); return {}; }
    const bool accelerate=IsModEnabled(ModFeature::TimeMachine), lock=IsModEnabled(ModFeature::MachineLock);
    if (!accelerate && !lock) { s_Reason=tm::BlockReason::Disabled; s_Lock.Reset(); return {}; }
    if constexpr (ExtendedFeatures) if (BlockNativeGameTick(game)) return {};
    Engine engine;
    if (lock) tm::HoldLock(engine,s_Lock,s_Module,game,s_Reason);
    else s_Lock.Reset();
    const auto snapshot=tm::Inspect(engine,s_Module,game,s_Reason);
    if (lock) {
        if (snapshot.eligible) tm::EngageLock(engine,s_Lock,snapshot);
        else if (tm::LockHardStop(s_Reason)) s_Lock.Reset();
    }
    if (!accelerate) return {};
    return snapshot;
}

HOOK_DEFINE_TRAMPOLINE(TimeMachineGameInput) {
    static void Callback(void* pointer) {
        if constexpr (ExtendedFeatures) {
            if (s_Ready && BeforeNativeGameInput(reinterpret_cast<std::uintptr_t>(pointer))) {
                s_Ramp.Reset();
                return;
            }
        }
        Orig(pointer);
    }
};
HOOK_DEFINE_TRAMPOLINE(TimeMachineInterpolation) {
    static void Callback(void* pointer) {
        if constexpr (ExtendedFeatures) {
            if (s_Ready && BlockNativeGameTick(reinterpret_cast<std::uintptr_t>(pointer))) return;
        }
        Orig(pointer);
    }
};

HOOK_DEFINE_TRAMPOLINE(TimeMachineGameUpdate) {
    static void Callback(void* pointer) {
        const auto game = reinterpret_cast<std::uintptr_t>(pointer);
        if (!s_Ready || s_Inside) { Orig(pointer); return; }
        if constexpr (ExtendedFeatures) {
            if (BlockNativeGameTick(game)) { s_Ramp.Reset(); return; }
        }
        s_Inside = true;
        tm::Dispatch(s_Ramp,
            [game](unsigned& frame) { return TryRead(game+tm::Frame,frame); },
            [pointer] { Orig(pointer); },
            [game] { return Inspect(game); },
            [game](bool ordinary) { if constexpr (ExtendedFeatures) AfterNativeGameTick(game,ordinary); },
            [] { if constexpr (ExtendedFeatures) CancelNativeGameTick(); });
        s_Inside = false;
    }
};
template<std::size_t N> bool Matches(std::uintptr_t p, const std::uint8_t (&bytes)[N]) {
    return Readable(p, N) && std::memcmp(reinterpret_cast<const void*>(p), bytes, N) == 0;
}
}

bool InitializeTimeMachine(std::uintptr_t module) {
    s_Ramp.Reset();
    s_Lock.Reset();
    if (s_Ready && module==s_InstalledModule && Matches(module+api::GameUpdate,s_InstalledPrologue) &&
        Matches(module+api::ProcessInput,s_InstalledInput) &&
        Matches(module+api::InterpolatePositions,s_InstalledInterpolation)) return true;
    s_Ready = false;
    s_Reason = tm::BlockReason::Unavailable;
    s_Module = module;
    if (!module || !Matches(module + api::GameUpdate, api::GameUpdateGuard) ||
        !Matches(module + api::ProcessInput, api::ProcessInputGuard) ||
        !Matches(module + api::InterpolatePositions, api::InterpolatePositionsGuard) ||
        !Matches(module + api::FireplaceAi, api::FireplaceAiGuard) ||
        !Matches(module + api::IsPaused, api::IsPausedGuard) ||
        !Matches(module + api::IsActionPressed, api::IsActionPressedGuard) ||
        !Matches(module + api::IsActionTriggered, api::IsActionTriggeredGuard) ||
        !Matches(module + api::MovementInput, api::MovementInputGuard)) return false;
    TimeMachineGameInput::InstallAtPtr(module+api::ProcessInput);
    TimeMachineInterpolation::InstallAtPtr(module+api::InterpolatePositions);
    TimeMachineGameUpdate::InstallAtPtr(module+api::GameUpdate);
    std::memcpy(s_InstalledInput,reinterpret_cast<const void*>(module+api::ProcessInput),sizeof(s_InstalledInput));
    std::memcpy(s_InstalledInterpolation,reinterpret_cast<const void*>(module+api::InterpolatePositions),sizeof(s_InstalledInterpolation));
    std::memcpy(s_InstalledPrologue, reinterpret_cast<const void*>(module + api::GameUpdate), sizeof(s_InstalledPrologue));
    s_InstalledModule = module;
    s_Inside = false;
    s_Ready = true;
    s_Reason = tm::BlockReason::NotInRun;
    return true;
}
const char* TimeMachineStatus() {
    return tm::ReasonText(IsModEnabled(ModFeature::TimeMachine) ? s_Reason : tm::BlockReason::Disabled);
}
unsigned TimeMachineSpeedTwentieths() { return s_Ramp.SpeedTwentieths(); }
bool MachineLockActive() { return s_Lock.active; }
void SetTimeMachineMaxSteps(unsigned steps) { s_Ramp.SetLimit(steps); }
void ResetTimeMachine() {
    s_Ramp.Reset();
    s_Lock.Reset();
    s_Reason = s_Ready ? tm::BlockReason::NotInRun : tm::BlockReason::Unavailable;
    // Preserve the live hook when the SDK refuses an unload. A successful
    // module reload has original bytes and is installed anew by Initialize.
}
}

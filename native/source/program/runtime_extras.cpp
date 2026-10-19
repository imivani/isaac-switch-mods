#include "runtime_extras.hpp"
#include "debug_kit.hpp"
#include "instant_break.hpp"
#include "specialist.hpp"
#include "mod_settings.hpp"
#include "hit_counter.hpp"
namespace isaac {
namespace { std::uintptr_t s_InputGame{}; bool s_InputBlocked{}; }
bool BeforeNativeGameInput(std::uintptr_t game) {
    s_InputGame=game; s_InputBlocked=ProcessDebugKitInput(game); return s_InputBlocked;
}
bool BlockNativeGameTick(std::uintptr_t game) {
    return (s_InputGame==game && s_InputBlocked) || IsDebugMenuOpen();
}
void AfterNativeGameTick(std::uintptr_t game, bool ordinaryTick) {
    if (IsModEnabled(ModFeature::InstantBreak)) UpdateInstantBreak(game);
    if (ordinaryTick) {
        if (IsModEnabled(ModFeature::Specialist)) UpdateSpecialist(game);
        UpdateDebugKit(game);
        UpdateGreedMachine(game);
    }
}
void CancelNativeGameTick() { CancelDebugKitHold(); }
}

#pragma once
#include <cstdint>
namespace isaac {
enum class ModFeature : unsigned {
    Eid, HitCounter, BossBars, Planetarium, Regret, TimeMachine, MachineLock, GreedNoJam,
    InstantBreak, Specialist, AnimatedItems, Birthright, CleanMap, Count
};
inline constexpr unsigned ModCount = static_cast<unsigned>(ModFeature::Count);
bool IsModEnabled(ModFeature feature);
bool IsModAvailable(ModFeature feature);
void SetModAvailable(ModFeature feature, bool available);
void ToggleMod(ModFeature feature);
const char* ModName(ModFeature feature);
const char* ModDescription(ModFeature feature);
// True when Y on the Mods page opens a settings page for this feature.
bool ModHasSettings(ModFeature feature);
void ResetModSettings();
// All enabled flags as a bitmask (bit = ModFeature), for saving and restoring.
std::uint32_t ModEnabledMask();
void SetModEnabledMask(std::uint32_t mask);
}

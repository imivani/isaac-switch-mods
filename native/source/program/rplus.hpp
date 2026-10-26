#pragma once
#include "feature_api.hpp"
#include <cstdint>
namespace isaac {
// Repentance+ Rules: certain Void portals after ???, The Lamb and Mega Satan,
// one after Mother once her ending has been seen, and a Boss Rush wave counter.
bool InitializeRepentancePlus(std::uintptr_t module);
void ResetRepentancePlus();
// Which rules are on (features/rplus/policy.hpp Rule bits), from the menu.
void SetRepentancePlusRules(std::uint8_t rules);
void RenderBossRushWave(const FeatureFrame& frame);
}

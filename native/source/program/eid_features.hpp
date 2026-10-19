#pragma once
#include "feature_api.hpp"

namespace isaac {
// The caller has already checked the exact game module, gameplay state and
// ordinary HUD visibility. Entity pointers are a snapshot of the current room.
void RenderEidFeatures(const FeatureFrame&, const std::uintptr_t* entities, unsigned count);
// Call before the game module unloads. UI and special-feature resources are
// reset here too; repeated resets are harmless.
void ResetEidFeatures();
// Menu options: nearby descriptions and the hold-Map browser.
void SetEidOptions(bool nearby, bool map);
}

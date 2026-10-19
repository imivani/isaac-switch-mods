#pragma once
#include "feature_api.hpp"

namespace isaac {
bool InitializeRegret(std::uintptr_t module);
void RenderRegret(const FeatureFrame& frame);
// Invoke before unloading the game module while sprite destructors are valid.
void ResetRegret();
}

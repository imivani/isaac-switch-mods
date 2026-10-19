// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "feature_api.hpp"

namespace isaac {
// Called once for each build-verified NRO mapping, before the HUD hook runs.
bool InitializePlanetarium(std::uintptr_t module);
// Call immediately before the original HUD render; this also handles pauses.
void BeginPlanetariumHud();
void RenderPlanetarium(const FeatureFrame& frame);
// Call while the matching game module is still mapped; this frees its sprite.
void ResetPlanetarium();
}

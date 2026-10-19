#pragma once
#include "feature_api.hpp"

namespace isaac {
// Call only after the exact Repentance module guard, before its first frame.
bool InitializeItemVisuals(std::uintptr_t module);
// Owned artwork must be destroyed while the original module is still mapped.
void ResetItemVisuals();
// Normal artwork is drawn with the original entities; this only reports errors.
void RenderItemVisualsStatus(const FeatureFrame& frame);
}

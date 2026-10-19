#pragma once
#include <cstdint>
#include "feature_api.hpp"
namespace isaac {
bool InitializeDebugKit(std::uintptr_t module);
void UpdateDebugKit(std::uintptr_t game);
void CancelDebugKitHold();
void ResetDebugKit();
void RenderDebugKit(const FeatureFrame& frame);
bool IsDebugMenuOpen();
// Runs before the game's input handler, swallowing menu buttons and the
// closing button until it is released. True freezes that simulation pass.
bool ProcessDebugKitInput(std::uintptr_t game);
}

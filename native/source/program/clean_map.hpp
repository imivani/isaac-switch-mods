#pragma once
#include <cstdint>

namespace isaac {
// Verify only the exact minimap draw sites. This installs no additional hook
// and owns no game memory or graphics.
bool InitializeCleanMap(std::uintptr_t module);
// Call first in the shared ANM2::Render callback, before item-specific gates.
// caller must be that callback's __builtin_return_address(0), not the address
// from inside this helper. The hook's B/BR entry preserves the native link.
bool ShouldHideCleanMapFrame(std::uintptr_t sprite, std::uintptr_t caller);
// No resources need releasing. Keep verified identity across a failed unload;
// a later InitializeCleanMap validates the newly mapped module again.
void ResetCleanMap();
}

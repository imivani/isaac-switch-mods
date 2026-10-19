#pragma once
#include <cstdint>
namespace isaac {
// Initializes only the exact-build guard. Resources append lazily once the
// game's existing music and costume registries are ready.
bool InitializeSpecialist(std::uintptr_t module);
// Call after the ordinary game update, never for accelerated replay passes.
void UpdateSpecialist(std::uintptr_t game);
// Remove our costumes/restore our music before an attempted module unload.
// Keeps registration bookkeeping usable if that unload fails.
void ResetSpecialist();
// Call only after the native module was successfully unloaded. No native calls.
void OnSpecialistModuleUnloaded();
}

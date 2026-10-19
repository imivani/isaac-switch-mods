#include "clean_map.hpp"
#include "feature_api.hpp"
#include "mod_settings.hpp"
#include "generated/build_profile.hpp"
#include "../../../features/clean_map/generated/native.hpp"
#include <cstring>

namespace isaac {
namespace {
namespace api = clean_map::native;
std::uintptr_t s_Module{};
bool s_Ready{};

template<std::size_t N>
bool Matches(std::uintptr_t address, const unsigned char (&bytes)[N]) {
    return feature_memory::Readable(address, N) &&
           std::memcmp(reinterpret_cast<const void*>(address), bytes, N) == 0;
}
}

bool InitializeCleanMap(std::uintptr_t module) {
    s_Module = module;
    s_Ready = module && module <= UINTPTR_MAX - profile::GameGlobal &&
        Matches(module + api::MinimapRender, api::MinimapRenderGuard) &&
        Matches(module + api::MinimapFrameSetup, api::MinimapFrameSetupGuard);
    return s_Ready;
}

bool ShouldHideCleanMapFrame(std::uintptr_t sprite, std::uintptr_t caller) {
    // Nearly all sprite draws exit before a settings read or memory query.
    if (!s_Ready || caller != s_Module + api::FrameReturn || !IsModEnabled(ModFeature::CleanMap)) return false;
    std::uintptr_t game{};
    if (!feature_memory::TryRead(s_Module + profile::GameGlobal, game) || game <= 0x10000 ||
        (game & 7) || game > UINTPTR_MAX - api::GameMinimapSprite) return false;
    return sprite == game + api::GameMinimapSprite && feature_memory::Readable(sprite, 0x158);
}

void ResetCleanMap() {
    // Predicate only: no textures, state changes, or borrowed allocations.
}
}

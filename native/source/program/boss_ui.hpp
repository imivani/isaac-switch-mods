#pragma once

#include "feature_api.hpp"
#include <cstdint>

namespace isaac {

struct BossUiStatus {
    bool attempted{};
    bool nativeSpritesReady{};
    std::uint32_t assetFailures{};
    std::uint32_t renderedBars{};
    std::uint32_t fallbackBars{};
    std::uint32_t omittedBosses{};
};

// Call after the normal HUD, inside the existing verified HUD-visible gate.
// Uses the upstream default bottom/segments layout, portraits and boss styles.
void RenderBossUi(const FeatureFrame& frame);

// Call on the game thread BEFORE unloading the Repentance NRO. ANM2 objects
// contain self-pointers and are always kept in fixed, non-movable storage.
void ResetBossUi();

// Log assetFailures only when it changes. Runtime readiness covers ANM2 data;
// texture-file closure and converted-image hashes must also pass packaging.
BossUiStatus GetBossUiStatus();

} // namespace isaac

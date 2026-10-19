// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>

namespace isaac_planetarium {

// The level generator uses the NEXT stage's saved seed on the alternate path.
// Bounds are checked before a native adapter can index the saved-seed table.
inline bool EffectiveStage(std::int32_t stage, std::int32_t stage_type, unsigned& index) {
    if (stage < 1 || stage > 14 || stage_type < 0 || stage_type > 255) return false;
    const unsigned result = static_cast<unsigned>(stage) +
                            ((stage_type & ~1) == 4 ? 1u : 0u);
    if (result > 14) return false;
    index = result;
    return true;
}

// Keep the complete tuple: the same stage can be regenerated with a new seed,
// and the ascent visits stage numbers that were already seen on the way down.
inline std::uint64_t FloorKey(std::uint32_t seed, std::int32_t stage,
                              std::int32_t stage_type, bool ascent) {
    return static_cast<std::uint64_t>(seed) |
           (static_cast<std::uint64_t>(stage) << 32) |
           (static_cast<std::uint64_t>(stage_type) << 40) |
           (static_cast<std::uint64_t>(ascent) << 48);
}

} // namespace isaac_planetarium

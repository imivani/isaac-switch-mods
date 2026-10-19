#pragma once
#include "feature_api.hpp"
#include "../../../eid/context.hpp"

namespace isaac {
// Caller must first enforce normal pickup visibility/knowledge restrictions.
unsigned ReadEidFlipItem(const FeatureFrame& frame, std::uintptr_t entity);
// True gives this selected pickup's Map preview priority over the reminder.
bool AugmentEidSpecial(const FeatureFrame& frame, std::uintptr_t entity,
                       std::uintptr_t player, const isaac_eid::ContextObject& object,
                       const isaac_eid::ContextState& state,
                       isaac_eid::ContextDescription& output);
void ResetEidSpecial();
} // namespace isaac

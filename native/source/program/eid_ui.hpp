#pragma once
#include "feature_api.hpp"
#include "catalogue.hpp"
#include "../../../eid/context.hpp"

namespace isaac {
// Call only after the existing selection/hidden-item policy accepts a pickup.
void RenderEidDescription(const FeatureFrame& frame, isaac_eid::Category category,
                          std::uint32_t id, const isaac_eid::Description& description);
// Room, pocket and state-aware text uses the same original EID font/layout.
// Text is copied into layout cells; mutable caller buffers are safe to reuse.
void RenderEidContext(const FeatureFrame& frame, std::uintptr_t selection,
                      const isaac_eid::ContextDescription& description);
// Owned game sprites must be released while their game module remains mapped.
void ResetEidUi();
// The picker and title Mods menu share the same outlined EID glyphs. These
// functions need a valid module/canvas, but never dereference a run or room.
void DrawEidMenuText(const FeatureFrame& frame, const char* text, float x, float y,
                     std::uint32_t color = 0xffffff, float maxWidth = 10000.f);
void DrawEidMenuDescription(const FeatureFrame& frame, unsigned item,
                            const char* name, const char* markup, float x, float y,
                            float width, float height, std::uint64_t milliseconds);
// Mod menu on paper: dark ink text, body-only descriptions and named icons.
void SetEidInk(bool ink);
float EidMenuTextWidth(const char* text);
// page >= 0 shows that page of the text; -1 turns pages every 5 seconds.
void DrawEidMenuBody(const FeatureFrame& frame, const char* markup, float x, float y,
                     float width, float height, std::uint64_t milliseconds, int page = -1);
// Lines the text takes at this width, from the same layout DrawEidMenuBody uses.
unsigned EidMenuBodyLines(const char* markup, float width, std::uint64_t milliseconds);
void DrawEidIcon(const FeatureFrame& frame, const char* token, float x, float y);
}

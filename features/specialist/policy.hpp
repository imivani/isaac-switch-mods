#pragma once
#include "generated/assets.hpp"
#include <cstdint>

namespace isaac::specialist {
constexpr unsigned CharacterCount = 41;
constexpr unsigned Binky = 438;
inline bool GoodItem(unsigned id) {
    // Original default custom list, followed by the exact 1.7.9b quality table.
    if (id == 4 || id == 12 || id == 118 || id == 182) return true;
    for (unsigned value : assets::QualityFour) if (value == id) return true;
    return false;
}
inline unsigned CostumeFor(int player_type) {
    // Upstream ReduceFlashing defaults true: Tainted Eden uses regular Eden.
    if (player_type == 30) player_type = 9;
    if (player_type < 0 || player_type >= static_cast<int>(CharacterCount)) player_type = 0;
    return assets::CharacterCostumes[player_type];
}
inline int MusicFor(int player_type) {
    // Secondary bodies do not choose the room's music in the original mod.
    if (player_type == 17 || player_type == 20 || player_type == 40) return -1;
    if (player_type == 23) return 2;
    if (player_type == 30) return 3;
    return player_type >= 21 && player_type <= 40 ? 1 : 0;
}
inline bool AltPathVisible(unsigned room_type, int stage_type, unsigned item_count, float x, float y) {
    if (room_type != 4 || stage_type < 4 || item_count == 0) return true;
    if (item_count == 2) return x <= 320.f && y <= 280.f;
    if (item_count > 2) return x <= 320.f && !(x == 320.f && y == 280.f);
    return false;
}
inline bool Coordinate(float value) { return value == value && value >= -100000.f && value <= 100000.f; }
inline bool ScanRadius(float x, float y) {
    const float dx = x - 640.f, dy = y - 580.f;
    return Coordinate(x) && Coordinate(y) && dx * dx + dy * dy <= 875.f * 875.f;
}
}

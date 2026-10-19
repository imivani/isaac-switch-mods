// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "planetarium.hpp"

namespace isaac_planetarium {

struct NativeStatLayout {
    float origin_x = 0, origin_y = 0;
    bool first_player = false, second_player = false;
    bool combined_deal_chance = false;
};

// A draw-pass latch, rather than a game-frame latch: paused frames and hidden
// HUD transitions can reuse a frame number while the stat column is absent.
class StatDrawPass {
public:
    void Begin() { open_ = true; observed_ = false; game_ = 0; }
    void Reset() { *this = StatDrawPass{}; }

    bool Observe(std::uintptr_t game, const NativeStatLayout& layout) {
        if (!open_ || observed_ || !game || (!layout.first_player && !layout.second_player) ||
            !Finite(layout.origin_x) || !Finite(layout.origin_y)) return false;
        // StatHUD::Render draws 8 icons, or 7 with the combined deal icon.
        // Jacob/Esau's two populated stat entries use 14 instead of 12 pixels.
        const float rows = layout.combined_deal_chance ? 7.f : 8.f;
        const float spacing = layout.first_player && layout.second_player ? 14.f : 12.f;
        x_ = layout.origin_x;
        y_ = layout.origin_y + rows * spacing;
        if (!Finite(y_)) return false;
        game_ = game;
        observed_ = true;
        return true;
    }

    bool Consume(std::uintptr_t game, HudLayout& layout) {
        const bool valid = open_ && observed_ && game_ == game;
        open_ = false;
        observed_ = false;
        if (!valid) return false;
        layout.native_stat_row = true;
        layout.native_stat_x = x_;
        layout.native_stat_y = y_;
        return true;
    }

private:
    bool open_ = false, observed_ = false;
    std::uintptr_t game_ = 0;
    float x_ = 0, y_ = 0;
};

} // namespace isaac_planetarium

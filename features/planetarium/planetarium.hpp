// SPDX-License-Identifier: GPL-3.0-only
// Native port of Sectimus/isaac-planetarium-chance, pinned at
// b724ac3641e89e0aaa8b43a301ccb168a07d1d6a. See NOTICE.md and LICENSE.
#pragma once
#include <cstddef>
#include <cstdint>

namespace isaac_planetarium {

struct FloorSample {
    // Obtained from the ORIGINAL game's Level::GetPlanetariumChance. The game
    // remains responsible for item, skip-count and stage formula details.
    float native_chance = 0;
    bool valid = false;
    bool telescope_available = false;
    bool ascent = false;
};

struct SavedEligibility {
    std::uint32_t magic = 0x504c4e54; // PLNT
    std::uint32_t version = 1;
    std::uint64_t run_key = 0;
    std::uint32_t available = 0;
    std::uint32_t can_spawn = 0;

    bool ValidFor(std::uint64_t key) const {
        return magic == 0x504c4e54 && version == 1 && run_key == key && available <= 1 && can_spawn <= 1;
    }
};

struct RunStart {
    std::uint64_t run_key = 0;
    std::uint64_t floor_key = 0;
    bool continued = false;
    bool telescope_available = false;
    bool initial_floor_has_treasure = false;
    const SavedEligibility* saved = nullptr;
};

struct HudVisibility {
    bool found_hud = true;
    bool hud_visible = true;
    bool no_hud_seed = false;
    bool beast_room = false;
    bool dogma_death_past_frame_80 = false;
    bool other_planetarium_provider = false;
};

struct HudLayout {
    // Flags are accumulated over the complete player list, like upstream.
    bool bomb_counter = true;
    bool poop_counter = false;
    bool red_heart_counter = false;
    bool soul_heart_counter = false;
    bool player_one_is_jacob = false;
    bool true_coop = false;
    bool duality = false;
    bool hard_greed_or_achievements_disabled = false;
    bool repentance_plus = false;
    float hud_offset = 0;
    float screen_shake_x = 0, screen_shake_y = 0;
    // The Switch adapter observes the position of the actual stat column.
    // That position already includes every native counter and HUD offset.
    bool native_stat_row = false;
    float native_stat_x = 0, native_stat_y = 0;
};

struct HudOutput {
    bool visible = false;
    bool chance_known = false;
    float percent = 0;
    float icon_x = 0, icon_y = 0;
    float text_x = 0, text_y = 0;
    float difference_percent = 0;
    float difference_x = 0, difference_y = 0;
    float difference_alpha = 0;
};

inline bool Finite(float value) { return value >= -3.402823466e38F && value <= 3.402823466e38F; }

inline float DifferenceSlide(float alpha) {
    const float frame = (2.9f - alpha) / 0.02f - 14.f;
    return frame > 0 ? 0.f : -(15.1f / (13.f * 13.f)) * frame * frame;
}

inline void ComputeCoordinates(const HudLayout& layout, float& x, float& y) {
    if (layout.native_stat_row && Finite(layout.native_stat_x) && Finite(layout.native_stat_y)) {
        x = layout.native_stat_x;
        y = layout.native_stat_y;
        return;
    }
    x = 0;
    y = layout.repentance_plus ? 170.f : 168.f;
    const int shift_count = int(layout.bomb_counter) + int(layout.poop_counter) +
                            int(layout.red_heart_counter) + int(layout.soul_heart_counter) - 1;
    if (shift_count > 0) y += 11.f * shift_count - 2.f;
    if (layout.player_one_is_jacob) y += 30.f;
    else if (layout.true_coop) {
        y += 16.f;
        if (layout.duality) y -= 2.f;
    }
    if (layout.duality) y -= 12.f;
    if (layout.hard_greed_or_achievements_disabled) y += 16.f;
    if (Finite(layout.hud_offset)) {
        x += layout.hud_offset * 20.f;
        y += layout.hud_offset * 12.f;
    }
}

class ChanceTracker {
public:
    void Reset() { *this = ChanceTracker{}; }

    void BeginRun(const RunStart& start, const FloorSample& sample) {
        Reset();
        initialized_ = true;
        run_key_ = start.run_key;
        floor_key_ = start.floor_key;
        available_ = start.telescope_available;
        if (start.continued) {
            // Upstream persists only these two eligibility values. With no
            // saved data its continuation fallback is Telescope availability.
            can_spawn_ = available_;
            if (start.saved && start.saved->ValidFor(run_key_)) {
                available_ = start.saved->available != 0;
                can_spawn_ = start.saved->can_spawn != 0;
            }
        } else {
            can_spawn_ = available_ && start.initial_floor_has_treasure;
        }
        UpdateChance(sample);
        difference_alpha_ = 0; // No popup on a new or continued run.
        previous_known_ = false;
    }

    // Call with the current floor identity. Item pickups on an already sampled
    // floor cannot change its frozen displayed chance. A missing sample may be
    // retried when native state becomes readable.
    void ObserveFloor(std::uint64_t key, const FloorSample& sample) {
        if (!initialized_) return;
        if (key == floor_key_ && chance_known_) return;
        floor_key_ = key;
        UpdateChance(sample);
    }

    SavedEligibility Save() const {
        SavedEligibility saved;
        saved.run_key = run_key_;
        saved.available = available_ ? 1u : 0u;
        saved.can_spawn = can_spawn_ ? 1u : 0u;
        return saved;
    }

    bool Initialized() const { return initialized_; }
    bool ChanceKnown() const { return chance_known_; }
    float Percent() const { return chance_; }
    std::uint64_t RunKey() const { return run_key_; }
    std::uint64_t FloorKey() const { return floor_key_; }

    HudOutput Render(const HudVisibility& visibility, const HudLayout& layout) {
        HudOutput out;
        dogma_ended_ = dogma_ended_ || visibility.dogma_death_past_frame_80;
        out.visible = initialized_ && visibility.found_hud && visibility.hud_visible &&
                      !visibility.no_hud_seed && !visibility.beast_room && !dogma_ended_ &&
                      !visibility.other_planetarium_provider;
        if (!out.visible) return out;
        out.chance_known = chance_known_;
        out.percent = chance_;
        ComputeCoordinates(layout, out.icon_x, out.icon_y);
        const float shake_x = Finite(layout.screen_shake_x) ? layout.screen_shake_x : 0;
        const float shake_y = Finite(layout.screen_shake_y) ? layout.screen_shake_y : 0;
        out.text_x = out.icon_x + shake_x + 16.f;
        out.text_y = out.icon_y + shake_y + 1.f;
        if (chance_known_ && previous_known_ && difference_alpha_ > 0) {
            out.difference_percent = chance_ - previous_;
            out.difference_alpha = difference_alpha_ > .5f ? .5f : difference_alpha_;
            out.difference_x = out.icon_x + shake_x + 46.f + DifferenceSlide(difference_alpha_);
            out.difference_y = out.text_y;
            difference_alpha_ -= .01f;
            if (difference_alpha_ < 0) difference_alpha_ = 0;
        }
        return out;
    }

private:
    void UpdateChance(const FloorSample& sample) {
        previous_ = chance_;
        previous_known_ = chance_known_;
        if (!sample.valid || !Finite(sample.native_chance)) {
            chance_known_ = false;
            difference_alpha_ = 0;
            return;
        }
        float fraction = sample.native_chance;
        if (fraction < 0) fraction = 0;
        if (fraction > 1) fraction = 1;
        // Native unlocks can happen mid-run. Upstream checks them when this
        // floor-update callback next runs, without changing the current layout.
        if (!available_ && sample.telescope_available) {
            available_ = true;
            can_spawn_ = true;
        }
        chance_ = sample.ascent || !can_spawn_ ? 0.f : fraction * 100.f;
        chance_known_ = true;
        if (previous_known_ && chance_ != previous_) difference_alpha_ = 2.9f;
    }

    std::uint64_t run_key_ = 0, floor_key_ = 0;
    bool initialized_ = false, available_ = false, can_spawn_ = false;
    bool chance_known_ = false, previous_known_ = false, dogma_ended_ = false;
    float chance_ = 0, previous_ = 0, difference_alpha_ = 0;
};

} // namespace isaac_planetarium

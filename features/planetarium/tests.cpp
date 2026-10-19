// SPDX-License-Identifier: GPL-3.0-only
#include "planetarium.hpp"
#include "identity.hpp"
#include "stat_layout.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace isaac_planetarium;

static bool Near(float a, float b) { return std::fabs(a - b) < .0001f; }
static FloorSample Sample(float chance, bool available = true, bool ascent = false) {
    return {chance, true, available, ascent};
}
static RunStart Start(bool continued = false) {
    RunStart start;
    start.run_key = 12345;
    start.floor_key = 111;
    start.continued = continued;
    start.telescope_available = true;
    start.initial_floor_has_treasure = true;
    return start;
}

int main() {
    unsigned effective{};
    assert(EffectiveStage(2, 0, effective) && effective == 2);
    assert(EffectiveStage(2, 4, effective) && effective == 3);
    assert(EffectiveStage(2, 5, effective) && effective == 3);
    assert(EffectiveStage(14, 0, effective) && effective == 14);
    assert(!EffectiveStage(14, 4, effective)); // Native saved-seed table ends at 14.
    assert(!EffectiveStage(0, 0, effective) && !EffectiveStage(-1, 0, effective));
    assert(!EffectiveStage(1, -1, effective) && !EffectiveStage(1, 256, effective));
    assert(FloorKey(700, 2, 0, false) != FloorKey(701, 2, 0, false)); // Regenerated floor.
    assert(FloorKey(700, 2, 0, false) != FloorKey(700, 2, 0, true)); // Ascent.
    assert(FloorKey(700, 2, 0, false) != FloorKey(700, 2, 4, false)); // Alternate path.
    assert(FloorKey(0, 1, 0, false) != FloorKey(UINT32_MAX, 1, 0, false));

    ChanceTracker tracker;
    assert(!tracker.Render({}, {}).visible);
    tracker.BeginRun(Start(), Sample(.01f));
    assert(tracker.ChanceKnown() && Near(tracker.Percent(), 1));
    auto hud = tracker.Render({}, {});
    assert(hud.visible && hud.chance_known && hud.difference_alpha == 0);

    // The original game supplies the item/skip/stage formula. Item changes on
    // an already-built floor MUST NOT make the displayed generation chance jump.
    tracker.ObserveFloor(111, Sample(.25f)); // Native getter changed after item gain.
    assert(Near(tracker.Percent(), 1));
    tracker.ObserveFloor(112, Sample(.25f)); // Next floor captures the new value.
    assert(Near(tracker.Percent(), 25));
    hud = tracker.Render({}, {});
    assert(Near(hud.difference_percent, 24) && Near(hud.difference_alpha, .5f));

    // Synthetic native outputs exercise all output branches without claiming
    // that a second reimplementation of the game formula was verified.
    const float native_floor_results[] = {.21f, .41f, .61f, 1.f, .1f, 0.f};
    for (unsigned i = 0; i < 6; ++i) {
        tracker.ObserveFloor(113 + i, Sample(native_floor_results[i]));
        assert(Near(tracker.Percent(), native_floor_results[i] * 100.f));
    }
    tracker.ObserveFloor(120, Sample(2.f));
    assert(Near(tracker.Percent(), 100));
    tracker.ObserveFloor(121, Sample(-1.f));
    assert(Near(tracker.Percent(), 0));
    tracker.ObserveFloor(122, Sample(.9f, true, true));
    assert(Near(tracker.Percent(), 0)); // Ascent cannot spawn a new planetarium.

    // An unavailable Telescope Lens means unlock/mode eligibility failed.
    auto locked_start = Start();
    locked_start.telescope_available = false;
    tracker.BeginRun(locked_start, Sample(.21f, false));
    assert(Near(tracker.Percent(), 0));
    tracker.ObserveFloor(112, Sample(.21f, true)); // Unlocked during this run.
    assert(Near(tracker.Percent(), 21));
    assert(tracker.Save().available == 1 && tracker.Save().can_spawn == 1);

    auto challenge_start = Start();
    challenge_start.initial_floor_has_treasure = false;
    tracker.BeginRun(challenge_start, Sample(.41f));
    assert(Near(tracker.Percent(), 0));
    const auto saved_challenge = tracker.Save();
    auto continued_challenge = Start(true);
    continued_challenge.saved = &saved_challenge;
    tracker.BeginRun(continued_challenge, Sample(.81f));
    assert(Near(tracker.Percent(), 0));
    assert(tracker.Render({}, {}).difference_alpha == 0);

    // Continue without saved mod data follows upstream's availability fallback.
    tracker.BeginRun(Start(true), Sample(.16f));
    assert(Near(tracker.Percent(), 16));
    auto wrong_run = saved_challenge;
    wrong_run.run_key = 999;
    continued_challenge.saved = &wrong_run;
    tracker.BeginRun(continued_challenge, Sample(.16f));
    assert(Near(tracker.Percent(), 16));
    auto invalid_save = saved_challenge;
    invalid_save.can_spawn = 99;
    continued_challenge.saved = &invalid_save;
    tracker.BeginRun(continued_challenge, Sample(.16f));
    assert(Near(tracker.Percent(), 16));

    tracker.BeginRun(Start(), FloorSample{});
    assert(!tracker.ChanceKnown());
    tracker.ObserveFloor(111, Sample(.07f));
    assert(tracker.ChanceKnown() && Near(tracker.Percent(), 7));
    tracker.ObserveFloor(112, Sample(std::numeric_limits<float>::quiet_NaN()));
    assert(!tracker.ChanceKnown());
    tracker.ObserveFloor(112, Sample(std::numeric_limits<float>::infinity()));
    assert(!tracker.ChanceKnown());
    tracker.ObserveFloor(112, Sample(.2f));
    assert(tracker.ChanceKnown() && Near(tracker.Percent(), 20));

    HudVisibility hidden;
    hidden.found_hud = false;
    assert(!tracker.Render(hidden, {}).visible);
    hidden = {}; hidden.hud_visible = false;
    assert(!tracker.Render(hidden, {}).visible);
    hidden = {}; hidden.no_hud_seed = true;
    assert(!tracker.Render(hidden, {}).visible);
    hidden = {}; hidden.beast_room = true;
    assert(!tracker.Render(hidden, {}).visible);
    hidden = {}; hidden.other_planetarium_provider = true;
    assert(!tracker.Render(hidden, {}).visible);
    hidden = {}; hidden.dogma_death_past_frame_80 = true;
    assert(!tracker.Render(hidden, {}).visible);
    assert(!tracker.Render({}, {}).visible); // Dogma suppression is latched.
    tracker.BeginRun(Start(), Sample(.01f));
    assert(tracker.Render({}, {}).visible);

    tracker.ObserveFloor(112, Sample(.21f));
    for (int frame = 0; frame < 300; ++frame) tracker.Render({}, {});
    assert(tracker.Render({}, {}).difference_alpha == 0);
    tracker.Reset();
    assert(!tracker.Initialized() && !tracker.Render({}, {}).visible);

    float x, y;
    ComputeCoordinates({}, x, y);
    assert(Near(x, 0) && Near(y, 168));
    HudLayout layout;
    layout.hud_offset = .5f;
    layout.hard_greed_or_achievements_disabled = true;
    ComputeCoordinates(layout, x, y);
    assert(Near(x, 10) && Near(y, 190));
    layout = {}; layout.player_one_is_jacob = true;
    ComputeCoordinates(layout, x, y);
    assert(Near(y, 198));
    layout = {}; layout.true_coop = true; layout.duality = true;
    ComputeCoordinates(layout, x, y);
    assert(Near(y, 170));
    layout = {}; layout.poop_counter = true; layout.red_heart_counter = true; layout.soul_heart_counter = true;
    ComputeCoordinates(layout, x, y);
    assert(Near(y, 199));
    layout = {}; layout.bomb_counter = false; layout.poop_counter = true;
    ComputeCoordinates(layout, x, y);
    assert(Near(y, 168));
    assert(DifferenceSlide(2.9f) < 0 && DifferenceSlide(.5f) == 0);

    // Native column observations carry the game's fully adjusted origin. The
    // extra row follows its real row count/spacing instead of reapplying an
    // incomplete reconstruction of the counter/character/options offsets.
    StatDrawPass pass;
    const NativeStatLayout ordinary{0, 72, true, false, false};
    layout = {};
    assert(!pass.Observe(123, ordinary)); // No parent HUD draw has started.
    pass.Begin();
    assert(pass.Observe(123, ordinary));
    assert(pass.Consume(123, layout));
    ComputeCoordinates(layout, x, y);
    assert(Near(x, 0) && Near(y, 168));
    assert(!pass.Consume(123, layout)); // No duplicate row in the same draw.
    assert(!pass.Observe(123, ordinary)); // Drawing is already finished.

    pass.Begin();
    assert(pass.Observe(123, {10, 94, true, false, false}));
    layout = {};
    layout.hud_offset = .5f;
    layout.hard_greed_or_achievements_disabled = true;
    layout.poop_counter = layout.red_heart_counter = true;
    assert(pass.Consume(123, layout));
    ComputeCoordinates(layout, x, y);
    assert(Near(x, 10) && Near(y, 190)); // All shifts were already in the origin.

    pass.Begin();
    assert(pass.Observe(123, {0, 86, true, true, false}));
    layout = {};
    assert(pass.Consume(123, layout));
    ComputeCoordinates(layout, x, y);
    assert(Near(y, 198)); // The game's two-player stat rows are 14 pixels tall.
    pass.Begin();
    assert(pass.Observe(123, {0, 86, true, true, true}));
    assert(pass.Consume(123, layout));
    ComputeCoordinates(layout, x, y);
    assert(Near(y, 184)); // Combined deal icon takes one actual native row.
    pass.Begin();
    assert(pass.Observe(123, {0, 72, true, false, true}));
    assert(pass.Consume(123, layout));
    ComputeCoordinates(layout, x, y);
    assert(Near(y, 156));

    pass.Begin();
    assert(pass.Observe(123, ordinary));
    pass.Begin(); // Next render may reuse the game frame while paused/hidden.
    assert(!pass.Consume(123, layout)); // No stale row if stats were skipped.
    pass.Begin();
    assert(pass.Observe(123, ordinary));
    assert(!pass.Consume(456, layout)); // A different game cannot use this row.
    pass.Begin();
    assert(!pass.Observe(123, {0, 72, false, false, false}));
    assert(!pass.Observe(0, ordinary));
    assert(!pass.Observe(123, {0, std::numeric_limits<float>::quiet_NaN(), true, false, false}));
    assert(!pass.Observe(123, {std::numeric_limits<float>::infinity(), 72, true, false, false}));
    assert(!pass.Consume(123, layout));
    pass.Begin();
    assert(pass.Observe(123, ordinary));
    pass.Reset(); // Unload attempt destroys draw state even if it later fails.
    assert(!pass.Consume(123, layout));
    pass.Begin();
    assert(pass.Observe(123, ordinary));
    assert(pass.Consume(123, layout)); // Failed-unload continuation can recover.

    // Exact upstream chance text uses +16,+1; change text uses +46,+1. Shake
    // is added once to text and never to the native icon position.
    tracker.BeginRun(Start(), Sample(.01f));
    tracker.ObserveFloor(112, Sample(.21f));
    layout.screen_shake_x = 2;
    layout.screen_shake_y = -3;
    hud = tracker.Render({}, layout);
    assert(Near(hud.icon_x, 0) && Near(hud.icon_y, 168));
    assert(Near(hud.text_x, 18) && Near(hud.text_y, 166));
    assert(Near(hud.difference_x, 48 + DifferenceSlide(2.9f)) && Near(hud.difference_y, 166));

    std::puts("PASS: Planetarium floor identity, cache, native-result handling, eligibility, continue, HUD lifecycle, native stat layout and draw-pass isolation");
}

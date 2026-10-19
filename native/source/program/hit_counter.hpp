#pragma once
#include <cstdint>
#include "feature_api.hpp"
namespace isaac {
// Hit Counter and Greed Machine Never Jams (iteration 5).
bool InitializeHitCounter(std::uintptr_t module);
bool InitializeGreedMachine(std::uintptr_t module);
void ResetHitCounter();
void ResetGreedMachine();
void RenderHitCounter(const FeatureFrame& frame);
// Ordinary game ticks only: clears a jam left earlier in the run.
void UpdateGreedMachine(std::uintptr_t game);

// Settings come from the mod menu (saved preferences); x/y of -1 = default.
void SetHitCounterLayout(int x, int y, unsigned size, unsigned opacity, bool floor);
void HitCounterBox(float& width, float& height);
void HitCounterCounts(unsigned& total, unsigned& floor);
// Draws the counter at its configured place, also outside a run (title menu).
void DrawHitCounterPreview(const FeatureFrame& frame, float alpha, bool outline);
}

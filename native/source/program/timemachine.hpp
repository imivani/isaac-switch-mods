#pragma once
#include <cstdint>
namespace isaac {
bool InitializeTimeMachine(std::uintptr_t module);
void ResetTimeMachine();
// Plain-language reason from the most recent gameplay inspection. Opening the
// picker preserves this value so its settings page can explain normal speed.
const char* TimeMachineStatus();
unsigned TimeMachineSpeedTwentieths();
// True while Machine Lock is holding a player against a machine or beggar.
bool MachineLockActive();
// Top speed from the menu: 1..5 extra game steps per frame (2x..6x).
void SetTimeMachineMaxSteps(unsigned steps);
}

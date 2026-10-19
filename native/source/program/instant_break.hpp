#pragma once
#include <cstdint>
namespace isaac {
bool InitializeInstantBreak(std::uintptr_t module);
void UpdateInstantBreak(std::uintptr_t game);
void ResetInstantBreakRoom();
void ResetInstantBreak();
}

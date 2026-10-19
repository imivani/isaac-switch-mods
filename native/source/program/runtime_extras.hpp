#pragma once
#include <cstdint>
namespace isaac {
bool BeforeNativeGameInput(std::uintptr_t game);
bool BlockNativeGameTick(std::uintptr_t game);
void AfterNativeGameTick(std::uintptr_t game, bool ordinaryTick);
void CancelNativeGameTick();
}

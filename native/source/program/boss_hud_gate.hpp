#pragma once
#include <cstdint>

namespace isaac {
// Guards the original HUD's final boss-bar section. This owns no hook/assets.
bool InitializeBossHudGate(std::uintptr_t module);

// Keep this token on the HUD callback's stack, across exactly one Orig(hud).
// The health bits and position flag are restored before enhanced drawing.
struct BossHudRestore {
    std::uintptr_t hud{};
    std::uint32_t health_bits{};
    std::uint8_t position{};
    bool active{};
};
BossHudRestore BeginBossHud(void* hud);
void EndBossHud(BossHudRestore& restore);
}

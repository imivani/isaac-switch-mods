#pragma once
#include <cstdint>
#include "feature_api.hpp"

// The paper mod menu (features/menu_ui) on the Switch: native host, renderer,
// saved settings and the pause-screen entry point.
namespace isaac {
enum class MenuOrigin : unsigned { Shortcut = 0, Pause = 1, Title = 2 };
// Checks every native function it uses; installs the pause-screen hooks.
// Returns false if the menu's core functions fail their guards.
bool InitializeModMenu(std::uintptr_t module);
// Release game-owned sprites before the game module unloads.
void ResetModMenu();
bool IsModMenuOpen();
bool IsModMenuOpenFrom(MenuOrigin origin);
// player may be 0 (title screen, or "use player 1").
void OpenModMenu(MenuOrigin origin, unsigned controller, std::uintptr_t player);
// Polls the opener's controller, runs the menu and saves settings once it has
// fully closed. Returns true while the menu is still visible.
bool UpdateModMenu(std::uintptr_t input);
void AbortModMenu();
void RenderModMenu(const FeatureFrame& frame);
// Loads saved settings once the game's save data is mounted, and keeps the
// features in step with them. Cheap; call every frame.
void PollModMenuPreferences();
// 0 = shortcut turned off.
unsigned ModMenuShortcutMilliseconds();
}

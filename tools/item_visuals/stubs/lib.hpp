// Host-only hook plumbing. Actual item_visuals.cpp callbacks are exercised;
// originals are bounded fixtures and InstallAtPtr never patches executable code.
#pragma once
#include <cstdint>
template<unsigned Tag> struct ItemVisualsTestHook {
    inline static std::uintptr_t original{};
    inline static unsigned installs{};
    static void InstallAtPtr(std::uintptr_t) { ++installs; }
    template<class... Args> static void Orig(Args... args) {
        if (original) reinterpret_cast<void (*)(Args...)>(original)(args...);
    }
};
#define HOOK_DEFINE_TRAMPOLINE(name) struct name : ItemVisualsTestHook<__COUNTER__>

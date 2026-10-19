#pragma once
#include <cstdint>

namespace isaac::debug_kit {
inline constexpr std::uint64_t HoldMilliseconds = 2000;
inline constexpr std::uint64_t CooldownMilliseconds = 10000;
inline constexpr unsigned ItemIds[] = {118, 38, 182, 313, 223, 356};
constexpr bool ValidController(unsigned id) { return (id >= 1 && id <= 8) || id == 33; }
struct Context {
    std::uintptr_t game{}, room{}, player{};
    unsigned runSeed{}, roomFrame{}, playerSeed{}, controller{};
    bool operator==(const Context&) const = default;
};
// This state machine also runs in the host tests. A changed room, player,
// controller or run cancels a partly held chord. All three buttons must be
// released before rearming; interrupted/paused holds cannot carry over.
class Chord {
    Context context_{};
    std::uint64_t started_{}, cooldownUntil_{}, hold_{HoldMilliseconds};
    bool tracking_{}, latched_{};
public:
    // Hold time from the menu settings (0.5 / 1 / 2 s); survives Reset.
    void SetHold(std::uint64_t milliseconds) { hold_ = milliseconds ? milliseconds : HoldMilliseconds; }
    void Cancel() { tracking_ = false; latched_ = true; }
    void Reset() { const auto hold = hold_; *this = {}; hold_ = hold; }
    bool Step(Context context, bool eligible, bool left, bool right, bool down, std::uint64_t now) {
        if (!(context == context_)) { context_ = context; tracking_ = false; latched_ = left || right || down; }
        if (!left && !right && !down) { tracking_ = false; latched_ = false; return false; }
        if (!eligible || !ValidController(context.controller)) { Cancel(); return false; }
        if (!left || !right || !down) { tracking_ = false; return false; }
        if (latched_ || now < cooldownUntil_) return false;
        if (!tracking_ || now < started_) { started_ = now; tracking_ = true; return false; }
        if (now - started_ < hold_) return false;
        latched_ = true;
        tracking_ = false;
        cooldownUntil_ = now + CooldownMilliseconds;
        return true;
    }
    unsigned Progress(std::uint64_t now) const {
        return tracking_ && now >= started_ ? static_cast<unsigned>((now - started_) * 100 / hold_) : 0;
    }
};
constexpr unsigned SpawnSeed(unsigned run, unsigned frame, unsigned player, unsigned item) {
    unsigned seed = run ^ (frame * 0x9e3779b9u) ^ (player * 0x85ebca6bu) ^ (item * 0xc2b2ae35u);
    seed ^= seed >> 16; seed *= 0x7feb352du; seed ^= seed >> 15;
    return seed ? seed : 1;
}
}

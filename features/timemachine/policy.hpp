#pragma once
#include <cstdint>

namespace isaac::timemachine {
// Native adaptation of TimeMachine's continuous-contact acceleration. Integer
// twentieths reproduce a 0.05 ramp without accumulating floating-point error.
inline constexpr unsigned MaxExtraSteps = 5;
inline constexpr unsigned RampDenominator = 20;
constexpr bool ValidController(unsigned id) { return (id >= 1 && id <= 8) || id == 33; }

constexpr bool Enabled(unsigned variant) {
    switch (variant) {
    case 1: case 2: case 3: case 4: case 5: case 6: case 7:
    case 9: case 11: case 13: case 15: case 16: case 17: case 18:
        return true;
    default: return false;
    }
}
constexpr bool CostsHealth(unsigned variant) {
    return variant == 2 || variant == 5 || variant == 15 || variant == 17;
}
constexpr bool Hazard(unsigned type) {
    // Entity::IsEnemy is exactly type-10 < 990 on this build. Shopkeepers
    // (17) are stationary decorations. Keep all other NPCs, including all
    // fireplaces and machine-spawned flies, at ordinary speed.
    return type == 4 || type == 7 || type == 9 || (type >= 10 && type < 1000 && type != 17);
}
constexpr bool InputAllowsAcceleration(unsigned pressed, unsigned triggered) {
    // Native input edges are not consumed by Game::Update. Replaying a fresh
    // direction press can manufacture a double tap (e.g. Mars).
    return (pressed & ~15u) == 0 && (triggered & 15u) == 0;
}
constexpr bool CanAfford(unsigned variant, int coins, int keys, int bombs,
                         int red, int soul, int bone, int eternal) {
    if (!Enabled(variant)) return false;
    // Leave the final heart at normal speed. This also handles the Womb blood
    // machine cost and zero-health characters without faking invincibility.
    if (CostsHealth(variant)) return red + soul + bone + eternal > 2;
    if (variant == 7) return keys > 0;
    if (variant == 9) return bombs > 0;
    return coins >= (variant == 16 ? 5 : 1);
}
inline bool Contact(float px, float py, float pr, float sx, float sy, float sr) {
    // Comparisons also reject NaN/infinity. A small tolerance accommodates the
    // native collision separation; contact alone never activates acceleration.
    if (!(px > -100000 && px < 100000 && py > -100000 && py < 100000 &&
          sx > -100000 && sx < 100000 && sy > -100000 && sy < 100000 &&
          pr > 0 && pr <= 100 && sr > 0 && sr <= 100)) return false;
    const float dx = sx - px, dy = sy - py, radius = pr + sr + 2;
    return dx * dx + dy * dy <= radius * radius;
}
inline bool Toward(float px, float py, float sx, float sy, float dx, float dy) {
    return (dx != 0 || dy != 0) && (sx - px) * dx + (sy - py) * dy > 0;
}
enum class ContactDecision { None, Accelerate, NormalSpeedOnly };
constexpr ContactDecision DecideContact(bool touching, bool toward, unsigned variant,
                                         bool affordable, bool finishing) {
    if (!touching || !Enabled(variant)) return ContactDecision::None;
    // Idle co-op partners can still donate on contact; check them before the
    // movement test, not only the player asking for acceleration.
    if (CostsHealth(variant) && !affordable) return ContactDecision::NormalSpeedOnly;
    if (!toward || (!affordable && !finishing)) return ContactDecision::None;
    return ContactDecision::Accelerate;
}

struct Identity {
    std::uintptr_t game{}, room{}, player{}, machine{};
    std::uint32_t runSeed{}, roomFrame{}, playerSeed{}, machineSeed{};
    int roomIndex{}, dimension{}, stage{}, stageType{};
    bool operator==(const Identity& o) const {
        return game == o.game && room == o.room && player == o.player && machine == o.machine &&
            runSeed == o.runSeed && roomFrame == o.roomFrame && playerSeed == o.playerSeed &&
            machineSeed == o.machineSeed && roomIndex == o.roomIndex && dimension == o.dimension &&
            stage == o.stage && stageType == o.stageType;
    }
};
struct Snapshot {
    Identity identity{};
    std::uint32_t frame{};
    bool eligible{};
};
class Ramp {
    Identity identity_{};
    unsigned speed_{}, remainder_{}, limit_{MaxExtraSteps};
    bool active_{};
public:
    void Reset() { identity_ = {}; speed_ = remainder_ = 0; active_ = false; }
    // Top speed in extra steps per frame (1..MaxExtraSteps); survives Reset.
    void SetLimit(unsigned steps) {
        limit_ = steps < 1 ? 1 : steps > MaxExtraSteps ? MaxExtraSteps : steps;
        if (speed_ > limit_ * RampDenominator) speed_ = limit_ * RampDenominator;
    }
    unsigned Limit() const { return limit_; }
    unsigned Begin(const Snapshot& state) {
        if (!state.eligible) { Reset(); return 0; }
        if (!active_ || !(identity_ == state.identity)) {
            Reset(); identity_ = state.identity; active_ = true;
        }
        if (speed_ < limit_ * RampDenominator) ++speed_;
        remainder_ += speed_;
        const unsigned extra = remainder_ / RampDenominator;
        remainder_ %= RampDenominator;
        return extra;
    }
    bool Accept(const Snapshot& state) const {
        return active_ && state.eligible && identity_ == state.identity;
    }
    unsigned SpeedTwentieths() const { return speed_; }
};

// The same scheduler is used by the console hook and host tests. A fresh
// snapshot is mandatory before EACH extra tick; no entity pointer is followed
// across a call into the game. All timers advance through its original update.
template<class Inspect, class Step>
unsigned Accelerate(Ramp& ramp, Inspect inspect, Step step) {
    const auto initial = inspect();
    const unsigned wanted = ramp.Begin(initial);
    std::uint32_t frame = initial.frame;
    unsigned completed = 0;
    for (unsigned i = 0; i < wanted; ++i) {
        const auto before = inspect();
        if (!ramp.Accept(before) || before.frame != frame) { ramp.Reset(); break; }
        step();
        ++completed;
        const auto after = inspect();
        if (!ramp.Accept(after) || after.frame != frame + 1) { ramp.Reset(); break; }
        frame = after.frame;
    }
    return completed;
}
}

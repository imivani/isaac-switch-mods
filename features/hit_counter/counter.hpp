#pragma once
#include <cstdint>

// Portable Hit Counter logic shared by the Switch hook and host tests.
namespace isaac::hit_counter {
// DAMAGE_NO_PENALTIES | DAMAGE_RED_HEARTS: Entity_Player::TakeDamage skips its
// own penalty counters for these (blood donations, IV Bag, self-damage items).
inline constexpr std::uint64_t SelfDamageFlags = 0x10000020;
inline constexpr std::uint64_t FakeDamageFlag = 0x200000;
// A seeded restart keeps the seed but starts again from frame zero.
inline constexpr std::uint32_t RestartFrames = 60;
inline constexpr unsigned MaxCount = 9999;

// landed: TakeDamage's result. Holy Mantle (and the similar shields) refuses
// the damage, returning false, but starts the player's invincibility timer.
// That still counts as being hit; self-inflicted and fake damage do not.
constexpr bool CountsAsHit(bool landed, int invincibleBefore, int invincibleAfter, std::uint64_t flags) {
    if (flags & (SelfDamageFlags | FakeDamageFlag)) return false;
    return landed || (invincibleBefore <= 0 && invincibleAfter > 0);
}

struct RunStamp {
    std::uint32_t seed{}, frame{};
    int stage{}, stageType{};
};
class Counter {
    RunStamp last_{};
    unsigned total_{}, floor_{};
    bool valid_{};
public:
    void Reset() { *this = {}; }
    // Called with fresh game data before every read or hit. A different run
    // seed, or the same seed back at its first second, starts a new count.
    // Exiting to the menu and continuing the same run keeps it.
    void Sync(const RunStamp& now) {
        const bool restarted = valid_ && now.seed == last_.seed &&
            now.frame < last_.frame && now.frame < RestartFrames;
        if (!valid_ || now.seed != last_.seed || restarted) { total_ = floor_ = 0; valid_ = true; }
        else if (now.stage != last_.stage || now.stageType != last_.stageType) floor_ = 0;
        last_ = now;
    }
    void Hit(const RunStamp& now) {
        Sync(now);
        if (total_ < MaxCount) ++total_;
        if (floor_ < MaxCount) ++floor_;
    }
    bool Valid() const { return valid_; }
    unsigned Total() const { return total_; }
    unsigned Floor() const { return floor_; }
};

struct Settings {
    // Negative position means "not placed yet": use the default for the screen.
    float x{-1}, y{-1};
    unsigned size{100}, opacity{85};
    bool floor{true};
};
inline constexpr unsigned MinSize = 50, MaxSize = 300, SizeStep = 10;
inline constexpr unsigned MinOpacity = 10, MaxOpacity = 100, OpacityStep = 10;
// Base scale of the HUD counter font at 100%.
inline constexpr float BaseScale = 1.0f;

struct Placement { float x, y; };
inline Placement Place(const Settings& s, float width, float height, float textWidth, float textHeight) {
    // Default: top center, which the stock HUD leaves empty.
    float x = s.x < 0 ? width * 0.5f - textWidth * 0.5f : s.x;
    float y = s.y < 0 ? 4.f : s.y;
    const float maxX = width - textWidth > 0 ? width - textWidth : 0;
    const float maxY = height - textHeight > 0 ? height - textHeight : 0;
    if (!(x >= 0)) x = 0;
    if (x > maxX) x = maxX;
    if (!(y >= 0)) y = 0;
    if (y > maxY) y = maxY;
    return {x, y};
}
inline float Scale(const Settings& s) { return BaseScale * static_cast<float>(s.size) / 100.f; }
inline float Alpha(const Settings& s) { return static_cast<float>(s.opacity) / 100.f; }
}

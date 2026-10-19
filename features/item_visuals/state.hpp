#pragma once
#include "generated.hpp"
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <cstring>

namespace isaac_item_visuals {
constexpr std::size_t AnimationCount = sizeof(data::Animations) / sizeof(data::Animations[0]);
constexpr std::size_t CharacterCount = sizeof(data::Birthrights) / sizeof(data::Birthrights[0]);
constexpr unsigned Birthright = 619;
constexpr std::size_t NoAnimation = AnimationCount;

constexpr std::size_t AnimationIndex(unsigned item) {
    for (std::size_t i = 0; i < AnimationCount; ++i)
        if (data::Animations[i].item == item) return i;
    return NoAnimation;
}
constexpr bool EligibleItem(unsigned item) { return item == Birthright || AnimationIndex(item) != NoAnimation; }

struct Pose { float y{}, scale_x{1.f}, scale_y{1.f}; };

// Exact source-default step curve, keyed by the original pedestal's frame.
constexpr Pose PedestalPose(int frame, bool shop) {
    if (shop) return {10.f, 1.f, 1.f};
    if (frame <= 2) return {-3.f, 1.f, 1.f};
    if (frame <= 4) return {-4.f, 1.f, 1.04f};
    if (frame <= 6) return {-5.f, 1.08f, .98f};
    if (frame <= 8) return {-6.f, 1.08f, .98f};
    if (frame <= 11) return {-7.f, 1.f, 1.f};
    if (frame <= 13) return {-6.f, 1.f, 1.04f};
    if (frame <= 15) return {-5.f, 1.f, 1.f};
    if (frame <= 17) return {-4.f, 1.08f, .98f};
    return {-3.f, 1.08f, .98f};
}
constexpr Pose HeldPose(unsigned age) {
    return {age <= 1 ? -25.f : age >= 36 ? -12.f : -16.f, 1.f, 1.f};
}
constexpr unsigned AnimationFrame(unsigned age, unsigned frames) { return frames ? age % frames : 0; }
inline bool Coordinate(float value) { return std::isfinite(value) && value > -100000.f && value < 100000.f; }

struct Player { float x{}, y{}; unsigned type{}; bool valid{}; };
inline int NearestPlayer(float x, float y, const Player* players, std::size_t count) {
    if (!Coordinate(x) || !Coordinate(y) || !players || count > 8) return -1;
    int chosen = -1;
    double distance = 0;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& player = players[i];
        if (!player.valid || player.type >= CharacterCount || !Coordinate(player.x) || !Coordinate(player.y)) continue;
        const double dx = static_cast<double>(player.x) - x, dy = static_cast<double>(player.y) - y;
        const double squared = dx * dx + dy * dy;
        if (chosen < 0 || squared < distance) { chosen = static_cast<int>(i); distance = squared; }
    }
    return chosen;
}

inline bool KnownSheet(const char* path) {
    if (!path || !path[0]) return false;
    // The native layer supplies a fully resolved path. Both PNG logical names
    // and PCX physical names are accepted; missing/hidden art always falls back.
    constexpr char hidden[] = "questionmark.";
    constexpr char blank[] = "nil.";
    char lower[512]{};
    std::size_t size = 0;
    for (; size < sizeof(lower) - 1 && path[size]; ++size) {
        const auto c = path[size];
        lower[size] = c >= 'A' && c <= 'Z' ? static_cast<char>(c + ('a' - 'A')) : c;
    }
    if (size == sizeof(lower) - 1 && path[size]) return false;
    return !std::strstr(lower, hidden) && !std::strstr(lower, blank);
}

struct Identity {
    std::uintptr_t entity{};
    std::uint32_t init_seed{};
    std::int32_t born{};
    unsigned item{};
    bool held{};
    bool Same(const Identity& other) const {
        return entity == other.entity && init_seed == other.init_seed && born == other.born &&
               item == other.item && held == other.held;
    }
};

// No game objects are owned or dereferenced by this cache. A fresh subtype,
// reused allocation, new room/run, or time rewind restarts its animation.
template<std::size_t Capacity> class Timeline {
    static_assert(Capacity > 0);
    struct Entry { Identity key{}; std::int32_t started{}, seen{}; bool used{}; };
    Entry entries_[Capacity]{};
public:
    void Reset() { for (auto& entry : entries_) entry = {}; }
    unsigned Observe(const Identity& key, std::int32_t now) {
        if (!key.entity || !key.item || now < 0) return 0;
        std::size_t replacement = 0;
        bool free_found = false;
        for (std::size_t i = 0; i < Capacity; ++i) {
            auto& entry = entries_[i];
            if (entry.used && entry.key.entity == key.entity && entry.key.held == key.held) {
                // A hidden/removed item that reappears starts a new effect,
                // as upstream does, rather than advancing an unseen effect.
                if (!entry.key.Same(key) || now < entry.seen || now - entry.seen > 2) entry.started = now;
                entry.key = key;
                entry.seen = now;
                return static_cast<unsigned>(now - entry.started);
            }
            if (!entry.used && !free_found) { replacement = i; free_found = true; }
            else if (!free_found && entry.seen < entries_[replacement].seen) replacement = i;
        }
        entries_[replacement] = {key, now, now, true};
        return 0;
    }
};
}

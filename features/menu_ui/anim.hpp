#pragma once
#include <cstdint>

// Small, dependency-free animation helpers (no libm). All time is in
// milliseconds from the game's own clock, so motion continues while paused.
namespace isaac::menu_ui::anim {
inline constexpr float Pi = 3.14159265f;

inline float Clamp01(float t) { return t < 0 ? 0 : t > 1 ? 1 : t; }
inline float Progress(std::uint64_t now, std::uint64_t start, unsigned duration) {
    if (!duration || now <= start) return now >= start && !duration ? 1.f : 0.f;
    return Clamp01(static_cast<float>(now - start) / static_cast<float>(duration));
}
inline float EaseOutCubic(float t) { t = Clamp01(t); const float u = 1 - t; return 1 - u * u * u; }
inline float EaseInCubic(float t) { t = Clamp01(t); return t * t * t; }
// Slight overshoot for paper landing and item "pop".
inline float EaseOutBack(float t) {
    t = Clamp01(t);
    constexpr float c1 = 1.70158f, c3 = c1 + 1;
    const float u = t - 1;
    return 1 + c3 * u * u * u + c1 * u * u;
}
// Sine for gentle bobbing; |error| < 0.002 over the full turn.
inline float Sin(float x) {
    constexpr float tau = 2 * Pi;
    x -= static_cast<float>(static_cast<long>(x / tau)) * tau;
    if (x < -Pi) x += tau;
    if (x > Pi) x -= tau;
    const float y = 4 / Pi * x - 4 / (Pi * Pi) * x * (x < 0 ? -x : x);
    return 0.225f * (y * (y < 0 ? -y : y) - y) + y;
}
// Periodic wave in [-1, 1] with the given period.
inline float Wave(std::uint64_t now, unsigned period) {
    if (!period) return 0;
    return Sin(static_cast<float>(now % period) / static_cast<float>(period) * 2 * Pi);
}
// Frame-rate independent approach toward a target (half-life in ms).
inline float Approach(float current, float target, std::uint64_t elapsed, float halfLife) {
    if (halfLife <= 0 || elapsed > 1000) return target;
    // keep = 0.5^(elapsed / halfLife), via (1 - a/16)^16 ~= e^-a.
    const float a = 0.693147f * static_cast<float>(elapsed) / halfLife;
    if (a >= 16) return target;
    float keep = 1 - a / 16;
    for (int i = 0; i < 4; ++i) keep *= keep;
    const float next = target + (current - target) * keep;
    return (next - target) * (next - target) < 0.0004f ? target : next;
}
}

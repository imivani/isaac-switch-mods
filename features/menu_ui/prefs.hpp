#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>

// Saved mod-menu preferences. Plain data plus a small, tolerant text format
// ("key=value" lines) so a damaged or older file can never break the menu.
namespace isaac::menu_ui {
enum class Category : std::uint8_t { Collectible, Trinket, Card, Pill, Pickup, Count };
inline constexpr unsigned CategoryCount = static_cast<unsigned>(Category::Count);
struct Entry {
    std::uint8_t category{};
    std::uint16_t id{}, extra{};  // pickups: id = variant, extra = subtype
    bool operator==(const Entry& o) const { return category == o.category && id == o.id && extra == o.extra; }
};
inline constexpr unsigned MaxFavorites = 64, MaxRecents = 10;
// 2: Repentance+ Rules (mod 13) and its rule switches. Mods a version-1 file
// did not know about start switched on.
inline constexpr unsigned PrefsVersion = 2;
inline constexpr std::uint32_t Version1Mods = 0x1fff;

struct Prefs {
    std::uint32_t mods = 0xffffffffu;   // enabled mods, bit = ModFeature
    bool hasMods{};                     // the file recorded a mod selection
    std::int16_t hitX = -1, hitY = -1;  // -1 = default (top centre)
    std::uint16_t hitSize = 100;
    std::uint8_t hitOpacity = 85;
    bool hitFloor = true;
    std::uint8_t timeMachineSteps = 5;  // extra steps per frame, 1..5 (2x..6x)
    bool eidNearby = true, eidMap = true;
    std::uint8_t rplus = 0xf;           // Repentance+ rules on (features/rplus/policy.hpp bits)
    bool sounds = true, reduceMotion = false, pausePrompt = true;
    std::uint8_t shortcut = 3;          // 0 off, 1 = 0.5 s, 2 = 1 s, 3 = 2 s
    std::uint8_t openTab = 0;           // 0 = last used, otherwise tab + 1
    std::uint8_t dim = 60;              // background darkness, percent
    std::uint8_t lastTab = 0;
    Entry favorites[MaxFavorites]{};
    unsigned favoriteCount{};
    Entry recents[MaxRecents]{};
    unsigned recentCount{};

    unsigned ShortcutMilliseconds() const {
        constexpr unsigned values[] = {0, 500, 1000, 2000};
        return values[shortcut < 4 ? shortcut : 3];
    }
    void Clamp() {
        if (hitSize < 50) hitSize = 50;
        if (hitSize > 300) hitSize = 300;
        hitSize = static_cast<std::uint16_t>((hitSize + 5) / 10 * 10);
        if (hitOpacity < 10) hitOpacity = 10;
        if (hitOpacity > 100) hitOpacity = 100;
        hitOpacity = static_cast<std::uint8_t>((hitOpacity + 5) / 10 * 10);
        if (hitX < -1) hitX = -1;
        if (hitY < -1) hitY = -1;
        if (hitX > 4096) hitX = 4096;
        if (hitY > 2160) hitY = 2160;
        if (timeMachineSteps < 1) timeMachineSteps = 1;
        if (timeMachineSteps > 5) timeMachineSteps = 5;
        if (shortcut > 3) shortcut = 3;
        if (openTab > 5) openTab = 0;
        if (lastTab > 4) lastTab = 0;
        if (dim < 20) dim = 20;
        if (dim > 90) dim = 90;
        dim = static_cast<std::uint8_t>((dim + 5) / 10 * 10);
        if (favoriteCount > MaxFavorites) favoriteCount = MaxFavorites;
        if (recentCount > MaxRecents) recentCount = MaxRecents;
    }
    bool IsFavorite(const Entry& e) const {
        for (unsigned i = 0; i < favoriteCount; ++i) if (favorites[i] == e) return true;
        return false;
    }
    // Returns the new state. A full list keeps its oldest entries.
    bool ToggleFavorite(const Entry& e) {
        for (unsigned i = 0; i < favoriteCount; ++i) if (favorites[i] == e) {
            for (unsigned j = i + 1; j < favoriteCount; ++j) favorites[j - 1] = favorites[j];
            --favoriteCount;
            return false;
        }
        if (favoriteCount >= MaxFavorites) return false;
        favorites[favoriteCount++] = e;
        return true;
    }
    void AddRecent(const Entry& e) {
        unsigned keep = recentCount;
        for (unsigned i = 0; i < recentCount; ++i) if (recents[i] == e) { keep = i; break; }
        if (keep == recentCount && recentCount < MaxRecents) ++recentCount;
        if (keep >= recentCount) keep = recentCount - 1;
        for (unsigned i = keep; i > 0; --i) recents[i] = recents[i - 1];
        recents[0] = e;
    }
};

namespace prefs_detail {
inline bool Number(const char*& p, const char* end, long& out) {
    bool negative = false, digits = false;
    long value = 0;
    if (p < end && *p == '-') { negative = true; ++p; }
    while (p < end && *p >= '0' && *p <= '9') {
        digits = true;
        if (value < 100000000) value = value * 10 + (*p - '0');
        ++p;
    }
    out = negative ? -value : value;
    return digits;
}
}

// Writes "isaacmods 1" then key=value lines. Returns bytes written (0 = no room).
inline unsigned Serialize(const Prefs& p, char* out, unsigned capacity) {
    unsigned used = 0;
    auto put = [&](const char* format, auto... args) {
        if (used >= capacity) return;
        const int n = std::snprintf(out + used, capacity - used, format, args...);
        if (n < 0 || static_cast<unsigned>(n) >= capacity - used) { used = capacity; return; }
        used += static_cast<unsigned>(n);
    };
    put("isaacmods %u\n", PrefsVersion);
    put("mods=%lu\n", static_cast<unsigned long>(p.mods));
    put("hit=%d,%d,%u,%u,%u\n", p.hitX, p.hitY, p.hitSize, p.hitOpacity, p.hitFloor ? 1u : 0u);
    put("timemachine=%u\n", p.timeMachineSteps);
    put("eid=%u,%u\n", p.eidNearby ? 1u : 0u, p.eidMap ? 1u : 0u);
    put("rplus=%u\n", p.rplus);
    put("menu=%u,%u,%u,%u,%u,%u,%u\n", p.sounds ? 1u : 0u, p.reduceMotion ? 1u : 0u, p.pausePrompt ? 1u : 0u,
        p.shortcut, p.openTab, p.dim, p.lastTab);
    put("favorites=");
    for (unsigned i = 0; i < p.favoriteCount; ++i)
        put("%s%u:%u:%u", i ? "," : "", p.favorites[i].category, p.favorites[i].id, p.favorites[i].extra);
    put("\nrecents=");
    for (unsigned i = 0; i < p.recentCount; ++i)
        put("%s%u:%u:%u", i ? "," : "", p.recents[i].category, p.recents[i].id, p.recents[i].extra);
    put("\n");
    return used >= capacity ? 0 : used;
}

// Unknown keys and malformed values are skipped; values are clamped.
inline bool Parse(const char* text, unsigned length, Prefs& out) {
    using prefs_detail::Number;
    const char* p = text;
    const char* end = text + length;
    static constexpr char magic[] = "isaacmods ";
    if (length < sizeof(magic) - 1 || std::memcmp(text, magic, sizeof(magic) - 1) != 0) return false;
    p += sizeof(magic) - 1;
    long version{};
    if (!Number(p, end, version) || version < 1) return false;
    Prefs result;
    auto line_end = [&](const char* q) { while (q < end && *q != '\n') ++q; return q; };
    p = line_end(p);
    while (p < end) {
        if (*p == '\n' || *p == '\r') { ++p; continue; }
        const char* stop = line_end(p);
        const char* eq = p;
        while (eq < stop && *eq != '=') ++eq;
        if (eq == stop) { p = stop; continue; }
        const auto key_length = static_cast<unsigned>(eq - p);
        auto is = [&](const char* key) { return std::strlen(key) == key_length && std::memcmp(p, key, key_length) == 0; };
        const char* v = eq + 1;
        long values[8]{};
        unsigned count = 0;
        auto list = [&] {
            count = 0;
            const char* q = v;
            while (q < stop && count < 8) {
                if (!Number(q, stop, values[count])) break;
                ++count;
                if (q < stop && *q == ',') ++q; else break;
            }
        };
        if (is("mods")) { list(); if (count == 1 && values[0] >= 0) { result.mods = static_cast<std::uint32_t>(values[0]); result.hasMods = true; } }
        else if (is("hit")) {
            list();
            if (count == 5) {
                result.hitX = static_cast<std::int16_t>(values[0]); result.hitY = static_cast<std::int16_t>(values[1]);
                result.hitSize = static_cast<std::uint16_t>(values[2] < 0 ? 100 : values[2] > 1000 ? 1000 : values[2]);
                result.hitOpacity = static_cast<std::uint8_t>(values[3] < 0 ? 85 : values[3] > 100 ? 100 : values[3]);
                result.hitFloor = values[4] != 0;
            }
        }
        else if (is("timemachine")) { list(); if (count == 1 && values[0] >= 0 && values[0] < 100) result.timeMachineSteps = static_cast<std::uint8_t>(values[0]); }
        else if (is("eid")) { list(); if (count == 2) { result.eidNearby = values[0] != 0; result.eidMap = values[1] != 0; } }
        else if (is("rplus")) { list(); if (count == 1 && values[0] >= 0) result.rplus = static_cast<std::uint8_t>(values[0] & 0xf); }
        else if (is("menu")) {
            list();
            if (count == 7) {
                result.sounds = values[0] != 0; result.reduceMotion = values[1] != 0; result.pausePrompt = values[2] != 0;
                for (unsigned i = 3; i < 7; ++i) if (values[i] < 0 || values[i] > 255) values[i] = 0;
                result.shortcut = static_cast<std::uint8_t>(values[3]); result.openTab = static_cast<std::uint8_t>(values[4]);
                result.dim = static_cast<std::uint8_t>(values[5]); result.lastTab = static_cast<std::uint8_t>(values[6]);
            }
        }
        else if (is("favorites") || is("recents")) {
            const bool favorites = is("favorites");
            const char* q = v;
            while (q < stop) {
                long c{}, id{}, extra{};
                if (!Number(q, stop, c) || q >= stop || *q++ != ':' || !Number(q, stop, id) || q >= stop || *q++ != ':' ||
                    !Number(q, stop, extra)) break;
                if (c >= 0 && c < static_cast<long>(CategoryCount) && id > 0 && id < 65536 && extra >= 0 && extra < 65536) {
                    const Entry e{static_cast<std::uint8_t>(c), static_cast<std::uint16_t>(id), static_cast<std::uint16_t>(extra)};
                    if (favorites && result.favoriteCount < MaxFavorites && !result.IsFavorite(e)) result.favorites[result.favoriteCount++] = e;
                    if (!favorites && result.recentCount < MaxRecents) result.recents[result.recentCount++] = e;
                }
                if (q < stop && *q == ',') ++q; else break;
            }
        }
        p = stop;
    }
    if (version < 2 && result.hasMods) result.mods |= ~Version1Mods;
    result.Clamp();
    out = result;
    return true;
}
}

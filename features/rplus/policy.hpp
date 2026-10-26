#pragma once
#include <cstdint>
#include <cstdio>

// Repentance+ Rules: the decisions, kept free of game access so the host tests
// cover them. native/source/program/rplus.cpp supplies the game state.
//
// In Switch 1.7.9b Room::SpawnClearAward rolls RNG::Random(5) after ???/The
// Lamb (0 = Void portal) and RNG::Random(2) after Mega Satan (non-zero = the
// run ends, 0 = big chest plus portal). Both then still require
// Level::IsStageAvailable(The Void), which checks the unlock, Greed Mode and
// challenges. Repentance+ makes those rolls certain once The Void is unlocked
// and adds a portal after Mother once her ending has been seen.
namespace isaac::rplus {
enum Rule : std::uint8_t {
    VoidAfterChest = 1,      // ??? and The Lamb
    VoidAfterMegaSatan = 2,
    VoidAfterMother = 4,
    BossRushWaves = 8,
};
inline constexpr std::uint8_t AllRules = VoidAfterChest | VoidAfterMegaSatan | VoidAfterMother | BossRushWaves;

enum class Roll : std::uint8_t { Other, Chest, MegaSatan };

// Which of the two portal rolls a call to RNG::Random came from, by its return
// offset inside the game module.
inline Roll RollFor(std::uintptr_t returnOffset, std::uintptr_t chestReturn, std::uintptr_t megaSatanReturn) {
    if (returnOffset == chestReturn) return Roll::Chest;
    if (returnOffset == megaSatanReturn) return Roll::MegaSatan;
    return Roll::Other;
}

// The value RNG::Random hands back. The game's own roll always runs first, so
// the seed advances exactly as in vanilla; only the result changes.
inline unsigned RollResult(Roll roll, unsigned vanilla, bool enabled, std::uint8_t rules) {
    if (!enabled) return vanilla;
    if (roll == Roll::Chest && (rules & VoidAfterChest)) return 0;
    if (roll == Roll::MegaSatan && (rules & VoidAfterMegaSatan)) return 0;
    return vanilla;
}

// The stage SpawnClearAward compares: Curse of the Labyrinth (curse bit 2) and
// challenge 44 count one floor further.
inline int AwardStage(int stage, std::uint32_t curses, int challenge) {
    return ((curses & 2) || challenge == 44) ? stage + 1 : stage;
}

struct MotherFacts {
    int roomIndex{}, roomType{}, awardStage{};
    bool voidAvailable{};    // Level::IsStageAvailable(The Void)
    bool endingSeen{};       // Ending 21 (cutscene 24) shown at least once
};
// The same room test SpawnClearAward uses before it gives Mother's big chest.
inline bool IsMotherRoom(const MotherFacts& f) { return f.roomIndex == -10 && f.roomType == 5 && f.awardStage == 8; }
inline bool AddMotherPortal(const MotherFacts& f, bool enabled, std::uint8_t rules) {
    return enabled && (rules & VoidAfterMother) && IsMotherRoom(f) && f.voidAvailable && f.endingSeen;
}

// Grid index of a world position, as SpawnClearAward computes it (-1 = outside).
inline int GridIndex(float x, float y, int width, int height) {
    const int column = static_cast<int>((x - 40.f) / 40.f + .5f), row = static_cast<int>((y - 120.f) / 40.f + .5f);
    if (column < 0 || row < 0 || column >= width || row >= height) return -1;
    return column + width * row;
}

// "Wave 3/15" while a Boss Rush is running. Returns false when nothing shows.
inline bool WaveLabel(bool enabled, std::uint8_t rules, int roomType, bool active, int wave, int total, char* out, unsigned size) {
    if (!enabled || !(rules & BossRushWaves) || roomType != 17 || !active || wave < 1 || !size) return false;
    if (total >= wave && total <= 30) std::snprintf(out, size, "Wave %d/%d", wave, total);
    else std::snprintf(out, size, "Wave %d", wave);
    return true;
}
}

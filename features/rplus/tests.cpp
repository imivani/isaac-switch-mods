// Host tests for the Repentance+ Rules decisions (features/rplus/policy.hpp).
#include "policy.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

using namespace isaac::rplus;
unsigned cases{};
#define CHECK(x) do { assert(x); ++cases; } while (0)

int main() {
    constexpr std::uintptr_t chest = 0x459190, mega = 0x4582cc;
    // Only the two portal rolls are recognised.
    CHECK(RollFor(chest, chest, mega) == Roll::Chest);
    CHECK(RollFor(mega, chest, mega) == Roll::MegaSatan);
    CHECK(RollFor(chest + 4, chest, mega) == Roll::Other && RollFor(0, chest, mega) == Roll::Other);
    // A portal roll becomes 0 ("portal") only while the mod and its rule are on.
    CHECK(RollResult(Roll::Chest, 3, true, AllRules) == 0);
    CHECK(RollResult(Roll::MegaSatan, 1, true, AllRules) == 0);
    CHECK(RollResult(Roll::Chest, 3, false, AllRules) == 3);
    CHECK(RollResult(Roll::Chest, 3, true, AllRules & ~VoidAfterChest) == 3);
    CHECK(RollResult(Roll::MegaSatan, 1, true, AllRules & ~VoidAfterMegaSatan) == 1);
    CHECK(RollResult(Roll::MegaSatan, 1, true, VoidAfterChest) == 1);
    CHECK(RollResult(Roll::Other, 7, true, AllRules) == 7);
    CHECK(RollResult(Roll::Chest, 0, true, 0) == 0);  // vanilla already said "portal"

    // Award stage: Labyrinth and challenge 44 count one floor further, like the game.
    CHECK(AwardStage(8, 0, 0) == 8 && AwardStage(7, 2, 0) == 8 && AwardStage(7, 0, 44) == 8 && AwardStage(7, 1 | 4, 0) == 7);

    // Mother: room index -10, a boss room, award stage 8; Void reachable; ending seen.
    MotherFacts f{-10, 5, 8, true, true};
    CHECK(IsMotherRoom(f) && AddMotherPortal(f, true, AllRules));
    CHECK(!AddMotherPortal(f, false, AllRules) && !AddMotherPortal(f, true, AllRules & ~VoidAfterMother));
    auto with = [&](auto change) { MotherFacts g = f; change(g); return AddMotherPortal(g, true, AllRules); };
    CHECK(!with([](MotherFacts& g) { g.endingSeen = false; }));       // first Mother kill: just the ending
    CHECK(!with([](MotherFacts& g) { g.voidAvailable = false; }));    // Void locked, Greed Mode or a challenge
    CHECK(!with([](MotherFacts& g) { g.awardStage = 6; }));           // Mom in Mausoleum II uses the same room
    CHECK(!with([](MotherFacts& g) { g.roomIndex = 84; }));
    CHECK(!with([](MotherFacts& g) { g.roomType = 1; }));

    // Grid index exactly as SpawnClearAward computes it.
    CHECK(GridIndex(40, 120, 15, 9) == 0);
    CHECK(GridIndex(320, 280, 15, 9) == 7 + 15 * 4);
    CHECK(GridIndex(339, 299, 15, 9) == 7 + 15 * 4 && GridIndex(341, 301, 15, 9) == 8 + 15 * 5);
    CHECK(GridIndex(40 + 40 * 15, 120, 15, 9) == -1 && GridIndex(40, 120 + 40 * 9, 15, 9) == -1);

    // Boss Rush wave label.
    char label[24];
    CHECK(WaveLabel(true, AllRules, 17, true, 3, 15, label, sizeof(label)) && !std::strcmp(label, "Wave 3/15"));
    CHECK(WaveLabel(true, AllRules, 17, true, 15, 15, label, sizeof(label)) && !std::strcmp(label, "Wave 15/15"));
    CHECK(WaveLabel(true, AllRules, 17, true, 4, 0, label, sizeof(label)) && !std::strcmp(label, "Wave 4"));   // unknown total
    CHECK(WaveLabel(true, AllRules, 17, true, 4, 999, label, sizeof(label)) && !std::strcmp(label, "Wave 4"));
    CHECK(!WaveLabel(true, AllRules, 17, true, 0, 15, label, sizeof(label)));   // not started
    CHECK(!WaveLabel(true, AllRules, 17, false, 3, 15, label, sizeof(label)));  // finished
    CHECK(!WaveLabel(true, AllRules, 11, true, 3, 15, label, sizeof(label)));   // a challenge room, not Boss Rush
    CHECK(!WaveLabel(false, AllRules, 17, true, 3, 15, label, sizeof(label)));
    CHECK(!WaveLabel(true, AllRules & ~BossRushWaves, 17, true, 3, 15, label, sizeof(label)));
    std::printf("Repentance+ Rules: %u policy checks PASS\n", cases);
}

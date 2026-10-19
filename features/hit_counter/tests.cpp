#include "counter.hpp"
#include <cassert>
#include <cstdio>

using namespace isaac::hit_counter;
unsigned cases{};
#define CHECK(x) do { assert(x); ++cases; } while (0)

int main() {
    // Which damage counts.
    CHECK(CountsAsHit(true, 0, 60, 0));
    CHECK(CountsAsHit(true, 0, 0, 0x4));                       // explosion
    CHECK(!CountsAsHit(false, 0, 0, 0));                       // invincible, nothing happened
    CHECK(CountsAsHit(false, 0, 60, 0));                       // Holy Mantle broke
    CHECK(!CountsAsHit(false, 12, 30, 0));                     // already invincible
    CHECK(!CountsAsHit(true, 0, 60, 0x10000000));              // no-penalty damage
    CHECK(!CountsAsHit(true, 0, 60, 0x20));                    // red-heart self damage (blood donation)
    CHECK(!CountsAsHit(true, 0, 60, 0x10000020));
    CHECK(!CountsAsHit(true, 0, 60, 0x200000));                // fake damage
    CHECK(!CountsAsHit(false, 0, 60, 0x200000));

    // Run and floor tracking.
    Counter c;
    CHECK(!c.Valid() && !c.Total());
    c.Sync({100, 10, 1, 0});
    CHECK(c.Valid() && c.Total() == 0 && c.Floor() == 0);
    c.Hit({100, 20, 1, 0}); c.Hit({100, 40, 1, 0});
    CHECK(c.Total() == 2 && c.Floor() == 2);
    c.Sync({100, 900, 2, 0});                                  // next floor
    CHECK(c.Total() == 2 && c.Floor() == 0);
    c.Hit({100, 950, 2, 0});
    CHECK(c.Total() == 3 && c.Floor() == 1);
    c.Sync({100, 960, 2, 1});                                  // alt floor variant counts as a new floor
    CHECK(c.Total() == 3 && c.Floor() == 0);
    c.Sync({100, 940, 2, 1});                                  // continue after menu: frame slightly earlier
    CHECK(c.Total() == 3);
    c.Sync({100, 5, 1, 0});                                    // seeded restart: same seed, frame zero
    CHECK(c.Total() == 0 && c.Floor() == 0);
    c.Hit({100, 7, 1, 0});
    c.Sync({555, 3, 1, 0});                                    // new run
    CHECK(c.Total() == 0);
    c.Hit({777, 30, 1, 0});                                    // first hit arrives before any draw
    CHECK(c.Total() == 1 && c.Floor() == 1);
    for (unsigned i = 0; i < MaxCount + 5; ++i) c.Hit({777, 31 + i, 1, 0});
    CHECK(c.Total() == MaxCount && c.Floor() == MaxCount);
    c.Reset();
    CHECK(!c.Valid() && !c.Total());

    // Placement stays fully on screen and defaults to top center.
    Settings s;
    auto at = Place(s, 480, 270, 60, 10);
    CHECK(at.x == 210 && at.y == 4);
    s.x = -50; at = Place(s, 480, 270, 60, 10);
    CHECK(at.x == 210);                                        // negative means "default"
    s.x = 470; s.y = 300; at = Place(s, 480, 270, 60, 10);
    CHECK(at.x == 420 && at.y == 260);
    s.x = 5; s.y = 5; at = Place(s, 480, 270, 600, 10);
    CHECK(at.x == 0);
    CHECK(Scale(Settings{}) == 1.f && Alpha(Settings{}) == .85f);

    std::printf("Hit Counter: %u portable cases PASS\n", cases);
}

#include "state.hpp"
#include "exchange_scope.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>

using namespace isaac_item_visuals;
unsigned checks = 0;
#define CHECK(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#condition); std::exit(1); } } while (false)
bool Close(float a, float b) { return std::abs(a-b) < .00001f; }

int main() {
    CHECK(AnimationCount == 98);
    CHECK(CharacterCount == 41);
    CHECK(AnimationIndex(0) == NoAnimation && AnimationIndex(619) == NoAnimation && AnimationIndex(99999) == NoAnimation);
    CHECK(EligibleItem(619) && EligibleItem(3) && !EligibleItem(0) && !EligibleItem(1) && !EligibleItem(733));
    for (std::size_t i = 0; i < AnimationCount; ++i) {
        const auto& row = data::Animations[i];
        CHECK(AnimationIndex(row.item) == i);
        CHECK(row.frames > 1 && row.frames <= 10000);
        CHECK(i == 0 || data::Animations[i-1].item < row.item);
        for (unsigned age = 0; age < row.frames * 3 + 2; ++age) {
            CHECK(AnimationFrame(age, row.frames) < row.frames);
            CHECK(AnimationFrame(age, row.frames) == AnimationFrame(age + row.frames, row.frames));
        }
        CHECK(AnimationFrame(std::numeric_limits<unsigned>::max(), row.frames) < row.frames);
    }
    CHECK(AnimationFrame(100, 0) == 0);
    const int ys[20] = {-3,-3,-3,-4,-4,-5,-5,-6,-6,-7,-7,-7,-6,-6,-5,-5,-4,-4,-3,-3};
    const float sx[20] = {1,1,1,1,1,1.08f,1.08f,1.08f,1.08f,1,1,1,1,1,1,1,1.08f,1.08f,1.08f,1.08f};
    const float sy[20] = {1,1,1,1.04f,1.04f,.98f,.98f,.98f,.98f,1,1,1,1.04f,1.04f,1,1,.98f,.98f,.98f,.98f};
    for (int frame = 0; frame < 20; ++frame) {
        const auto pose = PedestalPose(frame, false), shop = PedestalPose(frame, true);
        CHECK(Close(pose.y, static_cast<float>(ys[frame])) && Close(pose.scale_x,sx[frame]) && Close(pose.scale_y,sy[frame]));
        CHECK(Close(shop.y,10) && Close(shop.scale_x,1) && Close(shop.scale_y,1));
    }
    for (unsigned age = 0; age < 100; ++age) CHECK(Close(HeldPose(age).y,age <= 1 ? -25.f : age >= 36 ? -12.f : -16.f));
    CHECK(KnownSheet("gfx/items/collectibles/Collectibles_003_SpoonBender.png"));
    CHECK(!KnownSheet("gfx/items/collectibles/QuestionMark.png"));
    CHECK(!KnownSheet("Items/Collectibles/QUESTIONMARK.PCX"));
    CHECK(!KnownSheet("gfx/nil.png") && !KnownSheet("") && !KnownSheet(nullptr));
    char long_path[600]; std::memset(long_path,'a',sizeof(long_path)); long_path[599]=0;
    CHECK(!KnownSheet(long_path));
    Player party[8] = {{1,0,0,true},{3,0,21,true},{-1,0,40,true}};
    CHECK(NearestPlayer(0,0,party,3) == 0); // Stable first-slot tie.
    CHECK(NearestPlayer(-2,0,party,3) == 2);
    CHECK(NearestPlayer(4,0,party,3) == 1);
    party[1].valid=false; CHECK(NearestPlayer(4,0,party,3) == 0);
    party[0].type=41; CHECK(NearestPlayer(4,0,party,3) == 2);
    party[2].x=std::numeric_limits<float>::quiet_NaN(); CHECK(NearestPlayer(4,0,party,3) == -1);
    CHECK(NearestPlayer(0,0,nullptr,1) == -1 && NearestPlayer(0,0,party,9) == -1);
    CHECK(NearestPlayer(std::numeric_limits<float>::infinity(),0,party,0) == -1);
    for (unsigned type=0; type<CharacterCount; ++type) {
        const Player player{0,0,type,true}; CHECK(NearestPlayer(0,0,&player,1) == 0);
    }
    Timeline<3> clock;
    Identity item{0x100000,10,0,3,false};
    CHECK(clock.Observe(item,10)==0);
    CHECK(clock.Observe(item,11)==1);
    CHECK(clock.Observe(item,11)==1); // Repeat renders do not advance animation.
    item.item=9; CHECK(clock.Observe(item,12)==0);
    item.item=3; CHECK(clock.Observe(item,13)==0); // Crown/reroll back starts fresh.
    CHECK(clock.Observe(item,14)==1);
    item.init_seed=11; CHECK(clock.Observe(item,15)==0); // Reused address.
    item.born=15; CHECK(clock.Observe(item,16)==0);
    CHECK(clock.Observe(item,17)==1);
    CHECK(clock.Observe(item,30)==0); // Disappeared/hidden then visible again.
    CHECK(clock.Observe(item,31)==1);
    CHECK(clock.Observe(item,12)==0); // Hourglass/time rewind.
    auto held=item; held.held=true;
    CHECK(clock.Observe(held,13)==0 && clock.Observe(item,13)==1);
    CHECK(clock.Observe(held,14)==1);
    clock.Reset(); CHECK(clock.Observe(held,15)==0);
    CHECK(clock.Observe({},0)==0 && clock.Observe(item,-1)==0);
    // Exercise bounded LRU replacement and interleaved identical item IDs.
    for (int now=0; now<2000; ++now) {
        const Identity many{static_cast<std::uintptr_t>(0x200000+now%5*0x1000),7,0,9,false};
        CHECK(clock.Observe(many,now)==0);
    }
    std::printf("PASS: %u source-default animation, visibility, co-op and lifetime checks\n",checks);
}

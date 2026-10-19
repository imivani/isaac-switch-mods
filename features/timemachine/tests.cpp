#include "policy.hpp"
#include "dispatch.hpp"
#include <cassert>
#include <cstdio>
#include <limits>
#include <initializer_list>

using namespace isaac::timemachine;
Snapshot Ready() {
    return {{0x10000,0x20000,0x30000,0x40000,123,15,234,345,4,0,2,0},100,true};
}
int main() {
    unsigned cases = 0;
    {
        auto state=Ready(); Ramp ramp;
        unsigned ordinary{},accelerated{},canceled{},steps{};
        bool valid=true,paused=false;
        auto read=[&](unsigned& frame){frame=state.frame;return valid;};
        auto step=[&]{++steps;if(!paused)++state.frame;};
        auto inspect=[&]{return state;};
        auto observe=[&](bool normal){normal?++ordinary:++accelerated;};
        auto cancel=[&]{++canceled;};
        for(unsigned i=0;i<120;++i) Dispatch(ramp,read,step,inspect,observe,cancel);
        assert(ordinary==120 && accelerated>100 && steps==ordinary+accelerated && canceled==0); ++cases;
        const auto old=steps; paused=true;
        Dispatch(ramp,read,step,inspect,observe,cancel);
        assert(steps==old+1 && ordinary==120 && canceled==1 && ramp.SpeedTwentieths()==0); ++cases;
        paused=false;valid=false;
        Dispatch(ramp,read,step,inspect,observe,cancel);
        assert(ordinary==120 && canceled==2); ++cases;
        valid=true;state.eligible=false;
        Dispatch(ramp,read,step,inspect,observe,cancel);
        assert(ordinary==121 && canceled==2); ++cases;
        state.eligible=true;
        for(unsigned i=0;i<120;++i) ramp.Begin(state);
        const auto extras=accelerated;
        Dispatch(ramp,read,step,inspect,[&](bool normal){observe(normal);if(normal)state.eligible=false;},cancel);
        assert(ordinary==122 && accelerated==extras); ++cases;
    }
    for (unsigned controller=0;controller<128;++controller) {
        assert(ValidController(controller) == ((controller>=1 && controller<=8) || controller==33));
        ++cases;
    }
    for (unsigned variant = 0; variant < 30; ++variant) {
        const bool expected = variant == 1 || variant == 2 || variant == 3 || variant == 4 ||
            variant == 5 || variant == 6 || variant == 7 || variant == 9 || variant == 11 ||
            variant == 13 || variant == 15 || variant == 16 || variant == 17 || variant == 18;
        assert(Enabled(variant) == expected); ++cases;
    }
    for (unsigned type : {4u,7u,9u,13u,18u,33u,85u,900u,999u}) { assert(Hazard(type)); ++cases; }
    for (unsigned type : {1u,2u,3u,5u,6u,8u,17u,1000u}) { assert(!Hazard(type)); ++cases; }
    assert(DecideContact(true,false,2,false,true) == ContactDecision::NormalSpeedOnly);
    assert(DecideContact(true,true,5,false,false) == ContactDecision::NormalSpeedOnly);
    assert(DecideContact(false,false,2,false,true) == ContactDecision::None);
    assert(DecideContact(true,false,2,true,true) == ContactDecision::None);
    assert(DecideContact(true,true,2,true,true) == ContactDecision::Accelerate);
    assert(DecideContact(true,true,1,false,true) == ContactDecision::Accelerate);
    assert(DecideContact(true,true,1,false,false) == ContactDecision::None);
    cases += 7;
    for (unsigned direction = 0; direction < 4; ++direction) {
        assert(InputAllowsAcceleration(1u << direction,0));
        assert(!InputAllowsAcceleration(1u << direction,1u << direction)); cases += 2;
    }
    for (unsigned action = 4; action <= 13; ++action) {
        assert(!InputAllowsAcceleration(1u | (1u << action),0)); ++cases;
    }
    assert(CanAfford(1,1,0,0,0,0,0,0));
    assert(!CanAfford(1,0,99,99,12,0,0,0));
    assert(!CanAfford(16,4,0,0,0,0,0,0));
    assert(CanAfford(16,5,0,0,0,0,0,0));
    assert(CanAfford(7,0,1,0,0,0,0,0));
    assert(!CanAfford(7,99,0,99,12,0,0,0));
    assert(CanAfford(9,0,0,1,0,0,0,0));
    assert(!CanAfford(9,99,99,0,12,0,0,0)); cases += 8;
    for (unsigned variant : {2u,5u,15u,17u}) {
        assert(!CanAfford(variant,99,99,99,0,0,0,0));
        assert(!CanAfford(variant,99,99,99,2,0,0,0));
        assert(!CanAfford(variant,99,99,99,0,2,0,0));
        assert(CanAfford(variant,0,0,0,3,0,0,0));
        assert(CanAfford(variant,0,0,0,0,3,0,0)); cases += 5;
    }
    assert(Contact(0,0,10,20,0,10));
    assert(!Contact(0,0,10,25,0,10));
    assert(!Contact(0,0,0,0,0,10));
    assert(!Contact(0,0,10,std::numeric_limits<float>::quiet_NaN(),0,10));
    assert(!Contact(0,0,10,std::numeric_limits<float>::infinity(),0,10));
    assert(Toward(0,0,20,0,1,0));
    assert(!Toward(0,0,20,0,-1,0));
    assert(!Toward(0,0,20,0,0,1));
    assert(!Toward(0,0,20,0,0,0)); cases += 9;
    {
        Ramp ramp; auto current = Ready(); unsigned calls = 0;
        for (unsigned frame = 0; frame < 100; ++frame) {
            ++current.frame;
            const auto ticks = Accelerate(ramp, [&] { return current; }, [&] { ++calls; ++current.frame; });
            assert(ticks <= 5);
        }
        assert(ramp.SpeedTwentieths() == 100 && calls > 200);
        for (unsigned i = 0; i < 10; ++i)
            assert(Accelerate(ramp, [&] { return current; }, [&] { ++current.frame; }) == 5);
        current.eligible = false;
        assert(Accelerate(ramp, [&] { return current; }, [&] { assert(false); }) == 0);
        assert(ramp.SpeedTwentieths() == 0);
        current.eligible = true;
        assert(Accelerate(ramp, [&] { return current; }, [&] { assert(false); }) == 0);
        ++cases;
    }
    // Each possible transition invalidates the current pass, even if a room or
    // pooled entity happens to reuse the same memory address.
    for (unsigned changed = 0; changed < 12; ++changed) {
        Ramp ramp; auto current = Ready();
        for (int i = 0; i < 100; ++i) ramp.Begin(current);
        unsigned ticks = Accelerate(ramp, [&] { return current; }, [&] {
            ++current.frame;
            switch (changed) {
            case 0: ++current.identity.game; break;
            case 1: ++current.identity.room; break;
            case 2: ++current.identity.player; break;
            case 3: ++current.identity.machine; break;
            case 4: ++current.identity.runSeed; break;
            case 5: ++current.identity.roomFrame; break;
            case 6: ++current.identity.playerSeed; break;
            case 7: ++current.identity.machineSeed; break;
            case 8: ++current.identity.roomIndex; break;
            case 9: ++current.identity.dimension; break;
            case 10: ++current.identity.stage; break;
            case 11: ++current.identity.stageType; break;
            }
        });
        assert(ticks == 1 && ramp.SpeedTwentieths() == 0); ++cases;
    }
    // Pause/death/movement/payout/hazard/resource changes all enter through the
    // eligibility snapshot; no accumulated ticks may run after it turns false.
    {
        Ramp ramp; auto current = Ready();
        for (int i = 0; i < 100; ++i) ramp.Begin(current);
        assert(Accelerate(ramp, [&] { return current; }, [&] { ++current.frame; current.eligible=false; }) == 1);
        assert(ramp.SpeedTwentieths() == 0); ++cases;
    }
    // A native early-return must never turn into repeated menu/transition ticks.
    {
        Ramp ramp; auto current = Ready();
        for (int i = 0; i < 100; ++i) ramp.Begin(current);
        assert(Accelerate(ramp, [&] { return current; }, [] {}) == 1);
        assert(ramp.SpeedTwentieths() == 0); ++cases;
    }
    // State changing between inspection and the very first step cancels safely.
    {
        Ramp ramp; auto current = Ready();
        for (int i = 0; i < 100; ++i) ramp.Begin(current);
        unsigned inspectCount = 0;
        assert(Accelerate(ramp, [&] { if (++inspectCount > 1) current.eligible=false; return current; }, [] { assert(false); }) == 0);
        ++cases;
    }
    std::printf("Time Machine: %u policy and lifecycle cases PASS\n",cases);
}

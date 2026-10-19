#include "test_fixture.hpp"
#include "legacy_adapter.hpp"
unsigned cases{};
void Expect(World& w,bool eligible,BlockReason expected) {
    BlockReason reason{}; const auto state=w.InspectNow(reason);
    if (state.eligible!=eligible || reason!=expected) {
        std::fprintf(stderr,"Expected eligible=%d reason=%s, got eligible=%d reason=%s\n",
                     eligible,ReasonText(expected),state.eligible,ReasonText(reason));
        std::abort();
    }
    ++cases;
}
int main() {
    // This is the concrete observed-class regression: the archived, actual
    // native inspector rejects an extinguished orange fire; the shared
    // production adapter now accepts the exact harmless native state.
    {
        World w; BlockReason reason{};
        assert(legacy::Inspect(w,w.module.Address(),w.game.Address()).eligible); ++cases;
        w.SetEntities({w.p1.Address(),w.slot.Address(),w.fire.Address()});
        assert(!legacy::Inspect(w,w.module.Address(),w.game.Address()).eligible); ++cases;
        Expect(w,true,BlockReason::Ready);
        Ramp ramp;
        for (unsigned i=0;i<120;++i) w.DispatchNow(ramp);
        assert(w.ordinary==120 && w.extra>200 && ramp.SpeedTwentieths()==100); ++cases;
        w.movement[0]={0,0}; const auto extra=w.extra;
        w.DispatchNow(ramp); assert(w.extra==extra && ramp.SpeedTwentieths()==0); ++cases;
        (void)reason;
    }
    for (unsigned variant=0;variant<14;++variant) {
        World w; w.fire.Put(0x3c,variant); w.SetEntities({w.slot.Address(),w.fire.Address()});
        Expect(w,variant<=3,variant<=3?BlockReason::Ready:BlockReason::Hazard);
        w.FireState(8,10,4ull<<32); Expect(w,false,BlockReason::Hazard);
    }
    for (int state:{0,1,2,4,8,9}) {
        World w; w.FireState(state,1,0); w.SetEntities({w.slot.Address(),w.fire.Address()});
        Expect(w,false,BlockReason::Hazard);
    }
    for (float hp:{0.f,1.1f,2.f,std::numeric_limits<float>::quiet_NaN()}) {
        World w; w.FireState(3,hp,0); w.SetEntities({w.slot.Address(),w.fire.Address()});
        Expect(w,false,BlockReason::Hazard);
    }
    for (auto collision:{1ull,4ull<<32}) {
        World w; w.FireState(3,1,collision); w.SetEntities({w.slot.Address(),w.fire.Address()});
        Expect(w,false,BlockReason::Hazard);
    }
    { World w; w.fireReadable=false; w.SetEntities({w.slot.Address(),w.fire.Address()}); Expect(w,false,BlockReason::Hazard); }
    for (unsigned type:{4,7,9,10,13,18,85,999}) {
        World w; w.other.Put(0x38,type); w.SetEntities({w.slot.Address(),w.other.Address()});
        Expect(w,false,BlockReason::Hazard);
    }
    { World w; w.other.Put<unsigned>(0x38,17); w.SetEntities({w.slot.Address(),w.other.Address()}); Expect(w,true,BlockReason::Ready); }
    for (unsigned controller:{1,2,3,4,5,6,7,8,33}) {
        World w; w.p1.Put(0x19ec,controller); Expect(w,true,BlockReason::Ready);
        assert(w.pressedCalls==14 && w.triggeredCalls==4 && w.movementCalls==1); ++cases;
        assert(legacy::Inspect(w,w.module.Address(),w.game.Address()).eligible); ++cases;
    }
    for (unsigned controller:{0u,9u,32u,34u,0xffffffffu}) {
        World w; w.p1.Put(0x19ec,controller); Expect(w,false,BlockReason::ControllerUnavailable);
        assert(!w.pressedCalls && !w.movementCalls); ++cases;
    }
    for (unsigned action=4;action<=13;++action) {
        World w; w.pressed[0]|=1u<<action; Expect(w,false,BlockReason::OtherAction);
    }
    for (unsigned action=0;action<4;++action) {
        World w; w.triggered[0]|=1u<<action; Expect(w,false,BlockReason::FreshMovement);
    }
    for (unsigned variant=1;variant<=18;++variant) {
        World w; w.Machine(variant,1);
        Expect(w,Enabled(variant),Enabled(variant)?BlockReason::Ready:BlockReason::UnsupportedMachine);
    }
    { World w; w.paused=true; Expect(w,false,BlockReason::Paused); }
    { World w; w.game.Put<int>(0x26fa9c,1); Expect(w,false,BlockReason::Transition); }
    { World w; w.descriptor.Put<std::uint8_t>(0x50,0); Expect(w,false,BlockReason::RoomNotClear); }
    { World w; w.room.Put<unsigned>(0x191c,99); Expect(w,false,BlockReason::RoomSettling); }
    { World w; w.p1.Put<std::uint8_t>(0x1c3,1); Expect(w,false,BlockReason::PlayerUnavailable); }
    { World w; w.p1.Put<std::uint8_t>(0x400,0); Expect(w,false,BlockReason::ControlsDisabled); }
    { World w; w.p1.Put<int>(0x404,1); Expect(w,false,BlockReason::ControlsDisabled); }
    { World w; w.SetPlayers({0xdeadbeef0}); Expect(w,false,BlockReason::PlayerUnavailable); }
    { World w; w.room.Put<std::uintptr_t>(profile::RoomEntities,0xdeadbeef0); Expect(w,false,BlockReason::InvalidState); }
    { World w; w.room.Put<unsigned>(profile::RoomEntityCount,1025); Expect(w,false,BlockReason::InvalidState); }
    { World w; w.SetEntities({0,0xdeadbeef0,w.slot.Address()}); Expect(w,true,BlockReason::Ready); }
    { World w; w.SetEntities({w.p1.Address()}); Expect(w,false,BlockReason::NoMachine); }
    { World w; w.slot.Put<int>(0x400,3); Expect(w,false,BlockReason::MachineBusy); }
    { World w; w.slot.Put<float>(0x310,100); Expect(w,false,BlockReason::NotTouching); }
    { World w; w.movement[0]={-1,0}; Expect(w,false,BlockReason::NotPushing); }
    { World w; w.movement[0]={0,0}; Expect(w,false,BlockReason::NotPushing); }
    { World w; w.movement[0]={std::numeric_limits<float>::quiet_NaN(),0}; Expect(w,false,BlockReason::InvalidState); }
    { World w; w.p1.Put<int>(0x16e0,0); Expect(w,false,BlockReason::NoResources); w.slot.Put<int>(0x400,2); Expect(w,true,BlockReason::Ready); }
    { World w; w.Machine(2,1); w.p1.Put<int>(0x16bc,2); Expect(w,false,BlockReason::LowHealth); }
    {
        World w; w.SetPlayers({w.p1.Address(),w.p2.Address()});
        Expect(w,true,BlockReason::Ready);
        w.movement[1]={-1,0}; Expect(w,false,BlockReason::PartnerMoving);
        w.movement[1]={0,0}; w.Machine(2,1); w.p2.Put<int>(0x16bc,2);
        Expect(w,false,BlockReason::LowHealth);
    }
    // Ordinary observers stay active when Time Machine is switched off. On
    // re-enable a new ramp starts; there are no saved extra ticks to replay.
    {
        World w; Ramp ramp;
        for (unsigned i=0;i<120;++i) w.DispatchNow(ramp);
        const auto extra=w.extra;
        for (unsigned i=0;i<20;++i) w.DispatchNow(ramp,false);
        assert(w.ordinary==140 && w.extra==extra && ramp.SpeedTwentieths()==0); ++cases;
        w.DispatchNow(ramp,true); assert(w.extra==extra && ramp.SpeedTwentieths()==1); ++cases;
    }
    // Real observers may change native memory before the accelerated pass.
    // A new hazard/room/low-health state stops all further game ticks.
    for (unsigned mutation=0;mutation<4;++mutation) {
        World w; Ramp ramp; BlockReason reason{};
        for (unsigned i=0;i<100;++i) ramp.Begin(w.InspectNow(reason));
        w.DispatchNow(ramp,true,[&](bool normal) {
            if (!normal) return;
            if (mutation==0) w.SetEntities({w.slot.Address(),w.other.Address()});
            if (mutation==1) { w.Machine(2,1); w.p1.Put<int>(0x16bc,2); }
            if (mutation==2) w.room.Put<unsigned>(0x191c,w.game.Get<unsigned>(Frame));
            if (mutation==3) w.module.Put<std::uintptr_t>(profile::GameGlobal,0);
        });
        assert(w.ordinary==1 && w.extra==0 && ramp.SpeedTwentieths()==0); ++cases;
    }
    // A fire relighting during an accelerated native tick cancels the rest.
    {
        World w; Ramp ramp; BlockReason reason{};
        w.SetEntities({w.slot.Address(),w.fire.Address()});
        for (unsigned i=0;i<100;++i) ramp.Begin(w.InspectNow(reason));
        w.DispatchNow(ramp,true,[&](bool normal) { if (!normal) w.FireState(9,1,0); });
        assert(w.ordinary==1 && w.extra==1 && ramp.SpeedTwentieths()==0); ++cases;
    }
    // Machine Lock: a player accepted against a machine stays on the contact
    // spot while holding toward it, then gets full control back.
    auto engaged=[](World& w,Lock& lock) {
        BlockReason reason{}; const auto state=w.InspectNow(reason);
        assert(state.eligible && reason==BlockReason::Ready);
        EngageLock(w,lock,state); assert(lock.active && lock.x==0 && lock.y==0);
        return reason;
    };
    auto hold=[](World& w,Lock& lock,BlockReason last=BlockReason::Ready) {
        return HoldLock(w,lock,w.module.Address(),w.game.Address(),last);
    };
    {
        // Sliding off around the round collision is undone after each update.
        World w; Lock lock; engaged(w,lock);
        w.p1.Put<float>(0x310,-3); w.p1.Put<float>(0x314,9);
        assert(hold(w,lock) && w.p1.Get<float>(0x310)==0 && w.p1.Get<float>(0x314)==0 && w.writes==2); ++cases;
        // Holding diagonally into the machine still counts as toward it.
        w.movement[0]={.7f,.7f}; w.p1.Put<float>(0x314,14);
        assert(hold(w,lock) && w.p1.Get<float>(0x314)==0); ++cases;
        // A fresh press of the same direction keeps the lock.
        assert(hold(w,lock,BlockReason::FreshMovement)); ++cases;
        // Re-engaging the same machine keeps the original spot.
        w.p1.Put<float>(0x314,4); BlockReason reason{}; auto state=w.InspectNow(reason);
        assert(state.eligible); EngageLock(w,lock,state);
        assert(lock.active && lock.y==0); ++cases;
    }
    for (unsigned stop=0;stop<14;++stop) {
        World w; Lock lock; engaged(w,lock);
        BlockReason last=BlockReason::Ready;
        switch (stop) {
        case 0: w.movement[0]={0,0}; break;                       // released
        case 1: w.movement[0]={-1,0}; break;                      // pushing away
        case 2: w.movement[0]={0,1}; break;                       // sideways
        case 3: last=BlockReason::Hazard; break;
        case 4: last=BlockReason::OtherAction; break;
        case 5: w.slot.Put<std::uint8_t>(0x1c2,0); break;         // removed
        case 6: w.slot.Put<unsigned>(0x3e8,99); break;            // different machine
        case 7: w.slot.Put<int>(0x400,3); break;                  // broken or jammed
        case 8: w.p1.Put<int>(0x16e0,0); break;                   // out of coins, idle
        case 9: w.room.Put<unsigned>(0x191c,11); break;           // new room entry
        case 10: w.p1.Put<std::uint8_t>(0x400,0); break;          // controls locked
        case 11: w.slot.Put<float>(0x310,60); break;              // machine moved away
        case 12: w.module.Put<std::uintptr_t>(profile::GameGlobal,0); break;
        case 13: last=BlockReason::LowHealth; break;
        }
        const auto writes=w.writes;
        assert(!hold(w,lock,last) && !lock.active && w.writes==writes); ++cases;
    }
    {
        // Paying out (busy states) keeps the lock briefly; a machine that
        // never returns to idle releases it.
        World w; Lock lock; engaged(w,lock);
        w.p1.Put<int>(0x16e0,0); w.slot.Put<int>(0x400,2);
        assert(hold(w,lock)); ++cases;                            // finishing a paid use
        w.slot.Put<int>(0x400,6);
        for (unsigned i=0;i<=LockBusyFrames;++i) { assert(hold(w,lock)); w.Tick(); }
        assert(!hold(w,lock) && !lock.active); ++cases;
    }
    {
        // Blood donation machines keep the last heart: no lock when low.
        World w; Lock lock; w.Machine(2,1); engaged(w,lock);
        w.p1.Put<int>(0x16bc,2);
        assert(!hold(w,lock) && !lock.active); ++cases;
    }
    {
        // Dispatching with the lock between every accelerated update.
        World w; Ramp ramp; Lock lock; BlockReason reason=BlockReason::NotInRun;
        for (unsigned i=0;i<60;++i) {
            Dispatch(ramp,[&](unsigned& frame) { return w.TryRead(w.game.Address()+Frame,frame); },
                [&] { w.Tick(); w.p1.Put<float>(0x314,w.p1.Get<float>(0x314)+2.f); },
                [&] {
                    HoldLock(w,lock,w.module.Address(),w.game.Address(),reason);
                    const auto state=w.InspectNow(reason);
                    if (state.eligible) EngageLock(w,lock,state);
                    return state;
                },
                [](bool) {}, [] {});
        }
        assert(lock.active && ramp.SpeedTwentieths()==60); ++cases;
        // Engaged after the first update (y=2); every later slide is undone.
        assert(lock.y==2.f && w.p1.Get<float>(0x314)==2.f); ++cases;
    }
    std::printf("Time Machine: %u actual-adapter and dispatch cases PASS\n",cases);
}

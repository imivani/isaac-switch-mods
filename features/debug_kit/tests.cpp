#include "policy.hpp"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>
using namespace isaac::debug_kit;
unsigned checks{};
void Check(bool value) { ++checks; if (!value) { std::fprintf(stderr,"Debug kit check %u failed\n",checks); std::exit(1); } }
int main() {
    for (unsigned id=0;id<1024;++id) Check(ValidController(id)==((id>=1&&id<=8)||id==33));
    for (unsigned controller:{1u,8u,33u}) {
        Context context{1,2,3,4,5,6,controller};
        Chord chord;
        Check(!chord.Step(context,true,false,false,false,0));
        Check(!chord.Step(context,true,true,true,true,100));
        Check(!chord.Step(context,true,true,true,true,2099));
        Check(chord.Step(context,true,true,true,true,2100));
        for (unsigned t=2200;t<20000;t+=10) Check(!chord.Step(context,true,true,true,true,t));
        Check(!chord.Step(context,true,false,true,false,21000));
        Check(!chord.Step(context,true,true,true,true,24000));
        Check(!chord.Step(context,true,false,false,false,25000));
        Check(!chord.Step(context,true,true,true,true,25100));
        Check(chord.Step(context,true,true,true,true,27100));
        Check(!chord.Step(context,true,false,false,false,28000));
        Check(!chord.Step(context,true,true,true,true,30000)); // Cooldown.
        Check(!chord.Step(context,true,true,true,true,37100)); // Fresh hold starts now.
        Check(!chord.Step(context,false,true,true,true,38000)); // Combat/pause cancels.
        Check(!chord.Step(context,true,true,true,true,42000));
        Check(!chord.Step(context,true,false,false,false,43000));
        Check(!chord.Step(context,true,true,true,true,43100));
        ++context.roomFrame;
        Check(!chord.Step(context,true,true,true,true,45100)); // Re-entered room.
        Check(!chord.Step(context,true,false,false,false,46000));
        Check(!chord.Step(context,true,true,true,true,47000));
        Check(!chord.Step(context,true,true,false,true,48999)); // Incomplete chord resets timer.
        Check(!chord.Step(context,true,true,true,true,49000));
        Check(!chord.Step(context,true,true,true,true,50999));
        Check(chord.Step(context,true,true,true,true,51000));
        chord.Reset();
        Check(!chord.Step(context,true,true,true,true,60000)); // Already held on startup.
        Check(!chord.Step(context,true,true,true,true,70000));
        Check(!chord.Step(context,true,false,false,false,71000));
        Check(!chord.Step(context,true,true,true,true,72000));
        chord.Cancel();
        Check(!chord.Step(context,true,true,true,true,75000));
    }
    // Frame count and acceleration do not substitute for elapsed wall time.
    Context context{1,2,3,4,5,6,33}; Chord chord;
    chord.Step(context,true,false,false,false,1);
    for (unsigned i=0;i<10000;++i) Check(!chord.Step(context,true,true,true,true,1000+i/10));
    Check(chord.Step(context,true,true,true,true,3000));
    for (unsigned frame=0;frame<10000;++frame)
        for (auto item:ItemIds) Check(SpawnSeed(0,frame,0,item)!=0);
    Check(ItemIds[0]==118 && ItemIds[1]==38);
    std::printf("PASS: %u debug kit checks; no hardware execution\n",checks);
}

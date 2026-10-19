#pragma once
#include "policy.hpp"
namespace isaac::timemachine {
// One ordinary game update plus optional accelerated updates. Observers see
// only successful game ticks. Input shortcuts run only on the ordinary tick.
template<class ReadFrame, class Step, class Inspect, class Observe, class Cancel>
void Dispatch(Ramp& ramp, ReadFrame readFrame, Step step, Inspect inspect, Observe observe, Cancel cancel) {
    unsigned before{},after{};
    const bool readable=readFrame(before);
    step();
    if (!readable || !readFrame(after) || after!=before+1) {
        ramp.Reset(); cancel(); return;
    }
    observe(true);
    Accelerate(ramp,inspect,[&] {
        unsigned previous{},current{};
        const bool valid=readFrame(previous);
        step();
        if (valid && readFrame(current) && current==previous+1) observe(false);
    });
}
}

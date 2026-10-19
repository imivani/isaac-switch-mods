#include "test_fixture.hpp"
#include "../../native/source/program/mod_settings.hpp"
#include "../../native/source/program/generated/timemachine_guards.hpp"
#include <memory>
#include <type_traits>

enum class HookKind { TimeMachineGameInput,TimeMachineInterpolation,TimeMachineGameUpdate };
struct Host {
    World world;
    bool enabled{true},lockEnabled{true},menu{},release{},block{};
    unsigned installs{},inputCalls{},updates{},interpolations{},menuPolls{},observerCancels{};
    std::function<void()> onUpdate;
    std::function<void(bool)> onObserver;
    void Original(HookKind kind,void*) {
        if (kind==HookKind::TimeMachineGameInput) { ++inputCalls; return; }
        if (kind==HookKind::TimeMachineInterpolation) { ++interpolations; return; }
        ++updates;
        if (!world.paused) world.Tick();
        if (onUpdate) onUpdate();
    }
    void Install(std::uintptr_t p) {
        assert(world.Readable(p,20));
        std::memset(reinterpret_cast<void*>(p),0xcc,20); ++installs;
    }
    template<std::size_t N> void Guard(std::uintptr_t p,const std::uint8_t (&bytes)[N]) {
        assert(world.Readable(world.module.Address()+p,N));
        std::memcpy(reinterpret_cast<void*>(world.module.Address()+p),bytes,N);
    }
    Host() {
        namespace n=tmachine::native;
        Guard(n::ProcessInput,n::ProcessInputGuard); Guard(n::GameUpdate,n::GameUpdateGuard);
        Guard(n::InterpolatePositions,n::InterpolatePositionsGuard); Guard(n::FireplaceAi,n::FireplaceAiGuard);
        Guard(n::IsPaused,n::IsPausedGuard); Guard(n::IsActionPressed,n::IsActionPressedGuard);
        Guard(n::IsActionTriggered,n::IsActionTriggeredGuard); Guard(n::MovementInput,n::MovementInputGuard);
    }
};
std::unique_ptr<Host> host;
template<HookKind kind> struct HookStub {
    static void Orig(void* p) { host->Original(kind,p); }
    static void InstallAtPtr(std::uintptr_t p) { host->Install(p); }
};
#define HOOK_DEFINE_TRAMPOLINE(name) struct name : HookStub<HookKind::name>
namespace isaac::feature_memory {
bool Readable(std::uintptr_t p,std::size_t n) { return host->world.Readable(p,n); }
template<class T> bool TryRead(std::uintptr_t p,T& value) { return host->world.TryRead(p,value); }
}
namespace isaac {
bool IsModEnabled(ModFeature f) {
    assert(f==ModFeature::TimeMachine || f==ModFeature::MachineLock);
    return f==ModFeature::TimeMachine ? host->enabled : host->lockEnabled;
}
bool BeforeNativeGameInput(std::uintptr_t game) {
    assert(game==host->world.game.Address()); ++host->menuPolls;
    host->block=host->menu || host->release;
    if (host->release) host->release=false;
    return host->block;
}
bool BlockNativeGameTick(std::uintptr_t game) {
    assert(game==host->world.game.Address()); return host->block || host->menu;
}
void AfterNativeGameTick(std::uintptr_t,bool ordinary) {
    ordinary?++host->world.ordinary:++host->world.extra;
    if (host->onObserver) host->onObserver(ordinary);
}
void CancelNativeGameTick() { ++host->observerCancels; }
}
// Generated from the complete production .cpp. Only platform includes, native
// helper calls, and the trampoline/memory boundaries above are substituted.
#include "native_hook_under_test.cpp"

unsigned cases{};
void Fresh() {
    host=std::make_unique<Host>();
    assert(isaac::InitializeTimeMachine(host->world.module.Address()));
    assert(host->installs==3); ++cases;
}
void Pass() {
    void* game=reinterpret_cast<void*>(host->world.game.Address());
    isaac::TimeMachineGameInput::Callback(game);
    isaac::TimeMachineGameUpdate::Callback(game);
    isaac::TimeMachineInterpolation::Callback(game);
}
int main() {
    Fresh();
    for (unsigned i=0;i<120;++i) Pass();
    assert(host->world.ordinary==120 && host->world.extra>200 && host->menuPolls==120);
    assert(host->inputCalls==120 && host->interpolations==120 && !host->observerCancels); ++cases;
    assert(std::strstr(isaac::TimeMachineStatus(),"Speeding up")); ++cases;
    const auto extra=host->world.extra;
    host->enabled=false;
    for (unsigned i=0;i<20;++i) Pass();
    assert(host->world.ordinary==140 && host->world.extra==extra && !isaac::TimeMachineSpeedTwentieths()); ++cases;
    assert(std::strcmp(isaac::TimeMachineStatus(),"Turned off")==0); ++cases;
    host->menu=true;
    const auto frame=host->world.game.Get<unsigned>(Frame), updates=host->updates;
    for (unsigned i=0;i<20;++i) Pass();
    assert(host->world.game.Get<unsigned>(Frame)==frame && host->updates==updates && host->menuPolls==160);
    assert(host->inputCalls==140 && host->interpolations==140 && !host->observerCancels); ++cases;
    host->menu=false; host->release=true; Pass();
    assert(host->updates==updates && host->inputCalls==140 && host->interpolations==140); ++cases;
    Pass(); assert(host->updates==updates+1 && host->inputCalls==141 && host->interpolations==141); ++cases;
    host->enabled=true; Pass();
    assert(host->world.extra==extra && isaac::TimeMachineSpeedTwentieths()==1); ++cases;

    Fresh();
    host->world.SetEntities({host->world.slot.Address(),host->world.fire.Address()});
    for (unsigned i=0;i<120;++i) Pass();
    assert(host->world.extra>200); ++cases;
    host->world.FireState(8,10,4ull<<32);
    const auto safeExtra=host->world.extra; Pass();
    assert(host->world.extra==safeExtra && std::strstr(isaac::TimeMachineStatus(),"danger")); ++cases;

    // A menu opened by the ordinary observer blocks the same pass's extras
    // and its subsequent interpolation, without canceling the menu itself.
    Fresh();
    for (unsigned i=0;i<120;++i) Pass();
    const auto priorExtra=host->world.extra, priorInterpolate=host->interpolations;
    host->onObserver=[](bool ordinary) { if (ordinary) host->menu=true; };
    Pass();
    assert(host->world.extra==priorExtra && host->interpolations==priorInterpolate && host->menu);
    assert(!host->observerCancels); ++cases;

    Fresh(); host->world.paused=true; Pass();
    assert(host->updates==1 && host->world.ordinary==0 && host->observerCancels==1); ++cases;

    // Successful reset/re-initialization reuses each live hook exactly once.
    Fresh(); isaac::ResetTimeMachine();
    assert(isaac::InitializeTimeMachine(host->world.module.Address()) && host->installs==3); ++cases;
    // All new hook/helper guards are checked before any instruction is patched.
    for (auto offset:{tmachine::native::ProcessInput,tmachine::native::InterpolatePositions,
                     tmachine::native::FireplaceAi,tmachine::native::IsActionTriggered}) {
        host=std::make_unique<Host>(); host->world.module.Put<std::uint8_t>(offset,0);
        assert(!isaac::InitializeTimeMachine(host->world.module.Address()) && host->installs==0); ++cases;
    }
    // Machine Lock in the production callback: a slide during each native
    // update is undone, including with Time Machine off; turning Machine
    // Lock off leaves movement alone.
    Fresh();
    host->onUpdate=[] { host->world.p1.Put<float>(0x314,host->world.p1.Get<float>(0x314)+3.f); };
    for (unsigned i=0;i<10;++i) Pass();
    assert(isaac::MachineLockActive() && host->world.p1.Get<float>(0x314)==3.f); ++cases;
    host->enabled=false; Pass();
    assert(isaac::MachineLockActive() && host->world.p1.Get<float>(0x314)==3.f); ++cases;
    host->lockEnabled=false; Pass();
    assert(!isaac::MachineLockActive() && host->world.p1.Get<float>(0x314)==6.f); ++cases;
    host->enabled=host->lockEnabled=true;
    host->world.movement[0]={0,0}; Pass();
    assert(!isaac::MachineLockActive()); ++cases;
    isaac::ResetTimeMachine(); assert(!isaac::MachineLockActive()); ++cases;
    std::printf("Time Machine: %u production-hook integration cases PASS\n",cases);
}

// Host integration test for the production native/source/program/hit_counter.cpp.
// run_tests.py substitutes only platform boundaries: hook installation,
// readable-memory checks and the three native font calls.
#include "feature_api_host.hpp"
#include "../../native/source/program/mod_settings.hpp"
#include "../../native/source/program/generated/build_profile.hpp"
#include "../../native/source/program/generated/iteration5_guards.hpp"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <string>
#include <vector>

struct Block {
    std::vector<std::uint64_t> data;
    explicit Block(std::size_t size):data((size+7)/8) {}
    std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(data.data()); }
    std::size_t Size() const { return data.size()*8; }
    template<class T> void Put(std::size_t offset,T value) {
        assert(offset+sizeof(value)<=Size());
        std::memcpy(reinterpret_cast<void*>(Address()+offset),&value,sizeof(value));
    }
    template<class T> T Get(std::size_t offset) const {
        T value{}; std::memcpy(&value,reinterpret_cast<const void*>(Address()+offset),sizeof(value)); return value;
    }
    template<std::size_t N> void Bytes(std::size_t offset,const std::uint8_t (&bytes)[N]) {
        assert(offset+N<=Size()); std::memcpy(reinterpret_cast<void*>(Address()+offset),bytes,N);
    }
};
struct Drawn { std::string text; float x,y,scale,alpha; };
struct Host {
    Block module{0xabd000},game{0x347d50},manager{0x77100},player{0x2850},soul{0x2850};
    bool hits{true},greed{true};
    unsigned installs{},toggles{},menuText{},rects{};
    std::vector<Drawn> drawn;
    std::function<bool(void*,float,std::uint64_t,void*,int)> damage;
    Host() {
        namespace n=isaac::iteration5::native;
        module.Bytes(n::PlayerTakeDamage,n::PlayerTakeDamageGuard); module.Bytes(n::GreedBreakChance,n::GreedBreakChanceGuard);
        module.Bytes(n::JamDecision,n::JamDecisionGuard); module.Bytes(n::JamFlagWrite,n::JamFlagWriteGuard);
        module.Bytes(n::JamFlagInit,n::JamFlagInitGuard); module.Bytes(n::FontStringWidth,n::FontStringWidthGuard);
        module.Bytes(n::FontLineHeight,n::FontLineHeightGuard);
        module.Put(isaac::profile::GameGlobal,game.Address()); module.Put(isaac::profile::ManagerGlobal,manager.Address());
        module.Put<float>(isaac::profile::ScreenWidth,480); module.Put<float>(isaac::profile::ScreenHeight,270);
        manager.Put<std::uint8_t>(isaac::profile::ManagerFont,1);
        Run(1234,100,1,0);
    }
    void Run(unsigned seed,unsigned frame,int stage,int type) {
        game.Put<unsigned>(0x25d44,seed); game.Put<unsigned>(0x24f99c,frame); game.Put<int>(0,stage); game.Put<int>(4,type);
    }
    bool Readable(std::uintptr_t p,std::size_t size) const {
        if (!size || p+size<p) return false;
        for (const Block* b:{&module,&game,&manager,&player,&soul})
            if (p>=b->Address() && p-b->Address()<=b->Size() && size<=b->Size()-(p-b->Address())) return true;
        return false;
    }
};
std::unique_ptr<Host> host;
// Keeps replaced fake modules mapped so every case gets a distinct address.
std::vector<std::unique_ptr<Host>> retired;

enum class HookKind { PlayerTakeDamage, GreedBreakChance };
template<HookKind> struct HookStub;
template<> struct HookStub<HookKind::PlayerTakeDamage> {
    static bool Orig(void* p,float amount,std::uint64_t flags,void* source,int countdown) { return host->damage(p,amount,flags,source,countdown); }
    static void InstallAtPtr(std::uintptr_t p) { assert(p==host->module.Address()+isaac::iteration5::native::PlayerTakeDamage); ++host->installs; }
};
template<> struct HookStub<HookKind::GreedBreakChance> {
    static float Orig(void*) { return 42.f; }
    static void InstallAtPtr(std::uintptr_t p) { assert(p==host->module.Address()+isaac::iteration5::native::GreedBreakChance); ++host->installs; }
};
#define HOOK_DEFINE_TRAMPOLINE(name) struct name : HookStub<HookKind::name>

bool menuOpen{};
namespace isaac {
bool IsModMenuOpen() { return menuOpen; }
namespace feature_memory { bool Readable(std::uintptr_t p,std::size_t n) { return host->Readable(p,n); } }
bool IsModEnabled(ModFeature f) {
    assert(f==ModFeature::HitCounter || f==ModFeature::GreedNoJam);
    return f==ModFeature::HitCounter ? host->hits : host->greed;
}
void ToggleMod(ModFeature f) { assert(f==ModFeature::HitCounter); host->hits=!host->hits; ++host->toggles; }
void DrawEidMenuText(const FeatureFrame&,const char*,float,float,std::uint32_t=0xffffff,float=10000.f) { ++host->menuText; }
}
namespace host_native {
int Width(const void* font,const char* text) {
    assert(reinterpret_cast<std::uintptr_t>(font)==host->manager.Address()+isaac::profile::ManagerFont);
    return static_cast<int>(6*std::strlen(text));
}
std::uint16_t Line(const void*) { return 10; }
template<class Color> void DrawText(const void* font,const char* text,float x,float y,float scale,const Color* color) {
    assert(reinterpret_cast<std::uintptr_t>(font)==host->manager.Address()+isaac::profile::ManagerFont);
    assert(color->r==1 && color->g==1 && color->b==1);
    host->drawn.push_back({text,x,y,scale,color->a});
}
}
#include "native_hit_counter_under_test.cpp"

unsigned cases{};
#define CHECK(x) do { assert(x); ++cases; } while (0)
isaac::FeatureFrame Frame(bool inRun=true) {
    return {host->module.Address(),inRun?host->game.Address():0,0,480,270,{nullptr,
        [](void*,const char* text,float x,float y,isaac::FeatureColor c,float scale) { host->drawn.push_back({text,x,y,scale,c.a}); },
        [](void*,float,float,float,float,isaac::FeatureColor) { ++host->rects; }}};
}
bool Damage(std::uint64_t flags=0) {
    return isaac::PlayerTakeDamage::Callback(reinterpret_cast<void*>(host->player.Address()),1.f,flags,nullptr,30);
}
const Drawn& Render() {
    host->drawn.clear(); isaac::RenderHitCounter(Frame()); assert(host->drawn.size()==1); return host->drawn.back();
}
void Fresh() {
    host=std::make_unique<Host>();
    // A real hit lands: TakeDamage returns true and starts invincibility.
    host->damage=[](void* p,float,std::uint64_t,void*,int) {
        auto* b=reinterpret_cast<std::uint8_t*>(p);
        int inv{}; std::memcpy(&inv,b+0x1734,4);
        if (inv>0) return false;
        inv=60; std::memcpy(b+0x1734,&inv,4); return true;
    };
    assert(isaac::InitializeHitCounter(host->module.Address()) && isaac::InitializeGreedMachine(host->module.Address()));
}
int main() {
    Fresh();
    CHECK(host->installs==2);
    CHECK(Render().text=="Hits: 0  Floor: 0");
    CHECK(Damage() && Render().text=="Hits: 1  Floor: 1");
    CHECK(!Damage() && Render().text=="Hits: 1  Floor: 1");       // invincible: no second hit
    host->player.Put<int>(0x1734,0);
    CHECK(Damage(0x10000000) && Render().text=="Hits: 1  Floor: 1"); // no-penalty self damage
    host->player.Put<int>(0x1734,0);
    CHECK(Damage(0x20) && Render().text=="Hits: 1  Floor: 1");       // blood donation
    host->player.Put<int>(0x1734,0);
    // Holy Mantle: refused damage, but the shield break starts invincibility.
    host->damage=[](void* p,float,std::uint64_t,void*,int) { int inv=60; std::memcpy(reinterpret_cast<std::uint8_t*>(p)+0x1734,&inv,4); return false; };
    CHECK(!Damage() && Render().text=="Hits: 2  Floor: 2");
    host->player.Put<int>(0x1734,0);
    // Tainted Forgotten: the soul forwards to the body through the same hook.
    host->damage=[](void*,float a,std::uint64_t f,void* s,int c) {
        static bool inner{};
        if (inner) return true;
        inner=true;
        const bool result=isaac::PlayerTakeDamage::Callback(reinterpret_cast<void*>(host->soul.Address()),a,f|0x800,s,c);
        inner=false; return result;
    };
    CHECK(Damage() && Render().text=="Hits: 3  Floor: 3");
    // Default placement: top center in the HUD font at 100% and 85% opacity.
    const auto& d=Render();
    CHECK(std::fabs(d.x-(240.f-6*17/2.f))<.01f && d.y==4 && d.scale==1.f && std::fabs(d.alpha-.85f)<1e-6f);
    host->Run(1234,900,2,0);
    CHECK(Render().text=="Hits: 3  Floor: 0");                       // next floor
    host->Run(999,3,1,0);
    CHECK(Render().text=="Hits: 0  Floor: 0");                       // new run
    // Hidden counter still counts, so turning it back on is accurate.
    host->hits=false;
    host->damage=[](void*,float,std::uint64_t,void*,int) { return true; };
    Damage();
    host->hits=true;
    CHECK(Render().text=="Hits: 1  Floor: 1");
    // Title screen: no game, nothing drawn outside the settings page.
    host->drawn.clear(); isaac::RenderHitCounter(Frame(false)); CHECK(host->drawn.empty());

    // Layout comes from the saved mod-menu preferences.
    isaac::SetHitCounterLayout(-1,-1,120,75,true);
    const auto& styled=Render();
    CHECK(styled.scale==1.2f && std::fabs(styled.alpha-.75f)<1e-6f);
    float boxW{},boxH{}; isaac::HitCounterBox(boxW,boxH);
    CHECK(std::fabs(boxW-6*17*1.2f)<.01f && std::fabs(boxH-12.f)<.01f);
    unsigned total{},floor{}; isaac::HitCounterCounts(total,floor); CHECK(total==1 && floor==1);
    isaac::SetHitCounterLayout(0,300,120,75,true);
    const auto& moved=Render();
    CHECK(moved.x==0 && std::fabs(moved.y-(270-12.f))<.01f);            // clamped to the bottom-left corner at 120%
    isaac::SetHitCounterLayout(-1,-1,100,85,false);
    CHECK(Render().text=="Hits: 1");
    // While the mod menu is open the live counter hides; the menu draws its
    // own preview (with an outline while it is being moved), also on the title screen.
    menuOpen=true;
    host->drawn.clear(); isaac::RenderHitCounter(Frame()); CHECK(host->drawn.empty());
    host->rects=0; isaac::DrawHitCounterPreview(Frame(false),.5f,true);
    CHECK(host->drawn.size()==1 && std::fabs(host->drawn[0].alpha-.425f)<1e-6f && host->rects>0);
    menuOpen=false;
    isaac::SetHitCounterLayout(-1,-1,100,85,true);

    // Greed machine: zero chance while enabled, the game's own value when off.
    CHECK(isaac::GreedBreakChance::Callback(nullptr)==0.f);
    host->greed=false; CHECK(isaac::GreedBreakChance::Callback(nullptr)==42.f); host->greed=true;
    const std::uint64_t flags=(1ull<<34)|(1ull<<17)|1;
    host->game.Put<std::uint64_t>(0x26fa50,flags);
    isaac::UpdateGreedMachine(host->game.Address());
    CHECK(host->game.Get<std::uint64_t>(0x26fa50)==((1ull<<17)|1));
    host->game.Put<std::uint64_t>(0x26fa50,flags); host->greed=false;
    isaac::UpdateGreedMachine(host->game.Address());
    CHECK(host->game.Get<std::uint64_t>(0x26fa50)==flags);

    // Every guard is checked before anything is installed.
    namespace n=isaac::iteration5::native;
    for (auto offset:{n::PlayerTakeDamage,n::FontStringWidth,n::FontLineHeight}) {
        retired.push_back(std::move(host)); host=std::make_unique<Host>(); host->module.Put<std::uint8_t>(offset,host->module.Get<std::uint8_t>(offset)^0xff);
        CHECK(!isaac::InitializeHitCounter(host->module.Address()) && host->installs==0);
        host->damage=[](void*,float,std::uint64_t,void*,int) { return true; };
        Damage(); host->drawn.clear(); isaac::RenderHitCounter(Frame()); CHECK(host->drawn.empty());
        isaac::DrawHitCounterPreview(Frame(),1.f,true); CHECK(host->drawn.empty());
    }
    for (auto offset:{n::GreedBreakChance,n::JamDecision,n::JamFlagWrite,n::JamFlagInit}) {
        retired.push_back(std::move(host)); host=std::make_unique<Host>(); host->module.Put<std::uint8_t>(offset,host->module.Get<std::uint8_t>(offset)^0xff);
        CHECK(!isaac::InitializeGreedMachine(host->module.Address()) && host->installs==0);
        host->game.Put<std::uint64_t>(0x26fa50,1ull<<34); isaac::UpdateGreedMachine(host->game.Address());
        CHECK(host->game.Get<std::uint64_t>(0x26fa50)==(1ull<<34));
    }
    std::printf("Hit Counter and Greed machine: %u production-hook cases PASS\n",cases);
}

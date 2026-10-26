// Host integration test for the production native/source/program/rplus.cpp.
// run_tests.py substitutes only platform boundaries: hook installation, the
// return-address read, readable-memory checks and the game calls behind Native<>.
#include "feature_api_host.hpp"
#include "../../native/source/program/mod_settings.hpp"
#include "../../native/source/program/generated/build_profile.hpp"
#include "../../native/source/program/generated/rplus_guards.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
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
    template<std::size_t N> void Bytes(std::size_t offset,const std::uint8_t (&bytes)[N]) {
        assert(offset+N<=Size()); std::memcpy(reinterpret_cast<void*>(Address()+offset),bytes,N);
    }
};
struct Spawned { int index, type; unsigned variant, seed; int vardata; };
struct Drawn { std::string text; float x, y; };
struct Host {
    Block module{0xabd000},game{0x26fb00},manager{0x1000},room{0x19d8};
    bool enabled{true},voidOpen{true};
    unsigned curses{},installs{},rolls{},awards{},vanilla{3};
    std::uintptr_t returnAddress{};
    std::vector<Spawned> spawned;
    std::vector<Drawn> drawn;
    Host() {
        namespace n=isaac::rplus::native;
#define PUT(name) module.Bytes(n::name,n::name##Guard)
        PUT(RngRandom); PUT(SpawnClearAward); PUT(SpawnGridEntity); PUT(GetCenterPos); PUT(FindFreePickupSpawnPosition);
        PUT(FixTrapdoorPos); PUT(RandomU32); PUT(IsStageAvailable); PUT(GetCurses); PUT(IncreaseCutsceneCounter);
        PUT(MegaSatanRoll); PUT(MegaSatanEnd); PUT(ChestRoll); PUT(MotherRoomCheck); PUT(AmbushCall); PUT(AmbushFields);
#undef PUT
        module.Put(isaac::profile::GameGlobal,game.Address()); module.Put(isaac::profile::ManagerGlobal,manager.Address());
        room.Put<int>(n::RoomWidth,15); room.Put<int>(n::RoomHeight,9);
        Mother();
    }
    void Mother(int stage=8,int index=-10,int type=5,int seen=1) {
        namespace n=isaac::rplus::native;
        game.Put<int>(n::GameStage,stage); game.Put<int>(n::GameRoomIndex,index); room.Put<int>(n::RoomType,type);
        manager.Put<int>(n::ManagerPersistentData+n::CutsceneCounters+n::MotherCutscene*4,seen);
    }
    bool Readable(std::uintptr_t p,std::size_t size) const {
        if (!size || p+size<p) return false;
        for (const Block* b:{&module,&game,&manager,&room})
            if (p>=b->Address() && p-b->Address()<=b->Size() && size<=b->Size()-(p-b->Address())) return true;
        return false;
    }
};
std::unique_ptr<Host> host;

namespace fake {
struct Vec2 { float x, y; };
unsigned GetCurses(const void* level) { assert(reinterpret_cast<std::uintptr_t>(level)==host->game.Address()); return host->curses; }
bool IsStageAvailable(const void* level,int stage,int type) {
    assert(reinterpret_cast<std::uintptr_t>(level)==host->game.Address() && stage==12 && type==0); return host->voidOpen;
}
Vec2 GetCenterPos(const void* room) { assert(reinterpret_cast<std::uintptr_t>(room)==host->room.Address()); return {320,240}; }
Vec2 FindFree(const void* room,const Vec2* pos,float radius,bool,bool) {
    assert(reinterpret_cast<std::uintptr_t>(room)==host->room.Address() && radius==80.f); return {pos->x,pos->y+40};
}
void FixTrapdoorPos(void*,int* index) { assert(*index>=0); }
unsigned RandomU32() { return 77; }
bool SpawnGridEntity(void* room,int index,int type,unsigned variant,unsigned seed,int vardata) {
    assert(reinterpret_cast<std::uintptr_t>(room)==host->room.Address());
    host->spawned.push_back({index,type,variant,seed,vardata}); return true;
}
}
// Every game call in rplus.cpp goes through Native<F>(offset); answer with a fake of the same type.
template<class F> F TestNative(std::uintptr_t offset) {
    namespace n=isaac::rplus::native;
    void* f = offset==n::GetCurses ? reinterpret_cast<void*>(&fake::GetCurses) :
        offset==n::IsStageAvailable ? reinterpret_cast<void*>(&fake::IsStageAvailable) :
        offset==n::GetCenterPos ? reinterpret_cast<void*>(&fake::GetCenterPos) :
        offset==n::FindFreePickupSpawnPosition ? reinterpret_cast<void*>(&fake::FindFree) :
        offset==n::FixTrapdoorPos ? reinterpret_cast<void*>(&fake::FixTrapdoorPos) :
        offset==n::RandomU32 ? reinterpret_cast<void*>(&fake::RandomU32) :
        offset==n::SpawnGridEntity ? reinterpret_cast<void*>(&fake::SpawnGridEntity) : nullptr;
    assert(f);
    return reinterpret_cast<F>(f);
}
namespace host_native { std::uintptr_t ReturnAddress() { return host->returnAddress; } }

enum class HookKind { PortalRoll, MotherAward };
template<HookKind> struct HookStub;
template<> struct HookStub<HookKind::PortalRoll> {
    static unsigned Orig(void*,unsigned range) { ++host->rolls; return host->vanilla % range; }
    static void InstallAtPtr(std::uintptr_t p) { assert(p==host->module.Address()+isaac::rplus::native::RngRandom); ++host->installs; }
};
template<> struct HookStub<HookKind::MotherAward> {
    static void Orig(void* room) { assert(reinterpret_cast<std::uintptr_t>(room)==host->room.Address()); ++host->awards; }
    static void InstallAtPtr(std::uintptr_t p) { assert(p==host->module.Address()+isaac::rplus::native::SpawnClearAward); ++host->installs; }
};
#define HOOK_DEFINE_TRAMPOLINE(name) struct name : HookStub<HookKind::name>

namespace isaac {
namespace feature_memory { bool Readable(std::uintptr_t p,std::size_t n) { return host->Readable(p,n); } }
bool IsModEnabled(ModFeature f) { assert(f==ModFeature::RepentancePlus); return host->enabled; }
}
#include "native_rplus_under_test.cpp"

unsigned cases{};
#define CHECK(x) do { assert(x); ++cases; } while (0)
unsigned Roll(std::uintptr_t returnOffset,unsigned range) {
    host->returnAddress=host->module.Address()+returnOffset;
    return isaac::PortalRoll::Callback(nullptr,range);
}
void Award() { host->spawned.clear(); isaac::MotherAward::Callback(reinterpret_cast<void*>(host->room.Address())); }
isaac::FeatureFrame Frame() {
    return {host->module.Address(),host->game.Address(),host->room.Address(),480,270,{nullptr,
        [](void*,const char* text,float x,float y,isaac::FeatureColor,float) { host->drawn.push_back({text,x,y}); },
        [](void*,float,float,float,float,isaac::FeatureColor) {}}};
}
int main() {
    namespace n=isaac::rplus::native;
    host=std::make_unique<Host>();
    CHECK(isaac::InitializeRepentancePlus(host->module.Address()) && host->installs==2);
    CHECK(isaac::InitializeRepentancePlus(host->module.Address()) && host->installs==2);  // same module: not twice

    // The two portal rolls return 0 ("portal"); the game's roll still runs every time.
    CHECK(Roll(n::ChestRollReturn,5)==0 && host->rolls==1);
    CHECK(Roll(n::MegaSatanRollReturn,2)==0 && host->rolls==2);
    CHECK(Roll(n::ChestRollReturn+4,5)==3 && Roll(0x123456,100)==3);       // every other call is untouched
    isaac::SetRepentancePlusRules(isaac::rplus::AllRules & ~isaac::rplus::VoidAfterMegaSatan);
    CHECK(Roll(n::MegaSatanRollReturn,2)==1 && Roll(n::ChestRollReturn,5)==0);
    isaac::SetRepentancePlusRules(isaac::rplus::AllRules);
    host->enabled=false;
    CHECK(Roll(n::ChestRollReturn,5)==3 && Roll(n::MegaSatanRollReturn,2)==1);
    host->enabled=true;

    // Mother: the award runs, then a Void portal (trapdoor, vardata 1) below the centre.
    Award();
    CHECK(host->awards==1 && host->spawned.size()==1);
    const auto& s=host->spawned[0];
    CHECK(s.type==17 && s.variant==0 && s.vardata==1 && s.seed==77 && s.index==isaac::rplus::GridIndex(320,280,15,9));
    host->Mother(8,-10,5,0); Award(); CHECK(host->spawned.empty() && host->awards==2);   // ending not seen yet
    host->Mother(); host->voidOpen=false; Award(); CHECK(host->spawned.empty());          // The Void unavailable
    host->voidOpen=true;
    host->Mother(6); Award(); CHECK(host->spawned.empty());                               // Mom, Mausoleum II
    host->Mother(7); host->curses=2; Award(); CHECK(host->spawned.size()==1);             // Corpse XL
    host->curses=0;
    host->Mother(8,84); Award(); CHECK(host->spawned.empty());                            // another room on the floor
    host->Mother(); isaac::SetRepentancePlusRules(isaac::rplus::AllRules & ~isaac::rplus::VoidAfterMother);
    Award(); CHECK(host->spawned.empty());
    isaac::SetRepentancePlusRules(isaac::rplus::AllRules);
    host->enabled=false; Award(); CHECK(host->spawned.empty() && host->awards==8);
    host->enabled=true;

    // Boss Rush wave counter, drawn with a shadow under the hit counter line.
    auto ambush=[&](int type,std::uint8_t active,int wave,int total) {
        host->room.Put<int>(n::RoomType,type);
        host->game.Put<std::uint8_t>(n::GameAmbush+n::AmbushActive,active);
        host->game.Put<int>(n::GameAmbush+n::AmbushWave,wave); host->game.Put<int>(n::GameAmbush+n::AmbushWaveTotal,total);
        host->drawn.clear(); isaac::RenderBossRushWave(Frame());
    };
    ambush(17,1,3,15); CHECK(host->drawn.size()==2 && host->drawn[1].text=="Wave 3/15" && host->drawn[1].y==22);
    ambush(17,0,15,15); CHECK(host->drawn.empty());
    ambush(5,1,3,15); CHECK(host->drawn.empty());
    isaac::SetRepentancePlusRules(isaac::rplus::AllRules & ~isaac::rplus::BossRushWaves);
    ambush(17,1,3,15); CHECK(host->drawn.empty());
    isaac::SetRepentancePlusRules(isaac::rplus::AllRules);

    // A different build: one changed byte at a pinned call site and nothing is installed.
    host=std::make_unique<Host>();
    host->module.Put<std::uint8_t>(n::ChestRoll,0xff);
    CHECK(!isaac::InitializeRepentancePlus(host->module.Address()) && host->installs==0);
    std::printf("Repentance+ Rules: %u native hook checks PASS\n", cases);
}

#include "../../features/instant_break/adapter.hpp"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <string>
#include <vector>

namespace ib = isaac::instant_break;
namespace native = ib::native;
namespace {
constexpr std::uintptr_t Module = 0x7100000000;
constexpr std::uintptr_t Game = 0x7200000000;
constexpr std::uintptr_t Room = 0x7300000000;
constexpr std::uintptr_t Descriptor = 0x7400000000;
constexpr std::uintptr_t Grid = 0x7500000000;
constexpr std::uintptr_t Fire = 0x7600000000;
constexpr std::uintptr_t Source = 0x7700000000;
int cases{};
const char* currentCase{};
[[noreturn]] void Fail(const char* expression, int line) {
    std::fprintf(stderr,"FAIL %s:%d: %s\n",currentCase,line,expression);
    std::exit(1);
}
#define CHECK(expression) do { if (!(expression)) Fail(#expression,__LINE__); } while (false)

struct FakeEngine {
    struct Region { std::uintptr_t address; std::vector<unsigned char> bytes; bool live{true}; };
    std::vector<Region> regions;
    std::vector<std::string> events;
    std::vector<std::uintptr_t> destroys, kills;
    std::function<void(std::uintptr_t)> onDestroy, onKill;
    Region* Find(std::uintptr_t address, std::size_t size) {
        for (auto& region : regions) if (region.live && address >= region.address &&
            address - region.address <= region.bytes.size() &&
            size <= region.bytes.size() - (address - region.address)) return &region;
        return nullptr;
    }
    void Map(std::uintptr_t address, std::size_t size) { regions.push_back({address,std::vector<unsigned char>(size)}); }
    void Unmap(std::uintptr_t address) {
        auto* region = Find(address,1); CHECK(region); region->live = false;
    }
    bool Readable(std::uintptr_t address, std::size_t size) { return size && Find(address,size); }
    template<class T> bool Read(std::uintptr_t address, T& value) {
        auto* region = Find(address,sizeof(value));
        if (!region) return false;
        std::memcpy(&value,region->bytes.data() + address - region->address,sizeof(value));
        return true;
    }
    template<class T> void Put(std::uintptr_t address, T value) {
        auto* region = Find(address,sizeof(value)); CHECK(region);
        std::memcpy(region->bytes.data() + address - region->address,&value,sizeof(value));
    }
    template<class T> T Get(std::uintptr_t address) { T value{}; CHECK(Read(address,value)); return value; }
    void DestroyGrid(std::uintptr_t address) {
        CHECK(Readable(address,0x1a4));
        events.push_back("destroy"); destroys.push_back(address);
        if (onDestroy) onDestroy(address); else Put<std::int32_t>(address + 0x10,1000);
    }
    void KillNpc(std::uintptr_t address) {
        CHECK(Readable(address,0xbd0));
        events.push_back("kill"); kills.push_back(address);
        if (onKill) onKill(address); else Put<std::uint8_t>(address + 0x1c3,1);
    }
};

struct Fixture {
    FakeEngine engine;
    ib::State state;
    std::uint32_t frame{1};
    int originalCalls{};
    bool accepted{true};
    std::function<void()> onOriginal;
    Fixture() {
        engine.Map(Module + native::GameGlobal,8);
        engine.Map(Module + native::PoopVtable,0x50);
        engine.Map(Game,0x24fa00);
        engine.Map(Room,0x1920);
        engine.Map(Descriptor,0x60);
        engine.Map(Fire,0xbd0);
        engine.Put<std::uintptr_t>(Module + native::GameGlobal,Game);
        engine.Put<std::uintptr_t>(Module + native::PoopVtable + 0x28,Module + native::PoopHurt);
        engine.Put<std::uintptr_t>(Game + ib::layout::GameRoom,Room);
        engine.Put<std::uintptr_t>(Room + 8,Descriptor);
        engine.Put<std::uint8_t>(Descriptor + 0x50,1);
        engine.Put<std::uint32_t>(Game + ib::layout::RunSeed,4444);
        engine.Put<std::int32_t>(Game + ib::layout::RoomIndex,84);
        engine.Put<std::int32_t>(Game + ib::layout::Dimension,0);
        engine.Put<std::uint32_t>(Room + ib::layout::EntryFrame,55);
        engine.Put<std::int32_t>(Room + 0x14,13);
        engine.Put<std::int32_t>(Room + 0x18,7);
        engine.Put<std::uint32_t>(Fire + 0x38,33);
        engine.Put<std::uint32_t>(Fire + 0x3c,0);
        engine.Put<std::uint8_t>(Fire + 0x1c2,1);
        engine.Put<std::uint8_t>(Fire + 0x1c3,0);
        engine.Put<std::uint32_t>(Fire + 0x3e8,999);
    }
    void Clear(bool clear) { engine.Put<std::uint8_t>(Descriptor + 0x50,clear ? 1 : 0); }
    std::uintptr_t Poop(std::size_t index = 5, std::int32_t value = 0, std::int32_t variant = 0,
                        std::uint32_t seed = 123, std::uintptr_t address = Grid) {
        engine.Map(address,0x1a4);
        engine.Put<std::uintptr_t>(address,Module + native::PoopVtable);
        engine.Put<std::int32_t>(address + 8,14);
        engine.Put<std::int32_t>(address + 0xc,variant);
        engine.Put<std::int32_t>(address + 0x10,value);
        engine.Put<std::uint32_t>(address + 0x18,seed);
        engine.Put<std::uint32_t>(address + 0x28,static_cast<std::uint32_t>(index));
        engine.Put<std::uintptr_t>(Room + ib::layout::GridArray + index * 8,address);
        return address;
    }
    void PoopState(std::int32_t value, std::uintptr_t address = Grid) { engine.Put<std::int32_t>(address + 0x10,value); }
    void Tick() {
        engine.Put<std::uint32_t>(Game + ib::layout::GameFrame,frame);
        ib::AfterUpdate(state,engine,Module,Game);
        ++frame;
    }
    bool Hit(float damage = 3.5f, std::uint64_t flags = 0x100000002, int countdown = 17,
             std::uintptr_t entity = Fire) {
        const auto source = reinterpret_cast<const void*>(Source);
        return ib::TakeDamage(engine,Module,entity,damage,flags,source,countdown,
            [&](std::uintptr_t target, float amount, std::uint64_t nativeFlags, const void* ref, int frames) {
                CHECK(target == entity); CHECK(std::memcmp(&damage,&amount,sizeof(amount)) == 0);
                CHECK(flags == nativeFlags); CHECK(ref == source); CHECK(frames == countdown);
                ++originalCalls; engine.events.push_back("original");
                if (onOriginal) onOriginal();
                return accepted;
            });
    }
};

template<class Test> void Case(const char* label, Test test) {
    currentCase = label;
    Fixture fixture;
    test(fixture);
    ++cases;
}

void FireCases() {
    Case("orange clear-room hit keeps original arguments and attribution before kill",[](Fixture& f) {
        CHECK(f.Hit()); CHECK(f.originalCalls == 1); CHECK(f.engine.kills.size() == 1);
        CHECK((f.engine.events == std::vector<std::string>{"original","kill"}));
    });
    Case("fire stays unchanged in combat",[](Fixture& f) { f.Clear(false); CHECK(f.Hit()); CHECK(f.engine.kills.empty()); });
    for (std::uint32_t variant : {1u,2u,3u,4u,5u,0xffffffffu}) {
        Case("non-orange fire stays unchanged",[&](Fixture& f) {
            f.engine.Put<std::uint32_t>(Fire + 0x3c,variant); CHECK(f.Hit()); CHECK(f.engine.kills.empty());
        });
    }
    Case("another NPC stays unchanged",[](Fixture& f) {
        f.engine.Put<std::uint32_t>(Fire + 0x38,10); CHECK(f.Hit()); CHECK(f.engine.kills.empty());
    });
    for (float damage : {0.f,-1.f,std::numeric_limits<float>::infinity(),
                        -std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}) {
        Case("non-positive or non-finite hit does not force fire death",[&](Fixture& f) {
            CHECK(f.Hit(damage)); CHECK(f.originalCalls == 1); CHECK(f.engine.kills.empty());
        });
    }
    Case("smallest positive accepted damage still counts as a hit",[](Fixture& f) {
        CHECK(f.Hit(std::numeric_limits<float>::denorm_min())); CHECK(f.engine.kills.size() == 1);
    });
    Case("native rejected hit keeps false return and fire alive",[](Fixture& f) {
        f.accepted = false; CHECK(!f.Hit()); CHECK(f.originalCalls == 1); CHECK(f.engine.kills.empty());
    });
    Case("dead fire not killed twice",[](Fixture& f) {
        CHECK(f.Hit()); CHECK(f.Hit()); CHECK(f.originalCalls == 2); CHECK(f.engine.kills.size() == 1);
    });
    Case("nonexistent fire passes through unchanged",[](Fixture& f) {
        f.engine.Put<std::uint8_t>(Fire + 0x1c2,0); CHECK(f.Hit()); CHECK(f.engine.kills.empty());
    });
    Case("missing fire memory passes through unchanged",[](Fixture& f) {
        f.engine.Unmap(Fire); CHECK(f.Hit()); CHECK(f.engine.kills.empty());
    });
    for (auto entity : {std::uintptr_t{},std::uintptr_t{0x10000},Fire + 1,Fire + 0x10000}) {
        Case("invalid fire pointers never call kill",[&](Fixture& f) {
            CHECK(f.Hit(2,3,4,entity)); CHECK(f.originalCalls == 1); CHECK(f.engine.kills.empty());
        });
    }
    const std::vector<std::function<void(Fixture&)>> invalidate{
        [](Fixture& f) { f.engine.Unmap(Fire); },
        [](Fixture& f) { f.engine.Put<std::uint8_t>(Fire + 0x1c3,1); },
        [](Fixture& f) { f.engine.Put<std::uint8_t>(Fire + 0x1c2,0); },
        [](Fixture& f) { f.engine.Put<std::uint32_t>(Fire + 0x3e8,1000); },
        [](Fixture& f) { f.engine.Put<std::uint32_t>(Fire + 0x3c,2); },
        [](Fixture& f) { f.engine.Put<std::uint32_t>(Fire + 0x38,20); },
        [](Fixture& f) { f.Clear(false); },
        [](Fixture& f) { f.engine.Put<std::uintptr_t>(Module + native::GameGlobal,Game + 8); },
        [](Fixture& f) { f.engine.Put<std::uintptr_t>(Game + ib::layout::GameRoom,Room + 8); },
        [](Fixture& f) { f.engine.Put<std::uintptr_t>(Room + 8,Descriptor + 8); },
        [](Fixture& f) { f.engine.Put<std::uint32_t>(Game + ib::layout::RunSeed,5555); },
        [](Fixture& f) { f.engine.Put<std::int32_t>(Game + ib::layout::RoomIndex,85); },
        [](Fixture& f) { f.engine.Put<std::int32_t>(Game + ib::layout::Dimension,1); },
        [](Fixture& f) { f.engine.Put<std::uint32_t>(Room + ib::layout::EntryFrame,66); },
    };
    for (const auto& change : invalidate) Case("original damage invalidates fire or room; no stale kill",[&](Fixture& f) {
        f.onOriginal = [&] { change(f); }; CHECK(f.Hit()); CHECK(f.originalCalls == 1); CHECK(f.engine.kills.empty());
    });
    Case("unclear before hit cannot become an eligible hit mid-call",[](Fixture& f) {
        f.Clear(false); f.onOriginal = [&] { f.Clear(true); }; CHECK(f.Hit()); CHECK(f.engine.kills.empty());
    });
    Case("no game global means original still runs",[](Fixture& f) {
        f.engine.Unmap(Module + native::GameGlobal); CHECK(f.Hit()); CHECK(f.originalCalls == 1); CHECK(f.engine.kills.empty());
    });
}

void PoopCases() {
    Case("untouched poop never auto-breaks",[](Fixture& f) {
        f.Poop(); for (int i = 0; i < 4; ++i) f.Tick(); CHECK(f.engine.destroys.empty());
    });
    Case("first clear-room hit routes through native destroy once",[](Fixture& f) {
        f.Poop(); f.Tick(); f.PoopState(250); f.Tick(); f.Tick();
        CHECK((f.engine.destroys == std::vector<std::uintptr_t>{Grid}));
    });
    Case("partly damaged poop on entering room does not auto-break",[](Fixture& f) {
        f.Poop(5,750); f.Tick(); f.Tick(); CHECK(f.engine.destroys.empty());
        f.PoopState(875); f.Tick(); CHECK(f.engine.destroys.size() == 1);
    });
    Case("combat damage only updates baseline; next clear-room hit breaks",[](Fixture& f) {
        f.Clear(false); f.Poop(); f.Tick(); f.PoopState(250); f.Tick();
        f.Clear(true); f.Tick(); CHECK(f.engine.destroys.empty());
        f.PoopState(500); f.Tick(); CHECK(f.engine.destroys.size() == 1);
    });
    Case("hit and room clear in same tick follows Lua clear-room check",[](Fixture& f) {
        f.Clear(false); f.Poop(); f.Tick(); f.PoopState(250); f.Clear(true); f.Tick(); CHECK(f.engine.destroys.size() == 1);
    });
    for (std::int32_t variant = 0; variant <= 10; ++variant) {
        Case("each native poop variant uses native destroy rather than custom reward logic",[&](Fixture& f) {
            f.Poop(5,0,variant); f.Tick(); f.PoopState(20); f.Tick(); CHECK(f.engine.destroys.size() == 1);
        });
    }
    Case("tough poop repeats native destroy on later ticks until native destruction",[](Fixture& f) {
        f.Poop(5,0,9); f.Tick(); f.PoopState(20);
        f.engine.onDestroy = [&](std::uintptr_t address) {
            const auto old = f.engine.Get<std::int32_t>(address + 0x10);
            f.PoopState(std::min(1000,old + 200),address);
        };
        f.Tick(); f.Tick(); f.Tick(); f.Tick(); f.Tick(); f.Tick();
        CHECK(f.engine.destroys.size() == 5); CHECK(f.engine.Get<std::int32_t>(Grid + 0x10) == 1000);
    });
    Case("repeated post-update call in same frame cannot multiply damage",[](Fixture& f) {
        f.Poop(); f.Tick(); f.PoopState(100); f.engine.onDestroy = [](std::uintptr_t) {};
        f.Tick(); --f.frame; f.Tick(); CHECK(f.engine.destroys.size() == 1);
        f.Tick(); CHECK(f.engine.destroys.size() == 2);
    });
    Case("becoming uncleared stops repeated tough-poop damage and refreshes baseline",[](Fixture& f) {
        f.Poop(); f.Tick(); f.PoopState(100); f.engine.onDestroy = [](std::uintptr_t) {};
        f.Tick(); f.Clear(false); f.PoopState(150); f.Tick(); f.Clear(true); f.Tick();
        CHECK(f.engine.destroys.size() == 1);
        f.PoopState(200); f.Tick(); CHECK(f.engine.destroys.size() == 2);
    });
    for (std::int32_t value : {-1,1000,1001,0x7fffffff}) {
        Case("destroyed or invalid poop state cannot be destroyed",[&](Fixture& f) {
            f.Poop(); f.Tick(); f.PoopState(value); f.Tick(); CHECK(f.engine.destroys.empty());
        });
    }
    const std::vector<std::function<void(Fixture&)>> badPoop{
        [](Fixture& f) { f.engine.Unmap(Grid); },
        [](Fixture& f) { f.engine.Put<std::uintptr_t>(Room + ib::layout::GridArray + 5 * 8,0); },
        [](Fixture& f) { f.engine.Put<std::uintptr_t>(Room + ib::layout::GridArray + 5 * 8,Grid + 1); },
        [](Fixture& f) { f.engine.Put<std::uintptr_t>(Grid,Module + native::PoopVtable + 8); },
        [](Fixture& f) { f.engine.Put<std::uintptr_t>(Module + native::PoopVtable + 0x28,Module + native::PoopHurt + 4); },
        [](Fixture& f) { f.engine.Put<std::int32_t>(Grid + 8,2); },
        [](Fixture& f) { f.engine.Put<std::uint32_t>(Grid + 0x28,6); },
    };
    for (const auto& corrupt : badPoop) Case("invalid grid never enters a native callback",[&](Fixture& f) {
        f.Poop(); f.Tick(); f.PoopState(250); corrupt(f); f.Tick(); CHECK(f.engine.destroys.empty());
    });
    Case("a replacement object in the same grid cell receives a fresh baseline",[](Fixture& f) {
        f.Poop(); f.Tick(); f.engine.Unmap(Grid); f.Poop(5,250,0,124,Grid + 0x1000);
        f.Tick(); CHECK(f.engine.destroys.empty()); f.PoopState(500,Grid + 0x1000); f.Tick();
        CHECK((f.engine.destroys == std::vector<std::uintptr_t>{Grid + 0x1000}));
    });
    for (bool changeSeed : {false,true}) Case("same address replaced by a new variant or seed resets baseline",[&](Fixture& f) {
        f.Poop(); f.Tick(); f.PoopState(250);
        if (changeSeed) f.engine.Put<std::uint32_t>(Grid + 0x18,999);
        else f.engine.Put<std::int32_t>(Grid + 0xc,4);
        f.Tick(); CHECK(f.engine.destroys.empty()); f.PoopState(500); f.Tick(); CHECK(f.engine.destroys.size() == 1);
    });
    Case("regrown poop uses lower state as a fresh baseline",[](Fixture& f) {
        f.Poop(5,750,4); f.Tick(); f.PoopState(0); f.Tick(); CHECK(f.engine.destroys.empty());
        f.PoopState(250); f.Tick(); CHECK(f.engine.destroys.size() == 1);
    });
    Case("a missing grid forgotten before it reappears",[](Fixture& f) {
        f.Poop(); f.Tick(); f.engine.Put<std::uintptr_t>(Room + ib::layout::GridArray + 5 * 8,0); f.Tick();
        f.engine.Put<std::uintptr_t>(Room + ib::layout::GridArray + 5 * 8,Grid); f.PoopState(250); f.Tick();
        CHECK(f.engine.destroys.empty());
    });
    Case("explicit room entry reset prevents inherited damage",[](Fixture& f) {
        f.Poop(); f.Tick(); f.PoopState(250); f.state.Reset(); f.Tick(); CHECK(f.engine.destroys.empty());
    });
    const std::vector<std::function<void(Fixture&)>> changes{
        [](Fixture& f) { f.engine.Put<std::uint32_t>(Game + ib::layout::RunSeed,5555); },
        [](Fixture& f) { f.engine.Put<std::uint32_t>(Room + ib::layout::EntryFrame,66); },
        [](Fixture& f) { f.engine.Put<std::int32_t>(Game + ib::layout::RoomIndex,85); },
        [](Fixture& f) { f.engine.Put<std::int32_t>(Game + ib::layout::Dimension,1); },
        [](Fixture& f) { f.frame = 0; },
    };
    for (const auto& change : changes) Case("changed run room or rewound frame resets baseline",[&](Fixture& f) {
        f.Poop(); f.Tick(); f.PoopState(250); change(f); f.Tick(); CHECK(f.engine.destroys.empty());
    });
    const std::vector<std::function<void(Fixture&)>> callbacks{
        [](Fixture& f) { f.Clear(false); },
        [](Fixture& f) { f.engine.Put<std::uintptr_t>(Module + native::GameGlobal,0); },
        [](Fixture& f) { f.engine.Put<std::uintptr_t>(Game + ib::layout::GameRoom,0); },
        [](Fixture& f) { f.engine.Put<std::uint32_t>(Room + ib::layout::EntryFrame,66); },
        [](Fixture& f) { f.engine.Put<std::uint32_t>(Game + ib::layout::GameFrame,f.frame + 1); },
    };
    for (const auto& callback : callbacks) Case("native callback that changes room state halts scan immediately",[&](Fixture& f) {
        f.Poop(5); f.Poop(6,0,0,124,Grid + 0x1000); f.Tick();
        f.PoopState(250); f.PoopState(250,Grid + 0x1000);
        f.engine.onDestroy = [&](std::uintptr_t) { callback(f); };
        f.Tick(); CHECK(f.engine.destroys.size() == 1);
    });
    Case("native callback can remove another grid without a stale second callback",[](Fixture& f) {
        f.Poop(5); f.Poop(6,0,0,124,Grid + 0x1000); f.Tick();
        f.PoopState(250); f.PoopState(250,Grid + 0x1000);
        f.engine.onDestroy = [&](std::uintptr_t) { f.engine.Unmap(Grid + 0x1000); };
        f.Tick(); CHECK(f.engine.destroys.size() == 1);
    });
    Case("multiple independent damaged poops each receive a native callback",[](Fixture& f) {
        f.Poop(5); f.Poop(6,0,0,124,Grid + 0x1000); f.Tick();
        f.PoopState(250); f.PoopState(250,Grid + 0x1000); f.Tick(); CHECK(f.engine.destroys.size() == 2);
    });
    Case("highest supported grid index is processed",[](Fixture& f) {
        f.engine.Put<std::int32_t>(Room + 0x14,28); f.engine.Put<std::int32_t>(Room + 0x18,16);
        f.Poop(447); f.Tick(); f.PoopState(250); f.Tick(); CHECK(f.engine.destroys.size() == 1);
    });
    for (auto shape : {std::pair{0,7},std::pair{-1,7},std::pair{13,0},std::pair{13,-1},
                       std::pair{449,1},std::pair{1,449},std::pair{28,17},std::pair{0x7fffffff,0x7fffffff}}) {
        Case("invalid grid dimensions fail closed without overflow",[&](Fixture& f) {
            f.Poop(); f.Tick(); f.PoopState(250);
            f.engine.Put<std::int32_t>(Room + 0x14,shape.first); f.engine.Put<std::int32_t>(Room + 0x18,shape.second);
            f.Tick(); CHECK(f.engine.destroys.empty());
        });
    }
    Case("invalid dimensions also discard stale baseline",[](Fixture& f) {
        f.Poop(); f.Tick(); f.PoopState(250); f.engine.Put<std::int32_t>(Room + 0x14,0); f.Tick();
        f.engine.Put<std::int32_t>(Room + 0x14,13); f.Tick(); CHECK(f.engine.destroys.empty());
    });
    Case("grid outside active room dimensions is not processed",[](Fixture& f) {
        f.Poop(100); f.Tick(); f.PoopState(250); f.Tick(); CHECK(f.engine.destroys.empty());
    });
    Case("mismatched expected game rejects update",[](Fixture& f) {
        f.Poop(); f.Tick(); f.PoopState(250);
        ib::AfterUpdate(f.state,f.engine,Module,Game + 8); CHECK(f.engine.destroys.empty());
    });
    for (std::int32_t dimension : {-1,3,0x7fffffff}) Case("invalid dimension fails closed",[&](Fixture& f) {
        f.Poop(); f.Tick(); f.PoopState(250); f.engine.Put<std::int32_t>(Game + ib::layout::Dimension,dimension);
        f.Tick(); CHECK(f.engine.destroys.empty()); CHECK(f.Hit()); CHECK(f.engine.kills.empty());
    });
}
}
int main() {
    FireCases(); PoopCases();
    std::printf("PASS: %d Instant Break adapter and lifecycle scenarios\n",cases);
}

#include "state.hpp"
#include "adapter.hpp"
#include "../../native/source/program/generated/item_menu_catalogue.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <map>
#include <vector>
using namespace isaac;
using namespace isaac::item_menu;
unsigned checks{};
#define CHECK(...) do { ++checks; assert((__VA_ARGS__)); } while(false)
struct Engine {
    static constexpr std::uintptr_t module=0x1000000,game=0x3000000,manager=0x4000000,input=0x5000000,room=0x6000000,descriptor=0x7000000,players=0x8000000,player=0x9000000,entities=0xa000000,pickup=0xb000000;
    std::map<std::uintptr_t,std::vector<unsigned char>> blocks;
    bool paused{},available{true},noSpace{},changeDuringSpawn{},failSpawn{};
    unsigned spawnCalls{},findCalls{},lastItem{},lastSeed{};
    void Block(std::uintptr_t p,std::size_t n) { blocks[p].resize(n); }
    unsigned char* Address(std::uintptr_t p,std::size_t n) {
        auto i=blocks.upper_bound(p); if(i==blocks.begin()) return nullptr; --i;
        return p>=i->first && p-i->first<=i->second.size() && n<=i->second.size()-(p-i->first) ? i->second.data()+p-i->first : nullptr;
    }
    bool Readable(std::uintptr_t p,std::size_t n) { return Address(p,n)!=nullptr; }
    template<class T> T Read(std::uintptr_t p,std::uintptr_t offset=0) { T v{}; if(auto* q=Address(p+offset,sizeof(v))) std::memcpy(&v,q,sizeof(v)); return v; }
    template<class T> void Write(std::uintptr_t p,std::uintptr_t offset,T v) { auto* q=Address(p+offset,sizeof(v)); assert(q); std::memcpy(q,&v,sizeof(v)); }
    bool Paused(std::uintptr_t p) { CHECK(p==game); return paused; }
    bool Collectible(std::uintptr_t p,unsigned id) { CHECK(p==manager && id>0); return available; }
    Vec2 FindPosition(std::uintptr_t p,const Vec2& wanted) { CHECK(p==room); ++findCalls; return noSpace ? Vec2{0,0} : wanted; }
    std::uintptr_t Spawn(std::uintptr_t p,const Vec2&,unsigned id,unsigned seed) {
        CHECK(p==game && seed); ++spawnCalls; lastItem=id; lastSeed=seed;
        if(changeDuringSpawn) Write<unsigned>(room,0x191c,100);
        Write<unsigned>(pickup,0x38,5); Write<unsigned>(pickup,0x3c,100); Write<unsigned>(pickup,0x40,id); Write<std::uint8_t>(pickup,0x1c2,1);
        return failSpawn ? 0 : pickup;
    }
    Engine() {
        Block(module,0xabcd50); Block(game,0x26faa0); Block(manager,0x36600); Block(input,0x80); Block(room,0x2000);
        Block(descriptor,0x80); Block(players,0x40); Block(player,0x1b00); Block(entities,0x40); Block(pickup,0x600);
        Write<std::uintptr_t>(module,0xabb448,game); Write<std::uintptr_t>(module,0xabcce0,manager); Write<std::uintptr_t>(module,0xaac768,input);
        Write<int>(manager,8,2); Write<std::uintptr_t>(game,0x21550,room); Write<std::uintptr_t>(room,8,descriptor); Write<std::uint8_t>(descriptor,0x50,1);
        Write<unsigned>(game,0x24f99c,50); Write<unsigned>(room,0x191c,1); Write<unsigned>(game,0x25d44,12345);
        Write<std::uintptr_t>(game,0x25c50,players); Write<std::uintptr_t>(game,0x25c58,players+8); Write<std::uintptr_t>(players,0,player);
        Write<unsigned>(player,0x38,1); Write<std::uint8_t>(player,0x1c2,1); Write<std::uint8_t>(player,0x400,1); Write<unsigned>(player,0x19ec,33); Write<unsigned>(player,0x3e8,6789);
        Write<float>(player,0x310,280); Write<float>(player,0x314,280); Write<int>(room,0x14,13); Write<int>(room,0x18,7);
        Write<std::uintptr_t>(room,0x19c8,entities); Write<unsigned>(room,0x19d4,1); Write<std::uintptr_t>(entities,0,player);
    }
    debug_kit::Context Context() { Snapshot s; CHECK(Inspect(*this,module,game,s)); debug_kit::Context c; CHECK(CurrentPlayer(*this,s,0,c)); return c; }
};
int main() {
    State s; std::uint64_t t=100;
    auto key=[&](unsigned b) { s.Update(0,t+=100); return s.Update(Bit(b),t+=100); };
    s.Open(Catalogue,CatalogueCount);
    CHECK(s.Count()==721 && s.Opened());
    CHECK(s.Update(ChordMask|Bit(A),t)==Event::None); // chord cannot confirm
    CHECK(s.Update(Bit(A),t+=100)==Event::None); // must release all first
    s.Update(0,t+=100); CHECK(s.Update(Bit(A),t+=100)==Event::Spawn);
    for(unsigned i=0;i<100;++i) CHECK(s.Update(Bit(A),t+=100)==Event::None); // held A never repeats
    key(Down); CHECK(s.Selection()==1);
    key(L); CHECK(s.Selection()==712);
    key(R); CHECK(s.Selection()==1);
    key(ZR); CHECK(s.Selected());
    key(ZL); CHECK(s.Selected());
    for(unsigned filter=1;filter<=4;++filter) {
        key(Y); const auto wanted=static_cast<Kind>(filter%4);
        CHECK(s.FilterKind()==wanted && s.Count()>0);
        for(unsigned i=0;i<s.Count();++i) CHECK(wanted==Kind::All || s.At(i)->kind==wanted);
        for(unsigned i=0;i<s.Count()*2;++i) { key(Down); CHECK(s.Selection()<s.Count()); }
    }
    // Every real item is reachable; no holes, duplicate IDs, or synthetic IDs.
    bool ids[1000]{};
    for(unsigned i=0;i<s.Count();++i) { const auto* item=s.At(i); CHECK(item->id<1000 && !ids[item->id] && item->name[0]); ids[item->id]=true; }
    CHECK(ids[25] && ids[581] && ids[118] && ids[38]);
    key(X); CHECK(s.Mods() && s.Count()==10); CHECK(key(A)==Event::Toggle);
    CHECK(key(B)==Event::Close && !s.Opened());
    s.Open(Catalogue,CatalogueCount,true); s.Update(0,t+=100);
    CHECK(s.Update(Bit(X),t+=100,true)==Event::None && s.Mods());
    CHECK(key(Plus)==Event::Close);
    // Direction repeats, diagonal input cannot confirm, backward time cannot burst.
    s.Open(Catalogue,CatalogueCount); s.Update(0,0); s.Update(Bit(Down),10);
    CHECK(s.Selection()==1); s.Update(Bit(Down),389); CHECK(s.Selection()==1);
    s.Update(Bit(Down),390); CHECK(s.Selection()==2);
    CHECK(s.Update(Bit(Down)|Bit(A),391)==Event::Spawn); // A is a fresh intentional confirmation
    s.Update(0,392); CHECK(s.Update(Bit(Up)|Bit(A),400)==Event::None); // navigation wins
    State empty; empty.Open(nullptr,0); empty.Update(0,0); CHECK(empty.Update(Bit(A),1)==Event::None);
    {
        Engine e; const auto c=e.Context();
        CHECK(SpawnItem(e,Engine::module,c,581,1)==SpawnResult::Spawned && e.spawnCalls==1 && e.lastItem==581);
        const auto firstSeed=e.lastSeed;
        CHECK(SpawnItem(e,Engine::module,c,25,2)==SpawnResult::Spawned && e.lastItem==25 && e.lastSeed!=firstSeed);
        e.noSpace=true; CHECK(SpawnItem(e,Engine::module,c,118,3)==SpawnResult::NoSpace && e.spawnCalls==2);
        e.noSpace=false; e.available=false; CHECK(SpawnItem(e,Engine::module,c,118,4)==SpawnResult::Unavailable);
        e.available=true; e.failSpawn=true; CHECK(SpawnItem(e,Engine::module,c,118,5)==SpawnResult::Failed);
        e.failSpawn=false; e.changeDuringSpawn=true; CHECK(SpawnItem(e,Engine::module,c,118,6)==SpawnResult::ContextChanged);
    }
    for(unsigned failure=0;failure<12;++failure) {
        Engine e; const auto c=e.Context();
        switch(failure) {
          case 0:e.paused=true;break;
          case 1:e.Write<unsigned>(Engine::room,0x191c,50);break;
          case 2:e.Write<unsigned>(Engine::game,0x25d44,999);break;
          case 3:e.Write<std::uint8_t>(Engine::descriptor,0x50,0);break;
          case 4:e.Write<std::uint8_t>(Engine::player,0x1c3,1);break;
          case 5:e.Write<std::uint8_t>(Engine::player,0x400,0);break;
          case 6:e.Write<unsigned>(Engine::player,0x19ec,0);break;
          case 7:e.Write<unsigned>(Engine::player,0x3e8,99);break;
          case 8:e.Write<std::uintptr_t>(Engine::game,0x25c58,Engine::players);break;
          case 9:e.Write<float>(Engine::game,0x26fa90,1);break;
          case 10:e.Write<int>(Engine::manager,8,0);break;
          case 11:e.Write<std::uintptr_t>(Engine::module,0xabb448,0);break;
        }
        CHECK(SpawnItem(e,Engine::module,c,118,1)==SpawnResult::ContextChanged && e.spawnCalls==0);
    }
    {
        Engine e; CHECK(!FreePosition(e,Engine::room,{NAN,200})); CHECK(!FreePosition(e,Engine::room,{INFINITY,200}));
        CHECK(!FreePosition(e,Engine::room,{280,280})); // player occupied
        CHECK(FreePosition(e,Engine::room,{280,220}));
        e.Write<unsigned>(Engine::room,0x19d4,5000); CHECK(!FreePosition(e,Engine::room,{280,220}));
    }
    std::printf("PASS %u checks; actual picker state, exact catalogue and native memory/spawn adapter; game calls simulated\n",checks);
}

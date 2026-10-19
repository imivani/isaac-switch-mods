#pragma once
#include "../debug_kit/policy.hpp"
#include <cstdint>
#include <cstddef>
namespace isaac::item_menu {
struct Vec2 { float x{}, y{}; };
struct Snapshot {
    std::uintptr_t game{}, manager{}, input{}, room{}, descriptor{}, players{};
    unsigned count{}, frame{}, entered{}, runSeed{};
};
template<class Engine> bool Live(Engine& engine, std::uintptr_t player) {
    return player>0x10000 && !(player&7) && engine.Readable(player,0x1b00) &&
        engine.template Read<unsigned>(player,0x38)==1 && engine.template Read<std::uint8_t>(player,0x1c2) &&
        !engine.template Read<std::uint8_t>(player,0x1c3) && engine.template Read<std::uint8_t>(player,0x400) &&
        engine.template Read<int>(player,0x404)==0;
}
template<class Engine> bool Inspect(Engine& engine, std::uintptr_t module, std::uintptr_t game, Snapshot& out) {
    out={};
    if (engine.template Read<std::uintptr_t>(module,0xabb448)!=game || !engine.Readable(game,0x26faa0)) return false;
    const auto manager=engine.template Read<std::uintptr_t>(module,0xabcce0);
    const auto input=engine.template Read<std::uintptr_t>(module,0xaac768);
    if (!engine.Readable(manager,0x36600) || engine.template Read<int>(manager,8)!=2 || !engine.Readable(input,0x80) ||
        engine.Paused(game) || engine.template Read<float>(game,0x26fa90)>0 || engine.template Read<int>(game,0x26fa9c)!=0) return false;
    const auto room=engine.template Read<std::uintptr_t>(game,0x21550);
    if (!engine.Readable(room,0x19d8)) return false;
    const auto descriptor=engine.template Read<std::uintptr_t>(room,8);
    if (!engine.Readable(descriptor,0x51) || !(engine.template Read<std::uint8_t>(descriptor,0x50)&1)) return false;
    const unsigned frame=engine.template Read<unsigned>(game,0x24f99c), entered=engine.template Read<unsigned>(room,0x191c);
    if (frame-entered<5) return false;
    const auto begin=engine.template Read<std::uintptr_t>(game,0x25c50), end=engine.template Read<std::uintptr_t>(game,0x25c58);
    if (end<=begin || end-begin>8*sizeof(std::uintptr_t) || (end-begin)%sizeof(std::uintptr_t) || !engine.Readable(begin,end-begin)) return false;
    out={game,manager,input,room,descriptor,begin,static_cast<unsigned>((end-begin)/sizeof(std::uintptr_t)),frame,entered,
        engine.template Read<unsigned>(game,0x25d44)};
    return true;
}
template<class Engine> bool CurrentPlayer(Engine& engine, const Snapshot& snap, unsigned index, debug_kit::Context& context) {
    if (index>=snap.count) return false;
    const auto player=engine.template Read<std::uintptr_t>(snap.players,index*sizeof(std::uintptr_t));
    if (!Live(engine,player)) return false;
    const auto controller=engine.template Read<unsigned>(player,0x19ec);
    if (!debug_kit::ValidController(controller)) return false;
    context={snap.game,snap.room,player,snap.runSeed,snap.entered,engine.template Read<unsigned>(player,0x3e8),controller};
    return true;
}
template<class Engine> bool SamePlayer(Engine& engine, std::uintptr_t module, const debug_kit::Context& context, Snapshot& snap) {
    if (!Inspect(engine,module,context.game,snap) || snap.room!=context.room || snap.entered!=context.roomFrame || snap.runSeed!=context.runSeed) return false;
    for (unsigned i=0;i<snap.count;++i) {
        debug_kit::Context current;
        if (CurrentPlayer(engine,snap,i,current) && current==context) return true;
    }
    return false;
}
template<class Engine> bool FreePosition(Engine& engine, std::uintptr_t room, Vec2 position) {
    const int width=engine.template Read<int>(room,0x14), height=engine.template Read<int>(room,0x18);
    if (width<1 || height<1 || width>32 || height>32 || width*height>448 ||
        !(position.x>=40 && position.x<=width*40 && position.y>=120 && position.y<=height*40+80)) return false;
    const int x=static_cast<int>((position.x-40)/40+.5f), y=static_cast<int>((position.y-120)/40+.5f);
    if (x<0 || x>=width || y<0 || y>=height) return false;
    const unsigned index=static_cast<unsigned>(y*width+x);
    if (engine.template Read<std::uintptr_t>(room,0x30+index*8) || engine.template Read<unsigned>(room,0xe98+index*4)>=1000) return false;
    const auto count=engine.template Read<unsigned>(room,0x19d4);
    const auto array=engine.template Read<std::uintptr_t>(room,0x19c8);
    if (count>4096 || (count && !engine.Readable(array,count*sizeof(std::uintptr_t)))) return false;
    for (unsigned i=0;i<count;++i) {
        const auto entity=engine.template Read<std::uintptr_t>(array,i*sizeof(std::uintptr_t));
        if (!engine.Readable(entity,0x410) || !engine.template Read<std::uint8_t>(entity,0x1c2)) continue;
        const auto type=engine.template Read<unsigned>(entity,0x38);
        if (type!=1 && type!=5 && type!=6) continue;
        const float dx=engine.template Read<float>(entity,0x310)-position.x, dy=engine.template Read<float>(entity,0x314)-position.y;
        if (dx*dx+dy*dy<26*26) return false;
    }
    return true;
}
enum class SpawnResult { Spawned, NoSpace, Unavailable, ContextChanged, Failed };
template<class Engine> SpawnResult SpawnItem(Engine& engine, std::uintptr_t module, const debug_kit::Context& context, unsigned item, unsigned serial) {
    Snapshot snap;
    if (!SamePlayer(engine,module,context,snap)) return SpawnResult::ContextChanged;
    if (!item || item>0xffff || !engine.Collectible(snap.manager,item)) return SpawnResult::Unavailable;
    const Vec2 center{engine.template Read<float>(context.player,0x310),engine.template Read<float>(context.player,0x314)};
    constexpr Vec2 offsets[]={{0,-60},{60,0},{0,60},{-60,0},{60,-60},{60,60},{-60,60},{-60,-60}};
    Vec2 position{}; bool found{};
    for (auto offset:offsets) {
        const Vec2 wanted{center.x+offset.x,center.y+offset.y};
        position=engine.FindPosition(snap.room,wanted);
        if (FreePosition(engine,snap.room,position)) { found=true; break; }
    }
    if (!found) return SpawnResult::NoSpace;
    // Revalidate around the native call: procedural effects can move rooms.
    if (!SamePlayer(engine,module,context,snap)) return SpawnResult::ContextChanged;
    const unsigned seed=debug_kit::SpawnSeed(snap.runSeed,snap.frame+serial,context.playerSeed,item);
    const auto pickup=engine.Spawn(snap.game,position,item,seed);
    if (!SamePlayer(engine,module,context,snap)) return SpawnResult::ContextChanged;
    if (!engine.Readable(pickup,0x568) || engine.template Read<unsigned>(pickup,0x38)!=5 ||
        engine.template Read<unsigned>(pickup,0x3c)!=100 || engine.template Read<unsigned>(pickup,0x40)!=item ||
        !engine.template Read<std::uint8_t>(pickup,0x1c2) || engine.template Read<std::uint8_t>(pickup,0x1c3)) return SpawnResult::Failed;
    return SpawnResult::Spawned;
}
}

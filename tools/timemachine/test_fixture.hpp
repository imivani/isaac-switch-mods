#pragma once
#include "../../features/timemachine/adapter.hpp"
#include "../../features/timemachine/dispatch.hpp"
#include <array>
#include <cassert>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>
#include <functional>

namespace tmachine = isaac::timemachine;
using namespace tmachine;
using namespace isaac;
struct Block {
    std::vector<std::uint64_t> data;
    explicit Block(std::size_t size):data((size+7)/8) {}
    std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(data.data()); }
    std::size_t Size() const { return data.size()*8; }
    template<class T> void Put(std::size_t offset,T value) {
        assert(offset+sizeof(value)<=Size());
        std::memcpy(reinterpret_cast<void*>(Address()+offset),&value,sizeof(value));
    }
    template<class T> T Get(std::size_t offset) const { return detail::Load<T>(Address(),offset); }
};
struct World {
    Block module{0xabcd00},game{0x347d50},manager{16},room{0x19d8},descriptor{0x58};
    Block players{64},entities{64},p1{0x2850},p2{0x2850},slot{0x410},fire{0xb78},other{0x410};
    std::vector<Block*> blocks;
    std::array<unsigned,2> pressed{2,0},triggered{};
    std::array<Direction,2> movement{Direction{1,0},Direction{0,0}};
    bool paused{}, fireReadable{true};
    unsigned pressedCalls{},triggeredCalls{},movementCalls{},ordinary{},extra{},canceled{},writes{};
    World():blocks{&module,&game,&manager,&room,&descriptor,&players,&entities,&p1,&p2,&slot,&fire,&other} {
        module.Put(profile::GameGlobal,game.Address()); module.Put(profile::ManagerGlobal,manager.Address());
        manager.Put<int>(8,2); game.Put(profile::GameRoom,room.Address()); room.Put(8,descriptor.Address());
        descriptor.Put<std::uint8_t>(0x50,1); game.Put<unsigned>(Frame,100); room.Put<unsigned>(0x191c,10);
        game.Put<unsigned>(0x25d44,123); game.Put<int>(0,2); game.Put<int>(4,0);
        game.Put<int>(0x21558,8); game.Put<int>(0x21560,0);
        Player(p1,11,1); Player(p2,12,33); Machine(1,1);
        Live(fire,33,0,13); FireState(3,1.f,0); Live(other,13,0,14);
        SetPlayers({p1.Address()}); SetEntities({p1.Address(),slot.Address()});
    }
    static void Live(Block& b,unsigned type,unsigned variant,unsigned seed) {
        b.Put(0x38,type); b.Put(0x3c,variant); b.Put(0x3e8,seed);
        b.Put<std::uint8_t>(0x1c1,1); b.Put<std::uint8_t>(0x1c2,1); b.Put<std::uint8_t>(0x1c3,0);
        b.Put<float>(0x310,0); b.Put<float>(0x314,0); b.Put<float>(0x344,10);
    }
    static void Player(Block& b,unsigned seed,unsigned controller) {
        Live(b,1,0,seed); b.Put<unsigned>(0x19ec,controller); b.Put<std::uint8_t>(0x400,1);
        b.Put<int>(0x16e0,99); b.Put<int>(0x16d4,99); b.Put<int>(0x16dc,99); b.Put<int>(0x16bc,12);
    }
    void Machine(unsigned variant,int state) {
        Live(slot,6,variant,15); slot.Put<float>(0x310,20); slot.Put<int>(0x400,state);
        slot.Put<std::int16_t>(0x40a,0);
    }
    void FireState(int state,float hp,std::uint64_t collision) {
        fire.Put<int>(0xb70,state); fire.Put<float>(0x354,hp); fire.Put(0x1e0,collision);
    }
    void SetPlayers(std::initializer_list<std::uintptr_t> values) {
        unsigned i=0; for (auto p:values) players.Put(i++*8,p);
        game.Put(profile::PlayersBegin,players.Address()); game.Put(profile::PlayersEnd,players.Address()+i*8);
    }
    void SetEntities(std::initializer_list<std::uintptr_t> values) {
        unsigned i=0; for (auto p:values) entities.Put(i++*8,p);
        room.Put(profile::RoomEntities,entities.Address()); room.Put<unsigned>(profile::RoomEntityCount,i);
    }
    bool Readable(std::uintptr_t p,std::size_t size) {
        if (!size || p+size<p) return false;
        for (const auto* b:blocks) {
            const auto limit=b==&fire && !fireReadable ? 0x410 : b->Size();
            if (p>=b->Address() && p-b->Address()<=limit && size<=limit-(p-b->Address())) return true;
        }
        return false;
    }
    template<class T> bool TryRead(std::uintptr_t p,T& value) {
        if (!Readable(p,sizeof(value))) return false;
        std::memcpy(&value,reinterpret_cast<const void*>(p),sizeof(value)); return true;
    }
    template<class T> void Write(std::uintptr_t p,T value) {
        assert(Readable(p,sizeof(value)));
        std::memcpy(reinterpret_cast<void*>(p),&value,sizeof(value)); ++writes;
    }
    bool Paused(std::uintptr_t g) { assert(g==game.Address()); return paused; }
    unsigned PlayerIndex(std::uintptr_t p) const {
        assert(p==p1.Address() || p==p2.Address()); return p==p1.Address()?0:1;
    }
    bool Pressed(std::uintptr_t m,unsigned action,unsigned controller,std::uintptr_t p) {
        assert(m==manager.Address() && action<=13 && controller==detail::Load<unsigned>(p,0x19ec));
        ++pressedCalls; return (pressed[PlayerIndex(p)]&(1u<<action))!=0;
    }
    bool Triggered(std::uintptr_t m,unsigned action,unsigned controller,std::uintptr_t p) {
        assert(m==manager.Address() && action<4 && controller==detail::Load<unsigned>(p,0x19ec));
        ++triggeredCalls; return (triggered[PlayerIndex(p)]&(1u<<action))!=0;
    }
    Direction Movement(std::uintptr_t p) { ++movementCalls; return movement[PlayerIndex(p)]; }
    Snapshot InspectNow(BlockReason& reason) { return tmachine::Inspect(*this,module.Address(),game.Address(),reason); }
    void Tick() { game.Put<unsigned>(Frame,game.Get<unsigned>(Frame)+1); }
    void DispatchNow(Ramp& ramp,bool enabled=true,std::function<void(bool)> observer={}) {
        BlockReason reason{};
        Dispatch(ramp,[&](unsigned& frame) { return TryRead(game.Address()+Frame,frame); },
            [&] { if (!paused) Tick(); },
            [&] { return enabled?InspectNow(reason):Snapshot{}; },
            [&](bool normal) { normal?++ordinary:++extra; if (observer) observer(normal); },
            [&] { ++canceled; });
    }
};

#pragma once
#include "policy.hpp"
#include "../../native/source/program/generated/build_profile.hpp"
#include <cstddef>
#include <cstring>

namespace isaac::timemachine {
inline constexpr std::uintptr_t Frame = 0x24f99c;
struct Direction { float x{}, y{}; };
enum class BlockReason {
    Unavailable, Disabled, NotInRun, Paused, Transition, RoomNotClear,
    RoomSettling, PlayerUnavailable, ControlsDisabled, ControllerUnavailable,
    OtherAction, FreshMovement, InvalidState, Hazard, NoMachine,
    UnsupportedMachine, MachineBusy, NotTouching, NotPushing,
    NoResources, LowHealth, PartnerMoving, Ready
};
inline const char* ReasonText(BlockReason reason) {
    switch (reason) {
    case BlockReason::Unavailable: return "Not available for this game build";
    case BlockReason::Disabled: return "Turned off";
    case BlockReason::NotInRun: return "Waiting for a run";
    case BlockReason::Paused: return "Game paused";
    case BlockReason::Transition: return "Room or floor changing";
    case BlockReason::RoomNotClear: return "Clear the room first";
    case BlockReason::RoomSettling: return "Waiting for room entry to finish";
    case BlockReason::PlayerUnavailable: return "Waiting for a living player";
    case BlockReason::ControlsDisabled: return "Player controls are temporarily locked";
    case BlockReason::ControllerUnavailable: return "Waiting for the player's controller";
    case BlockReason::OtherAction: return "Release shooting, map and other actions";
    case BlockReason::FreshMovement: return "Keep holding toward the machine";
    case BlockReason::InvalidState: return "Waiting for valid game state";
    case BlockReason::Hazard: return "An enemy, fire or other danger is nearby";
    case BlockReason::NoMachine: return "Find a supported machine or beggar";
    case BlockReason::UnsupportedMachine: return "This machine is not supported";
    case BlockReason::MachineBusy: return "Waiting for the machine";
    case BlockReason::NotTouching: return "Move closer to the machine";
    case BlockReason::NotPushing: return "Hold toward the machine";
    case BlockReason::NoResources: return "Not enough coins, keys or bombs";
    case BlockReason::LowHealth: return "Normal speed protects the last heart";
    case BlockReason::PartnerMoving: return "A co-op partner is moving elsewhere";
    case BlockReason::Ready: return "Speeding up while you keep holding";
    }
    return "Waiting";
}
namespace detail {
// Every load below is covered by a readable object/array span in Inspect.
template<class T> T Load(std::uintptr_t p, std::uintptr_t offset = 0) {
    T value{};
    std::memcpy(&value,reinterpret_cast<const void*>(p+offset),sizeof(value));
    return value;
}
struct Player {
    std::uintptr_t address{};
    unsigned seed{};
    float x{}, y{}, radius{}, dx{}, dy{};
    int coins{}, keys{}, bombs{}, red{}, soul{}, bone{}, eternal{};
};
}

// The game keeps extinguished fireplaces alive, with HP=1 and State=3,
// and clears both collision classes. ai_fireplace 0x111d20 also clears Dead.
// Only these exact ordinary-fire states are harmless; burning, white, moving,
// unknown, unreadable and relighting fireplaces remain a reason to stop.
template<class Engine>
bool ExtinguishedFire(Engine& engine, std::uintptr_t entity) {
    using detail::Load;
    return engine.Readable(entity,0xb74) && Load<unsigned>(entity,0x38)==33 &&
        Load<unsigned>(entity,0x3c)<=3 && Load<int>(entity,0xb70)==3 &&
        Load<float>(entity,0x354)==1.f && Load<std::uint64_t>(entity,0x1e0)==0;
}

// Shared by the actual Switch hook and the host's allocated native-memory
// fixtures. Tests replace only memory validation and native function calls.
template<class Engine>
Snapshot Inspect(Engine& engine, std::uintptr_t module, std::uintptr_t expectedGame,
                 BlockReason& reason) {
    using detail::Load;
    Snapshot out;
    reason = BlockReason::NotInRun;
    auto reject = [&](BlockReason why) { reason=why; return Snapshot{}; };
    auto readable = [&](std::uintptr_t p, std::size_t size) { return engine.Readable(p,size); };
    auto alive = [&](std::uintptr_t p, std::size_t size) {
        return p>0x10000 && !(p&7) && readable(p,size) &&
            Load<std::uint8_t>(p,profile::EntityExists) && !Load<std::uint8_t>(p,profile::EntityDead);
    };
    std::uintptr_t game{}, manager{}, room{}, descriptor{}, begin{}, end{}, array{};
    if (!module || !engine.TryRead(module+profile::GameGlobal,game) || game!=expectedGame ||
        !engine.TryRead(module+profile::ManagerGlobal,manager) || !readable(manager,12) ||
        Load<int>(manager,8)!=2 || !readable(game,0x347d4c)) return out;
    if (engine.Paused(game)) return reject(BlockReason::Paused);
    if (Load<float>(game,0x26fa90)>0 || Load<int>(game,0x26fa9c)!=0)
        return reject(BlockReason::Transition);
    room=Load<std::uintptr_t>(game,profile::GameRoom);
    if (!readable(room,0x19d8)) return reject(BlockReason::InvalidState);
    descriptor=Load<std::uintptr_t>(room,8);
    if (!readable(descriptor,0x51)) return reject(BlockReason::InvalidState);
    if (!(Load<std::uint8_t>(descriptor,0x50)&1)) return reject(BlockReason::RoomNotClear);
    out.frame=Load<unsigned>(game,Frame);
    const auto entered=Load<unsigned>(room,0x191c);
    if (out.frame-entered<5) return reject(BlockReason::RoomSettling);
    begin=Load<std::uintptr_t>(game,profile::PlayersBegin);
    end=Load<std::uintptr_t>(game,profile::PlayersEnd);
    if (end<=begin || end-begin>8*sizeof(std::uintptr_t) ||
        (end-begin)%sizeof(std::uintptr_t) || !readable(begin,end-begin))
        return reject(BlockReason::PlayerUnavailable);
    const auto playersCount=(end-begin)/sizeof(std::uintptr_t);
    detail::Player players[8]{};
    for (unsigned i=0;i<playersCount;++i) {
        auto& p=players[i];
        p.address=Load<std::uintptr_t>(begin+i*sizeof(std::uintptr_t));
        if (!alive(p.address,0x2850) || Load<unsigned>(p.address,profile::EntityType)!=1)
            return reject(BlockReason::PlayerUnavailable);
        if (!Load<std::uint8_t>(p.address,0x400) || Load<int>(p.address,0x404)>0)
            return reject(BlockReason::ControlsDisabled);
        const unsigned controller=Load<unsigned>(p.address,0x19ec);
        if (!ValidController(controller)) return reject(BlockReason::ControllerUnavailable);
        unsigned pressed{},triggered{};
        for (unsigned action=0;action<=13;++action)
            if (engine.Pressed(manager,action,controller,p.address)) pressed|=1u<<action;
        for (unsigned action=0;action<4;++action)
            if (engine.Triggered(manager,action,controller,p.address)) triggered|=1u<<action;
        if (!InputAllowsAcceleration(pressed,triggered))
            return reject((pressed&~15u) ? BlockReason::OtherAction : BlockReason::FreshMovement);
        const auto direction=engine.Movement(p.address);
        p.dx=direction.x; p.dy=direction.y;
        if (!(p.dx>=-1.01f && p.dx<=1.01f && p.dy>=-1.01f && p.dy<=1.01f))
            return reject(BlockReason::InvalidState);
        p.x=Load<float>(p.address,profile::EntityX); p.y=Load<float>(p.address,profile::EntityY);
        p.radius=Load<float>(p.address,0x344); p.seed=Load<unsigned>(p.address,0x3e8);
        p.coins=Load<int>(p.address,0x16e0); p.keys=Load<int>(p.address,0x16d4);
        p.bombs=Load<int>(p.address,0x16dc); p.red=Load<int>(p.address,0x16bc);
        p.soul=Load<int>(p.address,0x16c4); p.bone=Load<int>(p.address,0x2450);
        p.eternal=Load<int>(p.address,0x16c0);
        if (p.red<0 || p.red>99 || p.soul<0 || p.soul>99 || p.bone<0 || p.bone>99 ||
            p.eternal<0 || p.eternal>99) return reject(BlockReason::InvalidState);
    }
    array=Load<std::uintptr_t>(room,profile::RoomEntities);
    const auto count=Load<unsigned>(room,profile::RoomEntityCount);
    if (count>1024 || (count && !readable(array,count*sizeof(std::uintptr_t))))
        return reject(BlockReason::InvalidState);
    float best=1e12f;
    bool playerContact[8]{};
    reason=BlockReason::NoMachine;
    for (unsigned i=0;i<count;++i) {
        const auto entity=Load<std::uintptr_t>(array+i*sizeof(std::uintptr_t));
        if (!alive(entity,0x410)) continue;
        const auto type=Load<unsigned>(entity,profile::EntityType);
        if (Hazard(type) && !(type==33 && ExtinguishedFire(engine,entity)))
            return reject(BlockReason::Hazard);
        if (type!=6 || !Load<std::uint8_t>(entity,profile::EntityVisible)) continue;
        const auto variant=Load<unsigned>(entity,profile::EntityVariant);
        const auto state=Load<int>(entity,0x400);
        if (!Enabled(variant)) { if (!out.eligible) reason=BlockReason::UnsupportedMachine; continue; }
        if (state<1 || state>5 || state==3) { if (!out.eligible) reason=BlockReason::MachineBusy; continue; }
        const float x=Load<float>(entity,profile::EntityX),y=Load<float>(entity,profile::EntityY);
        const float radius=Load<float>(entity,0x344);
        for (unsigned j=0;j<playersCount;++j) {
            const auto& p=players[j];
            const bool touching=Contact(p.x,p.y,p.radius,x,y,radius);
            const bool toward=Toward(p.x,p.y,x,y,p.dx,p.dy);
            const bool affordable=CanAfford(variant,p.coins,p.keys,p.bombs,p.red,p.soul,p.bone,p.eternal);
            const bool finishing=state!=1 || Load<std::int16_t>(entity,0x40a)>0;
            const auto decision=DecideContact(touching,toward,variant,affordable,finishing);
            if (decision==ContactDecision::NormalSpeedOnly) return reject(BlockReason::LowHealth);
            if (decision!=ContactDecision::Accelerate) {
                if (!out.eligible) reason=!touching ? BlockReason::NotTouching :
                    !toward ? BlockReason::NotPushing : BlockReason::NoResources;
                continue;
            }
            playerContact[j]=true;
            const float distance=(x-p.x)*(x-p.x)+(y-p.y)*(y-p.y);
            if (distance>=best) continue;
            best=distance;
            out.identity={game,room,p.address,entity,Load<unsigned>(game,0x25d44),entered,
                p.seed,Load<unsigned>(entity,0x3e8),Load<int>(game,0x21558),Load<int>(game,0x21560),
                Load<int>(game,0),Load<int>(game,4)};
            out.eligible=true;
        }
    }
    for (unsigned i=0;i<playersCount;++i)
        if ((players[i].dx || players[i].dy) && !playerContact[i]) {
            if (out.eligible) return reject(BlockReason::PartnerMoving);
            return out;
        }
    if (out.eligible) reason=BlockReason::Ready;
    return out;
}

// Machine Lock: once a player is accepted by Inspect, keep them on the spot
// where contact began while they keep holding toward that machine. Contact
// pushes that are not exactly aligned otherwise slide the player around the
// machine's round collision until they lose contact.
inline constexpr unsigned LockBusyFrames = 150;
struct Lock {
    bool active{}, busy{};
    std::uintptr_t game{}, room{}, player{}, machine{};
    unsigned roomFrame{}, playerSeed{}, machineSeed{}, variant{}, busySince{};
    float x{}, y{};
    void Reset() { *this = {}; }
};
// Reasons computed before any machine is examined. Each one means the player
// should have full control again: danger, a new action, a transition, etc.
constexpr bool LockHardStop(BlockReason reason) {
    switch (reason) {
    case BlockReason::Unavailable: case BlockReason::Disabled: case BlockReason::NotInRun:
    case BlockReason::Transition: case BlockReason::RoomNotClear: case BlockReason::RoomSettling:
    case BlockReason::PlayerUnavailable: case BlockReason::ControlsDisabled:
    case BlockReason::ControllerUnavailable: case BlockReason::OtherAction:
    case BlockReason::InvalidState: case BlockReason::Hazard: case BlockReason::LowHealth:
    case BlockReason::PartnerMoving:
        return true;
    default: return false;
    }
}
template<class Engine>
void EngageLock(Engine& engine, Lock& lock, const Snapshot& state) {
    using detail::Load;
    if (!state.eligible) return;
    const auto& id=state.identity;
    if (lock.active && lock.game==id.game && lock.room==id.room && lock.roomFrame==id.roomFrame &&
        lock.player==id.player && lock.playerSeed==id.playerSeed &&
        lock.machine==id.machine && lock.machineSeed==id.machineSeed) return;
    lock.Reset();
    if (!engine.Readable(id.player,0x2850) || !engine.Readable(id.machine,0x410)) return;
    lock.active=true;
    lock.game=id.game; lock.room=id.room; lock.roomFrame=id.roomFrame;
    lock.player=id.player; lock.playerSeed=id.playerSeed;
    lock.machine=id.machine; lock.machineSeed=id.machineSeed;
    lock.variant=Load<unsigned>(id.machine,profile::EntityVariant);
    lock.x=Load<float>(id.player,profile::EntityX); lock.y=Load<float>(id.player,profile::EntityY);
}
// Runs after every native update, before the next inspection. Revalidates
// every object from scratch, then puts the player back on the locked spot.
// Returns true when the player was pinned.
template<class Engine>
bool HoldLock(Engine& engine, Lock& lock, std::uintptr_t module, std::uintptr_t game, BlockReason last) {
    using detail::Load;
    if (!lock.active) return false;
    auto release=[&] { lock.Reset(); return false; };
    if (LockHardStop(last)) return release();
    auto alive=[&](std::uintptr_t p, std::size_t size) {
        return p>0x10000 && !(p&7) && engine.Readable(p,size) &&
            Load<std::uint8_t>(p,profile::EntityExists) && !Load<std::uint8_t>(p,profile::EntityDead);
    };
    std::uintptr_t current{};
    if (!module || !engine.TryRead(module+profile::GameGlobal,current) || current!=game ||
        game!=lock.game || !engine.Readable(game,0x347d4c)) return release();
    const auto room=Load<std::uintptr_t>(game,profile::GameRoom);
    if (room!=lock.room || !engine.Readable(room,0x19d8) ||
        Load<unsigned>(room,0x191c)!=lock.roomFrame) return release();
    const auto p=lock.player, m=lock.machine;
    if (!alive(p,0x2850) || Load<unsigned>(p,profile::EntityType)!=1 ||
        Load<unsigned>(p,0x3e8)!=lock.playerSeed) return release();
    if (!Load<std::uint8_t>(p,0x400) || Load<int>(p,0x404)>0) return release();
    if (!alive(m,0x410) || Load<unsigned>(m,profile::EntityType)!=6 ||
        Load<unsigned>(m,profile::EntityVariant)!=lock.variant ||
        Load<unsigned>(m,0x3e8)!=lock.machineSeed ||
        !Load<std::uint8_t>(m,profile::EntityVisible)) return release();
    // State 3 is a broken or jammed machine; it never accepts payment again.
    const auto state=Load<int>(m,0x400);
    if (state==3) return release();
    const auto frame=Load<unsigned>(game,Frame);
    if (state<1 || state>5) {
        if (!lock.busy) { lock.busy=true; lock.busySince=frame; }
        else if (frame-lock.busySince>LockBusyFrames) return release();
    } else lock.busy=false;
    const auto direction=engine.Movement(p);
    if (!(direction.x>=-1.01f && direction.x<=1.01f && direction.y>=-1.01f && direction.y<=1.01f))
        return release();
    const float mx=Load<float>(m,profile::EntityX), my=Load<float>(m,profile::EntityY);
    if (!Toward(lock.x,lock.y,mx,my,direction.x,direction.y)) return release();
    if (!Contact(lock.x,lock.y,Load<float>(p,0x344),mx,my,Load<float>(m,0x344))) return release();
    const bool affordable=CanAfford(lock.variant,Load<int>(p,0x16e0),Load<int>(p,0x16d4),
        Load<int>(p,0x16dc),Load<int>(p,0x16bc),Load<int>(p,0x16c4),Load<int>(p,0x2450),Load<int>(p,0x16c0));
    const bool finishing=state!=1 || Load<std::int16_t>(m,0x40a)>0;
    if (!affordable && (CostsHealth(lock.variant) || !finishing)) return release();
    engine.Write(p+profile::EntityX,lock.x);
    engine.Write(p+profile::EntityY,lock.y);
    return true;
}
}

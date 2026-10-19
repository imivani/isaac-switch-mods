#pragma once
#include "native_profile.hpp"
#include "state.hpp"
#include <limits>

namespace isaac::instant_break {
namespace layout {
inline constexpr std::uintptr_t GameRoom = 0x21550;
inline constexpr std::uintptr_t GameFrame = 0x24f99c;
inline constexpr std::uintptr_t RunSeed = 0x25d44;
inline constexpr std::uintptr_t RoomIndex = 0x21558;
inline constexpr std::uintptr_t Dimension = 0x21560;
inline constexpr std::uintptr_t EntryFrame = 0x191c;
inline constexpr std::uintptr_t GridArray = 0x30;
}
struct RoomFrame {
    RoomIdentity identity{};
    std::uint32_t frame{};
    bool clear{};
};
inline bool Pointer(std::uintptr_t p) { return p > 0x10000 && !(p & 7); }
template<class Engine, class T>
bool At(Engine& engine, std::uintptr_t p, std::uintptr_t offset, T& value) {
    return p > 0x10000 && p + offset >= p && engine.Read(p + offset, value);
}
template<class Engine>
bool ReadRoom(Engine& engine, std::uintptr_t module, std::uintptr_t expectedGame, RoomFrame& out) {
    auto& key = out.identity;
    std::uint8_t flags{};
    return Pointer(module) && At(engine,module,native::GameGlobal,key.game) && Pointer(key.game) &&
        (!expectedGame || key.game == expectedGame) &&
        At(engine,key.game,layout::GameRoom,key.room) && Pointer(key.room) &&
        At(engine,key.room,8,key.descriptor) && Pointer(key.descriptor) &&
        At(engine,key.descriptor,0x50,flags) &&
        At(engine,key.game,layout::RunSeed,key.runSeed) &&
        At(engine,key.game,layout::RoomIndex,key.index) &&
        At(engine,key.game,layout::Dimension,key.dimension) && key.dimension >= 0 && key.dimension <= 2 &&
        At(engine,key.room,layout::EntryFrame,key.entryFrame) &&
        At(engine,key.game,layout::GameFrame,out.frame) && ((out.clear = (flags & 1) != 0),true);
}
template<class Engine>
bool ReadPoop(Engine& engine, std::uintptr_t module, std::uintptr_t address,
              std::size_t index, Poop& out) {
    std::uintptr_t vtable{}, hurt{};
    std::int32_t type{};
    std::uint32_t ownIndex{};
    if (!Pointer(address) || !engine.Readable(address,0x1a4) ||
        !At(engine,address,0,vtable) || vtable != module + native::PoopVtable ||
        !At(engine,vtable,0x28,hurt) || hurt != module + native::PoopHurt ||
        !At(engine,address,8,type) || type != 14 ||
        !At(engine,address,0x28,ownIndex) || ownIndex != index ||
        !At(engine,address,0xc,out.variant) || !At(engine,address,0x10,out.state) ||
        !At(engine,address,0x18,out.seed)) return false;
    out.address = address;
    return true;
}
template<class Engine>
void AfterUpdate(State& state, Engine& engine, std::uintptr_t module, std::uintptr_t game) {
    RoomFrame room;
    std::int32_t width{}, height{};
    if (!ReadRoom(engine,module,game,room) ||
        !At(engine,room.identity.room,0x14,width) ||
        !At(engine,room.identity.room,0x18,height) ||
        width <= 0 || height <= 0 || width > static_cast<int>(MaxGrid) ||
        height > static_cast<int>(MaxGrid) / width) { state.Reset(); return; }
    const auto count = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    if (!state.Begin(room.identity,room.frame)) return;
    for (std::size_t i = 0; i < count; ++i) {
        std::uintptr_t address{};
        Poop poop;
        if (!At(engine,room.identity.room,layout::GridArray + i * sizeof(address),address) ||
            !ReadPoop(engine,module,address,i,poop)) { state.Forget(i); continue; }
        if (!state.Observe(i,poop,room.clear)) continue;
        // No object pointer is kept across a game callback. Stop immediately if
        // that callback changes the run/room or ends the clear-room condition.
        engine.DestroyGrid(address);
        RoomFrame current;
        if (!ReadRoom(engine,module,game,current) || !(current.identity == room.identity) ||
            current.frame != room.frame || !current.clear) { state.Reset(); return; }
    }
    for (auto i = count; i < MaxGrid; ++i) state.Forget(i);
}
struct Fire {
    std::uint32_t seed{};
};
template<class Engine>
bool ReadFire(Engine& engine, std::uintptr_t address, Fire& fire) {
    std::uint32_t type{}, variant{};
    std::uint8_t exists{}, dead{};
    return Pointer(address) && engine.Readable(address,0xbd0) &&
        At(engine,address,0x38,type) && type == 33 &&
        At(engine,address,0x3c,variant) && variant == 0 &&
        At(engine,address,0x1c2,exists) && exists &&
        At(engine,address,0x1c3,dead) && !dead &&
        At(engine,address,0x3e8,fire.seed);
}
template<class Engine, class Original>
bool TakeDamage(Engine& engine, std::uintptr_t module, std::uintptr_t entity,
                float amount, std::uint64_t flags, const void* source, int countdown, Original original) {
    RoomFrame before;
    Fire fire;
    const bool candidate = amount > 0 && amount <= std::numeric_limits<float>::max() &&
        ReadFire(engine,entity,fire) && ReadRoom(engine,module,0,before) && before.clear;
    // Preserve the original hit, source, return value, and all native callbacks.
    const bool accepted = original(entity,amount,flags,source,countdown);
    if (candidate && accepted) {
        RoomFrame after;
        Fire current;
        if (ReadRoom(engine,module,before.identity.game,after) && after.clear &&
            after.identity == before.identity && ReadFire(engine,entity,current) && current.seed == fire.seed)
            engine.KillNpc(entity);
    }
    return accepted;
}
}

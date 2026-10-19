#include "regret.hpp"
#include "instant_break.hpp"
#include "debug_kit.hpp"
#include "native_sprite.hpp"
#include "generated/build_profile.hpp"
#include "lib.hpp"
#include "../../../features/regret/state.hpp"

#include <cstring>

namespace isaac {
namespace {
using namespace feature_memory;

constexpr std::uintptr_t PickupUpdate = 0x26c9b8;
constexpr std::uintptr_t PickupRemove = 0x276960;
constexpr std::uintptr_t PickupDestructor = 0x2691f8;
constexpr std::uintptr_t RoomInit = 0x45c020;
constexpr std::uintptr_t WorldToScreen = 0x489354;
constexpr std::uintptr_t GetCollectible = 0x3c0c50;
constexpr std::uintptr_t ManagerItemConfig = 0x36538;
constexpr std::uintptr_t ItemGraphicsString = 0x38;
constexpr std::uintptr_t EntityInitSeed = 0x3e8;
constexpr std::uintptr_t PrimaryActiveItem = 0x1964;
constexpr std::uintptr_t CurrentRoomIndex = 0x21558;
constexpr std::uintptr_t CurrentDimension = 0x21560;
constexpr std::uintptr_t RunSeed = 0x25d44;
constexpr std::uintptr_t StageSeeds = 0x25d58;
constexpr std::int32_t GlowingHourglass = 422;

constexpr std::uint8_t UpdatePrologue[] = {
    0xeb,0x2b,0xb8,0x6d,0xe9,0x23,0x01,0x6d,0xfd,0x7b,0x02,0xa9,0xfd,0x83,0x00,0x91,0xfc,0x6f,0x03,0xa9};
constexpr std::uint8_t RemovePrologue[] = {
    0xfd,0x7b,0xbe,0xa9,0xf3,0x0b,0x00,0xf9,0xfd,0x03,0x00,0x91,0xf3,0x03,0x00,0xaa,0x00,0xe4,0x42,0xf9};
constexpr std::uint8_t DestructorPrologue[] = {
    0xfd,0x7b,0xbd,0xa9,0xf5,0x0b,0x00,0xf9,0xfd,0x03,0x00,0x91,0xf4,0x4f,0x02,0xa9,0x08,0x42,0x00,0xf0};

struct RoomStamp {
    std::uintptr_t game{}, room{};
    std::int32_t index{}, dimension{}, stage{}, stageType{};
    std::uint32_t runSeed{}, floorSeed{};
    bool operator==(const RoomStamp& other) const {
        return game == other.game && room == other.room && index == other.index &&
               dimension == other.dimension && stage == other.stage && stageType == other.stageType &&
               runSeed == other.runSeed && floorSeed == other.floorSeed;
    }
};

constinit regret::State s_State;
constinit NativeSprite s_Sprite;
std::uintptr_t s_Module{};
bool s_HooksReady{};
unsigned s_RoomInitDepth{};
RoomStamp s_Room;
std::uint64_t s_RoomSerial{};
std::int32_t s_SpriteItem{};
regret::Animation s_SpriteAnimation{regret::Animation::Other};

bool ReadRoom(RoomStamp& stamp) {
    if (!s_Module || !TryRead(s_Module + profile::GameGlobal, stamp.game) || !stamp.game ||
        !TryRead(stamp.game + profile::GameRoom, stamp.room) || !stamp.room ||
        !TryRead(stamp.game + CurrentRoomIndex, stamp.index) ||
        !TryRead(stamp.game + CurrentDimension, stamp.dimension) ||
        !TryRead(stamp.game, stamp.stage) || !TryRead(stamp.game + 4, stamp.stageType) ||
        !TryRead(stamp.game + RunSeed, stamp.runSeed)) return false;
    if (stamp.stage < 0 || stamp.stage > 14 || stamp.stageType < 0 || stamp.stageType > 5) return false;
    const auto seedIndex = stamp.stage + ((stamp.stageType == 4 || stamp.stageType == 5) ? 1 : 0);
    if (seedIndex < 0 || seedIndex > 14 || stamp.dimension < 0 || stamp.dimension > 2) return false;
    return TryRead(stamp.game + StageSeeds + static_cast<std::uintptr_t>(seedIndex) * 4, stamp.floorSeed);
}

bool SyncRoom() {
    RoomStamp room{};
    if (!ReadRoom(room)) {
        s_State.SetRoom(0);
        s_Room = {};
        return false;
    }
    if (!(room == s_Room)) {
        s_Room = room;
        if (++s_RoomSerial == 0) ++s_RoomSerial;
        s_State.SetRoom(s_RoomSerial);
    }
    return true;
}

bool PrimaryHourglassHeld() {
    std::uintptr_t begin{}, end{};
    if (!TryRead(s_Room.game + profile::PlayersBegin, begin) ||
        !TryRead(s_Room.game + profile::PlayersEnd, end) || !begin || end <= begin ||
        end - begin > 64 * sizeof(std::uintptr_t) || (end - begin) % sizeof(std::uintptr_t) ||
        !Readable(begin, end - begin)) return true;
    for (auto address = begin; address < end; address += sizeof(std::uintptr_t)) {
        std::uintptr_t player{};
        std::int32_t active{};
        if (!TryRead(address, player) || !player || !TryRead(player + PrimaryActiveItem, active)) return true;
        if (active == GlowingHourglass) return true;
    }
    return false;
}

bool ReadPickup(std::uintptr_t entity, regret::Pickup& pickup, bool& exists) {
    if (!Readable(entity, EntityInitSeed + 4) ||
        Read<std::int32_t>(entity, profile::EntityType) != 5 ||
        Read<std::int32_t>(entity, profile::EntityVariant) != 100) return false;
    pickup.identity = {entity, Read<std::uint32_t>(entity, EntityInitSeed)};
    pickup.item = Read<std::int32_t>(entity, profile::EntitySubtype);
    pickup.worldX = Read<float>(entity, profile::EntityX);
    pickup.worldY = Read<float>(entity, profile::EntityY);
    exists = Read<std::uint8_t>(entity, profile::EntityExists) != 0;
    const auto animationData = Read<std::uintptr_t>(entity, 0x50 + 0x38);
    char animation[32];
    if (CopyNativeString(animationData, animation, sizeof(animation))) {
        if (std::strcmp(animation, "Idle") == 0) pickup.animation = regret::Animation::Idle;
        else if (std::strcmp(animation, "ShopIdle") == 0) pickup.animation = regret::Animation::ShopIdle;
    }
    return true;
}

void ObservePickup(std::uintptr_t entity) {
    if (!s_HooksReady || s_RoomInitDepth || !SyncRoom()) return;
    regret::Pickup pickup;
    bool exists{};
    if (!ReadPickup(entity, pickup, exists)) return;
    if (exists) s_State.Observe(pickup, PrimaryHourglassHeld());
    else s_State.Removed(pickup);
}

void RemovedPickup(std::uintptr_t entity) {
    if (!s_HooksReady || s_RoomInitDepth || !SyncRoom()) return;
    regret::Pickup pickup;
    bool exists{};
    if (ReadPickup(entity, pickup, exists)) s_State.Removed(pickup);
}

HOOK_DEFINE_TRAMPOLINE(RegretPickupUpdate) {
    static void Callback(void* entity) {
        // Removal only marks flags. The post-update read therefore remains
        // valid and captures subtype changes made during the original update.
        ObservePickup(reinterpret_cast<std::uintptr_t>(entity));
        Orig(entity);
        ObservePickup(reinterpret_cast<std::uintptr_t>(entity));
    }
};
HOOK_DEFINE_TRAMPOLINE(RegretPickupRemove) {
    static void Callback(void* entity) {
        RemovedPickup(reinterpret_cast<std::uintptr_t>(entity));
        Orig(entity);
    }
};
HOOK_DEFINE_TRAMPOLINE(RegretPickupDestructor) {
    static void Callback(void* entity) {
        // D1/D2 share an entry. Take the final values before object teardown;
        // there is no access to the pickup after the original destructor.
        RemovedPickup(reinterpret_cast<std::uintptr_t>(entity));
        Orig(entity);
    }
};
HOOK_DEFINE_TRAMPOLINE(RegretRoomInit) {
    static std::uint32_t Callback(void* room, const void* roomConfig, void* roomDescriptor) {
        // Room::Init also runs when restoring/re-entering the identical room.
        // Suppress old-pickup teardown events during initialization, then let
        // subsequent update callbacks admit the newly created pickups.
        ++s_RoomInitDepth;
        ResetInstantBreakRoom();
        CancelDebugKitHold();
        s_State.SetRoom(0);
        s_Room = {};
        const auto result = Orig(room, roomConfig, roomDescriptor);
        --s_RoomInitDepth;
        s_State.SetRoom(0);
        s_Room = {};
        s_SpriteItem = 0;
        s_SpriteAnimation = regret::Animation::Other;
        if (!s_RoomInitDepth) SyncRoom();
        return result;
    }
};

template<std::size_t N> bool Matches(std::uintptr_t address, const std::uint8_t (&bytes)[N]) {
    return (address & 0xfff) <= 0x1000 - N && Readable(address, N) &&
           std::memcmp(reinterpret_cast<const void*>(address), bytes, N) == 0;
}

bool SelectSprite(std::int32_t item, regret::Animation animation) {
    if (!s_Sprite.Created() && !s_Sprite.Create(s_Module)) return false;
    if (!s_Sprite.Loaded() && !s_Sprite.Load("gfx/005.100_collectible.anm2")) return false;
    if (s_SpriteItem != item) {
        std::uintptr_t manager{};
        if (!TryRead(s_Module + profile::ManagerGlobal, manager) || !manager ||
            !Readable(manager + ManagerItemConfig, 0x20)) return false;
        const auto config = reinterpret_cast<std::uintptr_t (*)(const void*, std::int32_t)>(s_Module + GetCollectible)(
            reinterpret_cast<const void*>(manager + ManagerItemConfig), item);
        if (!config || !s_Sprite.SetSpritesheetFromGameString(1, config + ItemGraphicsString)) return false;
        s_Sprite.LoadGraphics();
        s_SpriteItem = item;
    }
    if (s_SpriteAnimation != animation) {
        if (!s_Sprite.SetFrame(animation == regret::Animation::ShopIdle ? "ShopIdle" : "Idle", 0)) return false;
        s_SpriteAnimation = animation;
    }
    return true;
}
} // namespace

bool InitializeRegret(std::uintptr_t module) {
    // Called for each newly mapped, build-verified NRO. All guards run before
    // any patch, so a conflict never leaves a partly installed feature.
    s_Module = module;
    s_HooksReady = false;
    s_RoomInitDepth = 0;
    s_State.Reset();
    s_Room = {};
    if (!module || !Matches(module + PickupUpdate, UpdatePrologue) ||
        !Matches(module + PickupRemove, RemovePrologue) ||
        !Matches(module + PickupDestructor, DestructorPrologue) ||
        // Room::Init has the same verified first20 bytes as Pickup::Update.
        !Matches(module + RoomInit, UpdatePrologue)) return false;
    RegretPickupUpdate::InstallAtPtr(module + PickupUpdate);
    RegretPickupRemove::InstallAtPtr(module + PickupRemove);
    // exlaunch explicitly relocates ADRP, the fifth instruction in this entry.
    RegretPickupDestructor::InstallAtPtr(module + PickupDestructor);
    RegretRoomInit::InstallAtPtr(module + RoomInit);
    s_HooksReady = true;
    return true;
}

void RenderRegret(const FeatureFrame& frame) {
    if (!s_HooksReady || frame.module != s_Module) {
        frame.draw.Text("Regret: hook mismatch", 12, frame.height - 27, {1,0.6f,0.3f,1}, 0.65f);
        return;
    }
    if (s_RoomInitDepth || !SyncRoom() || frame.game != s_Room.game || frame.room != s_Room.room) return;
    std::uintptr_t entities{};
    unsigned count{};
    bool complete = TryRead(frame.room + profile::RoomEntities, entities) &&
                    TryRead(frame.room + profile::RoomEntityCount, count) && count <= 4096 &&
                    (!count || Readable(entities, count * sizeof(std::uintptr_t)));
    s_State.BeginSnapshot(s_RoomSerial, PrimaryHourglassHeld());
    if (complete) {
        for (unsigned i = 0; i < count; ++i) {
            const auto entity = Read<std::uintptr_t>(entities + i * sizeof(std::uintptr_t));
            if (!entity) continue;
            if (!Readable(entity, profile::EntityVariant + 4)) {
                complete = false;
                continue;
            }
            if (Read<std::int32_t>(entity, profile::EntityType) != 5 ||
                Read<std::int32_t>(entity, profile::EntityVariant) != 100) continue;
            regret::Pickup pickup;
            bool exists{};
            if (!ReadPickup(entity, pickup, exists)) {
                complete = false;
                continue;
            }
            if (exists) s_State.Observe(pickup);
            else s_State.Removed(pickup);
        }
    }
    s_State.EndSnapshot(complete);
    for (std::size_t i = 0; i < s_State.AfterimageCount(); ++i) {
        const auto& afterimage = *s_State.GetAfterimage(i);
        if (!SelectSprite(afterimage.item, afterimage.animation)) continue;
        auto position = reinterpret_cast<NativeVec2 (*)(const void*, NativeVec2)>(s_Module + WorldToScreen)(
            reinterpret_cast<const void*>(frame.room), {afterimage.worldX, afterimage.worldY});
        position.y -= afterimage.Rise();
        if (!(position.x >= -1000000 && position.x <= 1000000 && position.y >= -1000000 && position.y <= 1000000)) continue;
        s_Sprite.SetTint(1, 1, 1, afterimage.Alpha());
        s_Sprite.Render(position);
    }
    s_State.AdvanceRender();
}

void ResetRegret() {
    s_State.Reset();
    s_Room = {};
    s_Sprite.Destroy();
    s_SpriteItem = 0;
    s_SpriteAnimation = regret::Animation::Other;
    // Keep installed hook status. If the SDK refuses an unload, the next HUD
    // frame safely recreates the sprite without patching an already-hooked entry.
}

} // namespace isaac

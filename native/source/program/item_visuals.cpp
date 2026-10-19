#include "item_visuals.hpp"
#include "lib.hpp"
#include "native_sprite.hpp"
#include "mod_settings.hpp"
#include "clean_map.hpp"
#include "generated/build_profile.hpp"
#include "../../../features/item_visuals/state.hpp"
#include "../../../features/item_visuals/exchange_scope.hpp"
#include "../../../features/item_visuals/native_calls.hpp"

#include <cstring>

namespace isaac {
namespace {
using namespace feature_memory;
namespace visual = isaac_item_visuals;
constexpr std::uintptr_t PickupRender = 0x270c34;
constexpr std::uintptr_t PlayerSpriteRender = 0x2d9398;
constexpr std::uintptr_t SpriteRenderLayer = 0x9d70;
constexpr std::uintptr_t SpriteRender = 0x9bd4;
constexpr std::uintptr_t GetSpriteFrame = 0xa924;
constexpr std::uintptr_t GetCollectible = 0x3c0c50;
constexpr std::uintptr_t ManagerItemConfig = 0x36538;
constexpr std::uintptr_t HeldSprite = 0x12d8;
constexpr std::uintptr_t QueuedItem = 0x1c38;
constexpr std::uintptr_t FrameCount = 0x24f99c;
constexpr std::uintptr_t RunSeed = 0x25d44;
// Exact GetRenderDistance (0x35ae0): source SpriteOffset world units -> pixels.
constexpr float WorldToPixel = .65f;

// The five overwritten instructions at each site only save registers/adjust
// the stack. Addresses, raw ABI and bytes are recorded in native-layout.json.
constexpr std::uintptr_t HookOffsets[] = {PickupRender, PlayerSpriteRender, SpriteRenderLayer, SpriteRender};
constexpr std::uint8_t OriginalPrologues[][20] = {
    {0xff,0xc3,0x01,0xd1,0xe8,0x1b,0x00,0xfd,0xf7,0x27,0x00,0xf9,0xfd,0xfb,0x03,0xa9,0xfd,0xe3,0x00,0x91},
    {0xe8,0x0f,0x19,0xfc,0xfd,0x7b,0x01,0xa9,0xfd,0x43,0x00,0x91,0xfc,0x6f,0x02,0xa9,0xfa,0x67,0x03,0xa9},
    {0xfd,0x7b,0xbc,0xa9,0xf7,0x0b,0x00,0xf9,0xfd,0x03,0x00,0x91,0xf6,0x57,0x02,0xa9,0xf4,0x4f,0x03,0xa9},
    {0xfd,0x7b,0xbc,0xa9,0xf8,0x5f,0x01,0xa9,0xfd,0x03,0x00,0x91,0xf6,0x57,0x02,0xa9,0xf4,0x4f,0x03,0xa9}
};

constinit NativeSprite s_Animations[visual::AnimationCount];
constinit NativeSprite s_Birthrights[visual::CharacterCount];
bool s_AnimationAttempted[visual::AnimationCount]{};
bool s_AnimationReady[visual::AnimationCount]{};
bool s_BirthrightAttempted[visual::CharacterCount]{};
bool s_BirthrightReady[visual::CharacterCount]{};
constinit visual::Timeline<256> s_Timeline;
std::uintptr_t s_Module{}, s_Game{}, s_Room{};
std::uint32_t s_RunSeed{};
std::int32_t s_LastFrame{-1}, s_RoomIndex{-1}, s_Dimension{-1};
bool s_HookReady{}, s_AnimationFailure{}, s_BirthrightFailure{}, s_Blind{};
std::uint8_t s_InstalledPrologues[4][20]{};

struct Pass {
    std::uintptr_t entity{}, game{};
    std::int32_t now{};
    bool blind{};
};
Pass s_PickupPass{}, s_PlayerPass{};

// Only use Peek after one bounds/permission check covering the whole object.
template<class T> T Peek(std::uintptr_t pointer, std::uintptr_t offset = 0) {
    T value{};
    std::memcpy(&value, reinterpret_cast<const void*>(pointer + offset), sizeof(value));
    return value;
}
bool Pointer(std::uintptr_t pointer) { return pointer > 0x10000 && !(pointer & 7); }
bool Live(std::uintptr_t entity, unsigned type, unsigned variant, std::size_t size) {
    return Pointer(entity) && Readable(entity, size) && Peek<unsigned>(entity, 0x38) == type &&
           Peek<unsigned>(entity, 0x3c) == variant && Peek<std::uint8_t>(entity, 0x1c1) &&
           Peek<std::uint8_t>(entity, 0x1c2) && !Peek<std::uint8_t>(entity, 0x1c3);
}
bool Writable(std::uintptr_t pointer, std::size_t size) {
    if (!Readable(pointer, size)) return false;
    MemoryInfo info{};
    std::uint32_t page{};
    return svcQueryMemory(&info, &page, pointer) == 0 && (info.perm & Perm_W);
}
bool MatchPrologue(std::uintptr_t address, const std::uint8_t* bytes) {
    return Readable(address, 20) && std::memcmp(reinterpret_cast<const void*>(address), bytes, 20) == 0;
}
bool AllPrologues(std::uintptr_t module, const std::uint8_t (&bytes)[4][20]) {
    for (unsigned i = 0; i < 4; ++i) if (!MatchPrologue(module + HookOffsets[i], bytes[i])) return false;
    return true;
}

bool CurrentPass(Pass& pass) {
    std::uintptr_t game{};
    if (!s_HookReady || !TryRead(s_Module + profile::GameGlobal, game) ||
        !Pointer(game) || !Readable(game, FrameCount + sizeof(std::int32_t))) return false;
    const auto now = Peek<std::int32_t>(game, FrameCount);
    const auto room = Peek<std::uintptr_t>(game, 0x21550);
    const auto index = Peek<std::int32_t>(game, 0x21558);
    const auto dimension = Peek<std::int32_t>(game, 0x21560);
    const auto seed = Peek<std::uint32_t>(game, RunSeed);
    // Special rooms use negative indices. It is an identity component here,
    // never an array subscript, so preserve the game's signed value verbatim.
    if (now < 0 || !Pointer(room) || dimension < 0 || dimension > 2) return false;
    if (game != s_Game || room != s_Room || seed != s_RunSeed || index != s_RoomIndex ||
        dimension != s_Dimension || now < s_LastFrame) {
        s_Timeline.Reset();
        s_LastFrame = -1;
    }
    if (now != s_LastFrame) {
        s_Blind = (visual::NativeCall<unsigned (*)(const void*)>(s_Module, profile::GetCurses)(
            reinterpret_cast<const void*>(game)) & 0x40u) != 0;
    }
    s_Game = game; s_Room = room; s_RunSeed = seed;
    s_RoomIndex = index; s_Dimension = dimension; s_LastFrame = now;
    pass.game = game; pass.now = now; pass.blind = s_Blind;
    return true;
}

bool HeadLayer(std::uintptr_t sprite, std::uintptr_t& layer, char* path, std::size_t capacity) {
    if (!Pointer(sprite) || !Readable(sprite, 0x158) || !Peek<std::uint8_t>(sprite, 0x149)) return false;
    const auto states = Peek<std::uintptr_t>(sprite, 0xa8);
    const auto count = Peek<unsigned>(sprite, 0xb0);
    if (!Pointer(states) || count < 2 || count > 64 || !Readable(states, count * 0xb0)) return false;
    layer = states + 0xb0;
    return Peek<std::uint8_t>(layer, 0x7c) &&
           CopyNativeString(layer + 0x10, path, capacity) && visual::KnownSheet(path);
}

bool PositionAndCrop(const NativeVec2* position, const NativeVec2* crop_tl, const NativeVec2* crop_br,
                     NativeVec2& pos, NativeVec2& tl, NativeVec2& br) {
    return TryRead(reinterpret_cast<std::uintptr_t>(position), pos) &&
           TryRead(reinterpret_cast<std::uintptr_t>(crop_tl), tl) &&
           TryRead(reinterpret_cast<std::uintptr_t>(crop_br), br) &&
           visual::Coordinate(pos.x) && visual::Coordinate(pos.y) &&
           visual::Coordinate(tl.x) && visual::Coordinate(tl.y) &&
           visual::Coordinate(br.x) && visual::Coordinate(br.y);
}

NativeSprite* Animation(std::size_t index, unsigned age, visual::Pose pose) {
    if (index >= visual::AnimationCount) return nullptr;
    auto& sprite = s_Animations[index];
    const auto& source = visual::data::Animations[index];
    if (!s_AnimationAttempted[index]) {
        s_AnimationAttempted[index] = true;
        s_AnimationReady[index] = sprite.Create(s_Module) && sprite.Load(source.file);
        // A parsed ANM2 can contain valid frames but no loaded texture. Never
        // remove the original picture unless every replacement layer is ready.
        // These owned textures remain referenced until Reset; SetFrame only
        // changes animation positions, so validate once rather than every draw.
        for (unsigned layer = 0; s_AnimationReady[index] && layer < source.layers; ++layer)
            s_AnimationReady[index] = sprite.HasLayerTexture(layer);
        if (!s_AnimationReady[index]) s_AnimationFailure = true;
    }
    if (!s_AnimationReady[index] || !sprite.SetFrame("AnimatedPedestal", static_cast<int>(visual::AnimationFrame(age, source.frames)))) {
        s_AnimationFailure = true;
        return nullptr;
    }
    sprite.SetScale(pose.scale_x, pose.scale_y);
    return &sprite;
}

NativeSprite* BirthrightArt(unsigned character) {
    if (character >= visual::CharacterCount) return nullptr;
    auto& sprite = s_Birthrights[character];
    if (!s_BirthrightAttempted[character]) {
        s_BirthrightAttempted[character] = true;
        if (sprite.Create(s_Module) && sprite.Load("gfx/005.100_collectible.anm2") &&
            sprite.SetSpritesheet(1, visual::data::Birthrights[character])) {
            sprite.LoadGraphics();
            s_BirthrightReady[character] = sprite.SetFrame("Idle", 0) && sprite.HasLayerTexture(1);
            if (!s_BirthrightReady[character]) s_BirthrightFailure = true;
        } else s_BirthrightFailure = true;
    }
    return s_BirthrightReady[character] ? &sprite : nullptr;
}

int NearestCharacter(const Pass& pass, std::uintptr_t pickup) {
    const auto begin = Peek<std::uintptr_t>(pass.game, profile::PlayersBegin);
    const auto end = Peek<std::uintptr_t>(pass.game, profile::PlayersEnd);
    if (!Pointer(begin) || end <= begin || end - begin > 8 * sizeof(std::uintptr_t) ||
        (end - begin) % sizeof(std::uintptr_t) || !Readable(begin, end - begin)) return -1;
    const auto count = (end - begin) / sizeof(std::uintptr_t);
    visual::Player players[8]{};
    for (std::size_t i = 0; i < count; ++i) {
        const auto player = Peek<std::uintptr_t>(begin + i * sizeof(std::uintptr_t));
        if (!Live(player, 1, 0, 0x2850)) continue;
        players[i] = {Peek<float>(player, 0x310), Peek<float>(player, 0x314),
                      Peek<unsigned>(player, 0x1738), true};
    }
    const int chosen = visual::NearestPlayer(Peek<float>(pickup, 0x310), Peek<float>(pickup, 0x314), players, count);
    return chosen < 0 ? -1 : static_cast<int>(players[chosen].type);
}

visual::Identity Identity(std::uintptr_t entity, unsigned item, bool held) {
    return {entity, Peek<std::uint32_t>(entity, 0x3e8), Peek<std::int32_t>(entity, 0x2f4), item, held};
}

// Nesting is deliberate: the game can render a second player body or linked
// pickup while a parent pass is active. Always restore the prior context.
HOOK_DEFINE_TRAMPOLINE(ObserveItemPickupRender) {
    static void Callback(void* pickup, const NativeVec2* offset) {
        const auto previous = s_PickupPass;
        s_PickupPass = {};
        const auto entity = reinterpret_cast<std::uintptr_t>(pickup);
        if ((IsModEnabled(ModFeature::AnimatedItems) || IsModEnabled(ModFeature::Birthright)) &&
            Live(entity, 5, 100, 0x780) && CurrentPass(s_PickupPass)) s_PickupPass.entity = entity;
        Orig(pickup, offset);
        s_PickupPass = previous;
    }
};

HOOK_DEFINE_TRAMPOLINE(ObserveItemPlayerRender) {
    static void Callback(void* player, const NativeVec2* offset, bool render_held) {
        const auto previous = s_PlayerPass;
        s_PlayerPass = {};
        const auto entity = reinterpret_cast<std::uintptr_t>(player);
        if (render_held && (IsModEnabled(ModFeature::AnimatedItems) || IsModEnabled(ModFeature::Birthright)) &&
            Live(entity, 1, 0, 0x2850) && CurrentPass(s_PlayerPass)) s_PlayerPass.entity = entity;
        Orig(player, offset, render_held);
        s_PlayerPass = previous;
    }
};

std::uintptr_t PickupForSprite(std::uintptr_t sprite) {
    const auto parent = s_PickupPass.entity;
    if (!parent) return 0;
    if (sprite == parent + 0x50) return parent;
    // Native Pickup::Render explicitly draws this linked second item's head.
    // No arbitrary sprite or Flip ghost is admitted by the global hook.
    const auto linked = Peek<std::uintptr_t>(parent, 0x5d8);
    return sprite == linked + 0x50 && Live(linked, 5, 100, 0x780) &&
           Peek<std::uintptr_t>(linked, 0x5d0) == parent ? linked : 0;
}

HOOK_DEFINE_TRAMPOLINE(DrawItemPickupLayer) {
    static void Callback(void* sprite_pointer, unsigned layer_index, const NativeVec2* position,
                         const NativeVec2* crop_tl, const NativeVec2* crop_br) {
        const auto sprite = reinterpret_cast<std::uintptr_t>(sprite_pointer);
        const auto pickup = layer_index == 1 && s_HookReady ? PickupForSprite(sprite) : 0;
        std::uintptr_t layer{};
        char path[512];
        if (!pickup || Peek<std::uint8_t>(pickup, 0x562) ||
            !HeadLayer(sprite, layer, path, sizeof(path))) {
            Orig(sprite_pointer, layer_index, position, crop_tl, crop_br);
            return;
        }
        const auto item = Peek<unsigned>(pickup, 0x40);
        if (item == visual::Birthright) {
            if (!IsModEnabled(ModFeature::Birthright) || s_PickupPass.blind) {
                Orig(sprite_pointer, layer_index, position, crop_tl, crop_br); return;
            }
            const int character = NearestCharacter(s_PickupPass, pickup);
            auto* art = character >= 0 ? BirthrightArt(static_cast<unsigned>(character)) : nullptr;
            // Swapping owning handles exchanges their existing references;
            // both ANM2s remain alive and no disk loading occurs in this draw.
            visual::TextureDrawScope<NativeSprite> exchange(art, sprite, &s_BirthrightFailure);
            if (exchange.Active()) {
                Orig(sprite_pointer, layer_index, position, crop_tl, crop_br);
                return;
            }
            Orig(sprite_pointer, layer_index, position, crop_tl, crop_br);
            return;
        }
        if (!IsModEnabled(ModFeature::AnimatedItems)) {
            Orig(sprite_pointer, layer_index, position, crop_tl, crop_br); return;
        }
        const auto index = visual::AnimationIndex(item);
        NativeVec2 pos, tl, br;
        if (index == visual::NoAnimation || !PositionAndCrop(position, crop_tl, crop_br, pos, tl, br)) {
            Orig(sprite_pointer, layer_index, position, crop_tl, crop_br);
            return;
        }
        const int frame = visual::NativeCall<int (*)(const void*)>(s_Module, GetSpriteFrame)(sprite_pointer);
        if (frame < 0) { Orig(sprite_pointer, layer_index, position, crop_tl, crop_br); return; }
        const auto pose = visual::PedestalPose(frame, Peek<int>(pickup, 0x564) != 0);
        const auto age = s_Timeline.Observe(Identity(pickup, item, false), s_PickupPass.now);
        auto* animation = Animation(index, age, pose);
        const auto offset = Peek<NativeVec2>(pickup, 0x320);
        if (!animation || !visual::Coordinate(offset.x) || !visual::Coordinate(offset.y)) {
            Orig(sprite_pointer, layer_index, position, crop_tl, crop_br);
            return;
        }
        // Source effects follow the parent's world position, not its own
        // SpriteOffset. Preserve native dual-pedestal and Flip displacement.
        pos.x -= offset.x * WorldToPixel;
        pos.y += (pose.y - offset.y) * WorldToPixel;
        animation->Render(pos, tl, br);
    }
};

HOOK_DEFINE_TRAMPOLINE(DrawItemHeldSprite) {
    static void Callback(void* sprite_pointer, const NativeVec2* position,
                         const NativeVec2* crop_tl, const NativeVec2* crop_br) {
        if (ShouldHideCleanMapFrame(reinterpret_cast<std::uintptr_t>(sprite_pointer),
                                   reinterpret_cast<std::uintptr_t>(__builtin_return_address(0)))) return;
        const auto player = s_PlayerPass.entity;
        const auto sprite = reinterpret_cast<std::uintptr_t>(sprite_pointer);
        std::uintptr_t layer{};
        char path[512];
        NativeVec2 pos, tl, br;
        if (!s_HookReady || !player || s_PlayerPass.blind || sprite != player + HeldSprite ||
            !HeadLayer(sprite, layer, path, sizeof(path)) || !Writable(layer + 0x7c, 1) ||
            !PositionAndCrop(position, crop_tl, crop_br, pos, tl, br)) {
            Orig(sprite_pointer, position, crop_tl, crop_br);
            return;
        }
        const auto queued = Peek<std::uintptr_t>(player, QueuedItem);
        std::uintptr_t manager{};
        unsigned item{};
        if (!Pointer(queued) || !TryRead(queued + 4, item) ||
            !visual::EligibleItem(item) ||
            !TryRead(s_Module + profile::ManagerGlobal, manager) || !Pointer(manager) ||
            !Readable(manager + ManagerItemConfig, 0x30) ||
            visual::NativeCall<std::uintptr_t (*)(const void*, int)>(s_Module, GetCollectible)(
                reinterpret_cast<const void*>(manager + ManagerItemConfig), static_cast<int>(item)) != queued) {
            Orig(sprite_pointer, position, crop_tl, crop_br);
            return;
        }
        if (item == visual::Birthright) {
            if (!IsModEnabled(ModFeature::Birthright)) { Orig(sprite_pointer, position, crop_tl, crop_br); return; }
            // Native Pickup::Update copies its pickup ANM2 into the player's
            // held ANM2. Upstream's persistent sheet change therefore follows
            // the item into this animation too; reproduce it without leaving
            // any lasting change on the original pickup or player sprite.
            auto* art = BirthrightArt(Peek<unsigned>(player, 0x1738));
            visual::TextureDrawScope<NativeSprite> exchange(art, sprite, &s_BirthrightFailure);
            Orig(sprite_pointer, position, crop_tl, crop_br);
            return;
        }
        if (!IsModEnabled(ModFeature::AnimatedItems)) { Orig(sprite_pointer, position, crop_tl, crop_br); return; }
        const auto age = s_Timeline.Observe(Identity(player, item, true), s_PlayerPass.now);
        const auto pose = visual::HeldPose(age);
        auto* animation = Animation(visual::AnimationIndex(item), age, pose);
        if (!animation) { Orig(sprite_pointer, position, crop_tl, crop_br); return; }
        // Keep all native sparkle/other layers. Only the held item's head is
        // hidden for the duration of this one original render call.
        auto* visible = reinterpret_cast<std::uint8_t*>(layer + 0x7c);
        const auto saved = *visible;
        *visible = 0;
        Orig(sprite_pointer, position, crop_tl, crop_br);
        *visible = saved;
        pos.y += pose.y * WorldToPixel;
        animation->Render(pos, tl, br);
    }
};
} // namespace

bool InitializeItemVisuals(std::uintptr_t module) {
    if (module && module == s_Module && s_HookReady && AllPrologues(module, s_InstalledPrologues)) return true;
    s_Module = module;
    s_HookReady = false;
    s_PickupPass = {}; s_PlayerPass = {};
    if (!module || !AllPrologues(module, OriginalPrologues)) return false;
    // Validate all sites before changing any of them.
    ObserveItemPickupRender::InstallAtPtr(module + PickupRender);
    ObserveItemPlayerRender::InstallAtPtr(module + PlayerSpriteRender);
    DrawItemPickupLayer::InstallAtPtr(module + SpriteRenderLayer);
    DrawItemHeldSprite::InstallAtPtr(module + SpriteRender);
    for (unsigned i = 0; i < 4; ++i)
        std::memcpy(s_InstalledPrologues[i], reinterpret_cast<const void*>(module + HookOffsets[i]), 20);
    s_HookReady = true;
    return true;
}

void ResetItemVisuals() {
    for (auto& sprite : s_Animations) sprite.Destroy();
    for (auto& sprite : s_Birthrights) sprite.Destroy();
    for (auto& attempted : s_AnimationAttempted) attempted = false;
    for (auto& ready : s_AnimationReady) ready = false;
    for (auto& attempted : s_BirthrightAttempted) attempted = false;
    for (auto& ready : s_BirthrightReady) ready = false;
    s_Timeline.Reset(); s_PickupPass = {}; s_PlayerPass = {};
    s_Game = 0; s_Room = 0; s_RunSeed = 0;
    s_LastFrame = -1; s_RoomIndex = -1; s_Dimension = -1;
    s_AnimationFailure = false; s_BirthrightFailure = false; s_Blind = false;
    // Preserve hook identity if the SDK refuses the unload; fresh resources
    // can be recreated on the next frame. A real reload must recheck bytes.
}

void RenderItemVisualsStatus(const FeatureFrame& frame) {
    if (!frame.module || !frame.game || !frame.room || !frame.draw.text) return;
    if (!IsModEnabled(ModFeature::AnimatedItems) && !IsModEnabled(ModFeature::Birthright)) return;
    if (!s_HookReady || frame.module != s_Module)
        frame.draw.Text("Item visuals: hook mismatch", 12, frame.height - 52, {1,.6f,.3f,1}, .65f);
    else if ((s_AnimationFailure && IsModEnabled(ModFeature::AnimatedItems)) ||
             (s_BirthrightFailure && IsModEnabled(ModFeature::Birthright)))
        frame.draw.Text("Item artwork could not load", 12, frame.height - 52, {1,.6f,.3f,1}, .65f);
}
} // namespace isaac

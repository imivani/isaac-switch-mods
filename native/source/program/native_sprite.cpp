#include "native_sprite.hpp"
#include "feature_api.hpp"

#include <cstring>

namespace isaac {
namespace {
using namespace feature_memory;
constexpr std::uintptr_t Constructor = 0x6540;
constexpr std::uintptr_t Destructor = 0x71a4;
constexpr std::uintptr_t LoadAnimation = 0xc970;
constexpr std::uintptr_t ReplaceSpritesheet = 0xd270;
constexpr std::uintptr_t LoadSpriteGraphics = 0xcb6c;
constexpr std::uintptr_t SetAnimationFrame = 0xa65c;
constexpr std::uintptr_t RenderAnimation = 0x9bd4;
constexpr std::uintptr_t RenderAnimationLayer = 0x9d70;
constexpr std::uintptr_t SetColorTint = 0x37848;

// A borrowed long-form libc++ string. Only const-reference game functions
// receive it, and those functions copy its value into game-owned storage.
struct NativeString24 {
    std::uint64_t capacityAndLongFlag;
    std::uint64_t size;
    const char* data;
};
static_assert(sizeof(NativeString24) == 24);

bool BorrowString(const char* text, NativeString24& out) {
    if (!text) return false;
    std::size_t length = 0;
    while (length < 512 && text[length]) ++length;
    if (!length || length == 512) return false;
    out = {((length + 16u) & ~std::uint64_t{15}) | 1u, length, text};
    return true;
}
} // namespace

bool CopyNativeString(std::uintptr_t string, char* output, std::size_t capacity) {
    if (!output || !capacity) return false;
    output[0] = '\0';
    std::uint8_t bytes[24]{};
    if (!Readable(string, sizeof(bytes))) return false;
    std::memcpy(bytes, reinterpret_cast<const void*>(string), sizeof(bytes));
    std::uint64_t size = bytes[0] >> 1;
    std::uintptr_t data = string + 1;
    if (bytes[0] & 1) {
        std::memcpy(&size, bytes + 8, sizeof(size));
        std::memcpy(&data, bytes + 16, sizeof(data));
    } else if (size > 22) {
        return false;
    }
    if (size >= capacity || !Readable(data, static_cast<std::size_t>(size) + 1)) return false;
    std::memcpy(output, reinterpret_cast<const void*>(data), static_cast<std::size_t>(size));
    output[size] = '\0';
    return true;
}

bool NativeSprite::Create(std::uintptr_t module) {
    if (!module) return false;
    if (created_) return module == module_;
    module_ = module;
    reinterpret_cast<void (*)(void*)>(module_ + Constructor)(object_);
    created_ = true;
    return true;
}

bool NativeSprite::Load(const char* animationFile) {
    NativeString24 path{};
    if (!created_ || !BorrowString(animationFile, path)) return false;
    verified_animation_[0] = '\0';
    reinterpret_cast<void (*)(void*, const NativeString24*, bool)>(module_ + LoadAnimation)(object_, &path, true);
    const auto address = reinterpret_cast<std::uintptr_t>(object_);
    const auto count = Read<unsigned>(address, 0xa0);
    const auto animations = Read<std::uintptr_t>(address, 0x98);
    loaded_ = count > 0 && count <= 4096 && Readable(animations, static_cast<std::size_t>(count) * 0x150);
    return loaded_;
}

bool NativeSprite::SetSpritesheet(unsigned layer, const char* filename) {
    NativeString24 path{};
    if (!BorrowString(filename, path)) return false;
    return SetSpritesheetFromGameString(layer, reinterpret_cast<std::uintptr_t>(&path));
}

bool NativeSprite::SetSpritesheetFromGameString(unsigned layer, std::uintptr_t string) {
    if (!loaded_ || layer >= Read<unsigned>(reinterpret_cast<std::uintptr_t>(object_), 0xb0)) return false;
    char checked[512];
    if (!CopyNativeString(string, checked, sizeof(checked)) || !checked[0]) return false;
    reinterpret_cast<void (*)(void*, unsigned, const void*)>(module_ + ReplaceSpritesheet)(
        object_, layer, reinterpret_cast<const void*>(string));
    return true;
}

bool NativeSprite::HasAnimation(const char* animation) const {
    if (!loaded_ || !animation) return false;
    const auto address = reinterpret_cast<std::uintptr_t>(object_);
    const auto count = Read<unsigned>(address, 0xa0);
    const auto animations = Read<std::uintptr_t>(address, 0x98);
    if (!count || count > 4096 || !Readable(animations, static_cast<std::size_t>(count) * 0x150)) return false;
    for (unsigned i = 0; i < count; ++i) {
        char name[128];
        if (CopyNativeString(animations + static_cast<std::uintptr_t>(i) * 0x150, name, sizeof(name)) &&
            std::strcmp(name, animation) == 0) return true;
    }
    return false;
}

bool NativeSprite::SetFrame(const char* animation, int frame) {
    if (!loaded_ || !animation || !animation[0] || frame < 0) return false;
    if (std::strcmp(verified_animation_, animation) != 0) {
        const auto length = std::strlen(animation);
        if (length >= sizeof(verified_animation_) || !HasAnimation(animation)) return false;
        std::memcpy(verified_animation_, animation, length + 1);
    }
    reinterpret_cast<void (*)(void*, const char*, int)>(module_ + SetAnimationFrame)(object_, animation, frame);
    return true;
}

void NativeSprite::LoadGraphics() {
    if (loaded_) reinterpret_cast<void (*)(void*)>(module_ + LoadSpriteGraphics)(object_);
}

bool NativeSprite::HasLayerTexture(unsigned layer) const {
    if (!loaded_) return false;
    const auto sprite = reinterpret_cast<std::uintptr_t>(object_);
    const auto count = Read<unsigned>(sprite, 0xb0);
    const auto states = Read<std::uintptr_t>(sprite, 0xa8);
    if (count > 512 || layer >= count || !Readable(states, static_cast<std::size_t>(count) * 0xb0)) return false;
    const auto state = states + static_cast<std::uintptr_t>(layer) * 0xb0;
    // Native load_graphics (0xd2cc) sets +0x38 only after the loaded image
    // passes its validity check. A failed load can still have a control block.
    return Read<std::uint8_t>(state, 0x38) != 0 &&
           Readable(Read<std::uintptr_t>(state, 0xa0), 8) &&
           Readable(Read<std::uintptr_t>(state, 0xa8), 8);
}

void NativeSprite::SetTint(float r, float g, float b, float a) {
    if (created_) reinterpret_cast<void (*)(void*, float, float, float, float)>(module_ + SetColorTint)(object_ + 0xe8, r, g, b, a);
}

void NativeSprite::SetScale(float x, float y) {
    if (!created_) return;
    const NativeVec2 scale{x, y};
    std::memcpy(object_ + 0xdc, &scale, sizeof(scale));
}

void NativeSprite::Render(NativeVec2 position, NativeVec2 cropTopLeft, NativeVec2 cropBottomRight) {
    if (loaded_) reinterpret_cast<void (*)(void*, const NativeVec2*, const NativeVec2*, const NativeVec2*)>(module_ + RenderAnimation)(
        object_, &position, &cropTopLeft, &cropBottomRight);
}

void NativeSprite::RenderLayer(unsigned layer, NativeVec2 position, NativeVec2 cropTopLeft, NativeVec2 cropBottomRight) {
    if (!loaded_ || layer >= Read<unsigned>(reinterpret_cast<std::uintptr_t>(object_), 0xb0)) return;
    reinterpret_cast<void (*)(void*, unsigned, const NativeVec2*, const NativeVec2*, const NativeVec2*)>(module_ + RenderAnimationLayer)(
        object_, layer, &position, &cropTopLeft, &cropBottomRight);
}

bool NativeSprite::ExchangeLayerTexture(unsigned ownLayer, std::uintptr_t otherSprite, unsigned otherLayer) {
    if (!loaded_) return false;
    const auto pairFor = [](std::uintptr_t sprite, unsigned layer) -> std::uintptr_t {
        if (!Readable(sprite,0x158)) return 0;
        const auto count=Read<unsigned>(sprite,0xb0);
        const auto states=Read<std::uintptr_t>(sprite,0xa8);
        if (count>512 || layer>=count || !Readable(states,static_cast<std::size_t>(count)*0xb0)) return 0;
        const auto pair=states+static_cast<std::uintptr_t>(layer)*0xb0+0xa0;
        MemoryInfo info{}; std::uint32_t page{};
        if (svcQueryMemory(&info,&page,pair)!=0 || !(info.perm&Perm_W) || pair<info.addr ||
            pair-info.addr>info.size || 16>info.size-(pair-info.addr)) return 0;
        if (!Readable(Read<std::uintptr_t>(pair),8) || !Readable(Read<std::uintptr_t>(pair,8),8)) return 0;
        return pair;
    };
    const auto own=pairFor(reinterpret_cast<std::uintptr_t>(object_),ownLayer);
    const auto other=pairFor(otherSprite,otherLayer);
    if (!own || !other || own==other) return false;
    // Native LayerState::SetSpriteSheet and ANM2::Reset establish the paired
    // data/control layout. Exchange whole pairs: never increment/decrement a
    // reference or copy a refcounted pointer into a third owning location.
    std::byte temporary[16];
    std::memcpy(temporary,reinterpret_cast<const void*>(own),16);
    std::memcpy(reinterpret_cast<void*>(own),reinterpret_cast<const void*>(other),16);
    std::memcpy(reinterpret_cast<void*>(other),temporary,16);
    return true;
}

void NativeSprite::Destroy() {
    if (created_) reinterpret_cast<void (*)(void*)>(module_ + Destructor)(object_);
    created_ = false;
    loaded_ = false;
    module_ = 0;
    verified_animation_[0] = '\0';
}

} // namespace isaac

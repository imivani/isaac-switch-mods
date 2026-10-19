#pragma once

#include <cstddef>
#include <cstdint>

namespace isaac {

struct NativeVec2 { float x{}, y{}; };

// An ANM2 owned by this mod but constructed and destroyed by the game. The
// object contains self-pointers, so it must never be copied or moved.
class NativeSprite {
public:
    constexpr NativeSprite() = default;
    NativeSprite(const NativeSprite&) = delete;
    NativeSprite& operator=(const NativeSprite&) = delete;
    NativeSprite(NativeSprite&&) = delete;
    NativeSprite& operator=(NativeSprite&&) = delete;

    // module must already have passed the exact Repentance build guard.
    bool Create(std::uintptr_t module);
    bool Load(const char* animationFile);
    bool SetSpritesheet(unsigned layer, const char* filename);
    bool SetSpritesheetFromGameString(unsigned layer, std::uintptr_t string);
    bool SetFrame(const char* animation, int frame);
    // Parsed animation data does not prove its texture loaded. Require this
    // before suppressing any original game artwork.
    bool HasLayerTexture(unsigned layer) const;
    void LoadGraphics();
    void SetTint(float r, float g, float b, float a);
    void SetScale(float x, float y);
    void Render(NativeVec2 position, NativeVec2 cropTopLeft = {}, NativeVec2 cropBottomRight = {});
    void RenderLayer(unsigned layer, NativeVec2 position, NativeVec2 cropTopLeft = {}, NativeVec2 cropBottomRight = {});
    // Temporarily exchange live texture ownership pairs, draw, then exchange
    // back before either sprite can load, update, or be destroyed.
    bool ExchangeLayerTexture(unsigned ownLayer, std::uintptr_t otherSprite, unsigned otherLayer);

    // Must be called BEFORE the NRO unloads. There is deliberately no C++
    // destructor calling into the game after its lifetime has ended.
    void Destroy();
    bool Created() const { return created_; }
    bool Loaded() const { return loaded_; }

private:
    bool HasAnimation(const char* animation) const;
    std::uintptr_t module_{};
    bool created_{}, loaded_{};
    // The loaded animation table is immutable until Load/Destroy. Reuse its
    // last validated name when drawing many glyphs from one animation.
    char verified_animation_[128]{};
    // CreateGlobalAnim at 0x17924 allocates exactly 0x158 bytes for ANM2.
    alignas(8) std::byte object_[0x158]{};
};

// Read-only access to the game's 24-byte libc++ string representation.
// Copies into caller storage; never retains or frees a game's string buffer.
bool CopyNativeString(std::uintptr_t string, char* output, std::size_t capacity);

} // namespace isaac

#pragma once
#include <cstdint>

namespace isaac_item_visuals {
// Shared by the native draw and the actual NativeSprite host test. A temporary
// owning-handle exchange always reverses when this draw scope ends.
template<class Sprite> class TextureDrawScope {
    Sprite* sprite_{};
    std::uintptr_t other_{};
    bool* failure_{};
public:
    TextureDrawScope(Sprite* sprite, std::uintptr_t other, bool* failure = nullptr)
        : other_(other), failure_(failure) {
        if (sprite && sprite->ExchangeLayerTexture(1, other, 1)) sprite_ = sprite;
    }
    TextureDrawScope(const TextureDrawScope&) = delete;
    TextureDrawScope& operator=(const TextureDrawScope&) = delete;
    ~TextureDrawScope() {
        if (sprite_ && !sprite_->ExchangeLayerTexture(1, other_, 1) && failure_) *failure_ = true;
    }
    bool Active() const { return sprite_ != nullptr; }
};
}

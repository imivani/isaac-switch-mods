#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
extern "C" {
#include "lib/nx/kernel/svc.h"
}

namespace isaac {
struct FeatureColor { float r, g, b, a; };
struct FeatureDraw {
    void* context;
    void (*text)(void*, const char*, float, float, FeatureColor, float);
    void (*rectangle)(void*, float, float, float, float, FeatureColor);
    void Text(const char* value, float x, float y, FeatureColor color = {1,1,1,1}, float scale = 0.8f) const {
        text(context, value, x, y, color, scale);
    }
    void Rect(float x1, float y1, float x2, float y2, FeatureColor color) const {
        rectangle(context, x1, y1, x2, y2, color);
    }
};
struct FeatureFrame {
    std::uintptr_t module, game, room;
    float width, height;
    FeatureDraw draw;
};
namespace feature_memory {
inline bool Readable(std::uintptr_t p, std::size_t size) {
    if (p <= 0x10000 || !size || p + size < p) return false;
    MemoryInfo info{};
    std::uint32_t page = 0;
    if (svcQueryMemory(&info, &page, p) != 0 || !(info.perm & Perm_R)) return false;
    return p >= info.addr && p - info.addr <= info.size && size <= info.size - (p - info.addr);
}
template<class T> bool TryRead(std::uintptr_t p, T& out) {
    if (!Readable(p, sizeof(T))) return false;
    std::memcpy(&out, reinterpret_cast<const void*>(p), sizeof(T));
    return true;
}
template<class T> T Read(std::uintptr_t p, std::uintptr_t offset = 0) {
    T out{};
    TryRead(p + offset, out);
    return out;
}
}
}

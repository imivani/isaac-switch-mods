#include "overlay.hpp"
#include "feature_api.hpp"
#include "planetarium.hpp"
#include "regret.hpp"
#include "boss_ui.hpp"
#include "eid_features.hpp"
#include "debug_kit.hpp"
#include "item_visuals.hpp"
#include "mod_settings.hpp"
#include "hit_counter.hpp"
#include "rplus.hpp"
#include "mod_menu.hpp"
#include "../../../features/clean_map/generated/native.hpp"
#include "generated/build_profile.hpp"
#include "generated/features.hpp"
extern "C" {
#include "lib/nx/kernel/svc.h"
}
#include <cstdio>
#include <cstring>

namespace isaac {
namespace {
using namespace profile;
struct Color { float r, g, b, a; };
// Native Color's copy constructor makes its by-value argument indirect.
// These signatures describe the verified machine ABI, rather than recreating
// the game's C++ classes and accidentally passing Color in float registers.
using DrawText = void (*)(void*, const char*, float, float, float, float, const Color*, unsigned, bool);
using DrawQuad = void (*)(void*, float, float, float, float, const Color*);

template<class T> T Read(std::uintptr_t p, std::uintptr_t offset = 0) {
    T value;
    std::memcpy(&value, reinterpret_cast<const void*>(p + offset), sizeof(value));
    return value;
}
bool Pointer(std::uintptr_t p) { return p > 0x10000 && (p & 7) == 0; }
bool Readable(std::uintptr_t p, std::size_t size) {
    if (p <= 0x10000 || !size || p + size < p) return false;
    MemoryInfo info{};
    std::uint32_t page = 0;
    if (svcQueryMemory(&info, &page, p) != 0 || !(info.perm & Perm_R)) return false;
    return p >= info.addr && p - info.addr <= info.size && size <= info.size - (p - info.addr);
}
bool NoHudSeed(std::uintptr_t game) {
    // Mirror HUD::Render's special-seed lookup for effect 10. A malformed
    // tree hides the overlay instead of following arbitrary pointers.
    if (!Readable(game + 0x25da0, 8)) return true;
    auto node = Read<std::uintptr_t>(game, 0x25da0);
    for (unsigned depth = 0; node && depth < 64; ++depth) {
        if (!Pointer(node) || !Readable(node, 0x20)) return true;
        const auto key = Read<int>(node, 0x1c);
        if (key == 10) return true;
        node = Read<std::uintptr_t>(node, key < 10 ? 8 : 0);
    }
    return node != 0;
}

struct Canvas {
    std::uintptr_t base;
    void* font;
    float width, height;
    void Text(const char* text, float x, float y, Color color, float scale = 0.8f) const {
        if (!Readable(reinterpret_cast<std::uintptr_t>(font),1) || !Read<std::uint8_t>(reinterpret_cast<std::uintptr_t>(font))) return;
        reinterpret_cast<DrawText>(base + DrawStringScaled)(font, text, x, y, scale, scale, &color, 0, false);
    }
    void Rect(float x1, float y1, float x2, float y2, Color color) const {
        reinterpret_cast<DrawQuad>(base + FillQuad)(reinterpret_cast<void*>(base + ShapeRenderer), x1, y1, x2, y2, &color);
    }
};


}

void RenderOverlay(std::uintptr_t module, void* hud) {
    if constexpr (ExtendedFeatures) PollModMenuPreferences();
    const bool menuOpen=ExtendedFeatures && IsDebugMenuOpen();
    if (!menuOpen && (!hud || !Readable(reinterpret_cast<std::uintptr_t>(hud) + HudVisible, 1) ||
        !Read<std::uint8_t>(reinterpret_cast<std::uintptr_t>(hud), HudVisible))) return;
    const auto manager = Read<std::uintptr_t>(module, ManagerGlobal);
    const auto game = Read<std::uintptr_t>(module, GameGlobal);
    const float width = Read<float>(module, ScreenWidth), height = Read<float>(module, ScreenHeight);
    if (!Pointer(manager) || !Pointer(game) || !Readable(manager + ManagerFont, 0x100) ||
        !Readable(game, PlayersEnd + 8) || !(width >= 200 && width <= 4096) || !(height >= 150 && height <= 2160)) return;
    if (!menuOpen && NoHudSeed(game)) return;
    Canvas canvas{module, reinterpret_cast<void*>(manager + ManagerFont), width, height};
    if constexpr (DiagnosticOnly) {
        canvas.Text("Native mod test: LOADED", 12, height - 14, {0.5f,1,0.6f,1}, 0.65f);
        return;
    }
    if constexpr (ShowStatusBanner)
        canvas.Text("Isaac: four-mod test build", 12, height - 14, {0.5f,1,0.6f,1}, 0.65f);
    const auto room = Read<std::uintptr_t>(game, GameRoom);
    if (!Pointer(room) || !Readable(room, RoomEntityCount + 4)) return;
    const FeatureFrame frame{module, game, room, width, height, {
        &canvas,
        [](void* context, const char* text, float x, float y, FeatureColor color, float scale) {
            static_cast<Canvas*>(context)->Text(text, x, y, {color.r,color.g,color.b,color.a}, scale);
        },
        [](void* context, float x1, float y1, float x2, float y2, FeatureColor color) {
            static_cast<Canvas*>(context)->Rect(x1, y1, x2, y2, {color.r,color.g,color.b,color.a});
        }
    }};
    if (menuOpen) { RenderDebugKit(frame); return; }
    // A disappearing pickup may leave no live entities. Let its stored icon
    // finish fading, and keep the probability display alive in empty rooms.
    if (IsModEnabled(ModFeature::Regret)) RenderRegret(frame);
    if (IsModEnabled(ModFeature::Planetarium)) RenderPlanetarium(frame);
    if (IsModEnabled(ModFeature::BossBars)) RenderBossUi(frame);
    if constexpr (ExtendedFeatures) if (IsModEnabled(ModFeature::HitCounter)) RenderHitCounter(frame);
    if constexpr (ExtendedFeatures) if (IsModEnabled(ModFeature::RepentancePlus)) RenderBossRushWave(frame);
    if constexpr (ExtendedFeatures) RenderDebugKit(frame);
    if constexpr (ExtendedFeatures) RenderItemVisualsStatus(frame);
    const auto array = Read<std::uintptr_t>(room, RoomEntities);
    const auto count = Read<unsigned>(room, RoomEntityCount);
    // Room/grid descriptions and held-item reminders also work in rooms with
    // no active entities. Pass an empty list instead of skipping all EID.
    if (count > 4096 || (count && (!Pointer(array) || !Readable(array, count * sizeof(std::uintptr_t))))) return;
    const auto* entities = count ? reinterpret_cast<const std::uintptr_t*>(array) : nullptr;
    if (IsModEnabled(ModFeature::Eid)) RenderEidFeatures(frame, entities, count);
}

void RenderTitleModMenu(std::uintptr_t module) {
    const auto manager=feature_memory::Read<std::uintptr_t>(module,ManagerGlobal);
    const float width=feature_memory::Read<float>(module,ScreenWidth), height=feature_memory::Read<float>(module,ScreenHeight);
    if (!Pointer(manager) || !Readable(manager+clean_map::native::MenuNativeFont,0x100) || !(width>=250 && width<=4096) || !(height>=180 && height<=2160)) return;
    // Native menu color correction can leave a non-UI shader selected.
    // This exact helper is checked before the menu hooks are installed.
    reinterpret_cast<void (*)(unsigned)>(module+clean_map::native::LoadShader)(1);
    Canvas canvas{module,reinterpret_cast<void*>(manager+clean_map::native::MenuNativeFont),width,height};
    const FeatureFrame frame{module,0,0,width,height,{
        &canvas,
        [](void* ctx,const char* text,float x,float y,FeatureColor c,float scale) {
            static_cast<Canvas*>(ctx)->Text(text,x,y,{c.r,c.g,c.b,c.a},scale);
        },
        [](void* ctx,float x1,float y1,float x2,float y2,FeatureColor c) {
            static_cast<Canvas*>(ctx)->Rect(x1,y1,x2,y2,{c.r,c.g,c.b,c.a});
        }
    }};
    RenderDebugKit(frame);
}
}

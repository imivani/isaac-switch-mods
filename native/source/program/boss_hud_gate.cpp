#include "boss_hud_gate.hpp"
#include "feature_api.hpp"
#include "mod_settings.hpp"
#include "generated/build_profile.hpp"
#include "../../../features/clean_map/generated/native.hpp"
#include <cstring>

namespace isaac {
namespace {
namespace api = clean_map::native;
std::uintptr_t s_Module{};
bool s_Ready{};

bool Pointer(std::uintptr_t p) { return p > 0x10000 && !(p & 7); }
bool Writable(std::uintptr_t p, std::size_t size) {
    if (p <= 0x10000 || !size || p + size < p) return false;
    MemoryInfo info{}; std::uint32_t page{};
    constexpr std::uint32_t ReadWrite = 3; // Horizon Perm_R | Perm_W.
    return svcQueryMemory(&info, &page, p) == 0 && (info.perm & ReadWrite) == ReadWrite &&
        p >= info.addr && p - info.addr <= info.size && size <= info.size - (p - info.addr);
}
template<std::size_t N> bool Matches(std::uintptr_t p, const unsigned char (&guard)[N]) {
    return feature_memory::Readable(p, N) && std::memcmp(reinterpret_cast<const void*>(p), guard, N) == 0;
}

bool NativeBarRoom(std::uintptr_t game) {
    std::uintptr_t room{}, descriptor{}, data{};
    int boss{}, type{}, variant{};
    if (!feature_memory::TryRead(game + profile::GameRoom, room) || !Pointer(room) ||
        room > UINTPTR_MAX - api::RoomBossId ||
        !feature_memory::TryRead(room + api::RoomBossId, boss)) return false;
    // The same HUD branch retains Gideon's native wave count (83) and hides
    // boss IDs 55/88. Let those cases run without any temporary write.
    if (boss == 0x53 || boss == 0x37 || boss == 0x58) return false;
    // Mirror the independently verified Room::IsBeastRoom getter read-only.
    // Its return is true only for room-config type 16, variant 35.
    if (!feature_memory::TryRead(room + 8, descriptor) || !Pointer(descriptor) ||
        descriptor > UINTPTR_MAX - 0x10 || !feature_memory::TryRead(descriptor + 0x10, data) ||
        !Pointer(data) || data > UINTPTR_MAX - 8 ||
        !feature_memory::TryRead(data + 8, type) || !feature_memory::TryRead(data, variant)) return false;
    return type != 16 || variant != 35;
}
}

bool InitializeBossHudGate(std::uintptr_t module) {
    s_Module = module;
    s_Ready = module && module <= UINTPTR_MAX - profile::GameGlobal &&
        Matches(module + api::SetBossHealth, api::SetBossHealthGuard) &&
        Matches(module + api::BossHealthGuardSite, api::BossHealthGuardSiteGuard) &&
        Matches(module + api::BossHiddenReturn, api::BossHiddenReturnGuard);
    return s_Ready;
}

BossHudRestore BeginBossHud(void* hud_pointer) {
    BossHudRestore restore{};
    if (!s_Ready || !IsModEnabled(ModFeature::BossBars)) return restore;
    const auto hud = reinterpret_cast<std::uintptr_t>(hud_pointer);
    std::uintptr_t game{};
    if (!Pointer(hud) || hud > UINTPTR_MAX - api::HudBossPosition ||
        !feature_memory::TryRead(s_Module + profile::GameGlobal, game) || !Pointer(game) ||
        game > UINTPTR_MAX - api::GameHud || hud != game + api::GameHud ||
        !NativeBarRoom(game) || !Writable(hud + api::HudBossHealth, sizeof(std::uint32_t)) ||
        !Writable(hud + api::HudBossPosition, sizeof(std::uint8_t))) return restore;
    std::uint32_t health{};
    std::memcpy(&health, reinterpret_cast<const void*>(hud + api::HudBossHealth), sizeof(health));
    // Negative health already suppresses native bars. Do not reinterpret or
    // normalize NaNs/infinities; preserve the original behavior on bad data.
    if ((health & 0x80000000u) || (health & 0x7f800000u) == 0x7f800000u) return restore;
    restore = {hud, health, feature_memory::Read<std::uint8_t>(hud, api::HudBossPosition), true};
    const std::uint32_t hidden = 0xbf800000u; // -1.0f, the native hidden sentinel.
    std::memcpy(reinterpret_cast<void*>(hud + api::HudBossHealth), &hidden, sizeof(hidden));
    return restore;
}

void EndBossHud(BossHudRestore& restore) {
    if (!restore.active) return;
    // Always restore, even if a setting changed after Begin. Never retain a
    // borrowed game pointer past this one synchronous HUD call.
    if (Writable(restore.hud + api::HudBossHealth, sizeof(restore.health_bits)) &&
        Writable(restore.hud + api::HudBossPosition, sizeof(restore.position))) {
        std::memcpy(reinterpret_cast<void*>(restore.hud + api::HudBossHealth), &restore.health_bits, sizeof(restore.health_bits));
        std::memcpy(reinterpret_cast<void*>(restore.hud + api::HudBossPosition), &restore.position, sizeof(restore.position));
    }
    restore.active = false;
}
}

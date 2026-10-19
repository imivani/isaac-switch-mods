#include "instant_break.hpp"
#include "feature_api.hpp"
#include "lib.hpp"
#include "mod_settings.hpp"
#include "../../../features/instant_break/adapter.hpp"
#include <cstring>

namespace isaac {
namespace {
namespace ib = instant_break;
namespace api = instant_break::native;
std::uintptr_t s_Module{}, s_InstalledModule{};
bool s_Ready{};
std::uint8_t s_InstalledPrologue[20]{};
constinit ib::State s_State;

struct Engine {
    template<class T> bool Read(std::uintptr_t address, T& value) {
        return feature_memory::TryRead(address,value);
    }
    bool Readable(std::uintptr_t address, std::size_t bytes) {
        return feature_memory::Readable(address,bytes);
    }
    void DestroyGrid(std::uintptr_t address) {
        // The exact native wrapper dispatches the object's Hurt(4), preserving
        // its drops, particles, red-poop respawn and giant-poop ownership.
        reinterpret_cast<void (*)(void*,bool)>(s_Module + api::GridDestroy)(reinterpret_cast<void*>(address),false);
    }
    void KillNpc(std::uintptr_t address) {
        reinterpret_cast<void (*)(void*)>(s_Module + api::NpcKill)(reinterpret_cast<void*>(address));
    }
};
HOOK_DEFINE_TRAMPOLINE(InstantBreakNpcDamage) {
    // AAPCS64: this=x0, amount=s0, flags=x1, source=x2, countdown=w3;
    // the original returns its boolean in w0. No guessed EntityRef struct.
    static bool Callback(void* entity, float amount, std::uint64_t flags, const void* source, int countdown) {
        if (!s_Ready || !IsModEnabled(ModFeature::InstantBreak)) return Orig(entity,amount,flags,source,countdown);
        Engine engine;
        return ib::TakeDamage(engine,s_Module,reinterpret_cast<std::uintptr_t>(entity),amount,flags,source,countdown,
            [](std::uintptr_t pointer, float damage, std::uint64_t damageFlags, const void* reference, int frames) {
                return Orig(reinterpret_cast<void*>(pointer),damage,damageFlags,reference,frames);
            });
    }
};
template<std::size_t N>
bool Matches(std::uintptr_t address, const std::uint8_t (&bytes)[N]) {
    return feature_memory::Readable(address,N) && std::memcmp(reinterpret_cast<const void*>(address),bytes,N) == 0;
}
}
bool InitializeInstantBreak(std::uintptr_t module) {
    s_State.Reset();
    if (s_Ready && module == s_InstalledModule && Matches(module + api::NpcTakeDamage,s_InstalledPrologue)) return true;
    s_Ready = false;
    s_Module = module;
    if (!module || !Matches(module + api::NpcTakeDamage,api::NpcTakeDamageGuard) ||
        !Matches(module + api::NpcKill,api::NpcKillGuard) ||
        !Matches(module + api::GridDestroy,api::GridDestroyGuard) ||
        !Matches(module + api::PoopHurt,api::PoopHurtGuard) ||
        !Matches(module + api::RoomGridGetter,api::RoomGridGetterGuard) ||
        !Matches(module + api::RoomTriggerClear,api::RoomTriggerClearGuard)) return false;
    InstantBreakNpcDamage::InstallAtPtr(module + api::NpcTakeDamage);
    std::memcpy(s_InstalledPrologue,reinterpret_cast<const void*>(module + api::NpcTakeDamage),sizeof(s_InstalledPrologue));
    s_InstalledModule = module;
    s_Ready = true;
    return true;
}
void UpdateInstantBreak(std::uintptr_t game) {
    if (!s_Ready || !IsModEnabled(ModFeature::InstantBreak)) return;
    Engine engine;
    ib::AfterUpdate(s_State,engine,s_Module,game);
}
void ResetInstantBreakRoom() { s_State.Reset(); }
void ResetInstantBreak() {
    // A refused SDK unload leaves the hook active; keep its verified mapping.
    s_State.Reset();
}
}

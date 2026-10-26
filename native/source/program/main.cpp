#include "lib.hpp"
#include "overlay.hpp"
#include "planetarium.hpp"
#include "regret.hpp"
#include "boss_ui.hpp"
#include "eid_features.hpp"
#include "timemachine.hpp"
#include "debug_kit.hpp"
#include "instant_break.hpp"
#include "item_visuals.hpp"
#include "specialist.hpp"
#include "mod_settings.hpp"
#include "clean_map.hpp"
#include "boss_hud_gate.hpp"
#include "hit_counter.hpp"
#include "rplus.hpp"
#include "generated/build_profile.hpp"
#include "generated/features.hpp"
#include <cstring>

// Confirmed SDK exports; Module remains opaque. The SDK owns its layout.
namespace nn::ro {
struct Module;
std::uint32_t LoadModule(Module*, const void*, void*, std::size_t, int);
std::uint32_t UnloadModule(Module*);
std::uint32_t LookupModuleSymbol(std::uintptr_t*, const Module*, const char*);
}

namespace {
std::uintptr_t s_Repentance = 0;
nn::ro::Module* s_Module = nullptr;

HOOK_DEFINE_TRAMPOLINE(RenderHud) {
    static void Callback(void* hud) {
        if constexpr (!isaac::DiagnosticOnly) isaac::BeginPlanetariumHud();
        auto restore=isaac::BeginBossHud(hud);
        Orig(hud);
        isaac::EndBossHud(restore);
        if (s_Repentance) isaac::RenderOverlay(s_Repentance, hud);
    }
};

HOOK_DEFINE_TRAMPOLINE(LoadGameModule) {
    static std::uint32_t Callback(nn::ro::Module* mod, const void* image,
                                  void* bss, std::size_t bss_size, int flags) {
        // The input is an SDK NRO image, not necessarily its eventual address.
        const auto* bytes = static_cast<const std::uint8_t*>(image);
        const bool expected = bytes &&
            std::memcmp(bytes + 0x10, "NRO0", 4) == 0 &&
            std::memcmp(bytes + 0x40, isaac::profile::NroBuildId, 32) == 0;
        const auto result = Orig(mod, image, bss, bss_size, flags);
        if (result != 0 || !expected || s_Repentance) return result;
        std::uintptr_t render = 0;
        if (nn::ro::LookupModuleSymbol(&render, mod, isaac::profile::LookupSymbol) != 0 ||
            render < isaac::profile::HudRender ||
            (render & 0xfff) > 0x1000 - sizeof(isaac::profile::HudPrologue) ||
            std::memcmp(reinterpret_cast<const void*>(render), isaac::profile::HudPrologue,
                        sizeof(isaac::profile::HudPrologue)) != 0) {
            Logging.Log("Isaac prototype: loaded module failed the HUD guard; leaving it alone.\n");
            return result;
        }
        s_Repentance = render - isaac::profile::HudRender;
        s_Module = mod;
        if constexpr (!isaac::DiagnosticOnly) {
            isaac::ResetPlanetarium();
            isaac::ResetBossUi();
            isaac::SetModAvailable(isaac::ModFeature::BossBars,isaac::InitializeBossHudGate(s_Repentance));
            isaac::ResetEidFeatures();
            const bool readyPlanetarium=isaac::InitializePlanetarium(s_Repentance);
            isaac::SetModAvailable(isaac::ModFeature::Planetarium,readyPlanetarium);
            if (!readyPlanetarium)
                Logging.Log("Isaac mods: Planetarium layout hook failed its guard; that feature is disabled.\n");
            const bool readyTimeMachine=isaac::InitializeTimeMachine(s_Repentance);
            isaac::SetModAvailable(isaac::ModFeature::TimeMachine,readyTimeMachine);
            isaac::SetModAvailable(isaac::ModFeature::MachineLock,readyTimeMachine);
            if (!readyTimeMachine)
                Logging.Log("Isaac mods: game-update hooks failed their guards; Time Machine and update-dependent extras are disabled.\n");
            const bool readyRegret=isaac::InitializeRegret(s_Repentance);
            isaac::SetModAvailable(isaac::ModFeature::Regret,readyRegret);
            if (!readyRegret)
                Logging.Log("Isaac prototype: Regret hooks failed their guards; that feature is disabled.\n");
            if constexpr (isaac::ExtendedFeatures) {
                if (!isaac::InitializeDebugKit(s_Repentance))
                    Logging.Log("Isaac mods: test-item shortcut failed its guards; disabled.\n");
                const bool readyInstantBreak=isaac::InitializeInstantBreak(s_Repentance);
                isaac::SetModAvailable(isaac::ModFeature::InstantBreak,readyInstantBreak && readyTimeMachine);
                if (!readyInstantBreak)
                    Logging.Log("Isaac mods: instant fires/poops failed their guards; disabled.\n");
                const bool readyVisuals=isaac::InitializeItemVisuals(s_Repentance);
                isaac::SetModAvailable(isaac::ModFeature::AnimatedItems,readyVisuals);
                isaac::SetModAvailable(isaac::ModFeature::Birthright,readyVisuals);
                isaac::SetModAvailable(isaac::ModFeature::CleanMap,readyVisuals && isaac::InitializeCleanMap(s_Repentance));
                if (!readyVisuals)
                    Logging.Log("Isaac mods: item animations/Birthright failed their guards; disabled.\n");
                const bool readyHits=isaac::InitializeHitCounter(s_Repentance);
                isaac::SetModAvailable(isaac::ModFeature::HitCounter,readyHits);
                if (!readyHits)
                    Logging.Log("Isaac mods: Hit Counter failed its guards; disabled.\n");
                const bool readyRplus=isaac::InitializeRepentancePlus(s_Repentance);
                isaac::SetModAvailable(isaac::ModFeature::RepentancePlus,readyRplus);
                if (!readyRplus)
                    Logging.Log("Isaac mods: Repentance+ Rules failed their guards; vanilla rules are unchanged.\n");
                const bool readyGreed=isaac::InitializeGreedMachine(s_Repentance);
                isaac::SetModAvailable(isaac::ModFeature::GreedNoJam,readyGreed);
                if (!readyGreed)
                    Logging.Log("Isaac mods: Greed machine guard failed; jams are unchanged.\n");
                const bool readySpecialist=isaac::InitializeSpecialist(s_Repentance);
                isaac::SetModAvailable(isaac::ModFeature::Specialist,readySpecialist && readyTimeMachine);
                if (!readySpecialist)
                    Logging.Log("Isaac mods: Specialist failed its guards; disabled.\n");
            }
        }
        RenderHud::InstallAtPtr(render);
        Logging.Log("Isaac prototype: matched Repentance 1.7.9b; HUD hook installed.\n");
        return result;
    }
};

HOOK_DEFINE_TRAMPOLINE(UnloadGameModule) {
    static std::uint32_t Callback(nn::ro::Module* mod) {
        // Owned sprites call game code when destroyed. Release them while the
        // game module is still mapped, before asking the SDK to unload it.
        if (mod == s_Module) {
            isaac::ResetRegret();
            isaac::ResetPlanetarium();
            isaac::ResetBossUi();
            isaac::ResetEidFeatures();
            isaac::ResetTimeMachine();
            if constexpr (isaac::ExtendedFeatures) {
                isaac::ResetDebugKit();
                isaac::ResetInstantBreak();
                isaac::ResetItemVisuals();
                isaac::ResetCleanMap();
                isaac::ResetSpecialist();
                isaac::ResetHitCounter();
                isaac::ResetGreedMachine();
                isaac::ResetRepentancePlus();
            }
        }
        const auto result = Orig(mod);
        if (result == 0 && mod == s_Module) {
            if constexpr (isaac::ExtendedFeatures) isaac::OnSpecialistModuleUnloaded();
            s_Repentance = 0;
            s_Module = nullptr;
            Logging.Log("Isaac prototype: Repentance unloaded; cleared module addresses.\n");
        }
        return result;
    }
};
}

extern "C" void exl_main(void*, void*) {
    // Refuse to modify an SDK with different entry-point instructions.
    if (std::memcmp(reinterpret_cast<const void*>(nn::ro::LoadModule),
                    isaac::profile::SdkLoadPrologue, 20) != 0 ||
        std::memcmp(reinterpret_cast<const void*>(nn::ro::UnloadModule),
                    isaac::profile::SdkUnloadPrologue, 20) != 0) {
        Logging.Log("Isaac prototype: SDK entry points do not match; no hooks installed.\n");
        return;
    }
    exl::hook::Initialize();
    LoadGameModule::InstallAtFuncPtr(nn::ro::LoadModule);
    UnloadGameModule::InstallAtFuncPtr(nn::ro::UnloadModule);
    Logging.Log("Isaac prototype: waiting for the verified Repentance module.\n");
}

extern "C" NORETURN void exl_exception_entry() {
    EXL_ABORT("Isaac prototype exception");
}

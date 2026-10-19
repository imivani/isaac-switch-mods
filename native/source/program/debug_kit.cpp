#include "debug_kit.hpp"
#include "eid_ui.hpp"
#include "mod_menu.hpp"
#include "overlay.hpp"
#include "../../../features/menu_ui/menu.hpp"
#include "generated/build_profile.hpp"
#include "generated/debug_kit_guards.hpp"
#include "../../../features/item_menu/adapter.hpp"
#include "../../../features/clean_map/generated/native.hpp"
#include "lib.hpp"
#include <cstring>
#include <cstdio>

// Entry points for the paper mod menu (mod_menu.cpp): the in-run button hold,
// and X on the title screen's Mods menu. The pause-screen Y entry lives in
// mod_menu.cpp because it hooks the pause screen itself.
namespace isaac {
namespace {
using feature_memory::Readable;
using feature_memory::Read;
namespace api=debug_kit::native;
namespace menuApi=clean_map::native;
namespace picker=item_menu;
std::uintptr_t s_Module{},s_InstalledModule{};
bool s_Ready{},s_TitleReady{},s_ReleaseGate{},s_TitleGate{};
std::uint8_t s_MenuUpdateGuard[20]{},s_MenuRenderGuard[20]{};
constinit debug_kit::Chord s_Chords[8];
constinit debug_kit::Context s_Context;
unsigned s_Progress{};
std::uint32_t s_TitlePrevious[9]{};

std::uint64_t Now() { return reinterpret_cast<std::uint64_t (*)()>(s_Module+api::Milliseconds)(); }
bool DeviceExists(std::uintptr_t input,unsigned controller) {
    return Readable(input,0x80) && debug_kit::ValidController(controller) &&
        reinterpret_cast<bool (*)(void*,unsigned)>(s_Module+menuApi::DoesDeviceExist)(reinterpret_cast<void*>(input),controller);
}
struct Engine {
    template<class T> T Read(std::uintptr_t p,std::uintptr_t offset=0) { return feature_memory::Read<T>(p,offset); }
    bool Readable(std::uintptr_t p,std::size_t size) { return feature_memory::Readable(p,size); }
    bool Paused(std::uintptr_t game) { return reinterpret_cast<bool (*)(void*)>(s_Module+api::IsPaused)(reinterpret_cast<void*>(game)); }
};
std::uint32_t Buttons(std::uintptr_t input,unsigned controller) {
    if (!Readable(input,0x80) || !debug_kit::ValidController(controller)) return 0;
    std::uint32_t mask{};
    const auto pressed=reinterpret_cast<bool (*)(void*,unsigned,unsigned,unsigned*)>(s_Module+api::IsButtonPressed);
    for (unsigned i=0;i<15;++i) if (pressed(reinterpret_cast<void*>(input),i,controller,nullptr)) mask|=1u<<i;
    return mask;
}
template<std::size_t N> bool Matches(std::uintptr_t p,const std::uint8_t (&guard)[N]) {
    return Readable(p,N) && std::memcmp(reinterpret_cast<const void*>(p),guard,N)==0;
}
bool TitleScreen(std::uintptr_t menu) {
    return s_TitleReady && Read<std::uintptr_t>(s_Module,menuApi::MenuManagerGlobal)==menu && Readable(menu,0x3c) &&
        Read<unsigned>(menu,menuApi::MenuState)==3 && !Read<std::uint8_t>(menu,0) && !Read<std::uint8_t>(menu,0x20);
}
bool TitleInput(std::uintptr_t menu) {
    PollModMenuPreferences();
    if (!TitleScreen(menu)) {
        if (IsModMenuOpenFrom(MenuOrigin::Title)) AbortModMenu();
        s_TitleGate=false;
        for(auto& previous:s_TitlePrevious) previous=~0u;
        return false;
    }
    const auto input=Read<std::uintptr_t>(s_Module,menuApi::InputManagerGot);
    if (IsModMenuOpenFrom(MenuOrigin::Title)) {
        if (!UpdateModMenu(input)) s_TitleGate=true;
        return true;
    }
    // After closing, swallow the title menu's input until every button is up
    // so the closing B does not also leave the Mods screen.
    if (s_TitleGate) {
        bool held{};
        for(unsigned index=0;index<9;++index) {
            const unsigned controller=index<8?index+1:33;
            if (DeviceExists(input,controller) && Buttons(input,controller)) held=true;
        }
        if (held) return true;
        s_TitleGate=false;
        for(auto& previous:s_TitlePrevious) previous=~0u;
    }
    // The Switch menu accepts all devices. Preserve that behavior and bind
    // the menu to the one controller that actually presses X.
    for(unsigned index=0;index<9;++index) {
        const unsigned controller=index<8?index+1:33;
        if (!DeviceExists(input,controller)) { s_TitlePrevious[index]=~0u; continue; }
        const auto buttons=Buttons(input,controller);
        const bool open=(buttons & ~s_TitlePrevious[index] & menu_ui::Bit(menu_ui::X));
        s_TitlePrevious[index]=buttons;
        if (open) { OpenModMenu(MenuOrigin::Title,controller,0); return true; }
    }
    return false;
}
HOOK_DEFINE_TRAMPOLINE(ModsMenuUpdate) {
    static void Callback(void* menu) {
        if (!TitleInput(reinterpret_cast<std::uintptr_t>(menu))) Orig(menu);
    }
};
HOOK_DEFINE_TRAMPOLINE(ModsMenuRender) {
    static void Callback(void* menu) {
        Orig(menu);
        if (TitleScreen(reinterpret_cast<std::uintptr_t>(menu))) RenderTitleModMenu(s_Module);
    }
};
void CancelChords() {
    for (auto& chord:s_Chords) chord.Cancel();
    s_Progress=0;
}
}
bool InitializeDebugKit(std::uintptr_t module) {
    ResetDebugKit(); s_Module=module; s_TitleReady=false;
    s_Ready=module && Matches(module+api::IsButtonPressed,api::IsButtonPressedGuard) && Matches(module+api::IsPaused,api::IsPausedGuard) &&
        Matches(module+api::Milliseconds,api::MillisecondsGuard) && Matches(module+menuApi::DoesDeviceExist,menuApi::DoesDeviceExistGuard) &&
        InitializeModMenu(module);
    if (!s_Ready) return false;
    if (module==s_InstalledModule && Matches(module+menuApi::MenuUpdate,s_MenuUpdateGuard) && Matches(module+menuApi::MenuRender,s_MenuRenderGuard)) s_TitleReady=true;
    else if (Matches(module+menuApi::MenuUpdate,menuApi::MenuUpdateGuard) && Matches(module+menuApi::MenuRender,menuApi::MenuRenderGuard) &&
             Matches(module+menuApi::LoadShader,menuApi::LoadShaderGuard)) {
        ModsMenuUpdate::InstallAtPtr(module+menuApi::MenuUpdate);
        ModsMenuRender::InstallAtPtr(module+menuApi::MenuRender);
        std::memcpy(s_MenuUpdateGuard,reinterpret_cast<const void*>(module+menuApi::MenuUpdate),20);
        std::memcpy(s_MenuRenderGuard,reinterpret_cast<const void*>(module+menuApi::MenuRender),20);
        s_InstalledModule=module; s_TitleReady=true;
    }
    return true;
}
void CancelDebugKitHold() {
    CancelChords();
    if (IsModMenuOpenFrom(MenuOrigin::Shortcut)) AbortModMenu();
}
void ResetDebugKit() {
    ResetModMenu();
    for(auto& chord:s_Chords) chord.Reset();
    s_Context={}; s_ReleaseGate=s_TitleGate=false; s_Progress=0;
    for(auto& previous:s_TitlePrevious) previous=~0u;
}
bool IsDebugMenuOpen() { return s_Ready && IsModMenuOpenFrom(MenuOrigin::Shortcut); }
bool ProcessDebugKitInput(std::uintptr_t game) {
    if (!s_Ready) return false;
    PollModMenuPreferences();
    const bool open=IsModMenuOpenFrom(MenuOrigin::Shortcut);
    if (!open && !s_ReleaseGate) return false;
    Engine engine; picker::Snapshot snap;
    if (game!=s_Context.game || !picker::SamePlayer(engine,s_Module,s_Context,snap) || !DeviceExists(snap.input,s_Context.controller)) {
        if (open) AbortModMenu();
        s_ReleaseGate=false;
        return false;
    }
    if (open) {
        if (!UpdateModMenu(snap.input)) s_ReleaseGate=true;
        return true;
    }
    if (!Buttons(snap.input,s_Context.controller)) s_ReleaseGate=false;
    return true;
}
void UpdateDebugKit(std::uintptr_t game) {
    s_Progress=0;
    if (!s_Ready || IsModMenuOpen() || s_ReleaseGate) return;
    const unsigned hold=ModMenuShortcutMilliseconds();
    // Shortcut turned off: forget any partial hold so turning it back on starts clean.
    if (!hold) { for (auto& chord:s_Chords) chord.Reset(); return; }
    Engine engine; picker::Snapshot snap;
    if (!picker::Inspect(engine,s_Module,game,snap)) { CancelChords(); return; }
    const auto now=Now(); unsigned seen[8]{},seenCount{};
    for(unsigned i=0;i<8;++i) {
        auto& chord=s_Chords[i]; debug_kit::Context context;
        chord.SetHold(hold);
        if (!picker::CurrentPlayer(engine,snap,i,context)) { chord.Cancel(); continue; }
        bool duplicate{};
        for(unsigned j=0;j<seenCount;++j) if(seen[j]==context.controller) duplicate=true;
        if(duplicate) { chord.Cancel(); continue; }
        seen[seenCount++]=context.controller;
        const auto buttons=Buttons(snap.input,context.controller);
        if(chord.Step(context,true,buttons & menu_ui::Bit(menu_ui::LS),buttons & menu_ui::Bit(menu_ui::RS),buttons & menu_ui::Bit(menu_ui::Down),now)) {
            s_Context=context; s_ReleaseGate=false;
            OpenModMenu(MenuOrigin::Shortcut,context.controller,context.player);
            CancelChords();
            break;
        }
        if(chord.Progress(now)>s_Progress) s_Progress=chord.Progress(now);
    }
}
void RenderDebugKit(const FeatureFrame& frame) {
    if (!s_Ready) return;
    if (IsModMenuOpenFrom(frame.game?MenuOrigin::Shortcut:MenuOrigin::Title)) { RenderModMenu(frame); return; }
    if (!frame.game) { DrawEidMenuText(frame,"X: Mods",frame.width-66,frame.height-18,0xe4b4ed); return; }
    if (s_Progress) {
        char text[64]; std::snprintf(text,sizeof(text),"Opening mod menu... %u%%",s_Progress>100?100:s_Progress);
        DrawEidMenuText(frame,text,16,frame.height-26,0xe2c6ed);
    }
}
}

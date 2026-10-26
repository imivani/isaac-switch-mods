// Native tests for the paper mod menu: the complete production debug_kit.cpp,
// runtime_extras.cpp and mod_settings.cpp (this file) plus mod_menu.cpp
// (native_menu_boundary.cpp), on a fake console. Only the game's functions,
// hook installation and the menu's drawing helpers are replaced.
#include "native_test_host.hpp"
#include "../../native/source/program/debug_kit.hpp"
#include "../../native/source/program/mod_menu.hpp"
#include "../../native/source/program/runtime_extras.hpp"
#include "../../native/source/program/mod_settings.hpp"
#include "../../native/source/program/native_sprite.hpp"
#include "../../native/source/program/generated/debug_kit_guards.hpp"
#include "../../native/source/program/generated/menu_guards.hpp"
#include "../../native/source/program/generated/iteration5_guards.hpp"
#include "../../features/clean_map/generated/native.hpp"
#include <fstream>
#include <sstream>

unsigned g_Checks{};
std::unique_ptr<Host> host;
std::map<std::string, std::string> g_Files;
std::vector<std::unique_ptr<Host>> retired;  // keep old fake modules mapped: every case gets a new address
namespace mu = isaac::menu_ui;
namespace mn = isaac::menu_native;
namespace dk = isaac::debug_kit::native;
namespace cm = isaac::clean_map::native;
namespace i5 = isaac::iteration5::native;

extern "C" std::uint32_t svcQueryMemory(MemoryInfo* info, std::uint32_t* page, std::uint64_t p) {
    *page = 0;
    if (!host) return 1;
    for (const auto* b : host->blocks) if (p >= b->Address() && p - b->Address() < b->Size()) {
        info->addr = b->Address(); info->size = b->Size(); info->perm = Perm_R; return 0;
    }
    return 1;
}
// debug_kit.cpp boundaries (substituted by the runner).
bool HostDeviceExists(void* input, unsigned controller) {
    CHECK(reinterpret_cast<std::uintptr_t>(input) == host->input.Address() && controller < 34);
    return host->connected[controller];
}
bool HostPressed(void* input, unsigned button, unsigned controller, unsigned* result) {
    CHECK(reinterpret_cast<std::uintptr_t>(input) == host->input.Address() && button < 15 && controller < 34 && !result);
    return host->connected[controller] && (host->buttons[controller] & (1u << button));
}

isaac::FeatureFrame Screen(bool title = false) {
    return {host->module.Address(), title ? 0 : host->game.Address(), title ? 0 : host->room.Address(), 480, 270, {nullptr,
        [](void*, const char* text, float x, float y, isaac::FeatureColor, float) { host->draws.push_back({"canvas", {}, text, x, y}); },
        [](void*, float x, float y, float, float, isaac::FeatureColor c) { host->draws.push_back({"canvas-rect", {}, "", x, y, c.a}); }}};
}

namespace isaac {
// Drawing helpers from eid_ui.cpp / hit_counter.cpp (tested in their own suites).
void DrawEidMenuText(const FeatureFrame&, const char* text, float x, float y, std::uint32_t color, float width) {
    host->draws.push_back({"small", {color}, text ? text : "", x, y, width});
}
float EidMenuTextWidth(const char* text) { return text ? 5.f * static_cast<float>(std::strlen(text)) : 0; }
bool s_TestInk{};
void SetEidInk(bool ink) { s_TestInk = ink; }
void DrawEidMenuBody(const FeatureFrame& frame, const char* markup, float x, float y, float w, float h, std::uint64_t ms, int page) {
    CHECK(frame.module == host->module.Address() && markup && w > 40 && h > 10 && ms == host->now && s_TestInk && page >= -1);
    host->draws.push_back({"body", {}, markup, x, y, w});
}
// About 50 characters a line at the note's width.
unsigned EidMenuBodyLines(const char* markup, float width, std::uint64_t) {
    CHECK(markup && width > 40);
    return static_cast<unsigned>(std::strlen(markup) / 50 + 1);
}
void SetRepentancePlusRules(std::uint8_t rules) { host->rplusRules = rules; }
void DrawEidIcon(const FeatureFrame&, const char* token, float x, float y) { host->draws.push_back({"icon", {}, token, x, y}); }
void UpdateInstantBreak(std::uintptr_t game) { CHECK(game == host->game.Address()); ++host->instantTicks; }
void UpdateSpecialist(std::uintptr_t game) { CHECK(game == host->game.Address()); ++host->specialistTicks; }
void ResetInstantBreakRoom() { ++host->instantResets; }
void ResetSpecialist() { ++host->specialistResets; }
void UpdateGreedMachine(std::uintptr_t game) { CHECK(game == host->game.Address()); ++host->greedTicks; }
const char* TimeMachineStatus() { return "Hold toward a machine"; }
bool MachineLockActive() { return false; }
void SetTimeMachineMaxSteps(unsigned steps) { host->tmSteps = steps; }
void SetEidOptions(bool nearby, bool map) { host->eidNearby = nearby; host->eidMap = map; }
void SetHitCounterLayout(int x, int y, unsigned size, unsigned opacity, bool floor) {
    host->hitX = x; host->hitY = y; host->hitSize = size; host->hitOpacity = opacity; host->hitFloor = floor;
}
void HitCounterBox(float& w, float& h) { w = 102; h = 10; }
void HitCounterCounts(unsigned& total, unsigned& floor) { total = 3; floor = 1; }
void DrawHitCounterPreview(const FeatureFrame&, float alpha, bool outline) { host->draws.push_back({"hitcounter", {outline}, "", 0, 0, alpha}); }
void RenderTitleModMenu(std::uintptr_t module) {
    CHECK(module == host->module.Address()); ++host->titleRenders; RenderDebugKit(Screen(true));
}
// Game-owned sprites.
bool NativeSprite::Create(std::uintptr_t mod) {
    CHECK(mod == host->module.Address()); module_ = mod; created_ = true; host->sprites[this] = {}; host->sprites[this].created = true; return true;
}
bool NativeSprite::Load(const char* file) {
    CHECK(created_); ++host->spriteLoads; host->sprites[this].file = file; loaded_ = true; host->sprites[this].loaded = true; return true;
}
bool NativeSprite::SetSpritesheet(unsigned layer, const char* sheet) { CHECK(loaded_ && layer == 0); host->sprites[this].sheet = sheet; return true; }
void NativeSprite::LoadGraphics() { CHECK(loaded_); }
bool NativeSprite::SetFrame(const char* animation, int frame) {
    if (!loaded_) return false;
    auto& s = host->sprites[this];
    if (s.file == "gfx/codex_menu/menu.anm2") {
        bool known{};
        for (const char* name : {"ModsHeader", "ModsPiece", "ModsBottom", "ModsCursor", "OptionsTop", "OptionsBottom", "OptionsCursor",
                                 "Slider", "StatsTop", "StatsBlank", "Digits", "Collection", "ItemNote", "PageArrow", "Bullet", "Shadow", "Button"})
            known = known || std::strcmp(name, animation) == 0;
        CHECK(known && frame >= 0);
    } else CHECK(std::strcmp(animation, "Item") == 0 && frame == 0);
    s.animation = animation; s.frame = frame; return true;
}
void NativeSprite::SetScale(float x, float y) { CHECK(x > 0 && y > 0); host->sprites[this].sx = x; host->sprites[this].sy = y; }
void NativeSprite::SetTint(float, float, float, float a) { CHECK(a >= 0 && a <= 1); host->sprites[this].a = a; }
void NativeSprite::Render(NativeVec2 at, NativeVec2, NativeVec2) {
    CHECK(loaded_); ++host->spriteRenders;
    const auto& s = host->sprites[this];
    host->draws.push_back({"sprite", {s.frame}, s.file == "gfx/codex_menu/menu.anm2" ? s.animation : "item:" + s.sheet, at.x, at.y, s.a});
}
void NativeSprite::Destroy() { if (host) host->sprites.erase(this); created_ = loaded_ = false; module_ = 0; }
bool CopyNativeString(std::uintptr_t string, char* out, std::size_t capacity) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(string);
    if (!capacity || (bytes[0] & 1)) return false;
    const std::size_t n = bytes[0] >> 1;
    if (n >= capacity) return false;
    std::memcpy(out, bytes + 1, n); out[n] = 0; return true;
}
}
// Complete production translation units (include paths rewritten only).
#include "debug_kit_under_test.cpp"
#include "runtime_extras_under_test.cpp"
#include "mod_settings_under_test.cpp"

// ---------- helpers ----------
void Fresh(bool pauseGuards = true, bool fsGuards = true) {
    if (host) retired.push_back(std::move(host));
    host = std::make_unique<Host>();
    auto& h = *host;
    h.Guard(dk::IsButtonPressed, dk::IsButtonPressedGuard); h.Guard(dk::IsPaused, dk::IsPausedGuard);
    h.Guard(dk::Spawn, dk::SpawnGuard); h.Guard(dk::FindPosition, dk::FindPositionGuard);
    h.Guard(dk::Milliseconds, dk::MillisecondsGuard); h.Guard(dk::GetCollectible, dk::GetCollectibleGuard);
    h.Guard(cm::DoesDeviceExist, cm::DoesDeviceExistGuard); h.Guard(cm::MenuUpdate, cm::MenuUpdateGuard);
    h.Guard(cm::MenuRender, cm::MenuRenderGuard); h.Guard(cm::LoadShader, cm::LoadShaderGuard);
    h.Guard(i5::FontStringWidth, i5::FontStringWidthGuard);
#define G(name) h.Guard(mn::name, mn::name##Guard)
    G(PlaySound); G(Seed2String); G(AddCollectible); G(AddTrinket); G(AddCard); G(AddPill); G(AddCoins); G(AddBombs); G(AddKeys);
    G(AddSoulHearts); G(SetFullHearts); G(FullCharge); G(UseActiveItem); G(HasCollectible); G(ForceAddPillEffect); G(MakeShopItem);
    G(ApplyMapEffect); G(ApplyBlueMapEffect); G(ApplyCompassEffect); G(IsGreedMode); G(GetTrinket);
    if (pauseGuards) { G(PauseProcessInput); G(PauseRender); }
#undef G
#define F(name) h.Guard(mn::name##Stub, mn::name##Guard)
    if (fsGuards) { F(FsOpenFile); F(FsCloseFile); F(FsGetFileSize); F(FsReadFile); F(FsWriteFile); F(FsSetFileSize); F(FsCreateFile); F(FsIsMounted); F(FsCommitSaveData); }
#undef F
    h.module.Bytes(mn::SaveMountString, "sdmc:/", 7);
    h.module.Put(cm::InputManagerGot, h.input.Address()); h.module.Put(cm::MenuManagerGlobal, h.menuManager.Address());
    h.menuManager.Put<unsigned>(cm::MenuState, 3);
    h.pause.Put<int>(0, 1);
    isaac::ResetModSettings();
    menu_test::ForgetPreferences();
    CHECK(isaac::InitializeDebugKit(h.module.Address()));
    CHECK(h.installs == (pauseGuards ? 4u : 2u));
}
void Ordinary() { isaac::AfterNativeGameTick(host->game.Address(), true); }
bool Input() { return isaac::BeforeNativeGameInput(host->game.Address()); }
constexpr std::uint32_t Chord = mu::Bit(mu::LS) | mu::Bit(mu::RS) | mu::Bit(mu::Down);
void OpenRun(unsigned controller = 1, std::uint64_t hold = 2000) {
    host->now += 10001;   // past the shortcut's cooldown
    host->p1.Put<unsigned>(0x19ec, controller);
    host->buttons.fill(0); Ordinary();
    host->buttons[controller] = Chord; ++host->now; Ordinary();
    host->now += hold; Ordinary();
    CHECK(isaac::IsDebugMenuOpen() && menu_test::Menu().From() == mu::Origin::Shortcut);
    CHECK(isaac::BlockNativeGameTick(host->game.Address()));
    CHECK(Input()); host->buttons[controller] = 0; ++host->now; CHECK(Input());
}
void Key(unsigned controller, unsigned button) {
    host->buttons[controller] = 0; host->now += 50; Input();
    host->buttons[controller] = mu::Bit(button); host->now += 50; Input();
    host->buttons[controller] = 0; host->now += 50; Input();
}
void Settle(unsigned frames = 20) { for (unsigned i = 0; i < frames; ++i) { host->now += 16; Input(); } }
void Render() { host->draws.clear(); isaac::RenderDebugKit(Screen()); }
void TitlePass() { isaac::ModsMenuUpdate::Callback(reinterpret_cast<void*>(host->menuManager.Address())); }
void TitleRender() { host->draws.clear(); isaac::ModsMenuRender::Callback(reinterpret_cast<void*>(host->menuManager.Address())); }
void TitleKey(unsigned controller, unsigned button) {
    host->buttons[controller] = 0; host->now += 50; TitlePass();
    host->buttons[controller] = mu::Bit(button); host->now += 50; TitlePass();
    host->buttons[controller] = 0; host->now += 50; TitlePass();
}
void* Pause() { return reinterpret_cast<void*>(host->pause.Address()); }
void PauseKey(unsigned controller, unsigned button) {
    host->buttons[controller] = 0; host->now += 50; menu_test::PauseInput(Pause());
    host->buttons[controller] = mu::Bit(button); host->now += 50; menu_test::PauseInput(Pause());
    host->buttons[controller] = 0; host->now += 50; menu_test::PauseInput(Pause());
}
void CloseShortcutMenu(unsigned controller = 1) {
    Key(controller, mu::B);
    for (unsigned i = 0; i < 30 && isaac::IsDebugMenuOpen(); ++i) { host->now += 16; Input(); }
    CHECK(!isaac::IsDebugMenuOpen());
}
std::string Saved() { const auto f = g_Files.find("sdmc:/isaac_mods_menu.cfg"); return f == g_Files.end() ? "" : f->second; }

// Preview evidence: what the real backend draws for a few screens.
std::vector<std::pair<std::string, std::vector<Call>>> previews;
void Capture(const char* name) { previews.push_back({name, host->draws}); }
void SavePreview() {
    std::ofstream out("evidence/menu/native-draws.json");
    out << "{\"width\":480,\"height\":270,\"source\":\"production mod_menu.cpp draw calls on the fake console\",\"screens\":[";
    for (std::size_t s = 0; s < previews.size(); ++s) {
        out << (s ? "," : "") << "{\"name\":\"" << previews[s].first << "\",\"draws\":[";
        for (std::size_t i = 0; i < previews[s].second.size(); ++i) {
            const auto& d = previews[s].second[i];
            std::string text;
            for (char c : d.text) { if (c == '"' || c == '\\') text += '\\'; if (static_cast<unsigned char>(c) >= 32) text += c; }
            out << (i ? "," : "") << "{\"kind\":\"" << d.name << "\",\"text\":\"" << text << "\",\"x\":" << d.x << ",\"y\":" << d.y
                << ",\"value\":" << d.f << "}";
        }
        out << "]}";
    }
    out << "]}\n";
}

int main() {
    // ---------- shortcut hold opens the menu without touching the game ----------
    Fresh();
    CHECK(isaac::ModMenuShortcutMilliseconds() == 2000);
    CHECK(host->tmSteps == 5 && host->hitSize == 100 && host->hitX == -1 && host->eidNearby && host->eidMap);  // defaults pushed at start
    OpenRun(1);
    CHECK(menu_test::Menu().CurrentTab() == mu::Tab::Items && host->Count("PlaySound") == 1 && host->Last("PlaySound")->args[0] == 17);
    CHECK(host->Count("Spawn") == 0);
    Render();
    CHECK(host->Drawn("sprite") > 10 && host->Drawn("rect") >= 1 && host->Drawn("text") > 3 && host->shaders == 0);
    CHECK(host->spriteLoads >= 1);
    // Item pictures load a few per frame, never all at once.
    unsigned itemSprites{};
    for (const auto& [_, s] : host->sprites) itemSprites += s.file == "gfx/codex_eid_ui/item_icon.anm2";
    CHECK(itemSprites >= 1 && itemSprites <= 4);
    for (unsigned i = 0; i < 12; ++i) { host->now += 16; Render(); }
    itemSprites = 0;
    for (const auto& [_, s] : host->sprites) itemSprites += s.file == "gfx/codex_eid_ui/item_icon.anm2";
    CHECK(itemSprites >= 24 && itemSprites <= 48);
    for (const auto& [_, s] : host->sprites) if (s.file == "gfx/codex_eid_ui/item_icon.anm2") CHECK(s.sheet == "gfx/items/c001.png");
    CHECK(host->Drawn("body") == 1);   // EID description of the selected item, in ink
    Capture("items");

    // A: popup; A again: place on a pedestal near Isaac.
    const unsigned first = menu_test::Menu().VisibleAt(0)->entry.id;
    Key(1, mu::A); CHECK(menu_test::Menu().PopupOpen());
    Render(); Capture("items-popup");
    Key(1, mu::A);
    CHECK(host->Count("Spawn") == 1 && host->Last("Spawn")->args[0] == 100 && host->Last("Spawn")->args[1] == first);
    CHECK(host->Last("Spawn")->args[2] == 280 && host->Last("Spawn")->args[3] == 220);   // first free spot: above Isaac
    CHECK(std::string(menu_test::Menu().ToastText()).rfind("Placed", 0) == 0 && menu_test::Dirty());
    CHECK(menu_test::Prefs().recentCount == 1 && menu_test::Prefs().recents[0].id == first);
    // Give: AddCollectible with the item's full charge.
    Key(1, mu::A); Key(1, mu::Down); Key(1, mu::A);
    CHECK(host->Count("AddCollectible") == 1 && host->Last("AddCollectible")->args[0] == first && host->Last("AddCollectible")->args[1] == 6);
    // Shop: pedestal, then priced by the room.
    Key(1, mu::A); Key(1, mu::Down); Key(1, mu::Down); Key(1, mu::A);
    CHECK(host->Count("Spawn") == 2 && host->Count("MakeShopItem") == 1 && host->Last("MakeShopItem")->args[0] == -1);
    // No free space: nothing is spawned and the menu says so.
    host->noSpace = true; Key(1, mu::A); Key(1, mu::A);
    CHECK(host->Count("Spawn") == 2 && host->Count("FindPosition") >= 10 && std::string(menu_test::Menu().ToastText()).find("No free space") == 0);
    host->noSpace = false;
    // Y favourites the selection.
    Key(1, mu::Y); CHECK(menu_test::Prefs().favoriteCount == 1 && menu_test::Prefs().favorites[0].id == first);

    // ---------- the host interface directly: every item category ----------
    auto& h = menu_test::MenuHost();
    using E = mu::Entry;
    const auto C = [](mu::Category c) { return static_cast<std::uint8_t>(c); };
    CHECK(h.Act(mu::Action::Give, E{C(mu::Category::Collectible), 550, 0}) == mu::Result::Unavailable);   // story items never given
    CHECK(h.Act(mu::Action::Give, E{C(mu::Category::Collectible), 668, 0}) == mu::Result::Unavailable);
    CHECK(h.Act(mu::Action::Drop, E{C(mu::Category::Collectible), 668, 0}) == mu::Result::Done);          // but can be placed
    CHECK(h.Act(mu::Action::Give, E{C(mu::Category::Trinket), 2, 0}) == mu::Result::Done && host->Last("AddTrinket")->args[0] == 2);
    CHECK(h.Act(mu::Action::Drop, E{C(mu::Category::Trinket), 2, 0}) == mu::Result::Done && host->Last("Spawn")->args[0] == 350);
    CHECK(h.Act(mu::Action::Give, E{C(mu::Category::Card), 7, 0}) == mu::Result::Done && host->Last("AddCard")->args[0] == 7);
    CHECK(h.Act(mu::Action::Drop, E{C(mu::Category::Card), 7, 0}) == mu::Result::Done && host->Last("Spawn")->args[0] == 300);
    CHECK(h.Act(mu::Action::Give, E{C(mu::Category::Pill), 14, 0}) == mu::Result::Done);
    CHECK(host->Last("ForceAddPillEffect")->args[0] == 14 && host->Last("AddPill")->args[0] == 5);
    CHECK(h.Act(mu::Action::Drop, E{C(mu::Category::Pill), 14, 0}) == mu::Result::Done && host->Last("Spawn")->args[0] == 70 && host->Last("Spawn")->args[1] == 5);
    host->pillColor = 0; CHECK(h.Act(mu::Action::Give, E{C(mu::Category::Pill), 14, 0}) == mu::Result::Failed); host->pillColor = 5;
    CHECK(h.Act(mu::Action::Drop, E{C(mu::Category::Pickup), 10, 3}) == mu::Result::Done && host->Last("Spawn")->args[0] == 10 && host->Last("Spawn")->args[1] == 3);
    host->collectible = false;
    CHECK(h.Act(mu::Action::Give, E{C(mu::Category::Collectible), 1, 0}) == mu::Result::Unavailable); host->collectible = true;
    host->spawnFails = true; CHECK(h.Act(mu::Action::Drop, E{C(mu::Category::Card), 7, 0}) == mu::Result::Failed); host->spawnFails = false;
    host->game.Put<float>(0x26fa90, 1);   // room transition
    CHECK(h.Act(mu::Action::Give, E{C(mu::Category::Card), 7, 0}) == mu::Result::NotNow && h.UseTool(mu::Tool::Coins) == mu::Result::NotNow);
    host->game.Put<float>(0x26fa90, 0);
    // Tools.
    const auto calls = host->calls.size();
    CHECK(h.UseTool(mu::Tool::FullHealth) == mu::Result::Done && host->Count("SetFullHearts") == 1);
    CHECK(h.UseTool(mu::Tool::SoulHeart) == mu::Result::Done && host->Last("AddSoulHearts")->args[0] == 2);
    CHECK(h.UseTool(mu::Tool::Coins) == mu::Result::Done && host->Last("AddCoins")->args[0] == 10);
    CHECK(h.UseTool(mu::Tool::Bombs) == mu::Result::Done && host->Last("AddBombs")->args[0] == 5);
    CHECK(h.UseTool(mu::Tool::Keys) == mu::Result::Done && host->Last("AddKeys")->args[0] == 5);
    CHECK(h.UseTool(mu::Tool::Charge) == mu::Result::Done);
    host->chargeFails = true; CHECK(h.UseTool(mu::Tool::Charge) == mu::Result::Failed); host->chargeFails = false;
    CHECK(h.UseTool(mu::Tool::RevealMap) == mu::Result::Done && host->Count("ApplyMapEffect") == 1 && host->Count("ApplyBlueMapEffect") == 1 &&
          host->Count("ApplyCompassEffect") == 1);
    CHECK(h.UseTool(mu::Tool::Reroll) == mu::Result::Done);
    const auto* d6 = host->Last("UseActiveItem");
    CHECK(d6->args[0] == 105 && d6->args[1] == 1 && d6->args[2] == -1 && d6->args[3] == 0);
    CHECK(host->calls.size() > calls);
    // Run page facts.
    host->p1.Put<int>(0x1738, 4); host->p1.Put<float>(0x194c, 1.2f); host->p1.Put<float>(0x1834, 9.f);
    host->p1.Put<float>(0x1844, 4.5f); host->p1.Put<float>(0x1854, 260.f); host->p1.Put<float>(0x1838, 1.1f); host->p1.Put<float>(0x1950, 2.f);
    host->history.Put<std::uint8_t>(4, 0); host->history.Put<unsigned>(8, 118);
    host->history.Put<std::uint8_t>(0x1c + 4, 1); host->history.Put<unsigned>(0x1c + 8, 19);
    host->history.Put<std::uint8_t>(0x38 + 4, 0); host->history.Put<unsigned>(0x38 + 8, 9999);   // out of range: skipped
    host->p1.Put<std::uintptr_t>(0x24d0, host->history.Address()); host->p1.Put<std::uintptr_t>(0x24d8, host->history.Address() + 3 * 0x1c);
    mu::RunInfo run;
    CHECK(h.Run(run) && run.valid && std::string(run.seed) == "ABCD EFGH" && run.character == 4 && run.stage == 3 && run.stageType == 1);
    CHECK(run.hits == 3 && run.floorHits == 1 && run.speed == 1.2f && run.tears == 3.f && run.damage == 4.5f && run.range == 6.5f);
    CHECK(run.historyCount == 2 && run.history[0] == (19 | 0x8000) && run.history[1] == 118);
    host->p1.Put<std::uintptr_t>(0x24d8, host->history.Address() + 3 * 0x1c + 5);   // damaged vector: ignored
    CHECK(h.Run(run) && run.historyCount == 0);
    CHECK(h.Owned(1) && !h.Owned(2));
    CHECK(h.Describe(E{C(mu::Category::Collectible), 1, 0}) != nullptr && h.Describe(E{C(mu::Category::Pill), 0, 0}) != nullptr);
    CHECK(h.Describe(E{C(mu::Category::Collectible), 9999, 0}) == nullptr);

    // ---------- tabs, rendering and closing ----------
    Key(1, mu::R); CHECK(menu_test::Menu().CurrentTab() == mu::Tab::Mods);
    Settle(); Render(); Capture("mods");
    CHECK(host->DrewText("External Item Descriptions") || host->DrewText("External Item"));
    Key(1, mu::A); CHECK(!isaac::IsModEnabled(isaac::ModFeature::Eid));
    Key(1, mu::R); Settle(); Render(); Capture("run");
    CHECK(host->DrewText("ABCD EFGH"));
    Key(1, mu::R); Settle(); Render(); Capture("tools");
    Key(1, mu::A); CHECK(host->Count("SetFullHearts") == 2);
    Key(1, mu::R); Settle(); Render(); Capture("settings");
    CHECK(menu_test::Menu().CurrentTab() == mu::Tab::Settings);
    // Settings tab: Shortcut hold row (third) -> 0.5 s, pushed to the shortcut right away.
    Key(1, mu::Down); Key(1, mu::Down); Key(1, mu::Right);
    CHECK(menu_test::Prefs().shortcut == 0 && isaac::ModMenuShortcutMilliseconds() == 0);
    Key(1, mu::Right); CHECK(isaac::ModMenuShortcutMilliseconds() == 500);
    CloseShortcutMenu(1);
    // The closing B is swallowed until released, then the game gets input back.
    host->buttons[1] = mu::Bit(mu::B); CHECK(Input()); CHECK(isaac::BlockNativeGameTick(host->game.Address()));
    host->buttons[1] = 0; CHECK(Input()); CHECK(!Input()); CHECK(!isaac::BlockNativeGameTick(host->game.Address()));
    // Settings saved once fully closed, into the game's save data, then committed.
    CHECK(host->fsCreates == 1 && host->fsWrites == 1 && host->fsCommits == 1 && !menu_test::Dirty());
    const auto saved = Saved();
    CHECK(saved.rfind("isaacmods 2\n", 0) == 0 && saved.find("menu=1,0,1,1,0,60,4") != std::string::npos &&
          saved.find("rplus=15") != std::string::npos);
    mu::Prefs check;
    CHECK(mu::Parse(saved.data(), static_cast<unsigned>(saved.size()), check) && check.hasMods && !(check.mods & 1) && check.favoriteCount == 1);
    // Nothing changed: closing again writes nothing.
    OpenRun(1, 500); CloseShortcutMenu(1); host->buttons[1] = 0; Input(); Input();
    CHECK(host->fsWrites == 1);

    // ---------- saved settings are loaded once the save data is mounted ----------
    Fresh(); host->mounted = false;
    menu_test::ForgetPreferences();
    isaac::PollModMenuPreferences(); CHECK(host->fsReads == 0 && isaac::IsModEnabled(isaac::ModFeature::Eid));
    host->mounted = true; isaac::PollModMenuPreferences();
    CHECK(host->fsReads == 1 && !isaac::IsModEnabled(isaac::ModFeature::Eid) && isaac::ModMenuShortcutMilliseconds() == 500);
    CHECK(menu_test::Prefs().favoriteCount == 1 && menu_test::Prefs().lastTab == 4);
    isaac::PollModMenuPreferences(); Input(); CHECK(host->fsReads == 1);   // once per game start
    // Layout reaches the features.
    g_Files["sdmc:/isaac_mods_menu.cfg"] = "isaacmods 1\nhit=12,200,150,60,0\ntimemachine=2\neid=0,1\n";
    Fresh(); isaac::PollModMenuPreferences();
    CHECK(host->hitX == 12 && host->hitY == 200 && host->hitSize == 150 && host->hitOpacity == 60 && !host->hitFloor);
    CHECK(host->tmSteps == 2 && !host->eidNearby && host->eidMap && isaac::IsModEnabled(isaac::ModFeature::Eid));
    // A damaged file keeps the defaults; it is replaced on the next save.
    g_Files["sdmc:/isaac_mods_menu.cfg"] = std::string("\x01\x02garbage", 9);
    Fresh(); isaac::PollModMenuPreferences();
    CHECK(host->fsReads == 1 && host->hitSize == 100 && host->tmSteps == 5 && isaac::ModMenuShortcutMilliseconds() == 2000);
    // A failed write keeps the change pending for the next close.
    Fresh(); g_Files.clear(); isaac::PollModMenuPreferences();
    OpenRun(1); Key(1, mu::Y); host->fsWriteFails = true; CloseShortcutMenu(1);
    CHECK(host->fsCommits == 0 && menu_test::Dirty());
    host->buttons[1] = 0; Input(); Input(); host->fsWriteFails = false;
    OpenRun(1); CloseShortcutMenu(1); CHECK(host->fsCommits == 1 && !menu_test::Dirty());
    // Without the file-system guards the menu still works but never touches save data.
    g_Files.clear(); Fresh(true, false); isaac::PollModMenuPreferences();
    OpenRun(1); Key(1, mu::Y); CloseShortcutMenu(1);
    CHECK(host->fsReads == 0 && host->fsWrites == 0 && host->Count("OpenFile") == 0 && g_Files.empty());

    // ---------- shortcut setting ----------
    Fresh(); menu_test::Prefs().shortcut = 0;
    host->buttons[1] = Chord; Ordinary(); host->now += 5000; Ordinary(); CHECK(!isaac::IsDebugMenuOpen());
    menu_test::Prefs().shortcut = 1;   // turned on while still held: needs a release first
    host->now += 3000; Ordinary(); CHECK(!isaac::IsDebugMenuOpen());
    host->buttons[1] = 0; Ordinary();
    host->buttons[1] = Chord; Ordinary(); host->now += 499; Ordinary(); CHECK(!isaac::IsDebugMenuOpen());
    host->now += 2; Ordinary(); CHECK(isaac::IsDebugMenuOpen());

    // ---------- the run changes under the menu: it closes instantly ----------
    for (unsigned mutation = 0; mutation < 4; ++mutation) {
        Fresh(); OpenRun();
        if (mutation == 0) host->room.Put<unsigned>(0x191c, 50);
        if (mutation == 1) host->p1.Put<std::uint8_t>(0x1c3, 1);
        if (mutation == 2) host->game.Put<unsigned>(0x25d44, 999);
        if (mutation == 3) host->connected[1] = false;
        CHECK(!Input() && !isaac::IsDebugMenuOpen() && !isaac::IsModMenuOpen());
        CHECK(!isaac::BlockNativeGameTick(host->game.Address()));
    }
    Fresh(); OpenRun(); isaac::CancelNativeGameTick(); CHECK(!isaac::IsModMenuOpen());

    // ---------- pause screen: Y opens the menu over the paused game ----------
    Fresh();
    host->buttons[1] = mu::Bit(mu::Y); menu_test::PauseInput(Pause());
    CHECK(!isaac::IsModMenuOpen() && host->pauseInputs == 1);        // Y already held when paused: ignored
    host->buttons[1] = 0; menu_test::PauseInput(Pause()); CHECK(host->pauseInputs == 2);
    host->draws.clear(); menu_test::PauseRender(Pause());
    CHECK(host->pauseRenders == 1 && host->shaders == 1 && host->DrewText("Mods"));   // "Y Mods" prompt
    bool glyph{}; for (const auto& d : host->draws) glyph = glyph || (d.name == "sprite" && d.text == "Button" && d.args[0] == 3);
    CHECK(glyph);
    host->buttons[1] = mu::Bit(mu::Y); menu_test::PauseInput(Pause());
    CHECK(isaac::IsModMenuOpenFrom(isaac::MenuOrigin::Pause) && !isaac::IsDebugMenuOpen() && host->pauseInputs == 2);
    host->draws.clear(); menu_test::PauseRender(Pause());
    CHECK(host->pauseRenders == 1 && host->Drawn("sprite") > 10); Capture("pause");
    host->buttons[1] = 0; menu_test::PauseInput(Pause());
    PauseKey(1, mu::R); CHECK(menu_test::Menu().CurrentTab() == mu::Tab::Mods && host->pauseInputs == 2);
    PauseKey(1, mu::B);
    host->buttons[1] = mu::Bit(mu::B);
    for (unsigned i = 0; i < 30; ++i) { host->now += 16; menu_test::PauseInput(Pause()); }
    CHECK(!isaac::IsModMenuOpen() && host->pauseInputs == 2);     // closing B never reaches the pause screen
    host->buttons[1] = 0; menu_test::PauseInput(Pause()); menu_test::PauseInput(Pause());
    CHECK(host->pauseInputs == 4);
    // Only on the main pause page, and the prompt can be hidden.
    host->pause.Put<int>(0, 2); host->buttons[1] = mu::Bit(mu::Y); menu_test::PauseInput(Pause());
    CHECK(!isaac::IsModMenuOpen() && host->pauseInputs == 5);
    host->pause.Put<int>(0, 1); host->buttons[1] = 0; menu_test::PauseInput(Pause());
    menu_test::Prefs().pausePrompt = false; host->draws.clear(); menu_test::PauseRender(Pause()); CHECK(!host->DrewText("Mods"));
    // The pause hooks failing their guards removes only the pause entry.
    Fresh(false);
    host->buttons[1] = mu::Bit(mu::Y); menu_test::PauseInput(Pause()); host->buttons[1] = 0; menu_test::PauseInput(Pause());
    host->buttons[1] = mu::Bit(mu::Y); menu_test::PauseInput(Pause());
    CHECK(!isaac::IsModMenuOpen() && host->pauseInputs == 3);
    OpenRun(); CHECK(isaac::IsDebugMenuOpen());

    // ---------- title screen Mods menu: X opens it, items need a run ----------
    Fresh(); host->manager.Put<int>(8, 1);
    host->buttons[33] = mu::Bit(mu::X); TitlePass(); CHECK(!isaac::IsModMenuOpen());   // held X does not open
    TitleKey(33, mu::X); CHECK(isaac::IsModMenuOpenFrom(isaac::MenuOrigin::Title) && menu_test::Menu().CurrentTab() == mu::Tab::Mods);
    const auto nativeUpdates = host->nativeMenuUpdates;
    TitleKey(33, mu::A); CHECK(!isaac::IsModEnabled(isaac::ModFeature::Eid));
    host->buttons[1] = mu::Bit(mu::A); TitlePass(); host->buttons[1] = 0; TitlePass();
    CHECK(!isaac::IsModEnabled(isaac::ModFeature::Eid));            // another controller cannot drive it
    TitleRender(); CHECK(host->nativeMenuRenders == 1 && host->titleRenders == 1 && host->Drawn("sprite") > 10); Capture("title");
    TitleKey(33, mu::L); CHECK(menu_test::Menu().CurrentTab() == mu::Tab::Settings);   // Items/Run/Tools skipped on the title
    TitleKey(33, mu::R); CHECK(menu_test::Menu().CurrentTab() == mu::Tab::Mods);
    CHECK(host->nativeMenuUpdates == nativeUpdates);
    TitleKey(33, mu::B);
    host->buttons[33] = mu::Bit(mu::B);
    for (unsigned i = 0; i < 30; ++i) { host->now += 16; TitlePass(); }
    CHECK(!isaac::IsModMenuOpen() && host->nativeMenuUpdates == nativeUpdates);
    host->buttons[33] = 0; TitlePass(); CHECK(host->nativeMenuUpdates == nativeUpdates + 1);   // first neutral pass returns
    TitleRender(); CHECK(host->draws.size() == 1 && host->draws[0].text == "X: Mods");
    // Leaving the Mods screen closes the menu immediately.
    TitleKey(1, mu::X); CHECK(isaac::IsModMenuOpen());
    host->menuManager.Put<unsigned>(cm::MenuState, 2); TitlePass(); CHECK(!isaac::IsModMenuOpen());
    // Every controller can own it.
    for (unsigned controller : {1, 2, 3, 4, 5, 6, 7, 8, 33}) {
        Fresh(); host->manager.Put<int>(8, 1); TitleKey(controller, mu::X); CHECK(isaac::IsModMenuOpenFrom(isaac::MenuOrigin::Title));
        Fresh(); OpenRun(controller); CHECK(isaac::IsDebugMenuOpen());
    }

    // ---------- every native guard is checked before anything is installed ----------
    std::vector<std::uintptr_t> guarded{dk::IsButtonPressed, dk::IsPaused, dk::Milliseconds, cm::DoesDeviceExist, dk::Spawn, dk::FindPosition,
        dk::GetCollectible, cm::LoadShader, i5::FontStringWidth};
    for (auto o : {mn::PlaySound, mn::Seed2String, mn::AddCollectible, mn::AddTrinket, mn::AddCard, mn::AddPill, mn::AddCoins, mn::AddBombs,
                   mn::AddKeys, mn::AddSoulHearts, mn::SetFullHearts, mn::FullCharge, mn::UseActiveItem, mn::HasCollectible,
                   mn::ForceAddPillEffect, mn::MakeShopItem, mn::ApplyMapEffect, mn::ApplyBlueMapEffect, mn::ApplyCompassEffect,
                   mn::IsGreedMode, mn::GetTrinket}) guarded.push_back(o);
    for (auto offset : guarded) {
        Fresh();
        retired.push_back(std::move(host)); host = std::make_unique<Host>();
        // Copy every guard from the previous (good) module, then break one.
        std::memcpy(reinterpret_cast<void*>(host->module.Address()), reinterpret_cast<const void*>(retired.back()->module.Address()), host->module.Size());
        host->module.Put(isaac::profile::GameGlobal, host->game.Address()); host->module.Put(isaac::profile::ManagerGlobal, host->manager.Address());
        host->module.Put(cm::InputManagerGot, host->input.Address()); host->module.Put(cm::MenuManagerGlobal, host->menuManager.Address());
        host->menuManager.Put<unsigned>(cm::MenuState, 3);
        host->module.Put<std::uint8_t>(offset, host->module.Get<std::uint8_t>(offset) ^ 0xff);
        // The good module's hooked bytes were overwritten by the fake installer: restore the originals.
        host->Guard(mn::PauseProcessInput, mn::PauseProcessInputGuard); host->Guard(mn::PauseRender, mn::PauseRenderGuard);
        host->Guard(cm::MenuUpdate, cm::MenuUpdateGuard); host->Guard(cm::MenuRender, cm::MenuRenderGuard);
        CHECK(!isaac::InitializeDebugKit(host->module.Address()) && host->installs == 0);
        host->buttons[1] = Chord; Ordinary(); host->now += 3000; Ordinary(); CHECK(!isaac::IsModMenuOpen());
        host->manager.Put<int>(8, 1); TitleKey(1, mu::X); CHECK(!isaac::IsModMenuOpen());
    }

    // ---------- module unload releases every game-owned sprite ----------
    Fresh(); OpenRun(); for (unsigned i = 0; i < 10; ++i) { host->now += 16; Render(); }
    CHECK(!host->sprites.empty());
    isaac::ResetDebugKit(); CHECK(host->sprites.empty() && !isaac::IsModMenuOpen());
    SavePreview();
    std::printf("Mod menu native: %u checks PASS (shortcut, pause, title, actions, tools, run info, saving, guards)\n", g_Checks);
}

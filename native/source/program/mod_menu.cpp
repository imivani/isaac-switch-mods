#include "mod_menu.hpp"
#include "mod_settings.hpp"
#include "hit_counter.hpp"
#include "timemachine.hpp"
#include "eid_features.hpp"
#include "eid_ui.hpp"
#include "native_sprite.hpp"
#include "generated/build_profile.hpp"
#include "generated/menu_guards.hpp"
#include "generated/debug_kit_guards.hpp"
#include "generated/iteration5_guards.hpp"
#include "../../../features/clean_map/generated/native.hpp"
#include "../../../features/item_menu/adapter.hpp"
#include "../../../features/menu_ui/menu.hpp"
#include "lib.hpp"
extern "C" {
#include "lib/nx/kernel/svc.h"
}
#include <cstdio>
#include <cstring>

namespace isaac {
namespace {
using feature_memory::Readable;
using feature_memory::TryRead;
namespace mu = menu_ui;
namespace mn = menu_native;
namespace dk = debug_kit::native;
namespace cm = clean_map::native;
namespace i5 = iteration5::native;

struct ColorRaw { float r, g, b, a; };
struct NativeString { std::uint8_t bytes[24]; };
struct FileHandle { void* handle; };
struct WriteOption { int flags; };
struct Vec2 { float x, y; };

std::uintptr_t s_Module{};
bool s_Ready{}, s_PauseReady{}, s_FsReady{}, s_PrefsLoaded{}, s_Dirty{}, s_PauseGate{}, s_PausePreviousY{true};
std::uint8_t s_PauseInputGuard[20]{}, s_PauseRenderGuard[20]{};
std::uintptr_t s_PauseModule{}, s_Player{};
unsigned s_Controller{};
mu::Origin s_Origin{};
constinit mu::Menu s_Menu;
mu::PaintList s_Paints;
constinit mu::Prefs s_Prefs;
constinit NativeSprite s_Art;
bool s_ArtTried{}, s_ArtReady{};
constexpr unsigned IconSlots = 48, IconLoadsPerFrame = 4;
constinit NativeSprite s_Icons[IconSlots];
std::uint32_t s_IconKeys[IconSlots]{};
std::uint64_t s_IconUsed[IconSlots]{};
bool s_IconReady[IconSlots]{};
unsigned s_IconLoads{};
std::uint64_t s_Frame{};

template<class T> T Read(std::uintptr_t p, std::uintptr_t offset = 0) { return feature_memory::Read<T>(p, offset); }
template<std::size_t N> bool Matches(std::uintptr_t p, const std::uint8_t (&bytes)[N]) {
    return Readable(p, N) && std::memcmp(reinterpret_cast<const void*>(p), bytes, N) == 0;
}
template<class F> F Native(std::uintptr_t offset) { return reinterpret_cast<F>(s_Module + offset); }
// The game's own call stub for an SDK file function (byte-checked by FS_GUARD).
// Never call through the import slot directly: the game binds imports lazily,
// so an unused slot points at nn::ro's binder, which needs the stub's x16/x17
// setup (calling the slot crashed in nn::ro::BindEntry at the main menu).
template<class F> F Fs(std::uintptr_t stub) { return s_FsReady ? reinterpret_cast<F>(s_Module + stub) : nullptr; }
std::uint64_t Now() { return Native<std::uint64_t (*)()>(dk::Milliseconds)(); }
std::uintptr_t Game() { std::uintptr_t g{}; TryRead(s_Module + profile::GameGlobal, g); return g; }
std::uintptr_t Manager() { std::uintptr_t m{}; TryRead(s_Module + profile::ManagerGlobal, m); return m; }
bool Pointer(std::uintptr_t p) { return p > 0x10000 && !(p & 7); }

struct Engine {
    template<class T> T Read(std::uintptr_t p, std::uintptr_t offset = 0) { return feature_memory::Read<T>(p, offset); }
    bool Readable(std::uintptr_t p, std::size_t size) { return feature_memory::Readable(p, size); }
};
bool PlayerAlive(std::uintptr_t player) {
    return Pointer(player) && Readable(player, 0x2850) && Read<unsigned>(player, profile::EntityType) == 1 &&
        Read<std::uint8_t>(player, profile::EntityExists) && !Read<std::uint8_t>(player, profile::EntityDead);
}
std::uintptr_t PlayerOne(std::uintptr_t game) {
    const auto begin = Read<std::uintptr_t>(game, profile::PlayersBegin), end = Read<std::uintptr_t>(game, profile::PlayersEnd);
    if (end <= begin || end - begin > 8 * sizeof(std::uintptr_t) || !Readable(begin, end - begin)) return 0;
    for (auto p = begin; p < end; p += sizeof(std::uintptr_t)) {
        const auto player = Read<std::uintptr_t>(p);
        if (s_Player && player == s_Player && PlayerAlive(player)) return player;
    }
    const auto first = Read<std::uintptr_t>(begin);
    return PlayerAlive(first) ? first : 0;
}
bool InGame(std::uintptr_t& game, std::uintptr_t& player) {
    game = Game();
    const auto manager = Manager();
    if (!s_Ready || !Pointer(game) || !Readable(game, 0x26faa0) || !Readable(manager, 12) || Read<int>(manager, 8) != 2) return false;
    player = PlayerOne(game);
    return player != 0;
}
bool Transitioning(std::uintptr_t game) { return Read<float>(game, 0x26fa90) > 0 || Read<int>(game, 0x26fa9c) != 0; }

// ---------- saved settings ----------
void ApplyPrefs() {
    SetHitCounterLayout(s_Prefs.hitX, s_Prefs.hitY, s_Prefs.hitSize, s_Prefs.hitOpacity, s_Prefs.hitFloor);
    SetTimeMachineMaxSteps(s_Prefs.timeMachineSteps);
    SetEidOptions(s_Prefs.eidNearby, s_Prefs.eidMap);
}
bool Mounted() {
    auto mounted = Fs<bool (*)(const char*)>(mn::FsIsMountedStub);
    return s_FsReady && mounted && mounted(mn::SaveMount);
}
void LoadPrefs() {
    s_PrefsLoaded = true;
    auto open = Fs<std::uint32_t (*)(FileHandle*, const char*, int)>(mn::FsOpenFileStub);
    auto size = Fs<std::uint32_t (*)(std::int64_t*, FileHandle)>(mn::FsGetFileSizeStub);
    auto read = Fs<std::uint32_t (*)(std::size_t*, FileHandle, std::int64_t, void*, std::size_t)>(mn::FsReadFileStub);
    auto close = Fs<void (*)(FileHandle)>(mn::FsCloseFileStub);
    if (!open || !size || !read || !close) return;
    FileHandle file{};
    if (open(&file, mn::SaveFile, 1) != 0) return;  // missing: keep defaults
    static char buffer[4096];
    std::int64_t length{};
    std::size_t got{};
    if (size(&length, file) == 0 && length > 0 && length < static_cast<std::int64_t>(sizeof(buffer)) &&
        read(&got, file, 0, buffer, static_cast<std::size_t>(length)) == 0) {
        mu::Prefs loaded;
        if (mu::Parse(buffer, static_cast<unsigned>(got), loaded)) {
            s_Prefs = loaded;
            if (s_Prefs.hasMods) SetModEnabledMask(s_Prefs.mods);
        }
    }
    close(file);
}
void SavePrefs() {
    if (!s_Dirty || !Mounted()) return;
    auto open = Fs<std::uint32_t (*)(FileHandle*, const char*, int)>(mn::FsOpenFileStub);
    auto create = Fs<std::uint32_t (*)(const char*, std::int64_t)>(mn::FsCreateFileStub);
    auto resize = Fs<std::uint32_t (*)(FileHandle, std::int64_t)>(mn::FsSetFileSizeStub);
    auto write = Fs<std::uint32_t (*)(FileHandle, std::int64_t, const void*, std::size_t, const WriteOption*)>(mn::FsWriteFileStub);
    auto close = Fs<void (*)(FileHandle)>(mn::FsCloseFileStub);
    auto commit = Fs<std::uint32_t (*)(const char*)>(mn::FsCommitSaveDataStub);
    if (!open || !create || !resize || !write || !close || !commit) return;
    static char buffer[4096];
    s_Prefs.mods = ModEnabledMask(); s_Prefs.hasMods = true;
    const unsigned length = mu::Serialize(s_Prefs, buffer, sizeof(buffer));
    if (!length) return;
    FileHandle file{};
    // Write | AllowAppend. A separate small file beside the game's own saves.
    if (open(&file, mn::SaveFile, 6) != 0) {
        if (create(mn::SaveFile, 0) != 0 || open(&file, mn::SaveFile, 6) != 0) return;
    }
    const WriteOption flush{1};
    const bool written = resize(file, length) == 0 && write(file, 0, buffer, length, &flush) == 0;
    close(file);
    if (written && commit(mn::SaveMount) == 0) s_Dirty = false;
}

// ---------- drawing ----------
struct Canvas {
    void* font;
    void Text(const char* text, float x, float y, FeatureColor c, float scale) const {
        if (!font || !Read<std::uint8_t>(reinterpret_cast<std::uintptr_t>(font))) return;
        const ColorRaw color{c.r, c.g, c.b, c.a};
        Native<void (*)(void*, const char*, float, float, float, float, const ColorRaw*, unsigned, bool)>(profile::DrawStringScaled)(
            font, text, x, y, scale, scale, &color, 0, false);
    }
};
void Rect(float x1, float y1, float x2, float y2, std::uint32_t rgb, float alpha) {
    const ColorRaw color{((rgb >> 16) & 255) / 255.f, ((rgb >> 8) & 255) / 255.f, (rgb & 255) / 255.f, alpha};
    Native<void (*)(void*, float, float, float, float, const ColorRaw*)>(profile::FillQuad)(
        reinterpret_cast<void*>(s_Module + profile::ShapeRenderer), x1, y1, x2, y2, &color);
}
const void* FontFor(mu::Font font) {
    const auto manager = Manager();
    const std::uintptr_t offset = font == mu::Font::Ink ? 0x117120 : font == mu::Font::InkLarge ? 0x137170 : profile::ManagerFont;
    if (!Pointer(manager) || !Readable(manager + offset, 0x100) || !Read<std::uint8_t>(manager + offset)) return nullptr;
    return reinterpret_cast<const void*>(manager + offset);
}
float NativeWidth(const void* font, const char* text, float scale) {
    return static_cast<float>(Native<int (*)(const void*, const char*)>(i5::FontStringWidth)(font, text)) * scale;
}
void DrawText(const FeatureFrame& frame, const mu::Paint& p) {
    char text[160];
    std::snprintf(text, sizeof(text), "%s", p.text ? p.text : "");
    const auto color = FeatureColor{((p.color >> 16) & 255) / 255.f, ((p.color >> 8) & 255) / 255.f, (p.color & 255) / 255.f, p.alpha};
    if (p.font == mu::Font::Small) {
        float width = EidMenuTextWidth(text);
        const float x = p.x - (p.align == mu::Align::Center ? width * .5f : p.align == mu::Align::Right ? width : 0);
        DrawEidMenuText(frame, text, x, p.y, p.color, p.w > 0 ? p.w : 10000.f);
        return;
    }
    const void* font = FontFor(p.font);
    if (!font) return;
    float width = NativeWidth(font, text, p.scale);
    if (p.w > 0 && width > p.w) {
        // Shorten with an ellipsis to fit the column.
        std::size_t length = std::strlen(text);
        while (length > 0) {
            text[--length] = 0;
            char trial[168];
            std::snprintf(trial, sizeof(trial), "%s...", text);
            if (NativeWidth(font, trial, p.scale) <= p.w) { std::memcpy(text, trial, length + 4); break; }
        }
        width = NativeWidth(font, text, p.scale);
    }
    const float x = p.x - (p.align == mu::Align::Center ? width * .5f : p.align == mu::Align::Right ? width : 0);
    Canvas{const_cast<void*>(font)}.Text(text, x, p.y, color, p.scale);
}
NativeSprite* Icon(std::uint32_t id, bool trinket) {
    if (!id || id > 0xffff) return nullptr;
    const auto key = id | (trinket ? 0x20000u : 0x10000u);
    unsigned slot = IconSlots, oldest = 0;
    for (unsigned i = 0; i < IconSlots; ++i) {
        if (s_IconKeys[i] == key) { s_IconUsed[i] = s_Frame; return s_IconReady[i] ? &s_Icons[i] : nullptr; }
        if (!s_IconKeys[i] && slot == IconSlots) slot = i;
        if (s_IconUsed[i] < s_IconUsed[oldest]) oldest = i;
    }
    if (s_IconLoads >= IconLoadsPerFrame) return nullptr;  // spread loading over frames
    if (slot == IconSlots) {
        if (s_IconUsed[oldest] == s_Frame) return nullptr;
        slot = oldest;
        s_Icons[slot].Destroy(); s_IconReady[slot] = false;
    }
    ++s_IconLoads;
    s_IconKeys[slot] = key; s_IconUsed[slot] = s_Frame;
    const auto manager = Manager();
    if (!Pointer(manager) || !Readable(manager + 0x36538, 0x28)) return nullptr;
    const auto config = Native<std::uintptr_t (*)(const void*, int)>(trinket ? mn::GetTrinket : dk::GetCollectible)(
        reinterpret_cast<const void*>(manager + 0x36538), static_cast<int>(id));
    char graphics[512];
    if (!config || !CopyNativeString(config + 0x38, graphics, sizeof(graphics)) || !graphics[0]) return nullptr;
    auto& sprite = s_Icons[slot];
    if (!sprite.Create(s_Module) || !sprite.Load("gfx/codex_eid_ui/item_icon.anm2") || !sprite.SetSpritesheet(0, graphics)) return nullptr;
    sprite.LoadGraphics();
    if (!sprite.SetFrame("Item", 0)) return nullptr;
    s_IconReady[slot] = true;
    return &sprite;
}
void Execute(const FeatureFrame& frame) {
    ++s_Frame; s_IconLoads = 0;
    if (!s_ArtTried) { s_ArtTried = true; s_ArtReady = s_Art.Create(s_Module) && s_Art.Load(mu::art::File); }
    const auto now = Now();
    for (const auto& p : s_Paints) {
        switch (p.kind) {
        case mu::PaintKind::Rect: Rect(p.x, p.y, p.x + p.w, p.y + p.h, p.color, p.alpha); break;
        case mu::PaintKind::Art:
            if (s_ArtReady && s_Art.SetFrame(p.name, p.frame)) {
                s_Art.SetScale(p.scale, p.scale);
                s_Art.SetTint(1, 1, 1, p.alpha);
                s_Art.Render({p.x, p.y});
            }
            break;
        case mu::PaintKind::Text: DrawText(frame, p); break;
        case mu::PaintKind::Item:
            if (auto* sprite = Icon(p.id, p.flag)) {
                sprite->SetScale(2 * p.scale, 2 * p.scale);
                sprite->SetTint(1, 1, 1, p.alpha);
                sprite->Render({p.x, p.y});
            }
            break;
        case mu::PaintKind::Icon: DrawEidIcon(frame, p.name, p.x, p.y); break;
        case mu::PaintKind::Description:
            // EID markup colours are made for dark backgrounds: draw them as ink on paper.
            SetEidInk(true);
            DrawEidMenuBody(frame, p.text, p.x, p.y, p.w, p.h, now);
            SetEidInk(false);
            break;
        case mu::PaintKind::HitCounter: DrawHitCounterPreview(frame, p.alpha, p.flag); break;
        }
    }
}

// ---------- the host the menu model talks to ----------
class NativeHost final : public mu::Host {
public:
    std::uint64_t Now() override { return isaac::Now(); }
    bool InRun() override { std::uintptr_t g{}, p{}; return s_Origin != mu::Origin::Title && InGame(g, p); }
    unsigned ModCount() override { return isaac::ModCount; }
    bool ModAvailable(unsigned m) override { return IsModAvailable(static_cast<ModFeature>(m)); }
    bool ModEnabled(unsigned m) override { return IsModEnabled(static_cast<ModFeature>(m)); }
    void ToggleMod(unsigned m) override { isaac::ToggleMod(static_cast<ModFeature>(m)); }
    const char* ModName(unsigned m) override { return isaac::ModName(static_cast<ModFeature>(m)); }
    const char* ModDescription(unsigned m) override { return isaac::ModDescription(static_cast<ModFeature>(m)); }
    void ModStatus(unsigned m, char* out, unsigned size) override {
        out[0] = 0;
        const auto feature = static_cast<ModFeature>(m);
        if (!IsModEnabled(feature)) return;
        if (feature == ModFeature::TimeMachine && s_Origin != mu::Origin::Title) std::snprintf(out, size, "%s", TimeMachineStatus());
        else if (feature == ModFeature::MachineLock && MachineLockActive()) std::snprintf(out, size, "Holding Isaac in place");
        else if (feature == ModFeature::HitCounter && s_Origin != mu::Origin::Title) {
            unsigned total{}, floor{}; HitCounterCounts(total, floor);
            std::snprintf(out, size, "Hits this run: %u (floor %u)", total, floor);
        }
    }
    mu::Prefs& Preferences() override { return s_Prefs; }
    void PreferencesChanged() override { s_Prefs.mods = ModEnabledMask(); s_Prefs.hasMods = true; ApplyPrefs(); s_Dirty = true; }
    mu::Result Act(mu::Action action, const mu::Entry& entry) override;
    mu::Result UseTool(mu::Tool tool) override;
    bool Run(mu::RunInfo& out) override;
    bool Owned(unsigned id) override {
        std::uintptr_t g{}, p{};
        return InGame(g, p) && Native<bool (*)(void*, int, bool)>(mn::HasCollectible)(reinterpret_cast<void*>(p), static_cast<int>(id), false);
    }
    void Sound(unsigned id, float volume) override {
        const auto manager = Manager();
        if (Pointer(manager)) Native<void (*)(void*, int, float, int, bool, float)>(mn::PlaySound)(reinterpret_cast<void*>(manager), static_cast<int>(id), volume, 2, false, 1.f);
    }
    const char* Describe(const mu::Entry& entry) override {
        isaac_eid::Description description{};
        const auto category = static_cast<mu::Category>(entry.category);
        bool found = false;
        if (category == mu::Category::Collectible) found = isaac_eid::EnglishRepentance().Find(isaac_eid::Category::Collectible, entry.id, description);
        else if (category == mu::Category::Trinket) found = isaac_eid::EnglishRepentance().Find(isaac_eid::Category::Trinket, entry.id, description);
        else if (category == mu::Category::Card) found = isaac_eid::EnglishRepentance().Find(isaac_eid::Category::Card, entry.id, description);
        else if (category == mu::Category::Pill) found = isaac_eid::EnglishRepentance().Find(isaac_eid::Category::Pill, entry.id + 1u, description);
        return found ? description.markup : nullptr;
    }
    void HitCounterSize(float& w, float& h) override { HitCounterBox(w, h); }
};
constinit NativeHost s_Host;

// Story and event items that would cause odd states when added directly.
bool BlockedGift(unsigned id) { return id == 550 || id == 551 || id == 626 || id == 627 || id == 633 || id == 668; }

mu::Result Spawn(std::uintptr_t game, std::uintptr_t player, unsigned variant, unsigned subtype, std::uintptr_t& pickup) {
    Engine engine;
    const auto room = Read<std::uintptr_t>(game, profile::GameRoom);
    if (!Readable(room, 0x19d8)) return mu::Result::NotNow;
    const item_menu::Vec2 center{Read<float>(player, profile::EntityX), Read<float>(player, profile::EntityY)};
    constexpr item_menu::Vec2 offsets[] = {{0, -60}, {60, 0}, {0, 60}, {-60, 0}, {60, -60}, {60, 60}, {-60, 60}, {-60, -60}};
    item_menu::Vec2 position{};
    bool found{};
    for (auto offset : offsets) {
        const item_menu::Vec2 wanted{center.x + offset.x, center.y + offset.y};
        position = Native<item_menu::Vec2 (*)(void*, const item_menu::Vec2*, float, bool, bool)>(dk::FindPosition)(reinterpret_cast<void*>(room), &wanted, 10.f, true, false);
        if (item_menu::FreePosition(engine, room, position)) { found = true; break; }
    }
    if (!found) return mu::Result::NoSpace;
    const unsigned seed = debug_kit::SpawnSeed(Read<unsigned>(game, 0x25d44), Read<unsigned>(game, 0x24f99c) + static_cast<unsigned>(Now()), Read<unsigned>(player, 0x3e8), variant * 4096 + subtype);
    const item_menu::Vec2 zero{};
    pickup = Native<std::uintptr_t (*)(void*, unsigned, unsigned, const item_menu::Vec2*, const item_menu::Vec2*, void*, unsigned, unsigned, unsigned)>(dk::Spawn)(
        reinterpret_cast<void*>(game), 5, variant, &position, &zero, nullptr, subtype, seed, 0);
    if (!Readable(pickup, 0x570) || Read<unsigned>(pickup, profile::EntityType) != 5) return mu::Result::Failed;
    return mu::Result::Done;
}
mu::Result NativeHost::Act(mu::Action action, const mu::Entry& entry) {
    std::uintptr_t game{}, player{};
    if (!InGame(game, player)) return mu::Result::NotNow;
    if (Transitioning(game)) return mu::Result::NotNow;
    const auto category = static_cast<mu::Category>(entry.category);
    const auto manager = Manager();
    auto* p = reinterpret_cast<void*>(player);
    if (category == mu::Category::Collectible) {
        const auto config = Native<std::uintptr_t (*)(const void*, int)>(dk::GetCollectible)(reinterpret_cast<const void*>(manager + 0x36538), entry.id);
        if (!config) return mu::Result::Unavailable;
        if (action == mu::Action::Give) {
            if (BlockedGift(entry.id)) return mu::Result::Unavailable;
            const int charge = Readable(config + 0x74, 4) ? Read<int>(config, 0x74) : 0;
            Native<void (*)(void*, int, int, bool, int, int)>(mn::AddCollectible)(p, entry.id, charge < 0 ? 0 : charge, true, 0, 0);
            return mu::Result::Done;
        }
        std::uintptr_t pickup{};
        const auto result = Spawn(game, player, 100, entry.id, pickup);
        if (result == mu::Result::Done && action == mu::Action::Shop)
            Native<void (*)(void*, int)>(mn::MakeShopItem)(reinterpret_cast<void*>(pickup), -1);
        return result;
    }
    if (category == mu::Category::Trinket) {
        if (action == mu::Action::Give) { Native<void (*)(void*, int, bool)>(mn::AddTrinket)(p, entry.id, true); return mu::Result::Done; }
        std::uintptr_t pickup{}; return Spawn(game, player, 350, entry.id, pickup);
    }
    if (category == mu::Category::Card) {
        if (action == mu::Action::Give) { Native<void (*)(void*, int)>(mn::AddCard)(p, entry.id); return mu::Result::Done; }
        std::uintptr_t pickup{}; return Spawn(game, player, 300, entry.id, pickup);
    }
    if (category == mu::Category::Pill) {
        const int color = Native<int (*)(void*, int)>(mn::ForceAddPillEffect)(reinterpret_cast<void*>(game + mn::ItemPoolOffset), entry.id);
        if (color <= 0 || color > 0x8ff) return mu::Result::Failed;
        if (action == mu::Action::Give) { Native<void (*)(void*, int)>(mn::AddPill)(p, color); return mu::Result::Done; }
        std::uintptr_t pickup{}; return Spawn(game, player, 70, static_cast<unsigned>(color), pickup);
    }
    if (category == mu::Category::Pickup) { std::uintptr_t pickup{}; return Spawn(game, player, entry.id, entry.extra, pickup); }
    return mu::Result::Unavailable;
}
mu::Result NativeHost::UseTool(mu::Tool tool) {
    std::uintptr_t game{}, player{};
    if (!InGame(game, player) || Transitioning(game)) return mu::Result::NotNow;
    auto* p = reinterpret_cast<void*>(player);
    switch (tool) {
    case mu::Tool::FullHealth: Native<void (*)(void*)>(mn::SetFullHearts)(p); break;
    case mu::Tool::SoulHeart: Native<void (*)(void*, int)>(mn::AddSoulHearts)(p, 2); break;
    case mu::Tool::Coins: Native<void (*)(void*, int)>(mn::AddCoins)(p, 10); break;
    case mu::Tool::Bombs: Native<void (*)(void*, int)>(mn::AddBombs)(p, 5); break;
    case mu::Tool::Keys: Native<void (*)(void*, int)>(mn::AddKeys)(p, 5); break;
    case mu::Tool::Charge: if (!Native<bool (*)(void*, int, bool)>(mn::FullCharge)(p, 0, true)) return mu::Result::Failed; break;
    case mu::Tool::RevealMap: {
        auto* level = reinterpret_cast<void*>(game);
        Native<void (*)(void*)>(mn::ApplyMapEffect)(level);
        Native<void (*)(void*)>(mn::ApplyBlueMapEffect)(level);
        Native<void (*)(void*, bool)>(mn::ApplyCompassEffect)(level, true);
        break;
    }
    case mu::Tool::Reroll: Native<void (*)(void*, int, unsigned, int, int)>(mn::UseActiveItem)(p, 105, 1, -1, 0); break;
    default: return mu::Result::Unavailable;
    }
    return mu::Result::Done;
}
bool NativeHost::Run(mu::RunInfo& out) {
    out = {};
    std::uintptr_t game{}, player{};
    if (s_Origin == mu::Origin::Title || !InGame(game, player)) return false;
    out.valid = true;
    out.character = Read<int>(player, mn::PlayerType);
    out.stage = Read<int>(game, 0); out.stageType = Read<int>(game, 4);
    out.greed = Native<bool (*)(const void*)>(mn::IsGreedMode)(reinterpret_cast<const void*>(game));
    NativeString seed{};
    seed = Native<NativeString (*)(unsigned)>(mn::Seed2String)(Read<unsigned>(game, 0x25d44));
    if (!(seed.bytes[0] & 1)) CopyNativeString(reinterpret_cast<std::uintptr_t>(&seed), out.seed, sizeof(out.seed));
    out.frames = Read<unsigned>(game, 0x24f99c);
    HitCounterCounts(out.hits, out.floorHits);
    out.speed = Read<float>(player, 0x194c);
    out.tears = 30.f / (Read<float>(player, 0x1834) + 1.f);
    out.damage = Read<float>(player, 0x1844);
    out.range = Read<float>(player, 0x1854) / 40.f;
    out.shotSpeed = Read<float>(player, 0x1838);
    out.luck = Read<float>(player, 0x1950);
    const auto begin = Read<std::uintptr_t>(player, mn::PlayerHistoryBegin), end = Read<std::uintptr_t>(player, mn::PlayerHistoryBegin + 8);
    if (end > begin && (end - begin) % 0x1c == 0 && (end - begin) / 0x1c < 4096 && Readable(begin, end - begin)) {
        for (auto e = end; e > begin && out.historyCount < mu::MaxHistory;) {
            e -= 0x1c;
            const bool trinket = Read<std::uint8_t>(e, 4) != 0;
            const auto id = Read<unsigned>(e, 8);
            if (!id || id > (trinket ? 189u : 732u)) continue;
            out.history[out.historyCount++] = static_cast<std::uint16_t>(id | (trinket ? 0x8000u : 0u));
        }
    }
    return true;
}

// ---------- input ----------
bool DeviceExists(std::uintptr_t input, unsigned controller) {
    return Readable(input, 0x80) && debug_kit::ValidController(controller) &&
        Native<bool (*)(void*, unsigned)>(cm::DoesDeviceExist)(reinterpret_cast<void*>(input), controller);
}
std::uint32_t Buttons(std::uintptr_t input, unsigned controller) {
    if (!DeviceExists(input, controller)) return 0;
    std::uint32_t mask{};
    const auto pressed = Native<bool (*)(void*, unsigned, unsigned, unsigned*)>(dk::IsButtonPressed);
    for (unsigned i = 0; i < 15; ++i) if (pressed(reinterpret_cast<void*>(input), i, controller, nullptr)) mask |= 1u << i;
    return mask;
}
std::uintptr_t InputManager() { std::uintptr_t input{}; TryRead(s_Module + cm::InputManagerGot, input); return input; }

FeatureFrame PauseFrame(Canvas& canvas) {
    float width = Read<float>(s_Module, profile::ScreenWidth), height = Read<float>(s_Module, profile::ScreenHeight);
    if (!(width >= 200 && width <= 4096)) width = 480;
    if (!(height >= 150 && height <= 2160)) height = 270;
    const auto game = Game();
    return FeatureFrame{s_Module, game, Pointer(game) ? Read<std::uintptr_t>(game, profile::GameRoom) : 0, width, height, {
        &canvas,
        [](void* ctx, const char* text, float x, float y, FeatureColor c, float scale) { static_cast<Canvas*>(ctx)->Text(text, x, y, c, scale); },
        [](void*, float x1, float y1, float x2, float y2, FeatureColor c) {
            Rect(x1, y1, x2, y2, (static_cast<unsigned>(c.r * 255) << 16) | (static_cast<unsigned>(c.g * 255) << 8) | static_cast<unsigned>(c.b * 255), c.a);
        }}};
}
void PausePrompt(const FeatureFrame& frame) {
    if (!s_Prefs.pausePrompt || !s_ArtReady) return;
    const float x = frame.width - 78, y = frame.height - 26;
    if (s_Art.SetFrame(mu::art::Button.name, static_cast<int>(mu::art::Glyph::Y))) {
        s_Art.SetScale(1, 1); s_Art.SetTint(1, 1, 1, 1); s_Art.Render({x, y});
    }
    mu::Paint label{};
    label.kind = mu::PaintKind::Text; label.font = mu::Font::Ink; label.text = "Mods"; label.x = x + 30; label.y = y + 1;
    label.color = mu::Paper; label.scale = .62f; label.alpha = 1;
    DrawText(frame, label);
}

HOOK_DEFINE_TRAMPOLINE(PauseInput) {
    static void Callback(void* pause) {
        const auto input = InputManager();
        if (s_PauseReady && IsModMenuOpenFrom(MenuOrigin::Pause)) {
            if (!UpdateModMenu(input)) s_PauseGate = true;
            return;
        }
        std::uintptr_t game{}, player{};
        const bool live = s_PauseReady && InGame(game, player);
        const unsigned controller = live ? Read<unsigned>(player, 0x19ec) : 0;
        const auto buttons = live ? Buttons(input, controller) : 0;
        if (s_PauseGate) { if (!buttons) s_PauseGate = false; else return; }
        const bool y = (buttons & mu::Bit(mu::Y)) != 0;
        const bool pressed = y && !s_PausePreviousY;
        s_PausePreviousY = y;
        if (live && pressed && Read<int>(reinterpret_cast<std::uintptr_t>(pause), 0) == 1) {
            OpenModMenu(MenuOrigin::Pause, controller, player);
            return;
        }
        Orig(pause);
    }
};
HOOK_DEFINE_TRAMPOLINE(PauseRender) {
    static void Callback(void* pause) {
        if (s_PauseReady && IsModMenuOpenFrom(MenuOrigin::Pause)) {
            Native<void (*)(unsigned)>(cm::LoadShader)(1);
            Canvas canvas{reinterpret_cast<void*>(Manager() + profile::ManagerFont)};
            RenderModMenu(PauseFrame(canvas));
            return;
        }
        Orig(pause);
        if (s_PauseReady && Read<int>(reinterpret_cast<std::uintptr_t>(pause), 0) == 1) {
            Native<void (*)(unsigned)>(cm::LoadShader)(1);
            Canvas canvas{reinterpret_cast<void*>(Manager() + profile::ManagerFont)};
            if (!s_ArtTried) { s_ArtTried = true; s_ArtReady = s_Art.Create(s_Module) && s_Art.Load(mu::art::File); }
            PausePrompt(PauseFrame(canvas));
        }
    }
};
}

bool InitializeModMenu(std::uintptr_t module) {
    s_Module = module;
    s_Ready = module &&
        Matches(module + dk::Milliseconds, dk::MillisecondsGuard) && Matches(module + dk::IsButtonPressed, dk::IsButtonPressedGuard) &&
        Matches(module + dk::Spawn, dk::SpawnGuard) && Matches(module + dk::FindPosition, dk::FindPositionGuard) &&
        Matches(module + dk::GetCollectible, dk::GetCollectibleGuard) && Matches(module + cm::DoesDeviceExist, cm::DoesDeviceExistGuard) &&
        Matches(module + cm::LoadShader, cm::LoadShaderGuard) && Matches(module + i5::FontStringWidth, i5::FontStringWidthGuard);
#define MENU_GUARD(name) Matches(module + mn::name, mn::name##Guard)
    s_Ready = s_Ready && MENU_GUARD(PlaySound) && MENU_GUARD(Seed2String) && MENU_GUARD(AddCollectible) && MENU_GUARD(AddTrinket) &&
        MENU_GUARD(AddCard) && MENU_GUARD(AddPill) && MENU_GUARD(AddCoins) && MENU_GUARD(AddBombs) && MENU_GUARD(AddKeys) &&
        MENU_GUARD(AddSoulHearts) && MENU_GUARD(SetFullHearts) && MENU_GUARD(FullCharge) && MENU_GUARD(UseActiveItem) &&
        MENU_GUARD(HasCollectible) && MENU_GUARD(ForceAddPillEffect) && MENU_GUARD(MakeShopItem) && MENU_GUARD(ApplyMapEffect) &&
        MENU_GUARD(ApplyBlueMapEffect) && MENU_GUARD(ApplyCompassEffect) && MENU_GUARD(IsGreedMode) && MENU_GUARD(GetTrinket);
#define FS_GUARD(name) Matches(module + mn::name##Stub, mn::name##Guard)
    s_FsReady = s_Ready && FS_GUARD(FsOpenFile) && FS_GUARD(FsCloseFile) && FS_GUARD(FsGetFileSize) && FS_GUARD(FsReadFile) &&
        FS_GUARD(FsWriteFile) && FS_GUARD(FsSetFileSize) && FS_GUARD(FsCreateFile) && FS_GUARD(FsIsMounted) &&
        FS_GUARD(FsCommitSaveData) && Readable(module + mn::SaveMountString, 7) &&
        std::memcmp(reinterpret_cast<const void*>(module + mn::SaveMountString), "sdmc:/", 7) == 0;
    if (!s_Ready) { s_PauseReady = false; return false; }
    // Hooks already installed in this same module stay; anything else must pass the guards again.
    const bool installed = s_PauseReady && module == s_PauseModule && Matches(module + mn::PauseProcessInput, s_PauseInputGuard) &&
        Matches(module + mn::PauseRender, s_PauseRenderGuard);
    s_PauseReady = installed;
    if (!installed && MENU_GUARD(PauseProcessInput) && MENU_GUARD(PauseRender)) {
        PauseInput::InstallAtPtr(module + mn::PauseProcessInput);
        PauseRender::InstallAtPtr(module + mn::PauseRender);
        std::memcpy(s_PauseInputGuard, reinterpret_cast<const void*>(module + mn::PauseProcessInput), 20);
        std::memcpy(s_PauseRenderGuard, reinterpret_cast<const void*>(module + mn::PauseRender), 20);
        s_PauseModule = module; s_PauseReady = true;
    }
#undef MENU_GUARD
#undef FS_GUARD
    ApplyPrefs();
    return true;
}
void ResetModMenu() {
    s_Menu.Abort();
    s_Art.Destroy(); s_ArtTried = s_ArtReady = false;
    for (unsigned i = 0; i < IconSlots; ++i) { s_Icons[i].Destroy(); s_IconKeys[i] = 0; s_IconReady[i] = false; s_IconUsed[i] = 0; }
    s_PauseGate = false; s_PausePreviousY = true;
}
bool IsModMenuOpen() { return s_Ready && s_Menu.Opened(); }
bool IsModMenuOpenFrom(MenuOrigin origin) { return IsModMenuOpen() && static_cast<unsigned>(s_Origin) == static_cast<unsigned>(origin); }
void OpenModMenu(MenuOrigin origin, unsigned controller, std::uintptr_t player) {
    if (!s_Ready) return;
    s_Origin = static_cast<mu::Origin>(static_cast<unsigned>(origin));
    s_Controller = controller; s_Player = player;
    s_Menu.Open(s_Host, s_Origin, Now());
}
bool UpdateModMenu(std::uintptr_t input) {
    if (!IsModMenuOpen()) return false;
    if (!DeviceExists(input, s_Controller)) { s_Menu.Abort(); SavePrefs(); return false; }
    s_Menu.Update(s_Host, Buttons(input, s_Controller), Now());
    if (!s_Menu.Opened()) { SavePrefs(); return false; }
    return true;
}
void AbortModMenu() { if (IsModMenuOpen()) { s_Menu.Abort(); SavePrefs(); } }
void RenderModMenu(const FeatureFrame& frame) {
    if (!IsModMenuOpen()) return;
    const auto now = Now();
    if (s_Menu.Finish(now)) { SavePrefs(); return; }
    s_Menu.Paint(s_Host, s_Paints, frame.width, frame.height, now);
    Execute(frame);
}
void PollModMenuPreferences() {
    if (!s_Ready || s_PrefsLoaded || !Mounted()) return;
    LoadPrefs();
    ApplyPrefs();
}
unsigned ModMenuShortcutMilliseconds() { return s_Prefs.ShortcutMilliseconds(); }
}

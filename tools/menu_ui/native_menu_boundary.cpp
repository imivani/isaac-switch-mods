// Compiles the complete production mod_menu.cpp. run_native_tests.py changes
// exactly two lines of it: Native<F>(offset) and Fs<F>(stub) resolve through
// TestNative / TestFs below instead of the game module. Each fake game function
// is selected by offset AND must have exactly the function type the production
// code calls it with, so a wrong native signature fails the test.
#include "native_test_host.hpp"
#include <type_traits>
template<class F> F TestNative(std::uintptr_t offset);
template<class F> F TestFs(std::uintptr_t stub);
#include "mod_menu_under_test.cpp"

namespace fake {
namespace mn = isaac::menu_native;
namespace dk = isaac::debug_kit::native;
namespace cm = isaac::clean_map::native;
namespace i5 = isaac::iteration5::native;
namespace pr = isaac::profile;
using Vec2 = isaac::item_menu::Vec2;
using isaac::ColorRaw;
using isaac::FileHandle;
using isaac::NativeString;
using isaac::WriteOption;

void* Player() { return reinterpret_cast<void*>(host->p1.Address()); }
void CheckPlayer(void* p) { CHECK(p == Player() || p == reinterpret_cast<void*>(host->p2.Address())); }
std::uint64_t Milliseconds() { return host->now; }
void DrawString(void* font, const char* text, float x, float y, float sx, float sy, const ColorRaw* color, unsigned box, bool center) {
    const auto offset = reinterpret_cast<std::uintptr_t>(font) - host->manager.Address();
    CHECK((offset == pr::ManagerFont || offset == 0x117120 || offset == 0x137170) && text && color && sx == sy && !box && !center);
    CHECK(color->a >= 0 && color->a <= 1);
    host->draws.push_back({"text", {static_cast<long long>(offset)}, text, x, y, sx});
}
void FillQuad(void* renderer, float x1, float y1, float x2, float y2, const ColorRaw* color) {
    CHECK(reinterpret_cast<std::uintptr_t>(renderer) == host->module.Address() + pr::ShapeRenderer && color && x2 >= x1 && y2 >= y1);
    host->draws.push_back({"rect", {}, "", x1, y1, color->a});
}
int StringWidth(const void* font, const char* text) {
    const auto offset = reinterpret_cast<std::uintptr_t>(font) - host->manager.Address();
    CHECK(offset == pr::ManagerFont || offset == 0x117120 || offset == 0x137170);
    return static_cast<int>(6 * std::strlen(text));
}
std::uintptr_t Config(const void* table, int id, const char* name) {
    CHECK(reinterpret_cast<std::uintptr_t>(table) == host->manager.Address() + 0x36538);
    host->Record(name, {id});
    return host->collectible && id > 0 ? host->config.Address() : 0;
}
std::uintptr_t GetCollectible(const void* table, int id) { return Config(table, id, "GetCollectible"); }
std::uintptr_t GetTrinket(const void* table, int id) { return Config(table, id, "GetTrinket"); }
bool HasCollectible(void* p, int id, bool ignore) { CheckPlayer(p); CHECK(!ignore); host->Record("HasCollectible", {id}); return id == 1; }
void PlaySound(void* manager, int id, float volume, int delay, bool loop, float pitch) {
    CHECK(reinterpret_cast<std::uintptr_t>(manager) == host->manager.Address() && volume > 0 && volume <= 1 && pitch == 1.f);
    host->Record("PlaySound", {id, delay, loop});
}
Vec2 FindPosition(void* room, const Vec2* wanted, float radius, bool a, bool b) {
    CHECK(reinterpret_cast<std::uintptr_t>(room) == host->room.Address() && wanted && radius == 10 && a && !b);
    host->Record("FindPosition");
    return host->noSpace ? Vec2{0, 0} : *wanted;
}
std::uintptr_t Spawn(void* game, unsigned type, unsigned variant, const Vec2* position, const Vec2* velocity, void* spawner,
                     unsigned subtype, unsigned seed, unsigned flags) {
    CHECK(reinterpret_cast<std::uintptr_t>(game) == host->game.Address() && type == 5 && position && velocity &&
          !velocity->x && !velocity->y && !spawner && seed && !flags);
    host->Record("Spawn", {variant, subtype, static_cast<long long>(position->x), static_cast<long long>(position->y)});
    host->pickup.Put<unsigned>(0x38, 5); host->pickup.Put<unsigned>(0x3c, variant); host->pickup.Put<unsigned>(0x40, subtype);
    return host->spawnFails ? 0 : host->pickup.Address();
}
void AddCollectible(void* p, int id, int charge, bool first, int slot, int data) {
    CheckPlayer(p); CHECK(first && !slot && !data); host->Record("AddCollectible", {id, charge});
}
void MakeShopItem(void* pickup, int id) { CHECK(reinterpret_cast<std::uintptr_t>(pickup) == host->pickup.Address()); host->Record("MakeShopItem", {id}); }
void AddTrinket(void* p, int id, bool first) { CheckPlayer(p); CHECK(first); host->Record("AddTrinket", {id}); }
void AddCard(void* p, int id) { CheckPlayer(p); host->Record("AddCard", {id}); }
int ForceAddPillEffect(void* pool, int effect) {
    CHECK(reinterpret_cast<std::uintptr_t>(pool) == host->game.Address() + 0x242c0);
    host->Record("ForceAddPillEffect", {effect}); return host->pillColor;
}
void AddPill(void* p, int color) { CheckPlayer(p); host->Record("AddPill", {color}); }
void SetFullHearts(void* p) { CheckPlayer(p); host->Record("SetFullHearts"); }
void AddSoulHearts(void* p, int n) { CheckPlayer(p); host->Record("AddSoulHearts", {n}); }
void AddCoins(void* p, int n) { CheckPlayer(p); host->Record("AddCoins", {n}); }
void AddBombs(void* p, int n) { CheckPlayer(p); host->Record("AddBombs", {n}); }
void AddKeys(void* p, int n) { CheckPlayer(p); host->Record("AddKeys", {n}); }
bool FullCharge(void* p, int slot, bool force) { CheckPlayer(p); CHECK(!slot && force); host->Record("FullCharge"); return !host->chargeFails; }
void Level(void* level, const char* name) { CHECK(reinterpret_cast<std::uintptr_t>(level) == host->game.Address()); host->Record(name); }
void ApplyMapEffect(void* level) { Level(level, "ApplyMapEffect"); }
void ApplyBlueMapEffect(void* level) { Level(level, "ApplyBlueMapEffect"); }
void ApplyCompassEffect(void* level, bool persistent) { CHECK(persistent); Level(level, "ApplyCompassEffect"); }
void UseActiveItem(void* p, int id, unsigned flags, int slot, int data) { CheckPlayer(p); host->Record("UseActiveItem", {id, flags, slot, data}); }
bool IsGreedMode(const void* game) { CHECK(reinterpret_cast<std::uintptr_t>(game) == host->game.Address()); return false; }
NativeString Seed2String(unsigned seed) {
    host->Record("Seed2String", {seed});
    NativeString s{};
    s.bytes[0] = 9 << 1; std::memcpy(s.bytes + 1, "ABCD EFGH", 10);
    return s;
}
bool DoesDeviceExist(void* input, unsigned controller) {
    CHECK(reinterpret_cast<std::uintptr_t>(input) == host->input.Address() && controller < 34);
    return host->connected[controller];
}
bool IsButtonPressed(void* input, unsigned button, unsigned controller, unsigned* result) {
    CHECK(reinterpret_cast<std::uintptr_t>(input) == host->input.Address() && button < 15 && controller < 34 && !result);
    return host->connected[controller] && (host->buttons[controller] & (1u << button));
}
void LoadShader(unsigned shader) { CHECK(shader == 1); ++host->shaders; }

// nn::fs through the game's own call stubs. Handles point at g_Files entries.
std::string* File(FileHandle file) { return static_cast<std::string*>(file.handle); }
std::uint32_t OpenFile(FileHandle* file, const char* path, int mode) {
    CHECK(file && std::string(path) == "sdmc:/isaac_mods_menu.cfg" && (mode == 1 || mode == 6));
    host->Record("OpenFile", {mode});
    const auto found = g_Files.find(path);
    if (found == g_Files.end()) return 0x202;  // PathNotFound
    file->handle = &found->second; return 0;
}
std::uint32_t GetFileSize(std::int64_t* size, FileHandle file) { *size = static_cast<std::int64_t>(File(file)->size()); return 0; }
std::uint32_t ReadFile(std::size_t* got, FileHandle file, std::int64_t offset, void* buffer, std::size_t size) {
    CHECK(offset == 0 && size <= File(file)->size()); ++host->fsReads;
    std::memcpy(buffer, File(file)->data(), size); *got = size; return 0;
}
void CloseFile(FileHandle file) { CHECK(File(file)); host->Record("CloseFile"); }
std::uint32_t CreateFile(const char* path, std::int64_t size) {
    CHECK(size == 0 && !g_Files.count(path)); ++host->fsCreates; g_Files[path] = ""; return 0;
}
std::uint32_t SetFileSize(FileHandle file, std::int64_t size) { File(file)->resize(static_cast<std::size_t>(size)); return 0; }
std::uint32_t WriteFile(FileHandle file, std::int64_t offset, const void* data, std::size_t size, const WriteOption* option) {
    CHECK(offset == 0 && option && option->flags == 1 && size == File(file)->size());
    if (host->fsWriteFails) return 0x1234;
    std::memcpy(File(file)->data(), data, size); ++host->fsWrites; return 0;
}
std::uint32_t CommitSaveData(const char* mount) { CHECK(std::string(mount) == "sdmc"); ++host->fsCommits; return 0; }
bool IsMounted(const char* mount) { CHECK(std::string(mount) == "sdmc"); return host->mounted; }

template<class F, class G> F Pick(G function, const char* name) {
    if constexpr (std::is_same_v<F, G>) return function;
    else { std::fprintf(stderr, "native signature mismatch for %s: production calls %s\n", name, __PRETTY_FUNCTION__); std::abort(); }
}
}

template<class F> F TestNative(std::uintptr_t offset) {
    using namespace fake;
#define FAKE(off, fn) if (offset == off) return Pick<F>(&fake::fn, #fn)
    FAKE(dk::Milliseconds, Milliseconds); FAKE(pr::DrawStringScaled, DrawString); FAKE(pr::FillQuad, FillQuad);
    FAKE(i5::FontStringWidth, StringWidth); FAKE(dk::GetCollectible, GetCollectible); FAKE(mn::GetTrinket, GetTrinket);
    FAKE(mn::HasCollectible, HasCollectible); FAKE(mn::PlaySound, PlaySound); FAKE(dk::FindPosition, FindPosition);
    FAKE(dk::Spawn, Spawn); FAKE(mn::AddCollectible, AddCollectible); FAKE(mn::MakeShopItem, MakeShopItem);
    FAKE(mn::AddTrinket, AddTrinket); FAKE(mn::AddCard, AddCard); FAKE(mn::ForceAddPillEffect, ForceAddPillEffect);
    FAKE(mn::AddPill, AddPill); FAKE(mn::SetFullHearts, SetFullHearts); FAKE(mn::AddSoulHearts, AddSoulHearts);
    FAKE(mn::AddCoins, AddCoins); FAKE(mn::AddBombs, AddBombs); FAKE(mn::AddKeys, AddKeys); FAKE(mn::FullCharge, FullCharge);
    FAKE(mn::ApplyMapEffect, ApplyMapEffect); FAKE(mn::ApplyBlueMapEffect, ApplyBlueMapEffect);
    FAKE(mn::ApplyCompassEffect, ApplyCompassEffect); FAKE(mn::UseActiveItem, UseActiveItem); FAKE(mn::IsGreedMode, IsGreedMode);
    FAKE(mn::Seed2String, Seed2String); FAKE(cm::DoesDeviceExist, DoesDeviceExist); FAKE(dk::IsButtonPressed, IsButtonPressed);
    FAKE(cm::LoadShader, LoadShader);
#undef FAKE
    std::fprintf(stderr, "unexpected native call at 0x%zx\n", static_cast<std::size_t>(offset)); std::abort();
}
template<class F> F TestFs(std::uintptr_t stub) {
    using namespace fake;
#define FAKE(off, fn) if (stub == mn::off) return Pick<F>(&fake::fn, #fn)
    FAKE(FsOpenFileStub, OpenFile); FAKE(FsGetFileSizeStub, GetFileSize); FAKE(FsReadFileStub, ReadFile);
    FAKE(FsCloseFileStub, CloseFile); FAKE(FsCreateFileStub, CreateFile); FAKE(FsSetFileSizeStub, SetFileSize);
    FAKE(FsWriteFileStub, WriteFile); FAKE(FsCommitSaveDataStub, CommitSaveData); FAKE(FsIsMountedStub, IsMounted);
#undef FAKE
    std::fprintf(stderr, "unexpected file-system stub 0x%zx\n", static_cast<std::size_t>(stub)); std::abort();
}

namespace menu_test {
const isaac::menu_ui::Menu& Menu() { return isaac::s_Menu; }
isaac::menu_ui::Prefs& Prefs() { return isaac::s_Prefs; }
isaac::menu_ui::Host& MenuHost() { return isaac::s_Host; }
bool Dirty() { return isaac::s_Dirty; }
void ForgetPreferences() { isaac::s_PrefsLoaded = false; isaac::s_Prefs = {}; isaac::s_Dirty = false; }
void PauseInput(void* pause) { isaac::PauseInput::Callback(pause); }
void PauseRender(void* pause) { isaac::PauseRender::Callback(pause); }
}

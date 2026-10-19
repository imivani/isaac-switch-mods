// Host tests for the mod menu model, layout and preferences (no game).
#include "menu.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace isaac::menu_ui;
unsigned checks{};
#define CHECK(...) do { ++checks; if (!(__VA_ARGS__)) { std::fprintf(stderr, "CHECK %d: %s\n", __LINE__, #__VA_ARGS__); std::abort(); } } while (0)

struct FakeHost final : Host {
    std::uint64_t now = 10000;
    bool inRun = true, saved{};
    Prefs prefs;
    unsigned mods = 13, changed{}, toggles{};
    std::uint32_t enabled = 0x1fff, available = 0x1fff;
    std::vector<std::pair<unsigned, float>> sounds;
    std::vector<std::pair<Action, Entry>> acts;
    std::vector<Tool> tools;
    Result next = Result::Done;
    std::uint64_t Now() override { return now; }
    bool InRun() override { return inRun; }
    unsigned ModCount() override { return mods; }
    bool ModAvailable(unsigned m) override { return available & (1u << m); }
    bool ModEnabled(unsigned m) override { return enabled & available & (1u << m); }
    void ToggleMod(unsigned m) override { enabled ^= 1u << m; ++toggles; }
    const char* ModName(unsigned m) override {
        static const char* names[] = {"External Item Descriptions", "Hit Counter", "Enhanced Boss Bars", "Planetarium Chance",
            "Regret Pedestals", "Time Machine", "Machine Lock", "Greed Machine Never Jams", "Instant Fires and Poops",
            "Specialist Dance", "Animated Items", "Unique Birthright", "Clean Minimap"};
        return m < 13 ? names[m] : "";
    }
    const char* ModDescription(unsigned) override { return "A description."; }
    void ModStatus(unsigned m, char* out, unsigned size) override { std::snprintf(out, size, m == 5 ? "Speeding up" : ""); }
    Prefs& Preferences() override { return prefs; }
    void PreferencesChanged() override { ++changed; }
    Result Act(Action a, const Entry& e) override { acts.push_back({a, e}); return next; }
    Result UseTool(Tool t) override { tools.push_back(t); return next; }
    bool Run(RunInfo& out) override {
        if (!inRun) return false;
        out = {};
        out.valid = true; out.character = 2; out.stage = 3; out.stageType = 4; std::snprintf(out.seed, sizeof(out.seed), "ABCD EFGH");
        out.frames = 30 * 125; out.hits = 4; out.floorHits = 1; out.speed = 1; out.tears = 2.73f; out.damage = 3.5f;
        out.range = 6.5f; out.shotSpeed = 1; out.luck = 0;
        const std::uint16_t items[] = {519, 8, 67, 0x8000 | 5, 390};
        for (auto i : items) out.history[out.historyCount++] = i;
        return true;
    }
    bool Owned(unsigned id) override { return id == 1 || id == 519; }
    void Sound(unsigned id, float volume) override { sounds.push_back({id, volume}); }
    const char* Describe(const Entry&) override { return "Item text#Second line"; }
    void HitCounterSize(float& w, float& h) override { w = 102 * prefs.hitSize / 100.f; h = 10 * prefs.hitSize / 100.f; }
};

static Menu menu;
static PaintList paints;
FakeHost* H;
void Press(std::uint32_t buttons, unsigned hold = 16) {
    menu.Update(*H, 0, H->now += 16);
    menu.Update(*H, buttons, H->now += hold);
    menu.Update(*H, 0, H->now += 16);
}
void Frame() { menu.Paint(*H, paints, 480, 270, H->now += 16); }
bool HasText(const char* text) {
    for (const auto& p : paints) if (p.kind == PaintKind::Text && std::strstr(p.text, text)) return true;
    return false;
}
unsigned CountKind(PaintKind kind) { unsigned n = 0; for (const auto& p : paints) n += p.kind == kind; return n; }
bool HasArt(const char* name) { for (const auto& p : paints) if (p.kind == PaintKind::Art && !std::strcmp(p.name, name)) return true; return false; }

int main() {
    FakeHost host; H = &host;
    // Opening: chord/pause buttons still held are ignored until released.
    menu.Open(host, Origin::Shortcut, host.now);
    CHECK(menu.Opened() && menu.CurrentTab() == Tab::Items && host.sounds.back().first == sound::PaperIn);
    menu.Update(host, Bit(LS) | Bit(RS) | Bit(Down), host.now += 16);
    CHECK(menu.Opened() && menu.Selection() == 0);
    Frame();
    CHECK(HasArt("Collection") && HasArt("ItemNote") && CountKind(PaintKind::Item) >= 24 + 1 && HasText("ITEMS"));
    CHECK(menu.VisibleCount() > 700);
    // Grid navigation with wrap and paging.
    Press(Bit(Right)); CHECK(menu.Selection() == 1);
    Press(Bit(Down)); CHECK(menu.Selection() == 9);
    Press(Bit(ZR)); CHECK(menu.Selection() == 33);
    Press(Bit(Left)); Press(Bit(Up)); CHECK(menu.Selection() == 24);
    // Holding accelerates: 2 seconds of Right moves far more than one step per 95 ms.
    menu.Update(host, 0, host.now += 16);
    for (int i = 0; i < 125; ++i) menu.Update(host, Bit(Right), host.now += 16);
    menu.Update(host, 0, host.now += 16);
    CHECK(menu.Selection() > 24 + 20);
    // Favorites.
    const auto fav = menu.VisibleAt(menu.Selection())->entry;
    Press(Bit(Y)); CHECK(host.prefs.IsFavorite(fav) && host.changed == 1 && std::strstr(menu.ToastText(), "favorites"));
    // Filter card: quality 4 collectibles only.
    Press(Bit(X)); CHECK(menu.FilterOpen());
    Press(Bit(Down)); Press(Bit(Down)); // Quality row
    for (int i = 0; i < 5; ++i) Press(Bit(Right));
    for (unsigned i = 0; i < menu.VisibleCount(); ++i) CHECK(menu.VisibleAt(i)->quality == 4);
    const unsigned q4 = menu.VisibleCount();
    CHECK(q4 > 20 && q4 < 120);
    Frame(); CHECK(HasText("Filter") && HasText("match"));
    // Favorites only (with any quality) shows exactly the starred item.
    Press(Bit(Down)); Press(Bit(Down)); Press(Bit(Right));
    Press(Bit(Up)); Press(Bit(Up)); Press(Bit(Right));
    CHECK(menu.VisibleCount() == 1);
    CHECK(menu.VisibleAt(0)->entry == fav);
    Press(Bit(Down)); Press(Bit(Down)); Press(Bit(Left));
    CHECK(menu.VisibleCount() > 700);
    Press(Bit(B));
    CHECK(!menu.FilterOpen());
    // Category: pickups use the list view.
    Press(Bit(X));
    for (int i = 0; i < 4; ++i) Press(Bit(Right));
    CHECK(menu.FilterCategory() == Category::Pickup);
    Press(Bit(B));
    Frame();
    CHECK(CountKind(PaintKind::Icon) >= 1);
    // A opens the action popup; pickups only offer Drop + Cancel.
    Press(Bit(A)); CHECK(menu.PopupOpen());
    Frame(); CHECK(HasText("Drop on the floor") && HasText("Cancel") && !HasText("Give to Isaac"));
    Press(Bit(A)); CHECK(!menu.PopupOpen() && host.acts.size() == 1 && host.acts[0].first == Action::Drop &&
                         host.acts[0].second.category == static_cast<std::uint8_t>(Category::Pickup));
    CHECK(host.prefs.recentCount == 1 && std::strstr(menu.ToastText(), "Placed"));
    host.next = Result::NoSpace;
    Press(Bit(A)); Press(Bit(A));
    CHECK(std::strstr(menu.ToastText(), "No free space") && host.prefs.recentCount == 1);
    host.next = Result::Done;
    // Not in a run: items refuse politely.
    host.inRun = false; Press(Bit(A)); CHECK(!menu.PopupOpen() && std::strstr(menu.ToastText(), "Start a run")); host.inRun = true;

    // Tabs: R moves to Mods, page-turn sound.
    Press(Bit(R)); CHECK(menu.CurrentTab() == Tab::Mods && host.sounds.back().first == sound::PageTurn);
    Frame(); CHECK(HasArt("ModsHeader") && HasArt("ModsBottom") && HasText("Hit Counter") && HasText("ON"));
    Press(Bit(Down)); CHECK(menu.ModSelection() == 1);
    Press(Bit(A)); CHECK(!host.ModEnabled(1) && host.toggles == 1);
    Frame(); CHECK(HasText("OFF"));
    Press(Bit(A)); CHECK(host.ModEnabled(1));
    // Hit Counter settings: size slider, move mode, reset.
    Press(Bit(Y)); CHECK(menu.SettingsPage() == Page::HitCounter);
    Frame(); CHECK(HasText("Hit Counter") && HasText("Opacity") && HasArt("Slider"));
    Press(Bit(Down)); Press(Bit(Down)); Press(Bit(Right)); CHECK(host.prefs.hitSize == 110);
    for (int i = 0; i < 30; ++i) Press(Bit(Right));
    CHECK(host.prefs.hitSize == 300);
    Press(Bit(Up)); Press(Bit(A)); CHECK(menu.Moving());
    Frame(); CHECK(CountKind(PaintKind::HitCounter) == 1 && !HasArt("ModsHeader"));
    menu.Update(host, 0, host.now += 16);
    for (int i = 0; i < 400; ++i) menu.Update(host, Bit(Left) | Bit(Down), host.now += 16);
    menu.Update(host, 0, host.now += 16);
    CHECK(host.prefs.hitX == 0 && host.prefs.hitY == 270 - 30);
    Press(Bit(A)); CHECK(!menu.Moving() && std::strstr(menu.ToastText(), "Position saved"));
    for (int i = 0; i < 4; ++i) Press(Bit(Down));
    Press(Bit(A)); CHECK(host.prefs.hitX == -1 && host.prefs.hitSize == 100);
    Press(Bit(B)); CHECK(menu.SettingsPage() == Page::None && menu.Opened());
    // Mods without settings say so.
    Press(Bit(Down)); Press(Bit(Y)); CHECK(menu.SettingsPage() == Page::None && std::strstr(menu.ToastText(), "no extra settings"));
    // Time Machine top speed.
    while (menu.ModSelection() != mods::TimeMachine) Press(Bit(Down));
    Frame(); CHECK(HasText("Speeding up"));
    Press(Bit(Y)); Press(Bit(Left)); CHECK(host.prefs.timeMachineSteps == 4);
    Press(Bit(B));
    // Unavailable mods cannot be toggled.
    host.available &= ~(1u << 7);
    while (menu.ModSelection() != 7) Press(Bit(Down));
    const auto before = host.toggles; Press(Bit(A)); CHECK(host.toggles == before && std::strstr(menu.ToastText(), "unavailable"));

    // Run tab.
    Press(Bit(R)); CHECK(menu.CurrentTab() == Tab::Run);
    Frame(); CHECK(HasArt("StatsTop") && HasText("Cain") && HasText("Mines I") && HasText("ABCD EFGH") && HasText("2:05") && HasText("2.73"));
    CHECK(HasText("Conjoined"));
    unsigned items = 0; for (const auto& p : paints) items += p.kind == PaintKind::Item;
    CHECK(items == 5);
    // Tools tab.
    Press(Bit(R)); CHECK(menu.CurrentTab() == Tab::Tools);
    Press(Bit(Down)); Press(Bit(Down)); Press(Bit(A)); CHECK(host.tools.size() == 1 && host.tools[0] == Tool::Coins && std::strstr(menu.ToastText(), "Done"));
    Frame(); CHECK(HasText("TOOLS") && HasText("Reveal the map"));
    // Menu tab: sounds off silences the menu, dim slider, shortcut.
    Press(Bit(R)); CHECK(menu.CurrentTab() == Tab::Settings);
    Frame(); CHECK(HasArt("OptionsTop") && HasText("Shortcut hold"));
    Press(Bit(A)); CHECK(!host.prefs.sounds);
    const auto quiet = host.sounds.size(); Press(Bit(Down)); CHECK(host.sounds.size() == quiet);
    Press(Bit(A)); CHECK(host.prefs.reduceMotion);
    Press(Bit(Down)); Press(Bit(Left)); CHECK(host.prefs.shortcut == 2 && host.prefs.ShortcutMilliseconds() == 1000);
    for (int i = 0; i < 3; ++i) Press(Bit(Down));
    Press(Bit(Left)); CHECK(host.prefs.dim == 50);
    Press(Bit(Down)); Press(Bit(A)); CHECK(host.prefs.sounds && !host.prefs.reduceMotion && host.prefs.dim == 60);
    // Wrap to Items with R; B closes with the paper-out sound and saves the tab.
    Press(Bit(R)); CHECK(menu.CurrentTab() == Tab::Items);
    Press(Bit(B)); CHECK(menu.Closing() && host.sounds.back().first == sound::PaperOut && host.prefs.lastTab == 0);
    host.now += 1000; CHECK(menu.Finish(host.now) && !menu.Opened());

    // Title screen: only Mods and Menu; opening defaults to Mods.
    menu.Open(host, Origin::Title, host.now);
    CHECK(menu.CurrentTab() == Tab::Mods);
    Press(Bit(R)); CHECK(menu.CurrentTab() == Tab::Settings);
    Press(Bit(R)); CHECK(menu.CurrentTab() == Tab::Mods);
    Frame(); for (const auto& p : paints) if (p.kind == PaintKind::Text && !std::strcmp(p.text, "ITEMS")) CHECK(p.alpha < .3f);
    Press(Bit(Plus)); CHECK(menu.Closing());
    menu.Abort(); CHECK(!menu.Opened());

    // Motion: papers rise from below while opening, settle after; reduced motion is instant.
    host.prefs.reduceMotion = false;
    menu.Open(host, Origin::Pause, host.now);
    menu.Paint(host, paints, 480, 270, host.now + 30);
    float early{}; for (const auto& p : paints) if (p.kind == PaintKind::Art && !std::strcmp(p.name, "ModsHeader")) early = p.y;
    menu.Paint(host, paints, 480, 270, host.now + 1000);
    float late{}; for (const auto& p : paints) if (p.kind == PaintKind::Art && !std::strcmp(p.name, "ModsHeader")) late = p.y;
    CHECK(early > late + 100 && late > 0 && late < 30);
    host.prefs.reduceMotion = true;
    menu.Open(host, Origin::Pause, host.now);
    menu.Paint(host, paints, 480, 270, host.now + 1);
    for (const auto& p : paints) if (p.kind == PaintKind::Art && !std::strcmp(p.name, "ModsHeader")) CHECK(p.y == late);
    menu.Abort();

    // Preferences round-trip and tolerance.
    Prefs p;
    p.mods = 0x1abc; p.hitX = 12; p.hitY = 34; p.hitSize = 150; p.hitOpacity = 40; p.hitFloor = false;
    p.timeMachineSteps = 3; p.eidMap = false; p.sounds = false; p.shortcut = 1; p.openTab = 2; p.dim = 80; p.lastTab = 3;
    p.ToggleFavorite({0, 519, 0}); p.ToggleFavorite({4, 20, 2}); p.AddRecent({1, 5, 0}); p.AddRecent({0, 1, 0});
    char text[2048];
    const unsigned n = Serialize(p, text, sizeof(text));
    CHECK(n > 20 && std::strncmp(text, "isaacmods 1\n", 12) == 0);
    Prefs q;
    CHECK(Parse(text, n, q) && q.hasMods && q.mods == 0x1abc && q.hitX == 12 && q.hitY == 34 && q.hitSize == 150 &&
          q.hitOpacity == 40 && !q.hitFloor && q.timeMachineSteps == 3 && !q.eidMap && q.eidNearby && !q.sounds &&
          q.shortcut == 1 && q.openTab == 2 && q.dim == 80 && q.lastTab == 3 && q.favoriteCount == 2 &&
          q.recentCount == 2 && q.recents[0] == (Entry{0, 1, 0}));
    CHECK(!Parse("garbage", 7, q));
    const char odd[] = "isaacmods 7\nunknown=5\nhit=9999,-5,7,500,1\nmenu=1,0,1,9,9,5,9\nfavorites=0:1:0,9:1:0,0:0:0,x\n";
    Prefs r;
    CHECK(Parse(odd, sizeof(odd) - 1, r) && r.hitSize == 50 && r.hitOpacity == 100 && r.hitY == -1 && r.hitX == 4096 &&
          r.shortcut == 3 && r.openTab == 0 && r.dim == 20 && r.lastTab == 0 && r.favoriteCount == 1 && !r.hasMods);
    Prefs full;
    for (unsigned i = 0; i < 70; ++i) full.ToggleFavorite({0, static_cast<std::uint16_t>(i + 1), 0});
    CHECK(full.favoriteCount == MaxFavorites);
    for (unsigned i = 0; i < 20; ++i) full.AddRecent({0, static_cast<std::uint16_t>(i + 1), 0});
    CHECK(full.recentCount == MaxRecents && full.recents[0].id == 20);
    full.AddRecent({0, 15, 0}); CHECK(full.recents[0].id == 15 && full.recents[1].id == 20 && full.recentCount == MaxRecents);
    // Animation helpers.
    CHECK(anim::Sin(0) > -0.01f && anim::Sin(0) < 0.01f && anim::Sin(anim::Pi / 2) > .99f && anim::Sin(-anim::Pi / 2) < -.99f);
    CHECK(anim::EaseOutBack(1) > .999f && anim::EaseOutBack(.7f) > 1.f && anim::Approach(0, 10, 5000, 30) == 10);
    std::printf("Mod menu: %u model, layout, motion and preference checks PASS\n", checks);
}

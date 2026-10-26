// Host tests for the mod menu model, layout and preferences (no game).
#include "menu.hpp"
#include "ink.hpp"
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
    // letter > 0: every character is this wide (a wider font than estimated).
    float letter = 0;
    unsigned lines = 0;
    float TextWidth(Font font, const char* text, float scale) override {
        return letter > 0 ? static_cast<float>(std::strlen(text)) * letter * scale : Host::TextWidth(font, text, scale);
    }
    unsigned DescriptionLines(const char*, float) override { return lines; }
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

    // ---------- readable ink ----------
    for (const auto& m : ink::Named) {
        CHECK(ink::OnPaper(m.screen) == m.paper);
        CHECK(ink::Contrast(m.paper, ink::PaperColor) >= ink::MinimumContrast);
        CHECK(ink::Contrast(m.paper, 0xe9dfd0) >= ink::MinimumContrast);  // the action note's shade
        CHECK(ink::Contrast(m.paper, ink::Ink) >= 160);                   // still not plain ink
    }
    CHECK(ink::OnPaper(0xffffff) == ink::Ink && ink::OnPaper(0xeeeeee) == ink::Ink);
    for (unsigned r = 0; r < 256; r += 17)
        for (unsigned g = 0; g < 256; g += 17)
            for (unsigned b = 0; b < 256; b += 17)
                CHECK(ink::Contrast(ink::OnPaper((r << 16) | (g << 8) | b), ink::PaperColor) >= ink::MinimumContrast);
    for (auto c : {Ink, InkFaint, InkStrong, InkGood, InkBad}) CHECK(ink::Contrast(c, Paper) >= ink::MinimumContrast);
    CHECK(ink::Contrast(InkDisabled, Paper) < ink::MinimumContrast);  // meant to look unavailable

    // ---------- search matching ----------
    char key[24];
    CHECK(SearchNormalize("Mom's Knife", key, sizeof(key)) == 9 && !std::strcmp(key, "momsknife"));
    SearchNormalize("moms knife", key, sizeof(key));
    CHECK(SearchRank("Mom's Knife", 114, true, key) == 2);
    SearchNormalize("onion", key, sizeof(key));
    CHECK(SearchRank("The Sad Onion", 1, true, key) == 3 && SearchRank("Brimstone", 118, true, key) == 0);
    CHECK(SearchRank("The Sad Onion", 1, true, "nion") == 4);
    CHECK(SearchRank("Brimstone", 118, true, "118") == 1 && SearchRank("Brimstone", 118, false, "118") == 0);
    CHECK(SearchRank("The D6", 105, true, "d6") == 3 && SearchRank("The D6", 105, true, "6") == 4);

    // ---------- search on the Items page ----------
    menu.Open(host, Origin::Pause, host.now);
    while (menu.CurrentTab() != Tab::Items) Press(Bit(R));
    const unsigned everything = menu.VisibleCount();
    Press(Bit(RS)); CHECK(menu.SearchOpen());
    auto type = [&](char c) {
        const unsigned target = c == ' ' ? 36u : (c >= '0' && c <= '9') ? 26u + static_cast<unsigned>(c - '0') : static_cast<unsigned>(c - 'A');
        for (int guard = 0; menu.SearchCursor() != target && guard < 40; ++guard) {
            const unsigned at = menu.SearchCursor(), row = at < 36 ? at / 9 : 4, wanted = target < 36 ? target / 9 : 4;
            if (row != wanted) Press(Bit(row < wanted ? Down : Up)); else Press(Bit(at < target ? Right : Left));
        }
        CHECK(menu.SearchCursor() == target);
        Press(Bit(A));
    };
    auto typeAll = [&](const char* text) { for (const char* c = text; *c; ++c) type(*c); };
    type(' '); CHECK(!menu.SearchText()[0] && host.sounds.back().first == sound::ErrorBuzz);  // no leading space
    typeAll("BRIM");
    CHECK(!std::strcmp(menu.SearchText(), "BRIM") && menu.VisibleCount() >= 1 && !std::strcmp(menu.VisibleAt(0)->name, "Brimstone"));
    for (unsigned i = 0; i < menu.VisibleCount(); ++i) {
        char name[64]; SearchNormalize(menu.VisibleAt(i)->name, name, sizeof(name));
        CHECK(std::strstr(name, "brim") != nullptr);
    }
    Frame(); CHECK(HasText("Search") && HasText("BRIM") && HasText("match") && HasText("Delete") && HasText("Done"));
    Press(Bit(B)); CHECK(!std::strcmp(menu.SearchText(), "BRI"));
    Press(Bit(Y)); CHECK(!menu.SearchText()[0] && menu.VisibleCount() == everything);
    typeAll("118"); CHECK(menu.VisibleAt(0)->entry.id == 118 && menu.VisibleAt(0)->rank == 1);
    Press(Bit(Y)); typeAll("MOMS K");
    bool knife = false;
    for (unsigned i = 0; i < menu.VisibleCount(); ++i) {
        knife |= !std::strcmp(menu.VisibleAt(i)->name, "Mom's Knife");
        CHECK(menu.VisibleAt(i)->rank == 2);
    }
    CHECK(knife);
    // RS closes the board; the search keeps filtering and shows in the summary and the filter card.
    Press(Bit(RS)); CHECK(!menu.SearchOpen() && menu.VisibleCount() < 10);
    Frame(); CHECK(HasText("\"MOMS K\""));
    Press(Bit(X)); CHECK(menu.FilterOpen());
    Frame(); CHECK(HasText("Search") && HasText("MOMS K") && HasText("Sort"));
    // Search combines with the other filters: quality 4 only.
    Press(Bit(Down)); Press(Bit(Down)); for (int i = 0; i < 5; ++i) Press(Bit(Right));
    for (unsigned i = 0; i < menu.VisibleCount(); ++i) CHECK(menu.VisibleAt(i)->quality == 4 && menu.VisibleAt(i)->rank == 2);
    for (int i = 0; i < 5; ++i) Press(Bit(Left));
    Press(Bit(Up)); Press(Bit(Up)); Press(Bit(Up)); Press(Bit(Left));  // Search row: Left clears
    CHECK(!menu.SearchText()[0] && menu.VisibleCount() == everything);
    Press(Bit(A)); CHECK(menu.SearchOpen());                               // A on it types again
    typeAll("QQQ"); CHECK(menu.VisibleCount() == 0);
    Frame(); CHECK(HasText("Nothing matches"));
    for (int i = 0; i < 3; ++i) Press(Bit(B));
    CHECK(menu.SearchOpen() && !menu.SearchText()[0]);
    Press(Bit(B)); CHECK(!menu.SearchOpen() && menu.FilterOpen());          // B on an empty search closes it
    Press(Bit(B)); CHECK(!menu.FilterOpen());
    Press(Bit(RS)); typeAll("ABCDEFGHIJKLMNOPQRSTUVW");
    CHECK(std::strlen(menu.SearchText()) == 20);
    menu.Abort(); menu.Open(host, Origin::Pause, host.now); CHECK(!menu.SearchText()[0]);  // forgotten on close

    // ---------- long descriptions turn pages ----------
    while (menu.CurrentTab() != Tab::Items) Press(Bit(R));
    auto note = [&] { for (const auto& p : paints) if (p.kind == PaintKind::Description) return p.frame; return -2; };
    host.lines = 3; Frame(); CHECK(note() == 0 && !HasText("1/1") && !HasText("More text"));
    host.lines = 7; Press(Bit(Right)); Frame(); CHECK(note() == 0 && HasText("1/3") && HasText("More text"));
    host.now += 5000; Frame(); CHECK(note() == 1 && HasText("2/3"));
    Press(Bit(LS)); Frame(); CHECK(note() == 2 && HasText("3/3"));
    host.now += 12000; Frame(); CHECK(note() == 2);                         // turned by hand: stays
    Press(Bit(LS)); Frame(); CHECK(note() == 0);
    Press(Bit(Right)); Frame(); CHECK(note() == 0);                         // a new item starts at page 1
    host.lines = 0; Frame(); CHECK(note() == -1);                           // unknown: the backend turns pages

    // ---------- text fits even when the font is wider than expected ----------
    host.letter = 9;
    Frame();
    float left = 1e9f;
    for (const auto& p : paints)
        if (p.kind == PaintKind::Text && p.font == Font::Hud && p.y > 240) { CHECK(p.x >= 0); if (p.x < left) left = p.x; }
    CHECK(HasText("Use") && HasText("Close") && !HasText("Page") && left >= 8);  // least important prompt dropped
    // Prompt labels never overlap their neighbours.
    std::vector<std::pair<float, float>> spans;
    for (const auto& p : paints)
        if (p.kind == PaintKind::Text && p.font == Font::Hud && p.y > 240) spans.push_back({p.x, p.x + std::strlen(p.text) * 9.f});
    for (std::size_t i = 0; i < spans.size(); ++i)
        for (std::size_t j = 0; j < spans.size(); ++j)
            if (i != j) CHECK(spans[i].second <= spans[j].first || spans[j].second <= spans[i].first);
    // A long toast wraps onto two lines inside its strip.
    host.next = Result::NoSpace; Press(Bit(A)); Press(Bit(A)); host.next = Result::Done;
    Frame();
    unsigned toastLines = 0;
    for (const auto& p : paints)
        if (p.kind == PaintKind::Text && p.font == Font::Hud && p.align == Align::Center) { ++toastLines; CHECK(std::strlen(p.text) * 9.f <= 300); }
    CHECK(toastLines == 2);
    // Settings values give up their arrows before running into the label.
    host.letter = 16;
    while (menu.CurrentTab() != Tab::Settings) Press(Bit(R));
    Frame(); CHECK(HasText("Last tab") && !HasText("< Last tab >"));
    host.letter = 0;
    Frame(); CHECK(HasText("< Last tab >"));
    menu.Abort();

    // Preferences round-trip and tolerance.
    Prefs p;
    p.mods = 0x1abc; p.hitX = 12; p.hitY = 34; p.hitSize = 150; p.hitOpacity = 40; p.hitFloor = false;
    p.timeMachineSteps = 3; p.eidMap = false; p.sounds = false; p.shortcut = 1; p.openTab = 2; p.dim = 80; p.lastTab = 3;
    p.ToggleFavorite({0, 519, 0}); p.ToggleFavorite({4, 20, 2}); p.AddRecent({1, 5, 0}); p.AddRecent({0, 1, 0});
    char text[2048];
    const unsigned n = Serialize(p, text, sizeof(text));
    CHECK(n > 20 && std::strncmp(text, "isaacmods 2\n", 12) == 0);
    Prefs q;
    CHECK(Parse(text, n, q) && q.hasMods && q.mods == 0x1abc && q.hitX == 12 && q.hitY == 34 && q.hitSize == 150 &&
          q.hitOpacity == 40 && !q.hitFloor && q.timeMachineSteps == 3 && !q.eidMap && q.eidNearby && !q.sounds &&
          q.shortcut == 1 && q.openTab == 2 && q.dim == 80 && q.lastTab == 3 && q.favoriteCount == 2 &&
          q.recentCount == 2 && q.recents[0] == (Entry{0, 1, 0}));
    CHECK(!Parse("garbage", 7, q));
    // Repentance+ rule switches round-trip; version 1 files predate mod 13, which starts on.
    p.rplus = 5;
    CHECK(Parse(text, Serialize(p, text, sizeof(text)), q) && q.rplus == 5);
    const char v1[] = "isaacmods 1\nmods=8190\n", v2[] = "isaacmods 2\nmods=8190\nrplus=99\n";
    CHECK(Parse(v1, sizeof(v1) - 1, q) && (q.mods & (1u << 13)) && !(q.mods & 1) && (q.mods & 2) && q.rplus == 0xf);
    CHECK(Parse(v2, sizeof(v2) - 1, q) && !(q.mods & (1u << 13)) && q.rplus == (99 & 0xf));
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

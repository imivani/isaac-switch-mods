#pragma once
#include "anim.hpp"
#include "host.hpp"
#include "paint.hpp"
#include "prefs.hpp"
#include "generated/art.hpp"
#include "generated/catalogue.hpp"
#include <cstdint>
#include <cstdio>
#include <cstring>

// The mod menu: an Isaac paper notebook with Items, Mods, Run, Tools and Menu
// tabs. Pure logic and layout; all game access goes through Host.
namespace isaac::menu_ui {
namespace cat = catalogue;

// Button numbers from the exact Switch DeviceNX::GetButtonName.
enum Button : unsigned { Left = 0, Right = 1, Up = 2, Down = 3, A = 4, B = 5, X = 6, Y = 7, L = 8, ZL = 9,
                         LS = 10, R = 11, ZR = 12, RS = 13, Plus = 14 };
constexpr std::uint32_t Bit(unsigned button) { return 1u << button; }
inline constexpr std::uint32_t Directions = Bit(Left) | Bit(Right) | Bit(Up) | Bit(Down);

// Edges plus held repeats that speed up the longer a button is held.
class Input {
    std::uint32_t previous_{};
    std::uint64_t since_[16]{}, next_[16]{};
    bool released_{};
public:
    void Reset() { *this = {}; }
    bool Released() const { return released_; }
    std::uint32_t Poll(std::uint32_t buttons, std::uint64_t now) {
        if (!released_) {
            if (!buttons) { released_ = true; previous_ = 0; }
            return 0;
        }
        std::uint32_t pressed = buttons & ~previous_;
        constexpr unsigned repeatable[] = {Left, Right, Up, Down, ZL, ZR};
        for (unsigned b : repeatable) {
            if (!(buttons & Bit(b))) continue;
            if (pressed & Bit(b)) { since_[b] = now; next_[b] = now + 320; continue; }
            if (now >= next_[b]) {
                pressed |= Bit(b);
                const auto held = now - since_[b];
                next_[b] = now + (held > 1500 ? 35 : held > 800 ? 60 : 95);
            }
        }
        previous_ = buttons;
        return pressed;
    }
};

enum class Tab : std::uint8_t { Items, Mods, Run, Tools, Settings, Count };
inline constexpr unsigned TabCount = static_cast<unsigned>(Tab::Count);
inline constexpr const char* TabNames[] = {"ITEMS", "MODS", "RUN", "TOOLS", "MENU"};

enum class RowKind : std::uint8_t { Toggle, Choice, Slider, Action };
struct Row {
    const char* label{};
    RowKind kind{};
    int value{}, min{}, max{}, step{1};
    const char* const* choices{};
    const char* help{};
    const char* actionText{};
};
enum class Page : std::uint8_t { None, HitCounter, TimeMachine, Eid, Menu, RepentancePlus };

inline constexpr const char* CategoryNames[] = {"Collectibles", "Trinkets", "Cards & runes", "Pills", "Pickups"};
inline constexpr const char* TypeNames[] = {"All types", "Passive", "Familiar", "Active"};
inline constexpr const char* ShowNames[] = {"Everything", "Favorites", "Recent", "Owned", "Not owned"};
inline constexpr const char* SortNames[] = {"A-Z", "ID", "Quality"};
inline constexpr const char* ToolNames[] = {"Full health", "+1 soul heart", "+10 coins", "+5 bombs", "+5 keys",
                                            "Charge active item", "Reveal the map", "Reroll pedestals"};
inline constexpr const char* ToolHelp[] = {
    "Fill all of Isaac's red heart containers.",
    "Add one soul heart.",
    "Add ten coins.",
    "Add five bombs.",
    "Add five keys.",
    "Fully charge the active item.",
    "Show every room on this floor, with its icon, like the Treasure Map, Compass and Blue Map together.",
    "Reroll the pedestal items in this room, like the D6."};
inline constexpr const char* CharacterNames[] = {
    "Isaac", "Magdalene", "Cain", "Judas", "???", "Eve", "Samson", "Azazel", "Lazarus", "Eden", "The Lost",
    "Lazarus II", "Dark Judas", "Lilith", "Keeper", "Apollyon", "The Forgotten", "The Soul", "Bethany", "Jacob",
    "Esau", "Tainted Isaac", "Tainted Magdalene", "Tainted Cain", "Tainted Judas", "Tainted ???", "Tainted Eve",
    "Tainted Samson", "Tainted Azazel", "Tainted Lazarus", "Tainted Eden", "Tainted Lost", "Tainted Lilith",
    "Tainted Keeper", "Tainted Apollyon", "Tainted Forgotten", "Tainted Bethany", "Tainted Jacob",
    "Tainted Lazarus", "Tainted Jacob", "Tainted Soul"};
inline constexpr const char* FormNames[FormCount] = {"Guppy", "Beelzebub", "Fun Guy", "Seraphim", "Bob", "Spun",
    "Yes Mother?", "Conjoined", "Leviathan", "Oh Crap", "Bookworm", "Adult", "Spider Baby"};

inline const char* FloorName(int stage, int type, bool greed, char* out, unsigned size) {
    if (greed) { std::snprintf(out, size, "Greed floor %d", stage); return out; }
    static constexpr const char* chapters[4][6] = {
        {"Basement", "Cellar", "Burning Basement", "", "Downpour", "Dross"},
        {"Caves", "Catacombs", "Flooded Caves", "", "Mines", "Ashpit"},
        {"Depths", "Necropolis", "Dank Depths", "", "Mausoleum", "Gehenna"},
        {"Womb", "Utero", "Scarred Womb", "", "Corpse", "Corpse"}};
    if (stage >= 1 && stage <= 8 && type >= 0 && type <= 5 && *chapters[(stage - 1) / 2][type])
        std::snprintf(out, size, "%s %s", chapters[(stage - 1) / 2][type], stage % 2 ? "I" : "II");
    else if (stage == 9) std::snprintf(out, size, "Blue Womb");
    else if (stage == 10) std::snprintf(out, size, "%s", type ? "Cathedral" : "Sheol");
    else if (stage == 11) std::snprintf(out, size, "%s", type ? "The Chest" : "Dark Room");
    else if (stage == 12) std::snprintf(out, size, "The Void");
    else if (stage == 13) std::snprintf(out, size, "Home");
    else std::snprintf(out, size, "Floor %d", stage);
    return out;
}

struct Visible {
    Entry entry{};
    const char* name{};
    std::uint8_t quality{}, kind{};
    std::uint32_t pools{};
    const char* icon{};
    std::uint8_t rank{};  // search: 1 = this ID, 2 = name starts with it, 3 = a word does, 4 = anywhere
};

// Item search compares letters and digits only, ignoring case, so "moms knife"
// finds "Mom's Knife". Returns the length written to out (at most size - 1);
// starts[i] says a word starts at out[i] (apostrophes do not split words).
inline unsigned SearchNormalize(const char* text, char* out, unsigned size, bool* starts = nullptr) {
    unsigned n = 0;
    bool boundary = true;
    for (const char* c = text ? text : ""; *c && n + 1 < size; ++c) {
        char ch = *c;
        if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        if ((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9')) {
            if (starts) starts[n] = boundary;
            out[n++] = ch;
            boundary = false;
        } else if (ch != '\'') boundary = true;
    }
    out[n] = 0;
    return n;
}
// query must already be normalized. 0 = no match.
inline std::uint8_t SearchRank(const char* name, unsigned id, bool byId, const char* query) {
    if (!query || !*query) return 4;
    if (byId) {
        unsigned number = 0, digits = 0;
        for (const char* q = query; *q >= '0' && *q <= '9' && digits < 6; ++q, ++digits) number = number * 10 + static_cast<unsigned>(*q - '0');
        if (digits && !query[digits] && number == id) return 1;
    }
    char text[64];
    bool starts[64]{};
    const unsigned length = SearchNormalize(name, text, sizeof(text), starts);
    const unsigned wanted = static_cast<unsigned>(std::strlen(query));
    std::uint8_t best = 0;
    for (unsigned i = 0; i + wanted <= length; ++i) {
        if (std::memcmp(text + i, query, wanted) != 0) continue;
        const std::uint8_t rank = i == 0 ? 2 : starts[i] ? 3 : 4;
        if (!best || rank < best) best = rank;
        if (best == 2) break;
    }
    return best;
}

class Menu {
public:
    static constexpr unsigned OpenMs = 280, CloseMs = 190, TabMs = 170, ToastMs = 2400;
    static constexpr unsigned GridColumns = 8, GridRows = 3, ListColumns = 2, ListRows = 6;
    static constexpr unsigned ModRows = 10, MaxVisible = 1100;

    bool Opened() const { return open_; }
    bool Closing() const { return closing_; }
    Origin From() const { return origin_; }
    Tab CurrentTab() const { return tab_; }
    bool Moving() const { return moving_; }
    unsigned Selection() const { return selection_; }
    unsigned VisibleCount() const { return visibleCount_; }
    const Visible* VisibleAt(unsigned i) const { return i < visibleCount_ ? &visible_[i] : nullptr; }
    Page SettingsPage() const { return page_; }
    bool FilterOpen() const { return filterOpen_; }
    bool SearchOpen() const { return searchOpen_; }
    const char* SearchText() const { return search_; }
    unsigned SearchCursor() const { return searchCursor_; }
    bool PopupOpen() const { return popupOpen_; }
    const char* ToastText() const { return toast_; }
    unsigned ModSelection() const { return modSelection_; }
    unsigned RowSelection() const { return rowSelection_; }
    unsigned ToolSelection() const { return toolSelection_; }
    Category FilterCategory() const { return category_; }

    void Open(Host& host, Origin origin, std::uint64_t now) {
        auto& prefs = host.Preferences();
        const bool title = origin == Origin::Title;
        ResetState();
        open_ = true; origin_ = origin; openedAt_ = now; lastPaint_ = now;
        unsigned wanted = prefs.openTab ? prefs.openTab - 1u : prefs.lastTab;
        if (wanted >= TabCount) wanted = 1;
        tab_ = static_cast<Tab>(wanted);
        if (title && !TabUsable(tab_)) tab_ = Tab::Mods;
        input_.Reset();
        Refilter(host);
        Sound(host, sound::PaperIn, .7f);
    }
    void Close(Host& host, std::uint64_t now) {
        if (!open_ || closing_) return;
        closing_ = true; closedAt_ = now;
        auto& prefs = host.Preferences();
        if (prefs.lastTab != static_cast<std::uint8_t>(tab_)) { prefs.lastTab = static_cast<std::uint8_t>(tab_); host.PreferencesChanged(); }
        Sound(host, sound::PaperOut, .7f);
    }
    // Instantly gone (context lost, controller disconnected).
    void Abort() { open_ = closing_ = moving_ = false; }
    // True once a close animation has finished; the menu is then fully closed.
    bool Finish(std::uint64_t now) {
        if (open_ && closing_ && now - closedAt_ >= CloseDuration()) { open_ = closing_ = false; return true; }
        return false;
    }

    // Consumes the buttons while open. Returns true while the menu is visible.
    bool Update(Host& host, std::uint32_t buttons, std::uint64_t now) {
        if (!open_) return false;
        reduceMotion_ = host.Preferences().reduceMotion;
        if (closing_) { Finish(now); return open_; }
        const auto keys = input_.Poll(buttons, now);
        if (moving_) { UpdateMove(host, buttons, keys, now); return true; }
        if (!keys) return true;
        if (keys & Bit(Plus)) { Close(host, now); return true; }
        if (popupOpen_) { UpdatePopup(host, keys, now); return true; }
        if (searchOpen_) { UpdateSearch(host, keys, now); return true; }
        if (filterOpen_) { UpdateFilter(host, keys, now); return true; }
        if (page_ != Page::None && tab_ == Tab::Mods) { UpdateRows(host, keys, now, true); return true; }
        if (keys & (Bit(L) | Bit(R))) { SwitchTab(host, keys & Bit(R) ? 1 : -1, now); return true; }
        if ((keys & Bit(B)) && page_ == Page::None) { Close(host, now); return true; }
        switch (tab_) {
        case Tab::Items: UpdateItems(host, keys, now); break;
        case Tab::Mods: UpdateMods(host, keys, now); break;
        case Tab::Run: UpdateRun(host, keys); break;
        case Tab::Tools: UpdateTools(host, keys, now); break;
        case Tab::Settings: page_ = Page::Menu; UpdateRows(host, keys, now, false); page_ = Page::None; break;
        default: break;
        }
        return true;
    }

    void Paint(Host& host, PaintList& out, float width, float height, std::uint64_t now) {
        out.Clear();
        if (!open_) return;
        reduceMotion_ = host.Preferences().reduceMotion;
        const auto elapsed = now > lastPaint_ ? now - lastPaint_ : 0;
        lastPaint_ = now;
        const float ox = (width - 480.f) * .5f, oy = (height - 270.f) * .5f;
        screenW_ = width; screenH_ = height;
        if (moving_) { PaintMove(host, out, width, height, now); return; }
        const float appear = closing_ ? 1 - anim::EaseInCubic(anim::Progress(now, closedAt_, CloseDuration()))
                                      : anim::EaseOutCubic(anim::Progress(now, openedAt_, Duration(OpenMs)));
        const auto& prefs = host.Preferences();
        out.Rect(0, 0, width, height, 0x000000, prefs.dim / 100.f * appear);
        // Papers drop in with a small overshoot, staggered; they sink on close.
        auto rise = [&](unsigned delay) {
            if (closing_) return anim::EaseInCubic(anim::Progress(now, closedAt_, CloseDuration())) * 260.f;
            return (1 - anim::EaseOutBack(anim::Progress(now, openedAt_ + Duration(delay), Duration(OpenMs)))) * 280.f;
        };
        const float switchT = anim::EaseOutCubic(anim::Progress(now, tabChangedAt_, Duration(TabMs)));
        const float slide = (1 - switchT) * 34.f * tabDirection_;
        Frame frame{out, host, now, elapsed, ox + slide, oy, rise(0), rise(70), switchT * appear, appear};
        PaintTabs(host, out, ox, oy, appear, now);
        switch (tab_) {
        case Tab::Items: PaintItems(frame); break;
        case Tab::Mods: PaintMods(frame); break;
        case Tab::Run: PaintRun(frame); break;
        case Tab::Tools: PaintTools(frame); break;
        case Tab::Settings: PaintSettings(frame); break;
        default: break;
        }
        PaintPrompts(host, out, width, height, appear);
        PaintToast(host, out, width, height, now);
    }

private:
    struct Frame {
        PaintList& out;
        Host& host;
        std::uint64_t now, elapsed;
        float x, y, riseLeft, riseRight, alpha, appear;
    };

    // ---------- shared helpers ----------
    // Field-by-field reset: the object is large, never copy a temporary.
    void ResetState() {
        input_.Reset(); moveInput_.Reset();
        origin_ = Origin::Shortcut; tab_ = Tab::Mods; page_ = Page::None; category_ = Category::Collectible;
        type_ = show_ = sort_ = 0; quality_ = pool_ = -1;
        open_ = closing_ = filterOpen_ = popupOpen_ = moving_ = searchOpen_ = false;
        search_[0] = searchKey_[0] = 0; searchCursor_ = 0;
        selection_ = modSelection_ = rowSelection_ = toolSelection_ = filterRow_ = popupRow_ = historyPage_ = 0;
        visibleCount_ = 0;
        openedAt_ = closedAt_ = tabChangedAt_ = selectedAt_ = stampAt_ = popupAt_ = pageAt_ = toastAt_ = lastPaint_ = 0;
        moveSince_ = moveLast_ = 0; moveHeld_ = 0;
        tabDirection_ = 1; cursorY_ = rowCursor_ = toolCursor_ = gridX_ = gridY_ = 0;
        toast_[0] = 0;
        descEntry_ = {}; descPage_ = descPages_ = 0; descManual_ = false; descShownAt_ = 0;
    }
    unsigned Duration(unsigned ms) const { return reduceMotion_ ? 0 : ms; }
    unsigned CloseDuration() const { return Duration(CloseMs); }
    void Sound(Host& host, unsigned id, float volume) { if (host.Preferences().sounds) host.Sound(id, volume); }
    bool TabUsable(Tab tab) const { return origin_ != Origin::Title || tab == Tab::Mods || tab == Tab::Settings; }
    void Toast(const char* text, std::uint64_t now) {
        std::snprintf(toast_, sizeof(toast_), "%s", text); toastAt_ = now;
    }
    void SwitchTab(Host& host, int direction, std::uint64_t now) {
        unsigned next = static_cast<unsigned>(tab_);
        for (unsigned i = 0; i < TabCount; ++i) {
            next = (next + TabCount + static_cast<unsigned>(direction)) % TabCount;
            if (TabUsable(static_cast<Tab>(next))) break;
        }
        if (next == static_cast<unsigned>(tab_)) return;
        tab_ = static_cast<Tab>(next);
        tabDirection_ = direction > 0 ? 1.f : -1.f;
        tabChangedAt_ = now;
        page_ = Page::None; rowSelection_ = 0; filterOpen_ = popupOpen_ = searchOpen_ = false;
        Sound(host, sound::PageTurn, .8f);
    }
    float Bob(std::uint64_t now) const { return reduceMotion_ ? 0 : anim::Wave(now, 900) * 1.5f; }
    float Float(std::uint64_t now) const { return reduceMotion_ ? 0 : anim::Wave(now, 1600) * 1.5f; }
    float Pop(std::uint64_t now) const {
        return 1 + .2f * anim::EaseOutBack(anim::Progress(now, selectedAt_, Duration(180)));
    }
    void Select(unsigned& selection, unsigned value, Host& host, std::uint64_t now) {
        if (value == selection) return;
        selection = value; selectedAt_ = now;
        Sound(host, sound::Scroll, .5f);
    }

    // ---------- items ----------
    bool GridView() const { return category_ == Category::Collectible || category_ == Category::Trinket; }
    unsigned PerPage() const { return GridView() ? GridColumns * GridRows : ListColumns * ListRows; }
    // Filter card rows: 0 Search, 1 Show, 2 Type, 3 Quality, 4 Pool, 5 Only, 6 Sort.
    static constexpr unsigned FilterRows = 7;
    bool FilterRowActive(unsigned row) const {
        if (row == 2 || row == 3 || row == 4) return category_ == Category::Collectible;
        return true;
    }
    void Refilter(Host& host) {
        const auto& prefs = host.Preferences();
        visibleCount_ = 0;
        const bool searching = searchKey_[0] != 0;
        auto add = [&](const Visible& v) {
            if (show_ == 1 && !prefs.IsFavorite(v.entry)) return;
            if (show_ == 3 && (category_ != Category::Collectible || !host.Owned(v.entry.id))) return;
            if (show_ == 4 && category_ == Category::Collectible && host.Owned(v.entry.id)) return;
            // IDs are only meaningful for these; pickups are variant/subtype pairs.
            const auto rank = searching ? SearchRank(v.name, v.entry.id, category_ != Category::Pickup, searchKey_) : 0;
            if (searching && !rank) return;
            if (visibleCount_ < MaxVisible) { visible_[visibleCount_] = v; visible_[visibleCount_++].rank = rank; }
        };
        auto build = [&](const Entry& e) -> Visible {
            Visible v{e};
            switch (static_cast<Category>(e.category)) {
            case Category::Collectible:
                for (const auto& c : cat::Collectibles) if (c.id == e.id) { v.name = c.name; v.quality = c.quality; v.kind = c.kind; v.pools = c.pools; }
                break;
            case Category::Trinket: for (const auto& t : cat::Trinkets) if (t.id == e.id) v.name = t.name; break;
            case Category::Card: for (const auto& c : cat::Cards) if (c.id == e.id) { v.name = c.name; v.icon = c.flag ? "Rune" : "Card"; } break;
            case Category::Pill: for (const auto& p : cat::Pills) if (p.id == e.id) { v.name = p.name; v.icon = "Pill"; } break;
            case Category::Pickup:
                for (const auto& p : cat::Pickups) if (p.variant == e.id && p.subtype == e.extra) { v.name = p.name; v.icon = p.icon; }
                break;
            default: break;
            }
            return v;
        };
        if (show_ == 2) {
            for (unsigned i = 0; i < prefs.recentCount; ++i)
                if (prefs.recents[i].category == static_cast<std::uint8_t>(category_)) {
                    const auto v = build(prefs.recents[i]);
                    if (v.name) add(v);
                }
        } else switch (category_) {
        case Category::Collectible:
            for (const auto& c : cat::Collectibles) {
                if (c.hidden) continue;
                if (type_ && c.kind != type_) continue;
                if (quality_ >= 0 && c.quality != quality_) continue;
                if (pool_ >= 0 && !(c.pools & (1u << pool_))) continue;
                add(Visible{{static_cast<std::uint8_t>(Category::Collectible), c.id, 0}, c.name, c.quality, c.kind, c.pools, nullptr});
            }
            break;
        case Category::Trinket:
            for (const auto& t : cat::Trinkets) add(Visible{{static_cast<std::uint8_t>(Category::Trinket), t.id, 0}, t.name});
            break;
        case Category::Card:
            for (const auto& c : cat::Cards) add(Visible{{static_cast<std::uint8_t>(Category::Card), c.id, 0}, c.name, 0, 0, 0, c.flag ? "Rune" : "Card"});
            break;
        case Category::Pill:
            for (const auto& p : cat::Pills) add(Visible{{static_cast<std::uint8_t>(Category::Pill), p.id, 0}, p.name, 0, 0, 0, "Pill"});
            break;
        case Category::Pickup:
            for (const auto& p : cat::Pickups) add(Visible{{static_cast<std::uint8_t>(Category::Pickup), p.variant, p.subtype}, p.name, 0, 0, 0, p.icon});
            break;
        default: break;
        }
        if (sort_ && show_ != 2) {
            // Stable insertion sort (catalogue order is A-Z already).
            for (unsigned i = 1; i < visibleCount_; ++i) {
                const auto v = visible_[i];
                unsigned j = i;
                auto before = [&](const Visible& a, const Visible& b) {
                    if (sort_ == 1) return a.entry.id < b.entry.id || (a.entry.id == b.entry.id && a.entry.extra < b.entry.extra);
                    return a.quality > b.quality;
                };
                while (j > 0 && before(v, visible_[j - 1])) { visible_[j] = visible_[j - 1]; --j; }
                visible_[j] = v;
            }
        }
        if (searching) {
            // Best matches first; the chosen sort order stays within each group.
            for (unsigned i = 1; i < visibleCount_; ++i) {
                const auto v = visible_[i];
                unsigned j = i;
                while (j > 0 && v.rank < visible_[j - 1].rank) { visible_[j] = visible_[j - 1]; --j; }
                visible_[j] = v;
            }
        }
        if (selection_ >= visibleCount_) selection_ = visibleCount_ ? visibleCount_ - 1 : 0;
    }
    void MoveItems(Host& host, int delta, std::uint64_t now) {
        if (!visibleCount_) return;
        int next = static_cast<int>(selection_) + delta;
        if (next < 0) next = delta == -1 ? static_cast<int>(visibleCount_) - 1 : 0;
        if (next >= static_cast<int>(visibleCount_)) next = delta == 1 ? 0 : static_cast<int>(visibleCount_) - 1;
        Select(selection_, static_cast<unsigned>(next), host, now);
    }
    void UpdateItems(Host& host, std::uint32_t keys, std::uint64_t now) {
        const int across = GridView() ? 1 : static_cast<int>(ListRows);
        const int down = GridView() ? static_cast<int>(GridColumns) : 1;
        if (keys & Bit(Left)) MoveItems(host, -across, now);
        else if (keys & Bit(Right)) MoveItems(host, across, now);
        else if (keys & Bit(Up)) MoveItems(host, -down, now);
        else if (keys & Bit(Down)) MoveItems(host, down, now);
        else if (keys & Bit(ZL)) MoveItems(host, -static_cast<int>(PerPage()), now);
        else if (keys & Bit(ZR)) MoveItems(host, static_cast<int>(PerPage()), now);
        else if (keys & Bit(X)) { filterOpen_ = true; filterRow_ = 1; Sound(host, sound::PaperIn, .45f); }
        else if (keys & Bit(RS)) OpenSearch(host);
        else if ((keys & Bit(LS)) && descPages_ > 1) {
            descPage_ = (descPage_ + 1) % descPages_; descManual_ = true;
            Sound(host, sound::PageTurn, .4f);
        }
        else if ((keys & Bit(Y)) && visibleCount_) {
            auto& prefs = host.Preferences();
            const bool now_favorite = prefs.ToggleFavorite(visible_[selection_].entry);
            host.PreferencesChanged();
            Toast(now_favorite ? "Added to favorites" : (prefs.favoriteCount >= MaxFavorites ? "Favorites are full" : "Removed from favorites"), now);
            Sound(host, now_favorite ? sound::NoteAppear : sound::NoteHide, .6f);
            if (show_ == 1) Refilter(host);
        }
        else if ((keys & Bit(A)) && visibleCount_) {
            if (origin_ == Origin::Title || !host.InRun()) { Toast("Start a run to use items", now); Sound(host, sound::ErrorBuzz, .35f); return; }
            popupOpen_ = true; popupRow_ = 0; popupAt_ = now;
            Sound(host, sound::NoteAppear, .5f);
        }
    }
    unsigned PopupActions(Action* actions) const {
        unsigned n = 0;
        actions[n++] = Action::Drop;
        if (category_ != Category::Pickup) actions[n++] = Action::Give;
        if (category_ == Category::Collectible) actions[n++] = Action::Shop;
        return n;
    }
    static const char* ActionLabel(Action a, Category c) {
        if (a == Action::Give) return "Give to Isaac";
        if (a == Action::Shop) return "Place as a shop item";
        return c == Category::Collectible ? "Place on a pedestal" : "Drop on the floor";
    }
    void UpdatePopup(Host& host, std::uint32_t keys, std::uint64_t now) {
        Action actions[4];
        const unsigned n = PopupActions(actions);
        if (keys & Bit(B)) { popupOpen_ = false; Sound(host, sound::NoteHide, .5f); return; }
        if (keys & Bit(Up)) Select(popupRow_, (popupRow_ + n) % (n + 1), host, now);
        else if (keys & Bit(Down)) Select(popupRow_, (popupRow_ + 1) % (n + 1), host, now);
        else if (keys & Bit(A)) {
            popupOpen_ = false;
            if (popupRow_ >= n || !visibleCount_) { Sound(host, sound::NoteHide, .5f); return; }
            const auto& v = visible_[selection_];
            const auto result = host.Act(actions[popupRow_], v.entry);
            char text[96];
            switch (result) {
            case Result::Done:
                std::snprintf(text, sizeof(text), "%s %s", actions[popupRow_] == Action::Give ? "Gave" : "Placed", v.name ? v.name : "it");
                host.Preferences().AddRecent(v.entry); host.PreferencesChanged();
                Sound(host, sound::ThumbsUp, .35f);
                break;
            case Result::NoSpace: std::snprintf(text, sizeof(text), "No free space nearby. Move, then try again."); break;
            case Result::Unavailable: std::snprintf(text, sizeof(text), "That is not available in this game."); break;
            case Result::NotNow: std::snprintf(text, sizeof(text), "Not possible right now."); break;
            default: std::snprintf(text, sizeof(text), "That did not work."); break;
            }
            if (result != Result::Done) Sound(host, sound::ErrorBuzz, .35f);
            Toast(text, now);
        }
    }
    void UpdateFilter(Host& host, std::uint32_t keys, std::uint64_t now) {
        constexpr unsigned rows = FilterRows;
        if (keys & (Bit(B) | Bit(X))) { filterOpen_ = false; Sound(host, sound::PaperOut, .45f); return; }
        if (keys & Bit(Up)) { Select(filterRow_, (filterRow_ + rows - 1) % rows, host, now); return; }
        if (keys & Bit(Down)) { Select(filterRow_, (filterRow_ + 1) % rows, host, now); return; }
        if (filterRow_ == 0) {
            // Search: A or Right types, Left or Y clears.
            if (keys & (Bit(A) | Bit(Right) | Bit(RS))) OpenSearch(host);
            else if ((keys & (Bit(Left) | Bit(Y))) && search_[0]) { SetSearch(host, ""); Sound(host, sound::NoteHide, .5f); }
            return;
        }
        int delta = (keys & Bit(Right)) || (keys & Bit(A)) ? 1 : (keys & Bit(Left)) ? -1 : 0;
        if (!delta || !FilterRowActive(filterRow_)) return;
        auto cycle = [&](int value, int lo, int hi) { value += delta; return value > hi ? lo : value < lo ? hi : value; };
        switch (filterRow_) {
        case 1:
            category_ = static_cast<Category>(cycle(static_cast<int>(category_), 0, static_cast<int>(CategoryCount) - 1));
            if (category_ != Category::Collectible && show_ >= 3) show_ = 0;
            break;
        case 2: type_ = static_cast<std::uint8_t>(cycle(type_, 0, 3)); break;
        case 3: quality_ = static_cast<std::int8_t>(cycle(quality_, -1, 4)); break;
        case 4: pool_ = static_cast<std::int8_t>(cycle(pool_, -1, static_cast<int>(cat::PoolCount) - 1)); break;
        case 5: show_ = static_cast<std::uint8_t>(cycle(show_, 0, category_ == Category::Collectible ? 4 : 2)); break;
        case 6: sort_ = static_cast<std::uint8_t>(cycle(sort_, 0, 2)); break;
        default: break;
        }
        selection_ = 0;
        Refilter(host);
        Sound(host, sound::Scroll, .5f);
    }

    // ---------- search ----------
    // The letter board: 36 keys in 9 columns, then Space, Delete and Done.
    static constexpr char SearchKeys[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    static constexpr unsigned SearchColumns = 9, SearchLetters = 36, SearchLetterRows = SearchLetters / SearchColumns;
    static constexpr unsigned KeySpace = 36, KeyDelete = 37, KeyDone = 38, SearchMax = 20;
    void OpenSearch(Host& host) {
        searchOpen_ = true;
        Sound(host, sound::PaperIn, .45f);
    }
    void SetSearch(Host& host, const char* text) {
        std::snprintf(search_, sizeof(search_), "%s", text);
        SearchNormalize(search_, searchKey_, sizeof(searchKey_));
        selection_ = 0;
        Refilter(host);
    }
    void SearchMove(unsigned& key, std::uint32_t keys) {
        const unsigned wideRow = SearchLetterRows;
        unsigned row = key < SearchLetters ? key / SearchColumns : wideRow;
        unsigned column = key < SearchLetters ? key % SearchColumns : (key - KeySpace) * 3 + 1;
        if (keys & Bit(Left)) {
            if (row == wideRow) key = KeySpace + (key - KeySpace + 2) % 3;
            else key = row * SearchColumns + (column + SearchColumns - 1) % SearchColumns;
        } else if (keys & Bit(Right)) {
            if (row == wideRow) key = KeySpace + (key - KeySpace + 1) % 3;
            else key = row * SearchColumns + (column + 1) % SearchColumns;
        } else if (keys & (Bit(Up) | Bit(Down))) {
            row = (keys & Bit(Up)) ? (row + wideRow) % (wideRow + 1) : (row + 1) % (wideRow + 1);
            key = row == wideRow ? KeySpace + column / 3 : row * SearchColumns + column;
        }
    }
    void UpdateSearch(Host& host, std::uint32_t keys, std::uint64_t now) {
        const unsigned length = static_cast<unsigned>(std::strlen(search_));
        auto done = [&] { searchOpen_ = false; Sound(host, sound::PaperOut, .45f); };
        if (keys & Bit(RS)) { done(); return; }
        if (keys & Bit(B)) {
            if (!length) { done(); return; }
            char text[sizeof(search_)];
            std::snprintf(text, sizeof(text), "%.*s", static_cast<int>(length - 1), search_);
            SetSearch(host, text); Sound(host, sound::NoteHide, .4f);
            return;
        }
        if (keys & Bit(Y)) { if (length) { SetSearch(host, ""); Sound(host, sound::NoteHide, .5f); } return; }
        if (keys & Directions) {
            const unsigned before = searchCursor_;
            SearchMove(searchCursor_, keys);
            if (searchCursor_ != before) { selectedAt_ = now; Sound(host, sound::Scroll, .5f); }
            return;
        }
        if (!(keys & Bit(A))) return;
        if (searchCursor_ == KeyDone) { done(); return; }
        if (searchCursor_ == KeyDelete) { UpdateSearch(host, Bit(B), now); return; }
        // No leading or doubled spaces; they do not change the match anyway.
        const char typed = searchCursor_ == KeySpace ? ' ' : SearchKeys[searchCursor_];
        if (length >= SearchMax || (typed == ' ' && (!length || search_[length - 1] == ' '))) { Sound(host, sound::ErrorBuzz, .3f); return; }
        char text[sizeof(search_)];
        std::snprintf(text, sizeof(text), "%s%c", search_, typed);
        SetSearch(host, text);
        Sound(host, sound::Plop, .4f);
    }

    // ---------- mods and settings rows ----------
    static Page PageFor(unsigned mod) {
        if (mod == mods::HitCounter) return Page::HitCounter;
        if (mod == mods::TimeMachine) return Page::TimeMachine;
        if (mod == mods::Eid) return Page::Eid;
        if (mod == mods::RepentancePlus) return Page::RepentancePlus;
        return Page::None;
    }
    void UpdateMods(Host& host, std::uint32_t keys, std::uint64_t now) {
        const unsigned count = host.ModCount();
        if (!count) return;
        if (keys & Bit(Up)) Select(modSelection_, (modSelection_ + count - 1) % count, host, now);
        else if (keys & Bit(Down)) Select(modSelection_, (modSelection_ + 1) % count, host, now);
        else if (keys & (Bit(ZL) | Bit(Left))) Select(modSelection_, modSelection_ >= ModRows ? modSelection_ - ModRows : 0, host, now);
        else if (keys & (Bit(ZR) | Bit(Right))) Select(modSelection_, modSelection_ + ModRows < count ? modSelection_ + ModRows : count - 1, host, now);
        else if (keys & Bit(A)) {
            if (!host.ModAvailable(modSelection_)) { Toast("This mod is unavailable in this game build", now); Sound(host, sound::ErrorBuzz, .35f); return; }
            host.ToggleMod(modSelection_);
            stampAt_ = now;
            const bool on = host.ModEnabled(modSelection_);
            Sound(host, on ? sound::NoteAppear : sound::NoteHide, .6f);
            host.PreferencesChanged();
        } else if (keys & Bit(Y)) {
            const auto page = PageFor(modSelection_);
            if (page == Page::None) { Toast("This mod has no extra settings", now); Sound(host, sound::ErrorBuzz, .3f); return; }
            if (!host.ModAvailable(modSelection_)) { Toast("This mod is unavailable in this game build", now); return; }
            page_ = page; rowSelection_ = 0; pageAt_ = now;
            Sound(host, sound::PaperIn, .5f);
        }
    }
    unsigned BuildRows(Host& host, Page page, Row* rows) const {
        static constexpr const char* speeds[] = {"2x", "3x", "4x", "5x", "6x"};
        static constexpr const char* motion[] = {"Full", "Reduced"};
        static constexpr const char* hold[] = {"Off", "0.5 s", "1 s", "2 s"};
        static constexpr const char* tabs[] = {"Last tab", "Items", "Mods", "Run", "Tools", "Menu"};
        const auto& p = host.Preferences();
        unsigned n = 0;
        switch (page) {
        case Page::HitCounter:
            rows[n++] = {"Show counter", RowKind::Toggle, host.ModEnabled(mods::HitCounter) ? 1 : 0, 0, 1, 1, nullptr, "Show or hide the hit counter. It keeps counting while hidden."};
            rows[n++] = {"Position", RowKind::Action, 0, 0, 0, 1, nullptr, "Press A, then move the counter with the D-pad. Hold to go faster.", p.hitX < 0 ? "Top center" : "Custom"};
            rows[n++] = {"Size", RowKind::Slider, p.hitSize, 50, 300, 10, nullptr, "Text size, 50% to 300%."};
            rows[n++] = {"Opacity", RowKind::Slider, p.hitOpacity, 10, 100, 10, nullptr, "How solid the counter looks."};
            rows[n++] = {"Floor hits", RowKind::Toggle, p.hitFloor ? 1 : 0, 0, 1, 1, nullptr, "Also show the hits taken on this floor."};
            rows[n++] = {"Reset to defaults", RowKind::Action, 0, 0, 0, 1, nullptr, "Top center, 100%, 85%, floor hits on.", "A"};
            break;
        case Page::TimeMachine:
            rows[n++] = {"Top speed", RowKind::Choice, p.timeMachineSteps - 1, 0, 4, 1, speeds, "The fastest Time Machine is allowed to go."};
            break;
        case Page::Eid:
            rows[n++] = {"Nearby descriptions", RowKind::Toggle, p.eidNearby ? 1 : 0, 0, 1, 1, nullptr, "Describe items, pickups and rooms near Isaac."};
            rows[n++] = {"Hold Map to browse", RowKind::Toggle, p.eidMap ? 1 : 0, 0, 1, 1, nullptr, "Hold Map to look through your character and items."};
            break;
        case Page::RepentancePlus:
            rows[n++] = {"??? and Lamb", RowKind::Toggle, p.rplus & 1 ? 1 : 0, 0, 1, 1, nullptr, "A Void portal always appears after ??? and The Lamb, instead of 1 time in 5."};
            rows[n++] = {"Mega Satan", RowKind::Toggle, p.rplus & 2 ? 1 : 0, 0, 1, 1, nullptr, "Mega Satan always leaves a Void portal and a big chest; the chest still ends the run. Off: half the time the run just ends."};
            rows[n++] = {"Mother", RowKind::Toggle, p.rplus & 4 ? 1 : 0, 0, 1, 1, nullptr, "A Void portal also appears after Mother, once you have seen her ending."};
            rows[n++] = {"Boss Rush waves", RowKind::Toggle, p.rplus & 8 ? 1 : 0, 0, 1, 1, nullptr, "Show which Boss Rush wave you are on."};
            break;
        case Page::Menu:
            rows[n++] = {"Sounds", RowKind::Toggle, p.sounds ? 1 : 0, 0, 1, 1, nullptr, "Page, cursor and button sounds in this menu."};
            rows[n++] = {"Motion", RowKind::Choice, p.reduceMotion ? 1 : 0, 0, 1, 1, motion, "Reduced turns off sliding, bobbing and popping."};
            rows[n++] = {"Shortcut hold", RowKind::Choice, p.shortcut, 0, 3, 1, hold, "Hold both stick buttons and D-pad Down this long in a cleared room to open the menu."};
            rows[n++] = {"Pause menu Y", RowKind::Toggle, p.pausePrompt ? 1 : 0, 0, 1, 1, nullptr, "Show \"Y: Mods\" on the pause screen. Y works either way."};
            rows[n++] = {"Open on", RowKind::Choice, p.openTab, 0, 5, 1, tabs, "Which page opens first."};
            rows[n++] = {"Background", RowKind::Slider, p.dim, 20, 90, 10, nullptr, "How dark the game looks behind the menu."};
            rows[n++] = {"Reset menu settings", RowKind::Action, 0, 0, 0, 1, nullptr, "Put these menu settings back to normal. Favorites are kept.", "A"};
            break;
        default: break;
        }
        return n;
    }
    void ApplyRow(Host& host, Page page, unsigned row, int delta, bool activate, std::uint64_t now) {
        auto& p = host.Preferences();
        Row rows[8];
        const unsigned n = BuildRows(host, page, rows);
        if (row >= n) return;
        const auto& r = rows[row];
        int value = r.value;
        if (r.kind == RowKind::Toggle) { if (!delta && !activate) return; value = value ? 0 : 1; }
        else if (r.kind == RowKind::Choice) {
            if (activate) delta = 1;
            if (!delta) return;
            value += delta;
            if (value > r.max) value = r.min;
            if (value < r.min) value = r.max;
        } else if (r.kind == RowKind::Slider) {
            if (activate) delta = value >= r.max ? -(r.max - r.min) / r.step : 1;
            if (!delta) return;
            value += delta * r.step;
            if (value > r.max) value = r.max;
            if (value < r.min) value = r.min;
            if (value == r.value) return;
        } else if (!activate) return;
        switch (page) {
        case Page::HitCounter:
            if (row == 0) host.ToggleMod(mods::HitCounter);
            else if (row == 1) { moving_ = true; moveHeld_ = 0; moveLast_ = 0; moveInput_.Reset(); Sound(host, sound::PaperOut, .4f); return; }
            else if (row == 2) p.hitSize = static_cast<std::uint16_t>(value);
            else if (row == 3) p.hitOpacity = static_cast<std::uint8_t>(value);
            else if (row == 4) p.hitFloor = value != 0;
            else if (row == 5) { p.hitX = p.hitY = -1; p.hitSize = 100; p.hitOpacity = 85; p.hitFloor = true; Toast("Hit counter reset", now); }
            break;
        case Page::TimeMachine: p.timeMachineSteps = static_cast<std::uint8_t>(value + 1); break;
        case Page::Eid: if (row == 0) p.eidNearby = value != 0; else p.eidMap = value != 0; break;
        case Page::RepentancePlus:
            if (row < 4) p.rplus = static_cast<std::uint8_t>(value ? p.rplus | (1u << row) : p.rplus & ~(1u << row));
            break;
        case Page::Menu:
            if (row == 0) p.sounds = value != 0;
            else if (row == 1) p.reduceMotion = value != 0;
            else if (row == 2) p.shortcut = static_cast<std::uint8_t>(value);
            else if (row == 3) p.pausePrompt = value != 0;
            else if (row == 4) p.openTab = static_cast<std::uint8_t>(value);
            else if (row == 5) p.dim = static_cast<std::uint8_t>(value);
            else if (row == 6) { p.sounds = true; p.reduceMotion = false; p.pausePrompt = true; p.shortcut = 3; p.openTab = 0; p.dim = 60; Toast("Menu settings reset", now); }
            break;
        default: break;
        }
        p.Clamp();
        stampAt_ = now;
        host.PreferencesChanged();
        Sound(host, r.kind == RowKind::Toggle ? (value ? sound::NoteAppear : sound::NoteHide) : sound::Scroll, .55f);
    }
    void UpdateRows(Host& host, std::uint32_t keys, std::uint64_t now, bool modPage) {
        Row rows[8];
        const unsigned n = BuildRows(host, page_, rows);
        if (!n) return;
        if (modPage && (keys & Bit(B))) { page_ = Page::None; Sound(host, sound::PaperOut, .5f); return; }
        if (keys & Bit(Up)) Select(rowSelection_, (rowSelection_ + n - 1) % n, host, now);
        else if (keys & Bit(Down)) Select(rowSelection_, (rowSelection_ + 1) % n, host, now);
        else if (keys & Bit(Left)) ApplyRow(host, page_, rowSelection_, -1, false, now);
        else if (keys & Bit(Right)) ApplyRow(host, page_, rowSelection_, 1, false, now);
        else if (keys & Bit(A)) ApplyRow(host, page_, rowSelection_, 0, true, now);
    }
    void UpdateMove(Host& host, std::uint32_t buttons, std::uint32_t keys, std::uint64_t now) {
        (void)keys;
        const auto edges = moveInput_.Poll(buttons, now);
        if (edges & (Bit(A) | Bit(B) | Bit(Plus))) {
            moving_ = false; Toast("Position saved", now); Sound(host, sound::PaperIn, .4f);
            host.PreferencesChanged();
            return;
        }
        if (!moveInput_.Released()) return;
        const auto held = buttons & Directions;
        if (held != moveHeld_) { moveHeld_ = held; moveSince_ = now; }
        std::uint64_t elapsed = moveLast_ && now > moveLast_ ? now - moveLast_ : 0;
        if (elapsed > 50) elapsed = 50;
        moveLast_ = now;
        if (!held) return;
        const auto heldFor = now - moveSince_;
        float step = heldFor == 0 ? 1.f : heldFor < 350 ? 0.f : (heldFor < 1100 ? 45.f : 160.f) * static_cast<float>(elapsed) / 1000.f;
        if (step <= 0) return;
        auto& p = host.Preferences();
        float w{}, h{};
        host.HitCounterSize(w, h);
        float x = p.hitX < 0 ? screenW_ * .5f - w * .5f : p.hitX, y = p.hitY < 0 ? 4.f : p.hitY;
        if (held & Bit(Left)) x -= step;
        if (held & Bit(Right)) x += step;
        if (held & Bit(Up)) y -= step;
        if (held & Bit(Down)) y += step;
        const float maxX = screenW_ - w > 0 ? screenW_ - w : 0, maxY = screenH_ - h > 0 ? screenH_ - h : 0;
        x = x < 0 ? 0 : x > maxX ? maxX : x;
        y = y < 0 ? 0 : y > maxY ? maxY : y;
        p.hitX = static_cast<std::int16_t>(x + .5f);
        p.hitY = static_cast<std::int16_t>(y + .5f);
    }

    // ---------- run and tools ----------
    void UpdateRun(Host& host, std::uint32_t keys) {
        RunInfo info;
        if (!host.Run(info)) return;
        const unsigned pages = info.historyCount ? (info.historyCount + 17) / 18 : 1;
        if (keys & (Bit(Down) | Bit(ZR) | Bit(Right))) historyPage_ = (historyPage_ + 1) % pages;
        else if (keys & (Bit(Up) | Bit(ZL) | Bit(Left))) historyPage_ = (historyPage_ + pages - 1) % pages;
        else return;
        Sound(host, sound::Scroll, .5f);
    }
    void UpdateTools(Host& host, std::uint32_t keys, std::uint64_t now) {
        if (keys & Bit(Up)) Select(toolSelection_, (toolSelection_ + ToolCount - 1) % ToolCount, host, now);
        else if (keys & Bit(Down)) Select(toolSelection_, (toolSelection_ + 1) % ToolCount, host, now);
        else if (keys & Bit(A)) {
            if (origin_ == Origin::Title || !host.InRun()) { Toast("Start a run to use tools", now); Sound(host, sound::ErrorBuzz, .35f); return; }
            const auto result = host.UseTool(static_cast<Tool>(toolSelection_));
            char text[96];
            if (result == Result::Done) std::snprintf(text, sizeof(text), "Done: %s", ToolNames[toolSelection_]);
            else std::snprintf(text, sizeof(text), "%s", result == Result::NotNow ? "Not possible right now." : "That did not work.");
            Toast(text, now);
            stampAt_ = now;
            Sound(host, result == Result::Done ? sound::ThumbsUp : sound::ErrorBuzz, .35f);
        }
    }

    // ---------- painting ----------
    static void Cursor(PaintList& out, float x, float y, float alpha, float bob) {
        out.Art(art::ModsCursor.name, 0, x + bob, y, 1, alpha);
    }
    void PaintTabs(Host& host, PaintList& out, float ox, float oy, float alpha, std::uint64_t now) {
        float widths[TabCount];
        for (unsigned i = 0; i < TabCount; ++i) widths[i] = host.TextWidth(Font::Ink, TabNames[i], .62f);
        float total = 0;
        for (float w : widths) total += w + 16;
        float x = ox + 240 - total * .5f + 8;
        out.Art(art::Button.name, static_cast<int>(art::Glyph::L), x - 40, oy + 2, 1, alpha);
        for (unsigned i = 0; i < TabCount; ++i) {
            const auto tab = static_cast<Tab>(i);
            const bool selected = tab == tab_;
            const float a = !TabUsable(tab) ? .22f : selected ? 1.f : .5f;
            if (selected) Cursor(out, x - 7, oy + 10, alpha, Bob(now));
            out.Text(Font::Ink, TabNames[i], x, oy + 3, Paper, .62f, a * alpha);
            x += widths[i] + 16;
        }
        out.Art(art::Button.name, static_cast<int>(art::Glyph::R), x - 6, oy + 2, 1, alpha);
    }
    // Button prompts along the bottom right, laid out by their real width. If
    // they do not all fit, the least important ones are left out.
    void PaintPrompts(Host& host, PaintList& out, float width, float height, float alpha) {
        struct P { art::Glyph glyph; const char* label; unsigned priority; };  // 0 = always shown
        P prompts[8];
        unsigned n = 0;
        if (moving_) return;
        if (popupOpen_) { prompts[n++] = {art::Glyph::A, "Choose", 0}; prompts[n++] = {art::Glyph::B, "Cancel", 0}; }
        else if (searchOpen_) {
            prompts[n++] = {art::Glyph::A, "Type", 0}; prompts[n++] = {art::Glyph::B, "Delete", 1};
            prompts[n++] = {art::Glyph::Y, "Clear", 2}; prompts[n++] = {art::Glyph::RStick, "Done", 0};
        }
        else if (filterOpen_) { prompts[n++] = {art::Glyph::DLeft, "Change", 0}; prompts[n++] = {art::Glyph::B, "Done", 0}; }
        else if (tab_ == Tab::Items) {
            prompts[n++] = {art::Glyph::A, "Use", 0}; prompts[n++] = {art::Glyph::Y, "Favorite", 2};
            prompts[n++] = {art::Glyph::X, "Filter", 1}; prompts[n++] = {art::Glyph::RStick, "Search", 1};
            if (descPages_ > 1) prompts[n++] = {art::Glyph::LStick, "More text", 3};
            prompts[n++] = {art::Glyph::ZR, "Page", 4}; prompts[n++] = {art::Glyph::B, "Close", 0};
        } else if (tab_ == Tab::Mods && page_ != Page::None) {
            prompts[n++] = {art::Glyph::DLeft, "Change", 0}; prompts[n++] = {art::Glyph::A, "Select", 0}; prompts[n++] = {art::Glyph::B, "Back", 0};
        } else if (tab_ == Tab::Mods) {
            prompts[n++] = {art::Glyph::A, "On/Off", 0}; prompts[n++] = {art::Glyph::Y, "Settings", 1}; prompts[n++] = {art::Glyph::B, "Close", 0};
        } else if (tab_ == Tab::Run) { prompts[n++] = {art::Glyph::DDown, "More items", 1}; prompts[n++] = {art::Glyph::B, "Close", 0}; }
        else if (tab_ == Tab::Tools) { prompts[n++] = {art::Glyph::A, "Use", 0}; prompts[n++] = {art::Glyph::B, "Close", 0}; }
        else { prompts[n++] = {art::Glyph::DLeft, "Change", 0}; prompts[n++] = {art::Glyph::A, "Select", 0}; prompts[n++] = {art::Glyph::B, "Close", 0}; }
        float widths[8]{};
        auto total = [&] {
            float sum = 0;
            for (unsigned i = 0; i < n; ++i) sum += widths[i] + 36;
            return sum;
        };
        for (unsigned i = 0; i < n; ++i) widths[i] = host.TextWidth(Font::Hud, prompts[i].label, 1);
        while (n > 1 && total() > width - 16) {
            unsigned drop = n;
            for (unsigned i = 0; i < n; ++i)
                if (prompts[i].priority && (drop == n || prompts[i].priority >= prompts[drop].priority)) drop = i;
            if (drop == n) break;
            for (unsigned i = drop + 1; i < n; ++i) { prompts[i - 1] = prompts[i]; widths[i - 1] = widths[i]; }
            --n;
        }
        float x = width - 8;
        const float y = height - 15;
        for (unsigned i = n; i-- > 0;) {
            x -= widths[i];
            out.Text(Font::Hud, prompts[i].label, x, y + 3, Paper, 1, alpha);
            x -= 26;
            out.Art(art::Button.name, static_cast<int>(prompts[i].glyph), x - 4, y, 1, alpha);
            x -= 10;
        }
    }
    // A short dark strip above the prompts; long messages wrap onto two lines.
    void PaintToast(Host& host, PaintList& out, float width, float height, std::uint64_t now) {
        if (!toast_[0] || now - toastAt_ > ToastMs) return;
        const auto age = now - toastAt_;
        const float in = anim::EaseOutCubic(anim::Progress(now, toastAt_, Duration(150)));
        const float fade = age > ToastMs - 300 ? static_cast<float>(ToastMs - age) / 300.f : 1.f;
        constexpr float MaxLine = 300;
        char lines[2][sizeof(toast_)]{};
        std::snprintf(lines[0], sizeof(lines[0]), "%s", toast_);
        unsigned count = 1;
        if (host.TextWidth(Font::Hud, toast_, 1) > MaxLine) {
            // Break at the last space that keeps the first line within MaxLine.
            const unsigned length = static_cast<unsigned>(std::strlen(toast_));
            unsigned split = 0;
            for (unsigned i = 1; i < length; ++i) {
                if (toast_[i] != ' ') continue;
                char head[sizeof(toast_)];
                std::snprintf(head, sizeof(head), "%.*s", static_cast<int>(i), toast_);
                if (host.TextWidth(Font::Hud, head, 1) <= MaxLine) split = i; else break;
            }
            if (split) {
                std::snprintf(lines[0], sizeof(lines[0]), "%.*s", static_cast<int>(split), toast_);
                std::snprintf(lines[1], sizeof(lines[1]), "%s", toast_ + split + 1);
                count = 2;
            }
        }
        float w = 0;
        for (unsigned i = 0; i < count; ++i) {
            const float lw = host.TextWidth(Font::Hud, lines[i], 1);
            if (lw > w) w = lw;
        }
        if (w > MaxLine) w = MaxLine;
        w += 20;
        const float lineHeight = 12, y = height - 40 - lineHeight * static_cast<float>(count - 1) + (1 - in) * 12;
        out.Rect(width * .5f - w * .5f, y - 3, width * .5f + w * .5f, y + lineHeight * static_cast<float>(count), 0x140d0c, .82f * fade);
        for (unsigned i = 0; i < count; ++i)
            out.Text(Font::Hud, lines[i], width * .5f, y + lineHeight * static_cast<float>(i), Paper, 1, fade, Align::Center, MaxLine);
    }
    // A just-changed value on the selected row "stamps" in from 135%. room is
    // the width left beside the label (0 = no limit).
    void PaintRowValue(Host& host, PaintList& out, const Row& r, float right, float y, float alpha, std::uint64_t now, bool animate,
                       float room = 0) {
        const float stamp = animate ? 1 + .35f * (1 - anim::EaseOutCubic(anim::Progress(now, stampAt_, Duration(140)))) : 1.f;
        switch (r.kind) {
        case RowKind::Toggle:
            out.Text(Font::Ink, r.value ? "ON" : "OFF", right, y, r.value ? InkGood : InkFaint, .62f * stamp, alpha, Align::Right);
            break;
        case RowKind::Choice: {
            // Drop the arrows before shortening the value itself.
            const char* value = r.choices ? r.choices[r.value] : "";
            char text[48];
            std::snprintf(text, sizeof(text), "< %s >", value);
            if (room > 0 && host.TextWidth(Font::Ink, text, .58f) > room) std::snprintf(text, sizeof(text), "%s", value);
            out.Text(Font::Ink, text, right, y, Ink, .58f, alpha, Align::Right, room);
            break;
        }
        case RowKind::Slider: {
            const int frame = r.max > r.min ? (r.value - r.min) * 10 / (r.max - r.min) : 0;
            out.Art(art::Slider.name, frame, right - 48, y, .75f, alpha);
            out.Format(Font::Small, right - 52, y + 2, Ink, 1, alpha, Align::Right, 30, "%d%%", r.value);
            break;
        }
        case RowKind::Action:
            if (r.actionText) out.Text(Font::Ink, r.actionText, right, y, InkFaint, .55f, alpha, Align::Right, room);
            break;
        }
    }
    // Mods-style paper: header, body strips, torn bottom.
    static void ModsPaper(PaintList& out, float x, float y, unsigned pieces, float alpha) {
        out.Art(art::ModsHeader.name, 0, x, y, 1, alpha);
        for (unsigned i = 0; i < pieces; ++i) out.Art(art::ModsPiece.name, static_cast<int>(i % 5), x, y + 48 + 32.f * i, 1, alpha);
        out.Art(art::ModsBottom.name, 0, x, y + 48 + 32.f * pieces, 1, alpha);
    }

    void PaintMods(Frame& f) {
        auto& out = f.out;
        auto& host = f.host;
        const float lx = f.x + 10, ly = f.y + 20 + f.riseLeft;
        ModsPaper(out, lx, ly, 4, f.appear);
        const unsigned count = host.ModCount();
        const unsigned first = modSelection_ >= ModRows ? modSelection_ - ModRows + 1 : 0;
        cursorY_ = anim::Approach(cursorY_, static_cast<float>(modSelection_ - first), f.elapsed, reduceMotion_ ? 0 : 35);
        for (unsigned i = first; i < count && i < first + ModRows; ++i) {
            const float ry = ly + 50 + 15.f * (i - first);
            const bool available = host.ModAvailable(i), selected = i == modSelection_;
            if (selected) out.Rect(lx + 30, ry - 2, lx + 230, ry + 12, Ink, .07f * f.alpha);
            out.Text(Font::Ink, host.ModName(i), lx + 36, ry, available ? (selected ? InkStrong : Ink) : InkDisabled, .58f, f.alpha, Align::Left, 150);
            Row toggle{nullptr, RowKind::Toggle, host.ModEnabled(i) ? 1 : 0};
            if (!available) out.Text(Font::Ink, "--", lx + 226, ry, InkDisabled, .58f, f.alpha, Align::Right);
            else PaintRowValue(host, out, toggle, lx + 226, ry, f.alpha, f.now, selected);
        }
        Cursor(out, lx + 26, ly + 55 + 15.f * cursorY_, f.alpha, Bob(f.now));
        if (count > ModRows) out.Format(Font::Small, lx + 128, ly + 206, InkFaint, 1, f.alpha, Align::Center, 120, "%u / %u", modSelection_ + 1, count);
        // Right page: description and status, or the settings rows.
        const float rx = f.x + 262, ry = f.y + 40 + f.riseRight;
        out.Art(art::StatsBlank.name, 0, rx, ry, 1, f.appear);
        if (page_ != Page::None) { PaintRows(f, page_, rx, ry, true); return; }
        out.Text(Font::Ink, host.ModName(modSelection_), rx + 20, ry + 16, InkStrong, .62f, f.alpha, Align::Left, 170);
        out.Description(0, "", host.ModDescription(modSelection_), rx + 20, ry + 34, 170, 92);
        char status[96]{};
        host.ModStatus(modSelection_, status, sizeof(status));
        if (status[0]) out.Text(Font::Small, status, rx + 20, ry + 128, InkGood, 1, f.alpha, Align::Left, 170);
        if (PageFor(modSelection_) != Page::None) {
            out.Art(art::Button.name, static_cast<int>(art::Glyph::Y), rx + 14, ry + 142, 1, f.alpha);
            out.Text(Font::Ink, "Settings", rx + 44, ry + 145, Ink, .55f, f.alpha);
        }
    }
    void PaintRows(Frame& f, Page page, float x, float y, bool titled) {
        auto& out = f.out;
        Row rows[8];
        const unsigned n = BuildRows(f.host, page, rows);
        if (titled) {
            const char* titles[] = {"", "Hit Counter", "Time Machine", "Item Descriptions", "Menu", "Repentance+ Rules"};
            out.Text(Font::Ink, titles[static_cast<unsigned>(page)], x + 20, y + 14, InkStrong, .62f, f.alpha);
        }
        const float top = y + (titled ? 34 : 0);
        rowCursor_ = anim::Approach(rowCursor_, static_cast<float>(rowSelection_), f.elapsed, reduceMotion_ ? 0 : 35);
        for (unsigned i = 0; i < n; ++i) {
            const float ry = top + 16.f * i;
            const bool selected = i == rowSelection_;
            if (selected) out.Rect(x + 16, ry - 2, x + 186, ry + 12, Ink, .07f * f.alpha);
            // The label keeps up to 92 px (76 beside a slider); the value gets the rest.
            const float labelRoom = rows[i].kind == RowKind::Slider ? 76.f : 92.f;
            float label = f.host.TextWidth(Font::Ink, rows[i].label, .55f);
            if (label > labelRoom) label = labelRoom;
            out.Text(Font::Ink, rows[i].label, x + 22, ry, selected ? InkStrong : Ink, .55f, f.alpha, Align::Left, labelRoom);
            PaintRowValue(f.host, out, rows[i], x + 182, ry, f.alpha, f.now, selected, 182 - 22 - label - 8);
        }
        if (n) Cursor(out, x + 14, top + 5 + 16.f * rowCursor_, f.alpha, Bob(f.now));
        if (rowSelection_ < n && rows[rowSelection_].help)
            out.Description(0, "", rows[rowSelection_].help, x + 20, top + 16.f * n + 6, 170, 176 - (top - y) - 16.f * n - 14);
    }

    void PaintItems(Frame& f) {
        auto& out = f.out;
        auto& host = f.host;
        const float px = f.x + 45, py = f.y + 12 + f.riseLeft;
        out.Art(art::Collection.name, 0, px, py, 1, f.appear);
        char summary[128], quality[16];
        if (quality_ < 0) std::snprintf(quality, sizeof(quality), "any quality");
        else std::snprintf(quality, sizeof(quality), "quality %d", quality_);
        if (category_ == Category::Collectible)
            std::snprintf(summary, sizeof(summary), "%s - %s - %s - %s", CategoryNames[0], TypeNames[type_], quality,
                          pool_ < 0 ? "any pool" : cat::PoolNames[pool_]);
        else std::snprintf(summary, sizeof(summary), "%s", CategoryNames[static_cast<unsigned>(category_)]);
        if (search_[0]) {
            char filters[sizeof(summary)];
            std::snprintf(filters, sizeof(filters), "%s", summary);
            std::snprintf(summary, sizeof(summary), "\"%s\" - %s", search_, filters);
        }
        out.Text(Font::Small, summary, px + 30, py + 50, InkFaint, 1, f.alpha, Align::Left, 250);
        const unsigned per = PerPage(), page = visibleCount_ ? selection_ / per : 0, pages = visibleCount_ ? (visibleCount_ + per - 1) / per : 1;
        out.Format(Font::Small, px + 356, py + 50, InkFaint, 1, f.alpha, Align::Right, 120, "%s%s  %u/%u",
                   show_ ? ShowNames[show_] : "", show_ ? " -" : "", page + 1, pages);
        if (!visibleCount_) {
            char empty[64];
            if (search_[0]) std::snprintf(empty, sizeof(empty), "Nothing matches \"%s\".", search_);
            else std::snprintf(empty, sizeof(empty), "%s", show_ == 1 ? "No favorites yet. Press Y on an item." : "Nothing matches these filters.");
            out.Text(Font::Ink, empty, px + 195, py + 100, InkFaint, .6f, f.alpha, Align::Center, 330);
        } else if (GridView()) {
            for (unsigned slot = 0; slot < per && page * per + slot < visibleCount_; ++slot) {
                const unsigned index = page * per + slot;
                const auto& v = visible_[index];
                const float cx = px + 50 + 38.f * (slot % GridColumns), cy = py + 66 + 38.f * (slot / GridColumns);
                const bool selected = index == selection_;
                const float scale = selected ? Pop(f.now) : 1.f, lift = selected ? Float(f.now) : 0.f;
                out.Item(v.entry.id, category_ == Category::Trinket, cx - 16 * scale + 16, cy - 16 * scale + 16 + lift, scale, f.alpha);
                if (host.Preferences().IsFavorite(v.entry)) out.Art(art::Bullet.name, 1, cx + 30, cy + 2, 1, f.alpha);
            }
            const unsigned slot = selection_ - page * per;
            gridX_ = anim::Approach(gridX_, static_cast<float>(slot % GridColumns), f.elapsed, reduceMotion_ ? 0 : 30);
            gridY_ = anim::Approach(gridY_, static_cast<float>(slot / GridColumns), f.elapsed, reduceMotion_ ? 0 : 30);
            Cursor(out, px + 44 + 38.f * gridX_, py + 82 + 38.f * gridY_, f.alpha, Bob(f.now));
        } else {
            for (unsigned slot = 0; slot < per && page * per + slot < visibleCount_; ++slot) {
                const unsigned index = page * per + slot;
                const auto& v = visible_[index];
                const float cx = px + 44 + 170.f * (slot / ListRows), cy = py + 64 + 20.f * (slot % ListRows);
                const bool selected = index == selection_;
                if (selected) out.Rect(cx - 4, cy - 3, cx + 160, cy + 14, Ink, .07f * f.alpha);
                if (v.icon) out.Icon(v.icon, cx, cy, f.alpha);
                out.Text(Font::Ink, v.name, cx + 16, cy, selected ? InkStrong : Ink, .55f, f.alpha, Align::Left, 138);
                if (host.Preferences().IsFavorite(v.entry)) out.Art(art::Bullet.name, 1, cx + 156, cy + 5, 1, f.alpha);
            }
            const unsigned slot = selection_ - page * per;
            gridY_ = anim::Approach(gridY_, static_cast<float>(slot % ListRows), f.elapsed, reduceMotion_ ? 0 : 30);
            gridX_ = anim::Approach(gridX_, static_cast<float>(slot / ListRows), f.elapsed, reduceMotion_ ? 0 : 30);
            Cursor(out, px + 32 + 170.f * gridX_, py + 69 + 20.f * gridY_, f.alpha, Bob(f.now));
        }
        if (pages > 1) {
            out.Art(art::PageArrow.name, 0, px + 382, py + 112, 1, f.alpha);
            out.Art(art::PageArrow.name, 1, px + 12, py + 112, 1, f.alpha);
        }
        // The note under the page: name, quality, kind and description.
        const float nx = f.x + 63, ny = f.y + 192 + f.riseRight;
        out.Art(art::ItemNote.name, 0, nx, ny, 1, f.appear);
        if (visibleCount_) {
            const auto& v = visible_[selection_];
            if (category_ == Category::Collectible || category_ == Category::Trinket)
                out.Item(v.entry.id, category_ == Category::Trinket, nx + 16, ny + 16 + Float(f.now), 1, f.alpha);
            else if (v.icon) out.Icon(v.icon, nx + 24, ny + 26, f.alpha);
            out.Text(Font::Ink, v.name, nx + 56, ny + 5, InkStrong, .62f, f.alpha, Align::Left, 200);
            if (category_ == Category::Collectible) {
                char token[12];
                std::snprintf(token, sizeof(token), "Quality%u", v.quality);
                out.Icon(token, nx + 268, ny + 8, f.alpha);
                static constexpr const char* kinds[] = {"", "Passive", "Familiar", "Active"};
                out.Text(Font::Small, kinds[v.kind < 4 ? v.kind : 0], nx + 282, ny + 9, InkFaint, 1, f.alpha);
            }
            PaintNoteText(f, v.entry, nx, ny);
        } else descPages_ = 0;
        if (filterOpen_) PaintFilter(f);
        if (searchOpen_) PaintSearch(f);
        if (popupOpen_) PaintPopup(f, px, py);
    }
    // The note fits three lines above its torn bottom edge. Longer text turns
    // pages by itself every few seconds, LS turns them by hand, and a marker in
    // the right margin shows there is more.
    void PaintNoteText(Frame& f, const Entry& entry, float nx, float ny) {
        constexpr float Width = 262;
        constexpr unsigned Rows = 3, PageMs = 5000;
        const char* markup = f.host.Describe(entry);
        if (!markup) { descPages_ = 0; return; }
        if (!(entry == descEntry_)) { descEntry_ = entry; descShownAt_ = f.now; descPage_ = 0; descManual_ = false; }
        const unsigned lines = f.host.DescriptionLines(markup, Width);
        descPages_ = lines ? (lines + Rows - 1) / Rows : 0;
        int page = -1;  // line count unknown: the backend turns pages
        if (descPages_) {
            if (!descManual_) descPage_ = static_cast<unsigned>((f.now - descShownAt_) / PageMs) % descPages_;
            page = static_cast<int>(descPage_ % descPages_);
        }
        f.out.Description(0, "", markup, nx + 56, ny + 19, Width, 11.f * Rows + 2, page);
        if (descPages_ > 1)
            f.out.Format(Font::Small, nx + 338, ny + 41, InkFaint, 1, f.alpha, Align::Right, 18, "%d/%u", page + 1, descPages_);
    }
    void PaintFilter(Frame& f) {
        auto& out = f.out;
        const float x = f.x + 136, y = f.y + 46;
        out.Rect(0, 0, 2000, 2000, 0x000000, .25f * f.alpha);
        out.Art(art::StatsBlank.name, 0, x, y, 1, f.appear);
        out.Text(Font::Ink, "Filter", x + 20, y + 14, InkStrong, .62f, f.alpha);
        const char* labels[FilterRows] = {"Search", "Show", "Type", "Quality", "Pool", "Only", "Sort"};
        char values[FilterRows][40];
        std::snprintf(values[0], 40, "%s", search_[0] ? search_ : "None");
        std::snprintf(values[1], 40, "%s", CategoryNames[static_cast<unsigned>(category_)]);
        std::snprintf(values[2], 40, "%s", TypeNames[type_]);
        if (quality_ < 0) std::snprintf(values[3], 40, "Any"); else std::snprintf(values[3], 40, "%d", quality_);
        std::snprintf(values[4], 40, "%s", pool_ < 0 ? "Any" : cat::PoolNames[pool_]);
        std::snprintf(values[5], 40, "%s", ShowNames[show_]);
        std::snprintf(values[6], 40, "%s", SortNames[sort_]);
        for (unsigned i = 0; i < FilterRows; ++i) {
            const float ry = y + 36 + 17.f * i;
            const bool active = FilterRowActive(i), selected = i == filterRow_;
            if (selected) out.Rect(x + 16, ry - 2, x + 196, ry + 12, Ink, .07f * f.alpha);
            out.Text(Font::Ink, labels[i], x + 22, ry, active ? Ink : InkDisabled, .55f, f.alpha);
            char text[48];
            if (i == 0) std::snprintf(text, sizeof(text), "%.39s", values[i]);  // A opens the letter board
            else std::snprintf(text, sizeof(text), "< %.39s >", values[i]);
            out.Text(Font::Ink, text, x + 192, ry, active ? (selected ? InkStrong : Ink) : InkDisabled, .52f, f.alpha, Align::Right, 120);
        }
        Cursor(out, x + 14, y + 41 + 17.f * filterRow_, f.alpha, Bob(f.now));
        out.Format(Font::Small, x + 104, y + 156, InkFaint, 1, f.alpha, Align::Center, 170, "%u match%s", visibleCount_, visibleCount_ == 1 ? "" : "es");
    }
    void PaintSearch(Frame& f) {
        auto& out = f.out;
        const float x = f.x + 136, y = f.y + 46;
        out.Rect(0, 0, 2000, 2000, 0x000000, .25f * f.alpha);
        out.Art(art::StatsBlank.name, 0, x, y, 1, f.appear);
        out.Text(Font::Ink, "Search", x + 20, y + 14, InkStrong, .62f, f.alpha);
        // What has been typed so far, on a ruled line, with a blinking caret.
        out.Rect(x + 20, y + 47, x + 188, y + 48, Ink, .5f * f.alpha);
        char typed[sizeof(search_) + 2];
        const bool caret = reduceMotion_ || (f.now / 500) % 2 == 0;
        std::snprintf(typed, sizeof(typed), "%s%s", search_, caret ? "_" : "");
        if (search_[0] || caret) out.Text(Font::Ink, typed, x + 22, y + 33, InkStrong, .6f, f.alpha, Align::Left, 164);
        else out.Text(Font::Ink, "Type a name or ID", x + 22, y + 33, InkFaint, .55f, f.alpha, Align::Left, 164);
        for (unsigned key = 0; key < SearchLetters; ++key) {
            const float kx = x + 22 + 19.f * (key % SearchColumns), ky = y + 56 + 16.f * (key / SearchColumns);
            const bool selected = key == searchCursor_;
            if (selected) out.Rect(kx - 3, ky - 2, kx + 13, ky + 12, Ink, .14f * f.alpha);
            const char label[2] = {SearchKeys[key], 0};
            out.Text(Font::Ink, label, kx + 5, ky, selected ? InkStrong : Ink, selected ? .62f : .55f, f.alpha, Align::Center);
        }
        static constexpr const char* wide[] = {"Space", "Delete", "Done"};
        for (unsigned i = 0; i < 3; ++i) {
            const float kx = x + 22 + 57.f * i, ky = y + 56 + 16.f * SearchLetterRows + 2;
            const bool selected = KeySpace + i == searchCursor_;
            out.Rect(kx - 3, ky - 2, kx + 51, ky + 12, Ink, (selected ? .14f : .05f) * f.alpha);
            out.Text(Font::Ink, wide[i], kx + 24, ky, selected ? InkStrong : Ink, .52f, f.alpha, Align::Center, 52);
        }
        out.Format(Font::Small, x + 104, y + 156, InkFaint, 1, f.alpha, Align::Center, 170, "%u match%s", visibleCount_, visibleCount_ == 1 ? "" : "es");
    }
    void PaintPopup(Frame& f, float px, float py) {
        auto& out = f.out;
        Action actions[4];
        const unsigned n = PopupActions(actions);
        const float grow = anim::EaseOutBack(anim::Progress(f.now, popupAt_, Duration(160)));
        const float x = px + 120, y = py + 60 + (1 - grow) * 12;
        out.Rect(x, y, x + 150, y + 24 + 15.f * (n + 1), 0xe9dfd0, .97f * f.alpha);
        out.Rect(x, y, x + 150, y + 1, Ink, .6f * f.alpha);
        out.Rect(x, y + 23 + 15.f * (n + 1), x + 150, y + 24 + 15.f * (n + 1), Ink, .6f * f.alpha);
        out.Text(Font::Ink, visibleCount_ ? visible_[selection_].name : "", x + 10, y + 5, InkStrong, .55f, f.alpha, Align::Left, 132);
        for (unsigned i = 0; i <= n; ++i) {
            const float ry = y + 22 + 15.f * i;
            out.Text(Font::Ink, i < n ? ActionLabel(actions[i], category_) : "Cancel", x + 22, ry, i == popupRow_ ? InkStrong : Ink, .52f, f.alpha);
        }
        Cursor(out, x + 13, y + 27 + 15.f * popupRow_, f.alpha, Bob(f.now));
    }

    void PaintRun(Frame& f) {
        auto& out = f.out;
        RunInfo info;
        const bool ok = f.host.Run(info) && info.valid;
        const float lx = f.x + 22, ly = f.y + 24 + f.riseLeft;
        out.Art(art::StatsTop.name, 0, lx, ly, 1, f.appear);
        const float rx = f.x + 250, ry = f.y + 44 + f.riseRight;
        out.Art(art::StatsBlank.name, 0, rx, ry, 1, f.appear);
        if (!ok) {
            out.Text(Font::Ink, "Start a run to see it here.", lx + 104, ly + 100, InkFaint, .55f, f.alpha, Align::Center);
            return;
        }
        char floor[40];
        float y = ly + 42;
        auto line = [&](const char* label, const char* value) {
            out.Text(Font::Ink, label, lx + 22, y, InkFaint, .5f, f.alpha);
            out.Text(Font::Ink, value, lx + 168, y, Ink, .52f, f.alpha, Align::Right, 96);
            y += 13;
        };
        char value[48];
        line("Character", info.character >= 0 && info.character < 41 ? CharacterNames[info.character] : "?");
        line("Floor", FloorName(info.stage, info.stageType, info.greed, floor, sizeof(floor)));
        line("Seed", info.seed[0] ? info.seed : "-");
        std::snprintf(value, sizeof(value), "%u:%02u", info.frames / 30 / 60, info.frames / 30 % 60); line("Time", value);
        std::snprintf(value, sizeof(value), "%u (floor %u)", info.hits, info.floorHits); line("Hits", value);
        y += 3;
        struct Stat { const char* icon; const char* name; float value; } stats[] = {
            {"Speed", "Speed", info.speed}, {"Tears", "Tears", info.tears}, {"Damage", "Damage", info.damage},
            {"Range", "Range", info.range}, {"Shotspeed", "Shot speed", info.shotSpeed}, {"Luck", "Luck", info.luck}};
        for (const auto& s : stats) {
            out.Icon(s.icon, lx + 22, y + 1, f.alpha);
            out.Text(Font::Ink, s.name, lx + 36, y, InkFaint, .5f, f.alpha);
            std::snprintf(value, sizeof(value), "%.2f", static_cast<double>(s.value));
            out.Text(Font::Ink, value, lx + 168, y, Ink, .52f, f.alpha, Align::Right);
            y += 13;
        }
        // Right page: items picked up this run, newest first.
        out.Text(Font::Ink, "Picked up this run", rx + 20, ry + 14, InkStrong, .58f, f.alpha);
        const unsigned per = 18, pages = info.historyCount ? (info.historyCount + per - 1) / per : 1;
        if (historyPage_ >= pages) historyPage_ = 0;
        if (!info.historyCount) out.Text(Font::Ink, "Nothing yet.", rx + 104, ry + 70, InkFaint, .52f, f.alpha, Align::Center);
        for (unsigned i = 0; i < per && historyPage_ * per + i < info.historyCount; ++i) {
            const auto raw = info.history[historyPage_ * per + i];
            out.Item(raw & 0x7fff, (raw & 0x8000) != 0, rx + 18 + 29.f * (i % 6), ry + 32 + 29.f * (i / 6), .9f, f.alpha);
        }
        if (pages > 1) out.Format(Font::Small, rx + 104, ry + 122, InkFaint, 1, f.alpha, Align::Center, 120, "%u / %u", historyPage_ + 1, pages);
        // Transformation progress counted from the same history.
        unsigned forms[FormCount]{};
        for (unsigned i = 0; i < info.historyCount; ++i) {
            if (info.history[i] & 0x8000) continue;
            for (const auto& c : cat::Collectibles)
                if (c.id == info.history[i]) { for (unsigned b = 0; b < FormCount; ++b) if (c.forms & (1u << b)) ++forms[b]; break; }
        }
        unsigned shown = 0;
        for (unsigned b = 0; b < FormCount && shown < 4; ++b) {
            if (!forms[b]) continue;
            std::snprintf(value, sizeof(value), "%s %u/3", FormNames[b], forms[b] > 3 ? 3 : forms[b]);
            out.Text(Font::Small, value, rx + 20 + 84.f * (shown % 2), ry + 134 + 11.f * (shown / 2),
                     forms[b] >= 3 ? InkGood : Ink, 1, f.alpha, Align::Left, 80);
            ++shown;
        }
    }
    void PaintTools(Frame& f) {
        auto& out = f.out;
        const bool usable = origin_ != Origin::Title && f.host.InRun();
        const float lx = f.x + 22, ly = f.y + 40 + f.riseLeft;
        out.Art(art::StatsBlank.name, 0, lx, ly, 1, f.appear);
        out.Text(Font::InkLarge, "TOOLS", lx + 104, ly + 10, InkStrong, .7f, f.alpha, Align::Center);
        toolCursor_ = anim::Approach(toolCursor_, static_cast<float>(toolSelection_), f.elapsed, reduceMotion_ ? 0 : 35);
        for (unsigned i = 0; i < ToolCount; ++i) {
            const float ry = ly + 34 + 15.f * i;
            const bool selected = i == toolSelection_;
            if (selected) out.Rect(lx + 16, ry - 2, lx + 192, ry + 12, Ink, .07f * f.alpha);
            out.Text(Font::Ink, ToolNames[i], lx + 22, ry, usable ? (selected ? InkStrong : Ink) : InkDisabled, .55f, f.alpha);
        }
        Cursor(out, lx + 14, ly + 39 + 15.f * toolCursor_, f.alpha, Bob(f.now));
        const float rx = f.x + 250, ry = f.y + 40 + f.riseRight;
        out.Art(art::StatsBlank.name, 0, rx, ry, 1, f.appear);
        out.Text(Font::Ink, ToolNames[toolSelection_], rx + 20, ry + 16, InkStrong, .62f, f.alpha, Align::Left, 170);
        out.Description(0, "", ToolHelp[toolSelection_], rx + 20, ry + 36, 170, 80);
        out.Text(Font::Small, usable ? "Applies to Isaac when you close the menu." : "Start a run to use tools.", rx + 20, ry + 140, InkFaint, 1, f.alpha, Align::Left, 170);
    }
    void PaintSettings(Frame& f) {
        auto& out = f.out;
        const float lx = f.x + 18, ly = f.y + 14 + f.riseLeft;
        out.Art(art::OptionsTop.name, 0, lx, ly, 1, f.appear);
        out.Art(art::OptionsBottom.name, 0, lx, ly + 96, 1, f.appear);
        PaintRows(f, Page::Menu, lx + 2, ly + 46, false);
        const float rx = f.x + 258, ry = f.y + 44 + f.riseRight;
        out.Art(art::StatsBlank.name, 0, rx, ry, 1, f.appear);
        out.Text(Font::Ink, "Saved automatically", rx + 20, ry + 16, InkStrong, .55f, f.alpha);
        out.Description(0, "", "Menu, mod and hit counter choices, favorites and recent items are saved with your game and come back next time you play.", rx + 20, ry + 36, 170, 100);
    }
    void PaintMove(Host& host, PaintList& out, float width, float height, std::uint64_t now) {
        screenW_ = width; screenH_ = height;
        out.Rect(0, 0, width, height, 0x000000, .25f);
        out.HitCounter(1, true);
        static constexpr const char* move = "Move (hold to go faster)";
        const float moveW = host.TextWidth(Font::Hud, move, 1), doneW = host.TextWidth(Font::Hud, "Done", 1);
        const float w = 38 + moveW + 12 + 32 + doneW + 10, x = width * .5f - w * .5f;
        const float y = height - 34 + (reduceMotion_ ? 0 : anim::Wave(now, 1400) * 1.f);
        out.Rect(x, y, x + w, y + 22, 0x140d0c, .85f);
        out.Art(art::Button.name, static_cast<int>(art::Glyph::DLeft), x + 6, y + 3, 1, 1);
        out.Text(Font::Hud, move, x + 38, y + 7, Paper, 1, 1);
        out.Art(art::Button.name, static_cast<int>(art::Glyph::A), x + 38 + moveW + 12, y + 3, 1, 1);
        out.Text(Font::Hud, "Done", x + 38 + moveW + 44, y + 7, Paper, 1, 1);
    }

    // ---------- state ----------
    Input input_{}, moveInput_{};
    Origin origin_{};
    Tab tab_{Tab::Mods};
    Page page_{};
    Category category_{};
    std::uint8_t type_{}, show_{}, sort_{};
    std::int8_t quality_{-1}, pool_{-1};
    bool open_{}, closing_{}, filterOpen_{}, popupOpen_{}, moving_{}, reduceMotion_{}, searchOpen_{};
    char search_[24]{}, searchKey_[24]{};  // as typed; normalized for matching
    unsigned searchCursor_{};
    unsigned selection_{}, modSelection_{}, rowSelection_{}, toolSelection_{}, filterRow_{}, popupRow_{}, historyPage_{};
    unsigned visibleCount_{};
    Visible visible_[MaxVisible]{};
    std::uint64_t openedAt_{}, closedAt_{}, tabChangedAt_{}, selectedAt_{}, stampAt_{}, popupAt_{}, pageAt_{}, toastAt_{}, lastPaint_{};
    std::uint64_t moveSince_{}, moveLast_{};
    std::uint32_t moveHeld_{};
    float tabDirection_{1}, cursorY_{}, rowCursor_{}, toolCursor_{}, gridX_{}, gridY_{};
    float screenW_{480}, screenH_{270};
    char toast_[96]{};
    Entry descEntry_{};
    unsigned descPage_{}, descPages_{};
    bool descManual_{};
    std::uint64_t descShownAt_{};
};
}

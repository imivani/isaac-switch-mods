#pragma once
#include "prefs.hpp"
#include <cstdint>

// Everything the menu needs from the game, behind one interface. The Switch
// backend implements it with verified native calls; host tests use a fake.
namespace isaac::menu_ui {
enum class Origin : std::uint8_t { Shortcut, Pause, Title };
enum class Action : std::uint8_t { Drop, Give, Shop };
enum class Result : std::uint8_t { Done, NoSpace, Unavailable, NotNow, Failed };
enum class Tool : std::uint8_t { FullHealth, SoulHeart, Coins, Bombs, Keys, Charge, RevealMap, Reroll, Count };
inline constexpr unsigned ToolCount = static_cast<unsigned>(Tool::Count);
inline constexpr unsigned MaxHistory = 64, FormCount = 13;

struct RunInfo {
    bool valid{}, greed{};
    int character{}, stage{}, stageType{};
    char seed[16]{};
    unsigned frames{}, hits{}, floorHits{};
    float speed{}, tears{}, damage{}, range{}, shotSpeed{}, luck{};
    std::uint16_t history[MaxHistory]{};  // most recent first; high bit = trinket
    unsigned historyCount{};
};

class Host {
public:
    virtual std::uint64_t Now() = 0;
    virtual bool InRun() = 0;
    virtual unsigned ModCount() = 0;
    virtual bool ModAvailable(unsigned mod) = 0;
    virtual bool ModEnabled(unsigned mod) = 0;
    virtual void ToggleMod(unsigned mod) = 0;
    virtual const char* ModName(unsigned mod) = 0;
    virtual const char* ModDescription(unsigned mod) = 0;
    // Short live status ("Speeding up", "Hits: 3"); may be empty.
    virtual void ModStatus(unsigned mod, char* out, unsigned capacity) = 0;
    virtual Prefs& Preferences() = 0;
    // Push preference changes to the features and mark them for saving.
    virtual void PreferencesChanged() = 0;
    virtual Result Act(Action action, const Entry& entry) = 0;
    virtual Result UseTool(Tool tool) = 0;
    virtual bool Run(RunInfo& out) = 0;
    virtual bool Owned(unsigned collectible) = 0;
    virtual void Sound(unsigned id, float volume) = 0;
    // EID text for an entry (nullptr if none); used in the item note.
    virtual const char* Describe(const Entry& entry) = 0;
    // Current hit counter text box size at the current settings.
    virtual void HitCounterSize(float& width, float& height) = 0;
protected:
    // Never deleted through the interface (no heap use in the Switch module).
    ~Host() = default;
};

// Mod indices with settings pages (match ModFeature in mod_settings.hpp).
namespace mods {
inline constexpr unsigned Eid = 0, HitCounter = 1, TimeMachine = 5, MachineLock = 6;
}
// The game's own sound effects (index into sounds.xml).
namespace sound {
inline constexpr unsigned PageTurn = 8, PaperIn = 17, PaperOut = 18, Scroll = 188, NoteAppear = 189,
    NoteHide = 190, ThumbsUp = 174, ThumbsDown = 173, Plop = 155, ErrorBuzz = 127;
}
}

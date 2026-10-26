// Drives the real menu model through representative screens and writes each
// frame's draw commands as JSON for tools/menu_ui/preview.py.
#include "menu.hpp"
#include <cstdio>
#include <cstring>
#include <map>
#include <string>

using namespace isaac::menu_ui;

// Glyph advances exported by `preview.py --metrics` (font, code, advance), so
// layout measures text like the game. Without the file, Host's estimate is used.
std::map<std::pair<int, int>, float> g_Advance;

struct PreviewHost final : Host {
    std::uint64_t now = 100000;
    bool inRun = true;
    Prefs prefs;
    std::uint32_t enabled = 0x3fff & ~(1u << 9);
    const char* description = "{{ArrowUp}} {{Damage}} +1 Damage#{{ArrowUp}} {{Tears}} +0.7 Tears#Spectral, homing shots";
    std::uint64_t Now() override { return now; }
    bool InRun() override { return inRun; }
    unsigned ModCount() override { return 14; }
    bool ModAvailable(unsigned) override { return true; }
    bool ModEnabled(unsigned m) override { return enabled & (1u << m); }
    void ToggleMod(unsigned m) override { enabled ^= 1u << m; }
    const char* ModName(unsigned m) override {
        static const char* names[] = {"External Item Descriptions", "Hit Counter", "Enhanced Boss Bars", "Planetarium Chance",
            "Regret Pedestals", "Time Machine", "Machine Lock", "Greed Machine Never Jams", "Instant Fires and Poops",
            "Specialist Dance", "Animated Items", "Unique Birthright", "Clean Minimap", "Repentance+ Rules"};
        return names[m % 14];
    }
    const char* ModDescription(unsigned m) override {
        return m == 1 ? "Count the hits you take this run and on this floor. Press Y here to move it and change its size and opacity."
                      : "Speed up repeated machine and beggar uses in safe, cleared rooms while you hold toward them.";
    }
    void ModStatus(unsigned m, char* out, unsigned size) override { std::snprintf(out, size, "%s", m == 1 ? "Hits: 3 (floor 1)" : m == 5 ? "Speeding up" : ""); }
    Prefs& Preferences() override { return prefs; }
    void PreferencesChanged() override {}
    Result Act(Action, const Entry&) override { return Result::Done; }
    Result UseTool(Tool) override { return Result::Done; }
    bool Run(RunInfo& out) override {
        out = {};
        out.valid = true; out.character = 0; out.stage = 4; out.stageType = 1;
        std::snprintf(out.seed, sizeof(out.seed), "K3TQ 9ZWX");
        out.frames = 30 * 754; out.hits = 6; out.floorHits = 2;
        out.speed = 1.1f; out.tears = 3.33f; out.damage = 5.25f; out.range = 6.5f; out.shotSpeed = 1.f; out.luck = 1.f;
        const std::uint16_t items[] = {245, 4, 118, 0x8000 | 19, 182, 12, 519, 8, 67, 1, 2, 3};
        for (auto i : items) out.history[out.historyCount++] = i;
        return true;
    }
    bool Owned(unsigned) override { return false; }
    void Sound(unsigned, float) override {}
    const char* Describe(const Entry&) override { return description; }
    float TextWidth(Font font, const char* text, float scale) override {
        if (g_Advance.empty()) return Host::TextWidth(font, text, scale);
        const int f = static_cast<int>(font);
        float width = 0;
        for (const char* c = text; c && *c; ++c) {
            auto it = g_Advance.find({f, static_cast<unsigned char>(*c)});
            width += it != g_Advance.end() ? it->second : 6;
        }
        return f == static_cast<int>(Font::Small) ? width : width * scale;  // Small text is drawn unscaled
    }
    // Word-wrapped like preview.py's description(), with icons and colours dropped.
    unsigned DescriptionLines(const char* markup, float width) override {
        if (g_Advance.empty() || !markup) return 0;
        unsigned lines = 0;
        std::string paragraph;
        auto flush = [&] {
            float used = 0;
            bool any = false;
            std::size_t i = 0;
            while (i < paragraph.size()) {
                while (i < paragraph.size() && paragraph[i] == ' ') ++i;
                std::size_t j = i;
                while (j < paragraph.size() && paragraph[j] != ' ') ++j;
                if (j == i) break;
                const std::string word = paragraph.substr(i, j - i);
                i = j;
                if (word.rfind("{{", 0) == 0) continue;
                const float w = TextWidth(Font::Small, (word + " ").c_str(), 1);
                if (any && used + w > width) { ++lines; used = 0; }
                used += w; any = true;
            }
            if (any) ++lines;
            paragraph.clear();
        };
        for (const char* c = markup; *c; ++c) { if (*c == '#') flush(); else paragraph += *c; }
        flush();
        return lines;
    }
    void HitCounterSize(float& w, float& h) override { w = 102 * prefs.hitSize / 100.f; h = 10 * prefs.hitSize / 100.f; }
};

static Menu menu;
static PaintList paints;
static PreviewHost host;
static FILE* out;
static bool first = true;

std::string Escape(const char* s) {
    std::string r;
    for (; s && *s; ++s) {
        if (*s == '"' || *s == '\\') { r += '\\'; r += *s; }
        else if (static_cast<unsigned char>(*s) < 32) { char b[8]; std::snprintf(b, sizeof(b), "\\u%04x", *s); r += b; }
        else r += *s;
    }
    return r;
}
void Dump(const char* name, std::uint64_t at) {
    menu.Paint(host, paints, 480, 270, at);
    std::fprintf(out, "%s{\"name\":\"%s\",\"paint\":[", first ? "" : ",\n", name);
    first = false;
    bool comma = false;
    for (const auto& p : paints) {
        std::fprintf(out, "%s{\"k\":%d,\"f\":%d,\"a\":%d,\"flag\":%d,\"name\":\"%s\",\"text\":\"%s\",\"frame\":%d,\"id\":%u,\"color\":%u,"
                     "\"x\":%.2f,\"y\":%.2f,\"w\":%.2f,\"h\":%.2f,\"scale\":%.3f,\"alpha\":%.3f}",
                     comma ? "," : "", static_cast<int>(p.kind), static_cast<int>(p.font), static_cast<int>(p.align), p.flag ? 1 : 0,
                     Escape(p.name).c_str(), Escape(p.text).c_str(), p.frame, p.id, p.color, p.x, p.y, p.w, p.h, p.scale, p.alpha);
        comma = true;
    }
    std::fprintf(out, "]}");
}
void Press(std::uint32_t b) {
    menu.Update(host, 0, host.now += 40);
    menu.Update(host, b, host.now += 40);
    menu.Update(host, 0, host.now += 40);
}
int main(int argc, char** argv) {
    if (argc > 2) {
        if (FILE* metrics = std::fopen(argv[2], "r")) {
            int font{}, code{};
            float advance{};
            while (std::fscanf(metrics, "%d %d %f", &font, &code, &advance) == 3) g_Advance[{font, code}] = advance;
            std::fclose(metrics);
        }
    }
    out = std::fopen(argc > 1 ? argv[1] : "paints.json", "w");
    std::fprintf(out, "[");
    host.prefs.lastTab = 0;
    menu.Open(host, Origin::Pause, host.now);
    const auto opened = host.now;
    Dump("open-0080ms", opened + 80);
    Dump("open-0160ms", opened + 160);
    menu.Update(host, 0, host.now += 40);
    for (int i = 0; i < 10; ++i) Press(Bit(Right));
    host.prefs.ToggleFavorite(menu.VisibleAt(3)->entry);
    Dump("items", host.now += 2000);
    // Search: the letter board over a live-filtered list.
    Press(Bit(RS));
    Press(Bit(Right)); Press(Bit(A));                                    // B
    Press(Bit(Down)); Press(Bit(Right)); Press(Bit(Right)); Press(Bit(Right)); Press(Bit(Right)); Press(Bit(Right));
    Press(Bit(Right)); Press(Bit(Right)); Press(Bit(A));                 // R
    Press(Bit(Up)); Press(Bit(A));                                       // I
    Dump("items-search", host.now += 600);
    Press(Bit(RS));
    Dump("items-search-results", host.now += 600);
    Press(Bit(X)); Press(Bit(Up)); Press(Bit(Left)); Press(Bit(B));      // clear the search from the filter card
    // A long, coloured description turns pages; LS turns them by hand.
    host.description = "{{Collectible118}} {{ColorRed}}Brimstone{{CR}} replaces your tears with a {{ColorYellow}}chargeable blood beam{{CR}}"
        "#{{Damage}} The beam deals {{ColorLime}}13 times{{CR}} your damage over its duration"
        "#{{Blank}} {{ColorBlue}}Blue{{CR}}, {{ColorGray}}gray{{CR}}, {{ColorCyan}}cyan{{CR}}, {{ColorGold}}gold{{CR}} and {{ColorShinyPurple}}shiny purple{{CR}} keywords keep their colour"
        "#Synergises with most tear modifiers and many familiars, which then fire short beams of their own";
    Press(Bit(Right));
    Dump("items-description-page1", host.now += 600);
    Press(Bit(LS));
    Dump("items-description-page2", host.now += 600);
    host.description = "{{ArrowUp}} {{Damage}} +1 Damage#{{ArrowUp}} {{Tears}} +0.7 Tears#Spectral, homing shots";
    Press(Bit(Left));
    Press(Bit(A));
    Dump("items-action", host.now += 600);
    Press(Bit(B));
    Press(Bit(X));
    Press(Bit(Down)); Press(Bit(Down)); Press(Bit(Right)); Press(Bit(Right)); Press(Bit(Right)); Press(Bit(Right)); Press(Bit(Right));
    Dump("items-filter", host.now += 600);
    Press(Bit(Left)); Press(Bit(Up)); Press(Bit(Up));
    Press(Bit(Right)); Press(Bit(Right));
    Press(Bit(B));
    Dump("cards", host.now += 600);
    Press(Bit(R));
    Dump("mods-turning", host.now + 60);
    Dump("mods", host.now += 2000);
    Press(Bit(Down));
    Press(Bit(Y));
    Press(Bit(Down)); Press(Bit(Down));
    Dump("mods-hitcounter", host.now += 600);
    Press(Bit(B));
    for (int i = 0; i < 12; ++i) Press(Bit(Down));   // Repentance+ Rules
    Press(Bit(Y)); Press(Bit(Down));
    Dump("mods-rplus", host.now += 600);
    Press(Bit(B));
    Press(Bit(R));
    Dump("run", host.now += 2000);
    Press(Bit(R));
    Press(Bit(Down)); Press(Bit(Down)); Press(Bit(Down)); Press(Bit(Down)); Press(Bit(Down)); Press(Bit(Down));
    Dump("tools", host.now += 2000);
    Press(Bit(A));
    Dump("tools-toast", host.now += 300);
    Press(Bit(R));
    Press(Bit(Down)); Press(Bit(Down));
    Dump("menu", host.now += 2000);
    Press(Bit(B));
    Dump("closing-0090ms", host.now + 90);
    menu.Abort();
    menu.Open(host, Origin::Title, host.now += 5000);
    Dump("title-mods", host.now += 2000);
    Press(Bit(Down)); Press(Bit(Y)); Press(Bit(Down)); Press(Bit(A));
    Dump("hitcounter-move", host.now += 500);
    std::fprintf(out, "]\n");
    std::fclose(out);
    std::printf("Dumped menu preview scenes\n");
}

#include "eid_ui.hpp"
#include "native_sprite.hpp"
#include "generated/build_profile.hpp"
#include "../../../eid_ui/layout.hpp"
#include "../../../features/menu_ui/ink.hpp"
#include <cstdio>

namespace isaac {
namespace {
using namespace feature_memory;
using namespace isaac_eid_ui;
constexpr std::uintptr_t FrameCounter = 0x24f99c;
constexpr std::uintptr_t GetCollectible = 0x3c0c50, GetTrinket = 0x3c0c9c;
constexpr std::uintptr_t ItemConfig = 0x36538, ItemGraphics = 0x38;
constexpr unsigned ItemSlots = 16;
constexpr const char* SheetPaths[] = {
    "gfx/codex_eid_ui/eid_inline_icons.anm2", "gfx/codex_eid_ui/eid_player_icons.anm2",
    "gfx/codex_eid_ui/eid_transform_icons.anm2", "gfx/codex_eid_ui/eid_cardspills.anm2",
    "gfx/ui/buttons.anm2"
};

constinit NativeSprite s_Font, s_Sheets[5], s_Items[ItemSlots];
constinit Builder s_Builder;
constinit Layout s_Title, s_Body;
std::uintptr_t s_Module{};
std::uintptr_t s_Selection{};
std::uint32_t s_ItemKeys[ItemSlots]{}, s_FontColor{0xffffffff};
std::uint64_t s_ContentHash{};
std::int32_t s_FirstFrame{}, s_LastFrame{};
int s_Width{};
bool s_FontAttempted{}, s_FontReady{}, s_SheetAttempted[5]{}, s_SheetReady[5]{}, s_ItemReady[ItemSlots]{};
bool s_Ink{};

// On menu paper, light EID text becomes dark ink and each colour its own
// readable ink (features/menu_ui/ink.hpp).
std::uint32_t InkColor(std::uint32_t rgb) {
    return s_Ink ? menu_ui::ink::OnPaper(rgb) : rgb;
}

FeatureColor Color(std::uint32_t rgb, float alpha = .92f) {
    return {static_cast<float>((rgb >> 16) & 255) / 255.f,
            static_cast<float>((rgb >> 8) & 255) / 255.f,
            static_cast<float>(rgb & 255) / 255.f, alpha};
}

void ClearItems() {
    for (unsigned i = 0; i < ItemSlots; ++i) {
        s_Items[i].Destroy();
        s_ItemKeys[i] = 0;
        s_ItemReady[i] = false;
    }
}

void PrepareFont() {
    if (s_FontAttempted) return;
    s_FontAttempted = true;
    s_FontReady = s_Font.Create(s_Module) && s_Font.Load("gfx/codex_eid_ui/eid_glyphs.anm2") &&
                  s_Font.SetFrame("Glyphs", 0);
    if (s_FontReady) s_Font.SetScale(1.f, 1.f);
}

NativeSprite* Sheet(unsigned index) {
    if (index >= 5) return nullptr;
    if (!s_SheetAttempted[index]) {
        s_SheetAttempted[index] = true;
        s_SheetReady[index] = s_Sheets[index].Create(s_Module) && s_Sheets[index].Load(SheetPaths[index]);
        if (s_SheetReady[index]) {
            s_Sheets[index].SetScale(1.f, 1.f);
            s_Sheets[index].SetTint(1.f, 1.f, 1.f, .92f);
        }
    }
    return s_SheetReady[index] ? &s_Sheets[index] : nullptr;
}

NativeSprite* Item(std::uint32_t id, bool trinket) {
    if (!id || id > 65535) return nullptr;
    const auto key = id | (trinket ? 0x20000u : 0x10000u);
    unsigned slot = ItemSlots;
    for (unsigned i = 0; i < ItemSlots; ++i) {
        if (s_ItemKeys[i] == key) return s_ItemReady[i] ? &s_Items[i] : nullptr;
        if (!s_ItemKeys[i] && slot == ItemSlots) slot = i;
    }
    if (slot == ItemSlots) return nullptr;
    s_ItemKeys[slot] = key;
    std::uintptr_t manager{};
    if (!TryRead(s_Module + profile::ManagerGlobal, manager) || !Readable(manager + ItemConfig, 0x28)) return nullptr;
    const auto config = reinterpret_cast<std::uintptr_t (*)(const void*, int)>(s_Module + (trinket ? GetTrinket : GetCollectible))(
        reinterpret_cast<const void*>(manager + ItemConfig), static_cast<int>(id));
    char graphics[512];
    if (!config || !CopyNativeString(config + ItemGraphics, graphics, sizeof(graphics)) || !graphics[0]) return nullptr;
    auto& sprite = s_Items[slot];
    if (!sprite.Create(s_Module) || !sprite.Load("gfx/codex_eid_ui/item_icon.anm2") ||
        !sprite.SetSpritesheet(0, graphics)) return nullptr;
    sprite.LoadGraphics();
    if (!sprite.SetFrame("Item", 0)) return nullptr;
    sprite.SetTint(1.f, 1.f, 1.f, .96f);
    s_ItemReady[slot] = true;
    return &sprite;
}

void DrawGlyph(const FeatureFrame& frame, std::uint32_t code, float x, float y, std::uint32_t color) {
    const auto* glyph = Glyph(code);
    if (!glyph) glyph = Glyph('?');
    if (!glyph) return;
    if (s_FontReady && s_Font.SetFrame("Glyphs", glyph->frame)) {
        if (s_FontColor != color) {
            const auto tint = Color(color);
            s_Font.SetTint(tint.r, tint.g, tint.b, tint.a);
            s_FontColor = color;
        }
        s_Font.Render({x, y});
    } else if (frame.draw.text) {
        // Missing resource fallback remains readable and contains no markup or
        // implementation messages. The authentic font is the normal path.
        const char fallback[2] = {static_cast<char>(code >= 32 && code < 127 ? code : code == 7 ? '.' : '?'), 0};
        frame.draw.Text(fallback, x, y, Color(color), .5f);
    }
}

void DrawStatic(const FeatureFrame& frame, unsigned index, float x, float y) {
    if (index >= sizeof(assets::Icons) / sizeof(assets::Icons[0])) return;
    const auto& icon = assets::Icons[index];
    auto* sprite = Sheet(icon.sheet);
    if (sprite && sprite->SetFrame(icon.animation, icon.frame)) sprite->Render({x + icon.left, y + icon.top});
    else DrawGlyph(frame, 7, x + 3.f, y, White);
}

void DrawItem(const FeatureFrame& frame, std::uint32_t id, bool trinket, float x, float y) {
    auto* sprite = Item(id, trinket);
    if (sprite) sprite->Render({x, y - 2.f});
    else {
        const auto index = IconIndex(trinket ? "Trinket" : "Collectible", trinket ? 7 : 11);
        if (index >= 0) DrawStatic(frame, static_cast<unsigned>(index), x, y);
    }
}

void DrawLines(const FeatureFrame& frame, const Layout& layout, unsigned first, unsigned count, float x, float y) {
    for (unsigned line = first; line < layout.line_count && line < first + count; ++line) {
        const auto& row = layout.lines[line];
        for (unsigned i = row.first; i < row.first + row.count; ++i) {
            const auto& cell = layout.cells[i];
            const float cx = x + cell.x, cy = y + cell.y - first * LineHeight;
            if (cell.kind == Kind::Glyph) DrawGlyph(frame, cell.value, cx, cy, InkColor(cell.color));
            else if (cell.kind == Kind::Icon) DrawStatic(frame, cell.value, cx, cy);
            else DrawItem(frame, cell.value, cell.kind == Kind::Trinket, cx, cy);
        }
    }
}

void DrawSmallText(const FeatureFrame& frame, const char* text, float x, float y, std::uint32_t color) {
    for (const auto* cursor = text; *cursor; ++cursor) {
        const auto code = static_cast<unsigned char>(*cursor);
        DrawGlyph(frame, code, x, y, color);
        const auto* glyph = Glyph(code);
        x += glyph ? glyph->advance : 5;
    }
}
std::uint64_t ContentHash(const char* name, const char* markup) {
    std::uint64_t hash = 14695981039346656037ull;
    const char* texts[] = {name, markup};
    for (const char* text : texts) {
        for (const auto* cursor = text; *cursor; ++cursor) { hash ^= static_cast<unsigned char>(*cursor); hash *= 1099511628211ull; }
        hash ^= 0xff; hash *= 1099511628211ull;
    }
    return hash;
}

void DrawHeaderIcon(const FeatureFrame& frame, int variant, unsigned id, const char* token, float x, float y) {
    if (variant == 100 || variant == 350) { DrawItem(frame, id, variant == 350, x, y); return; }
    char numbered[32];
    if (!token) {
        const char* prefix = variant == 300 ? "Card" : variant == 1 ? "Player" : "Pill";
        std::snprintf(numbered, sizeof(numbered), "%s%u", prefix, id);
        token = numbered;
    }
    auto index = IconIndex(token, std::strlen(token));
    if (index < 0) index = IconIndex(variant == 300 ? "Card" : "Pill", 4);
    if (index >= 0) DrawStatic(frame, static_cast<unsigned>(index), x + 2.f, y + 1.f);
}

void RenderPanel(const FeatureFrame& frame, std::uintptr_t selection, const isaac_eid::Description& description,
                 int icon_variant, unsigned icon_id, const char* static_icon) {
    if (!frame.module || !description.name || !description.markup) return;
    if (s_Module && s_Module != frame.module) return;
    s_Module = frame.module;
    std::int32_t now{};
    if (!TryRead(frame.game + FrameCounter, now) || now < 0) return;
    const int width = frame.width >= 300.f ? 142 : 130;
    const auto content_hash = ContentHash(description.name, description.markup);
    if (selection != s_Selection || content_hash != s_ContentHash || width != s_Width) {
        ClearItems();
        s_Builder.Build(description.name, width - 20, false, s_Title, NamePurple);
        s_Builder.Build(description.markup, width, true, s_Body);
        s_Selection = selection;
        s_ContentHash = content_hash;
        s_Width = width;
        s_FirstFrame = now;
    } else if (now < s_LastFrame || now - s_LastFrame > 20) s_FirstFrame = now;
    s_LastFrame = now;
    PrepareFont();

    const float x = frame.width >= 300.f ? 58.f : 40.f;
    const float top = 45.f;
    const unsigned title_rows = s_Title.line_count > 3 ? 3 : s_Title.line_count;
    const float title_height = title_rows * LineHeight + 4.f > 17.f ? title_rows * LineHeight + 4.f : 17.f;
    const float body_top = top + title_height;
    const int available_rows = static_cast<int>((frame.height - body_top - 42.f) / LineHeight);
    if (available_rows < 1) return;
    const unsigned rows = static_cast<unsigned>(available_rows);
    const unsigned pages = s_Body.line_count ? (s_Body.line_count + rows - 1) / rows : 1;
    const unsigned page_frames = 360 + rows * 18;
    const auto elapsed = static_cast<unsigned>(now - s_FirstFrame);
    const unsigned page = (elapsed / page_frames) % pages;

    // Floating outlined text matches the PC mod and leaves the room visible.
    // The hanging bullet column keeps wrapped lines aligned beneath their text.
    DrawHeaderIcon(frame, icon_variant, icon_id, static_icon, x - 2.f, top - 1.f);
    DrawLines(frame, s_Title, 0, title_rows, x + 18.f, top - 2.f);
    DrawLines(frame, s_Body, page * rows, rows, x, body_top);
    if (pages > 1) {
        char pagination[24];
        std::snprintf(pagination, sizeof(pagination), "%u / %u", page + 1, pages);
        const unsigned visible_rows = s_Body.line_count - page * rows < rows ? s_Body.line_count - page * rows : rows;
        DrawSmallText(frame, pagination, x + 12.f, body_top + visible_rows * LineHeight + 2.f, 0xbeb4c6);
    }
}

} // namespace

void RenderEidDescription(const FeatureFrame& frame, isaac_eid::Category category,
                          std::uint32_t id, const isaac_eid::Description& description) {
    if (!id || id > 65535) return;
    RenderPanel(frame, (static_cast<std::uint32_t>(category) << 16) | id, description,
                static_cast<int>(category), id, nullptr);
}

void RenderEidContext(const FeatureFrame& frame, std::uintptr_t selection,
                      const isaac_eid::ContextDescription& description) {
    if (description.valid) RenderPanel(frame, selection, description.View(), description.icon_variant,
                                      description.icon_id, description.static_icon);
}

void DrawEidMenuText(const FeatureFrame& frame, const char* text, float x, float y,
                     std::uint32_t color, float maxWidth) {
    if (!frame.module || !text || (s_Module && s_Module != frame.module)) return;
    s_Module = frame.module; PrepareFont();
    const float limit = x + maxWidth;
    // Too long: stop early enough to end with "..." instead of cutting a word.
    const bool shorten = EidMenuTextWidth(text) > maxWidth;
    const auto* dot = Glyph('.');
    const float stop = shorten ? limit - 3 * (dot ? dot->advance : 3) : limit;
    const char* cursor = text; const char* end = text + std::strlen(text);
    while (cursor < end) {
        const auto code = Decode(cursor,end);
        const auto* glyph = Glyph(code);
        const auto advance = glyph ? glyph->advance : 5;
        if (x + advance > stop) break;
        DrawGlyph(frame,code,x,y,color); x += advance;
    }
    if (shorten) for (unsigned i = 0; i < 3; ++i) { DrawGlyph(frame,'.',x,y,color); x += dot ? dot->advance : 3; }
}

void DrawEidMenuDescription(const FeatureFrame& frame, unsigned item,
                            const char* name, const char* markup, float x, float y,
                            float width, float height, std::uint64_t milliseconds) {
    if (!frame.module || !name || !markup || width < 40 || height < 24 ||
        (s_Module && s_Module != frame.module)) return;
    s_Module = frame.module; PrepareFont();
    // Share the existing bounded layout buffers; the normal EID overlay is
    // suppressed during a menu. The tag forces a rebuild on leaving a menu.
    const auto selection = static_cast<std::uintptr_t>(0x7f000000u | item);
    const auto hash = ContentHash(name,markup);
    if (selection != s_Selection || hash != s_ContentHash || s_Width != static_cast<int>(width)) {
        ClearItems();
        s_Builder.Build(name, static_cast<int>(width)-22, false, s_Title, NamePurple);
        s_Builder.Build(markup, static_cast<int>(width), true, s_Body);
        s_Selection=selection; s_ContentHash=hash; s_Width=static_cast<int>(width);
        s_FirstFrame=static_cast<std::int32_t>(milliseconds & 0x7fffffff);
    }
    const unsigned titleRows = s_Title.line_count < 3 ? s_Title.line_count : 3;
    if (item) DrawItem(frame,item,false,x,y);
    DrawLines(frame,s_Title,0,titleRows,x+22,y);
    const float bodyY=y+(titleRows*LineHeight>20 ? titleRows*LineHeight : 20)+5;
    const int available=static_cast<int>((y+height-bodyY-15)/LineHeight);
    if (available<1) return;
    const unsigned rows=static_cast<unsigned>(available);
    const unsigned pages=s_Body.line_count ? (s_Body.line_count+rows-1)/rows : 1;
    const unsigned elapsed=(static_cast<unsigned>(milliseconds & 0x7fffffff)-static_cast<unsigned>(s_FirstFrame)) & 0x7fffffffu;
    const unsigned page=(elapsed/6500)%pages;
    DrawLines(frame,s_Body,page*rows,rows,x,bodyY);
    if (pages>1) {
        char label[40]; std::snprintf(label,sizeof(label),"Description %u/%u",page+1,pages);
        DrawSmallText(frame,label,x,y+height-11,0xbeb4c6);
    }
}

void SetEidInk(bool ink) { s_Ink = ink; }
float EidMenuTextWidth(const char* text) {
    if (!text) return 0;
    float width = 0;
    const char* cursor = text; const char* end = text + std::strlen(text);
    while (cursor < end) { const auto* glyph = Glyph(Decode(cursor, end)); width += glyph ? glyph->advance : 5; }
    return width;
}

void DrawEidIcon(const FeatureFrame& frame, const char* token, float x, float y) {
    if (!frame.module || !token || (s_Module && s_Module != frame.module)) return;
    s_Module = frame.module;
    const auto index = IconIndex(token, std::strlen(token));
    if (index >= 0) DrawStatic(frame, static_cast<unsigned>(index), x, y);
}

namespace {
// Menu body text is laid out once per text and width; drawing and counting
// lines share the result (and the bounded buffers the overlay also uses).
void PrepareMenuBody(const char* markup, float width, std::uint64_t milliseconds) {
    const auto selection = static_cast<std::uintptr_t>(0x7e000000u);
    const auto hash = ContentHash("", markup);
    if (selection != s_Selection || hash != s_ContentHash || s_Width != static_cast<int>(width)) {
        ClearItems();
        s_Title.Clear();
        s_Builder.Build(markup, static_cast<int>(width), true, s_Body);
        s_Selection = selection; s_ContentHash = hash; s_Width = static_cast<int>(width);
        s_FirstFrame = static_cast<std::int32_t>(milliseconds & 0x7fffffff);
    }
}
}

unsigned EidMenuBodyLines(const char* markup, float width, std::uint64_t milliseconds) {
    if (!markup || width < 40) return 0;
    PrepareMenuBody(markup, width, milliseconds);
    return s_Body.line_count;
}

void DrawEidMenuBody(const FeatureFrame& frame, const char* markup, float x, float y,
                     float width, float height, std::uint64_t milliseconds, int page) {
    if (!frame.module || !markup || width < 40 || height < 10 || (s_Module && s_Module != frame.module)) return;
    s_Module = frame.module; PrepareFont();
    PrepareMenuBody(markup, width, milliseconds);
    const int available = static_cast<int>(height / LineHeight);
    if (available < 1) return;
    const unsigned rows = static_cast<unsigned>(available);
    const unsigned pages = s_Body.line_count ? (s_Body.line_count + rows - 1) / rows : 1;
    // The menu picks the page (it shows the page marker); -1 turns pages by itself.
    const unsigned elapsed = (static_cast<unsigned>(milliseconds & 0x7fffffff) - static_cast<unsigned>(s_FirstFrame)) & 0x7fffffffu;
    const unsigned shown = page >= 0 ? static_cast<unsigned>(page) % pages : (elapsed / 5000) % pages;
    DrawLines(frame, s_Body, shown * rows, rows, x, y);
}

void ResetEidUi() {
    ClearItems();
    for (unsigned i = 0; i < 5; ++i) {
        s_Sheets[i].Destroy(); s_SheetAttempted[i] = s_SheetReady[i] = false;
    }
    s_Font.Destroy();
    s_FontAttempted = s_FontReady = false;
    s_FontColor = 0xffffffff;
    s_Module = 0;
    s_Selection = 0;
    s_ContentHash = 0;
    s_Width = 0;
    s_FirstFrame = s_LastFrame = 0;
    s_Title.Clear(); s_Body.Clear();
}
} // namespace isaac

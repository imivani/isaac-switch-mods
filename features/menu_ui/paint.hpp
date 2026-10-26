#pragma once
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

// A frame of the menu as plain drawing commands. The Switch backend draws
// them with the game's own sprites and fonts; host tests and the preview
// renderer read the same list.
namespace isaac::menu_ui {
enum class PaintKind : std::uint8_t { Rect, Art, Text, Item, Icon, Description, HitCounter };
enum class Font : std::uint8_t { Ink, InkLarge, Hud, Small };
enum class Align : std::uint8_t { Left, Center, Right };
// InkFaint is secondary text and still reads at 4.5:1 on paper; InkDisabled is
// only for things that cannot be used right now.
inline constexpr std::uint32_t Ink = 0x2a1f1d, InkFaint = 0x6b5d55, InkDisabled = 0x9a8c84, InkStrong = 0x140d0c,
    Paper = 0xf4ede1, InkGood = 0x2f5e28, InkBad = 0x7a2a22;

struct Paint {
    PaintKind kind{};
    Font font{};
    Align align{};
    bool flag{};               // Item: trinket. HitCounter: outline. Art: mirror.
    const char* name{};        // Art: animation. Icon: EID token. Description: title.
    const char* text{};        // Text. Description: markup.
    int frame{};
    std::uint32_t id{}, color{0xffffff};
    float x{}, y{}, w{}, h{}, scale{1}, alpha{1};
};

class PaintList {
public:
    static constexpr unsigned Capacity = 480, PoolSize = 8192;
    void Clear() { count_ = used_ = 0; }
    unsigned Count() const { return count_; }
    const Paint& operator[](unsigned i) const { return items_[i]; }
    const Paint* begin() const { return items_; }
    const Paint* end() const { return items_ + count_; }
    bool Full() const { return count_ >= Capacity; }

    void Rect(float x1, float y1, float x2, float y2, std::uint32_t color, float alpha) {
        if (auto* p = Add(PaintKind::Rect)) { p->x = x1; p->y = y1; p->w = x2 - x1; p->h = y2 - y1; p->color = color; p->alpha = alpha; }
    }
    void Art(const char* animation, int frame, float x, float y, float scale = 1, float alpha = 1, bool mirror = false) {
        if (auto* p = Add(PaintKind::Art)) { p->name = animation; p->frame = frame; p->x = x; p->y = y; p->scale = scale; p->alpha = alpha; p->flag = mirror; }
    }
    void Text(Font font, const char* text, float x, float y, std::uint32_t color, float scale = 1, float alpha = 1,
              Align align = Align::Left, float maxWidth = 0) {
        if (auto* p = Add(PaintKind::Text)) {
            p->font = font; p->text = Copy(text); p->x = x; p->y = y; p->color = color; p->scale = scale;
            p->alpha = alpha; p->align = align; p->w = maxWidth;
        }
    }
    // Like Text: maxWidth > 0 shortens the result to fit.
    void Format(Font font, float x, float y, std::uint32_t color, float scale, float alpha, Align align, float maxWidth,
                const char* format, ...) {
        char buffer[160];
        va_list args;
        va_start(args, format);
        std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        Text(font, buffer, x, y, color, scale, alpha, align, maxWidth);
    }
    void Item(std::uint32_t id, bool trinket, float x, float y, float scale = 1, float alpha = 1) {
        if (auto* p = Add(PaintKind::Item)) { p->id = id; p->flag = trinket; p->x = x; p->y = y; p->scale = scale; p->alpha = alpha; }
    }
    void Icon(const char* token, float x, float y, float alpha = 1) {
        if (auto* p = Add(PaintKind::Icon)) { p->name = Copy(token); p->x = x; p->y = y; p->alpha = alpha; }
    }
    // page >= 0 shows that page of a long text; -1 lets the backend turn pages.
    void Description(std::uint32_t item, const char* title, const char* markup, float x, float y, float w, float h, int page = -1) {
        if (auto* p = Add(PaintKind::Description)) {
            p->id = item; p->name = Copy(title); p->text = markup; p->x = x; p->y = y; p->w = w; p->h = h; p->frame = page;
        }
    }
    void HitCounter(float alpha, bool outline) {
        if (auto* p = Add(PaintKind::HitCounter)) { p->alpha = alpha; p->flag = outline; }
    }
    const char* Copy(const char* text) {
        if (!text) return "";
        const auto length = static_cast<unsigned>(std::strlen(text));
        if (used_ + length + 1 > PoolSize) return "";
        char* out = pool_ + used_;
        std::memcpy(out, text, length + 1);
        used_ += length + 1;
        return out;
    }

private:
    Paint* Add(PaintKind kind) {
        if (count_ >= Capacity) return nullptr;
        items_[count_] = Paint{};
        items_[count_].kind = kind;
        return &items_[count_++];
    }
    Paint items_[Capacity];
    char pool_[PoolSize];
    unsigned count_{}, used_{};
};
}

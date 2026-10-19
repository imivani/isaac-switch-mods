#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "generated/assets.hpp"
#include "../eid/markup.hpp"

namespace isaac_eid_ui {

inline constexpr std::uint32_t White = 0xffffff;
inline constexpr std::uint32_t NamePurple = 0xcc4ccc;
inline constexpr unsigned LineHeight = 11;
inline constexpr unsigned MaxCells = 4096, MaxLines = 192, MaxParagraphAtoms = 2048;

enum class Kind : std::uint8_t { Glyph, Icon, Collectible, Trinket };
struct Atom {
    Kind kind = Kind::Glyph;
    std::uint32_t value = 0, color = 0;
    std::int16_t width = 0;
    bool space = false;
};
struct Cell {
    Kind kind = Kind::Glyph;
    std::uint32_t value = 0, color = 0;
    std::int16_t x = 0, y = 0, width = 0;
};
struct Line { std::uint16_t first = 0, count = 0; std::int16_t width = 0; };
struct Layout {
    Cell cells[MaxCells]{};
    Line lines[MaxLines]{};
    unsigned cell_count = 0, line_count = 0;
    unsigned omitted_variable_paragraphs = 0, unknown_markup = 0;
    bool truncated = false;
    void Clear() {
        cell_count = line_count = omitted_variable_paragraphs = unknown_markup = 0;
        truncated = false;
    }
};

inline bool Equal(const char* text, std::size_t size, const char* expected) {
    const auto length = std::strlen(expected);
    return size == length && std::memcmp(text, expected, size) == 0;
}
inline const assets::Glyph* Glyph(std::uint32_t code) {
    std::size_t low = 0, high = sizeof(assets::Glyphs) / sizeof(assets::Glyphs[0]);
    while (low < high) {
        const auto mid = low + (high - low) / 2;
        if (assets::Glyphs[mid].code < code) low = mid + 1;
        else high = mid;
    }
    if (low < sizeof(assets::Glyphs) / sizeof(assets::Glyphs[0]) && assets::Glyphs[low].code == code)
        return &assets::Glyphs[low];
    return nullptr;
}
inline int IconIndex(const char* text, std::size_t size) {
    for (unsigned i = 0; i < sizeof(assets::Icons) / sizeof(assets::Icons[0]); ++i)
        if (Equal(text, size, assets::Icons[i].name)) return static_cast<int>(i);
    return -1;
}
inline bool Numbered(const char* text, std::size_t size, const char* prefix, std::uint32_t& value) {
    const auto prefix_size = std::strlen(prefix);
    if (size <= prefix_size || std::memcmp(text, prefix, prefix_size) != 0) return false;
    value = 0;
    for (auto i = prefix_size; i < size; ++i) {
        if (text[i] < '0' || text[i] > '9' || value > 6553) return false;
        value = value * 10 + static_cast<unsigned>(text[i] - '0');
    }
    return value > 0 && value <= 65535;
}

// Strict UTF-8 decoding prevents a wrapped line from splitting a multibyte
// character. Unsupported characters use the font's question mark, never bytes.
inline std::uint32_t Decode(const char*& cursor, const char* end) {
    const auto first = static_cast<unsigned char>(*cursor++);
    if (first < 0x80) return first;
    unsigned count = first >= 0xf0 && first <= 0xf4 ? 3 : first >= 0xe0 && first <= 0xef ? 2 :
                     first >= 0xc2 && first <= 0xdf ? 1 : 0;
    if (!count || static_cast<std::size_t>(end - cursor) < count) return '?';
    std::uint32_t value = first & (count == 1 ? 0x1f : count == 2 ? 0xf : 7);
    for (unsigned i = 0; i < count; ++i) {
        const auto byte = static_cast<unsigned char>(cursor[i]);
        if ((byte & 0xc0) != 0x80) return '?';
        value = (value << 6) | (byte & 0x3f);
    }
    cursor += count;
    if ((count == 1 && value < 0x80) || (count == 2 && value < 0x800) ||
        (count == 3 && value < 0x10000) || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return '?';
    return value;
}

inline bool SetColor(const char* text, std::size_t size, std::uint32_t base, std::uint32_t& color) {
    struct NamedColor { const char* name; std::uint32_t value; };
    static constexpr NamedColor colors[] = {
        {"ColorBlue", 0x0000ff}, {"ColorOrange", 0xff8a00}, {"ColorRed", 0xff0000},
        {"ColorSilver", 0xbfbfbf}, {"ColorYellow", 0xffff00}, {"ColorEIDText", White},
        {"ColorWhite", White}, {"ColorEIDObjName", NamePurple}, {"ColorObjName", NamePurple},
        {"ColorGray", 0x808080}, {"ColorLime", 0x00ff00}, {"ColorCyan", 0x00ffff},
        {"ColorEIDTransform", 0x8080ff}, {"ColorGold", 0xffcc40},
        {"ColorShinyPurple", 0xcc80ff}, {"ColorIsaac", 0xe3c6c5},
        {"ColorText", White}, {"BlinkBirthright", 0xe3c6c5}, {"BlinkYellowGreen", 0xccff66}
    };
    if (Equal(text, size, "CR") || Equal(text, size, "ColorReset") || Equal(text, size, "Reset")) {
        color = base; return true;
    }
    for (const auto& named : colors) if (Equal(text, size, named.name)) { color = named.value; return true; }
    return false;
}

class Builder {
public:
    // Width is the entire line, including the 12-pixel hanging bullet margin.
    void Build(const char* text, int width, bool bullets, Layout& output, std::uint32_t base = White) {
        output.Clear();
        if (!text || width < 30 || width > 2048) return;
        const char* cursor = text;
        std::size_t remaining = 0;
        while (remaining < 32768 && text[remaining]) ++remaining;
        if (remaining == 32768) { output.truncated = true; return; }
        while (remaining && !output.truncated) {
            std::size_t length = 0;
            while (length < remaining && cursor[length] != '#' && cursor[length] != '\n' && cursor[length] != '\r') ++length;
            Parse(cursor, length, base, output);
            if (variable_) ++output.omitted_variable_paragraphs;
            else if (atom_count_) Place(width, bullets, output);
            cursor += length;
            remaining -= length;
            while (remaining && (*cursor == '#' || *cursor == '\n' || *cursor == '\r')) { ++cursor; --remaining; }
        }
    }

private:
    Atom atoms_[MaxParagraphAtoms]{};
    unsigned atom_count_ = 0, indent_ = 0;
    bool variable_ = false, blank_ = false;

    void Add(Atom atom, Layout& output) {
        if (atom.space && (!atom_count_ || atoms_[atom_count_ - 1].space)) return;
        if (atom_count_ == MaxParagraphAtoms) { output.truncated = true; return; }
        atoms_[atom_count_++] = atom;
    }
    void AddGlyph(std::uint32_t code, std::uint32_t color, Layout& output) {
        const auto* glyph = Glyph(code);
        if (!glyph) { code = '?'; glyph = Glyph(code); }
        if (glyph) Add({Kind::Glyph, code, color, glyph->advance, code == ' '}, output);
    }
    void AddIcon(int index, Layout& output) {
        if (index < 0) return;
        Add({Kind::Icon, static_cast<std::uint32_t>(index), White, assets::Icons[index].width, false}, output);
    }
    void Parse(const char* text, std::size_t size, std::uint32_t base, Layout& output) {
        atom_count_ = indent_ = 0; variable_ = blank_ = false;
        auto color = base;
        isaac_eid::MarkupReader reader(text, size);
        isaac_eid::Token token;
        while (reader.Next(token) && !output.truncated) {
            if (token.kind == isaac_eid::TokenKind::Variable) { variable_ = true; continue; }
            if (token.kind == isaac_eid::TokenKind::Markup) {
                if (SetColor(token.text, token.size, base, color)) continue;
                if (Equal(token.text, token.size, "Blank")) { blank_ = true; continue; }
                if (Equal(token.text, token.size, "Indent") || Equal(token.text, token.size, "IND")) {
                    if (indent_ < 3) ++indent_;
                    continue;
                }
                if (Equal(token.text, token.size, "NoLB")) continue;
                std::uint32_t item{};
                if (Numbered(token.text, token.size, "Collectible", item)) {
                    Add({Kind::Collectible, item, White, 16, false}, output); continue;
                }
                if (Numbered(token.text, token.size, "Trinket", item)) {
                    Add({Kind::Trinket, item, White, 16, false}, output); continue;
                }
                const auto index = IconIndex(token.text, token.size);
                if (index >= 0) AddIcon(index, output);
                else ++output.unknown_markup;
            } else if (token.kind == isaac_eid::TokenKind::Text) {
                const char* cursor = token.text;
                const char* end = cursor + token.size;
                while (cursor < end && !output.truncated) {
                    const auto code = Decode(cursor, end);
                    if (code == '{' || code == '}') { variable_ = true; continue; }
                    if (code == '\t') { AddGlyph(' ', color, output); continue; }
                    if (code == 0x2191 || code == 1) AddIcon(IconIndex("ArrowUp", 7), output);
                    else if (code == 0x2193 || code == 2) AddIcon(IconIndex("ArrowDown", 9), output);
                    else if (code == 3) AddIcon(IconIndex("Warning", 7), output);
                    else if (code >= 32 || code == 7) AddGlyph(code, color, output);
                }
            }
        }
        while (atom_count_ && atoms_[atom_count_ - 1].space) --atom_count_;
    }

    bool NewLine(Layout& output) {
        if (output.line_count == MaxLines) { output.truncated = true; return false; }
        output.lines[output.line_count++] = {static_cast<std::uint16_t>(output.cell_count), 0, 0};
        return true;
    }
    bool Put(const Atom& atom, int x, Layout& output) {
        if (output.cell_count == MaxCells) { output.truncated = true; return false; }
        output.cells[output.cell_count++] = {atom.kind, atom.value, atom.color, static_cast<std::int16_t>(x),
                                             static_cast<std::int16_t>((output.line_count - 1) * LineHeight), atom.width};
        auto& line = output.lines[output.line_count - 1];
        ++line.count;
        if (x + atom.width > line.width) line.width = static_cast<std::int16_t>(x + atom.width);
        return true;
    }
    void Place(int width, bool bullets, Layout& output) {
        if (!NewLine(output)) return;
        const int left = static_cast<int>(indent_) * 12;
        const int hanging = bullets ? 12 : 0;
        const int start_x = left + hanging;
        if (start_x + 16 > width) { output.truncated = true; return; }
        unsigned position = 0;
        if (bullets && !blank_) {
            const auto& first = atoms_[0];
            const bool icon_bullet = first.kind != Kind::Glyph && atom_count_ > 1 && atoms_[1].space;
            if (icon_bullet) {
                // Oversized item images remain inline; only the intended small
                // stat/status icons replace the hanging bullet.
                if (first.kind == Kind::Icon) { Put(first, left - 2, output); position = 1; }
            }
            if (position == 0) {
                const auto* dot = Glyph(7);
                Put({Kind::Glyph, 7, White, dot ? dot->advance : std::int16_t{5}, false}, left, output);
            }
        }
        int x = start_x;
        while (position < atom_count_ && !output.truncated) {
            if (atoms_[position].space) {
                if (x > start_x && x + atoms_[position].width <= width) x += atoms_[position].width;
                ++position; continue;
            }
            unsigned word_end = position;
            int word_width = 0;
            while (word_end < atom_count_ && !atoms_[word_end].space) word_width += atoms_[word_end++].width;
            if (x > start_x && x + word_width > width) {
                if (!NewLine(output)) return;
                x = start_x;
            }
            while (position < word_end && !output.truncated) {
                const auto& atom = atoms_[position++];
                if (x + atom.width > width && x > start_x) {
                    if (!NewLine(output)) return;
                    x = start_x;
                }
                if (atom.width > width - start_x) { output.truncated = true; return; }
                if (!Put(atom, x, output)) return;
                x += atom.width;
            }
        }
    }
};
} // namespace isaac_eid_ui

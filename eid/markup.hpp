#pragma once
#include <cstddef>

namespace isaac_eid {

enum class TokenKind { Text, LineBreak, Markup, Variable };

struct Token {
    TokenKind kind;
    const char* text;
    std::size_t size;
};

// Zero allocations; every source byte is preserved as text or an explicit
// directive. A renderer must resolve symbols/styles and variable substitutions.
// Unknown directives are returned, not silently discarded.
class MarkupReader {
public:
    MarkupReader(const char* text, std::size_t size) : text_(text), size_(text ? size : 0) {}

    bool Next(Token& out) {
        if (position_ >= size_) return false;
        const auto start = position_;
        const char current = text_[position_];
        if (current == '#' || current == '\n' || current == '\r') {
            ++position_;
            if (current == '\r' && position_ < size_ && text_[position_] == '\n') ++position_;
            out = {TokenKind::LineBreak, text_ + start, position_ - start};
            return true;
        }
        if (current == '{') {
            if (position_ + 1 < size_ && text_[position_ + 1] == '{') {
                for (auto end = position_ + 2; end + 1 < size_; ++end) {
                    if (text_[end] == '}' && text_[end + 1] == '}') {
                        out = {TokenKind::Markup, text_ + start + 2, end - start - 2};
                        position_ = end + 2;
                        return true;
                    }
                    if (text_[end] == '#' || text_[end] == '\n' || text_[end] == '{') break;
                }
            } else {
                for (auto end = position_ + 1; end < size_; ++end) {
                    const char c = text_[end];
                    if (c == '}' && end > position_ + 1) {
                        out = {TokenKind::Variable, text_ + start + 1, end - start - 1};
                        position_ = end + 1;
                        return true;
                    }
                    if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                          (c >= 'a' && c <= 'z') || c == '_' || c == '.')) break;
                }
            }
        }
        // An unmatched brace is literal text. Advance at least once so malformed
        // input cannot trap a render loop.
        ++position_;
        while (position_ < size_ && text_[position_] != '#' && text_[position_] != '\n' &&
               text_[position_] != '\r' && text_[position_] != '{') ++position_;
        out = {TokenKind::Text, text_ + start, position_ - start};
        return true;
    }

private:
    const char* text_;
    std::size_t size_;
    std::size_t position_ = 0;
};

} // namespace isaac_eid

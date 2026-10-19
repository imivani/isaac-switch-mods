#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
namespace isaac::item_menu {
enum class Kind : unsigned { All, Passive, Active, Familiar };
struct Item { unsigned id; Kind kind; const char* name; };
// Button numbers are read from the exact Switch DeviceNX::GetButtonName.
enum Button : unsigned { Left=0, Right=1, Up=2, Down=3, A=4, B=5, X=6, Y=7, L=8, ZL=9, LS=10, R=11, ZR=12, RS=13, Plus=14 };
constexpr std::uint32_t Bit(unsigned button) { return 1u << button; }
inline constexpr std::uint32_t ChordMask = Bit(LS)|Bit(RS)|Bit(Down);
inline constexpr std::uint32_t MenuMask = (1u << 15)-1;
inline constexpr unsigned PageSize = 10;
enum class Event { None, Close, Spawn, Toggle, Settings };
struct Controls {
    std::uint32_t previous{};
    std::uint64_t nextRepeat{};
    int repeatDirection{};
    bool released{};
    void Reset() { *this = {}; }
    std::uint32_t Poll(std::uint32_t buttons, std::uint64_t now) {
        if (!released) {
            if (!buttons) { released = true; previous = 0; }
            return 0;
        }
        std::uint32_t pressed = buttons & ~previous;
        const auto directions = buttons & (Bit(Up)|Bit(Down)|Bit(Left)|Bit(Right));
        const int direction = directions && !(directions & (directions-1)) ? static_cast<int>(directions) : 0;
        if (direction != repeatDirection) { repeatDirection = direction; nextRepeat = now + 380; }
        else if (direction && now >= nextRepeat) { pressed |= static_cast<unsigned>(direction); nextRepeat = now + 95; }
        previous = buttons;
        return pressed;
    }
};
class State {
    const Item* items_{};
    unsigned itemCount_{}, count_{}, selected_{}, modSelected_{}, modCount_{10};
    unsigned filtered_[1024]{};
    Kind kind_{};
    bool mods_{}, open_{};
    Controls controls_{};
    void Filter() {
        count_ = selected_ = 0;
        for (unsigned i=0;i<itemCount_ && count_<1024;++i)
            if (kind_ == Kind::All || items_[i].kind == kind_) filtered_[count_++] = i;
    }
    void Move(int delta) {
        const unsigned n = mods_ ? modCount_ : count_;
        if (!n) return;
        unsigned& selected = mods_ ? modSelected_ : selected_;
        const int next = static_cast<int>(selected) + delta;
        selected = static_cast<unsigned>((next % static_cast<int>(n) + static_cast<int>(n)) % static_cast<int>(n));
    }
    static char Letter(const char* name) {
        const unsigned char c = static_cast<unsigned char>(*name);
        return c>='a'&&c<='z' ? static_cast<char>(c-32) : c>='A'&&c<='Z' ? c : '#';
    }
    void Jump(int delta) {
        if (!count_ || mods_) return;
        const char current=Letter(Selected()->name);
        for (unsigned i=0;i<count_;++i) {
            Move(delta);
            if (Letter(Selected()->name)!=current) {
                if (delta<0) {
                    const char target=Letter(Selected()->name);
                    while(selected_>0 && Letter(items_[filtered_[selected_-1]].name)==target) --selected_;
                }
                return;
            }
        }
    }
public:
    void Open(const Item* items, unsigned count, bool mods=false) {
        items_=items; itemCount_=count; open_=true; mods_=mods; controls_.Reset(); Filter();
    }
    void Close() { open_=false; controls_.Reset(); }
    // Ignore every button until all are released (after a sub-page closes).
    void WaitForRelease() { controls_.Reset(); }
    bool Opened() const { return open_; }
    bool Mods() const { return mods_; }
    unsigned Selection() const { return mods_ ? modSelected_ : selected_; }
    unsigned Count() const { return mods_ ? modCount_ : count_; }
    void SetModCount(unsigned count) { modCount_ = count ? count : 1; if (modSelected_ >= modCount_) modSelected_ = 0; }
    Kind FilterKind() const { return kind_; }
    const Item* At(unsigned row) const { return row<count_ ? &items_[filtered_[row]] : nullptr; }
    const Item* Selected() const { return At(selected_); }
    Event Update(std::uint32_t buttons, std::uint64_t now, bool titleOnly=false) {
        if (!open_) return Event::None;
        const auto keys=controls_.Poll(buttons,now);
        if (keys & (Bit(B)|Bit(Plus))) { Close(); return Event::Close; }
        if (!titleOnly && (keys & Bit(X))) { mods_=!mods_; return Event::None; }
        if (keys & Bit(Up)) Move(-1);
        else if (keys & Bit(Down)) Move(1);
        else if (keys & (Bit(Left)|Bit(L))) Move(-static_cast<int>(PageSize));
        else if (keys & (Bit(Right)|Bit(R))) Move(PageSize);
        else if (keys & Bit(ZL)) Jump(-1);
        else if (keys & Bit(ZR)) Jump(1);
        else if (!mods_ && (keys & Bit(Y))) { kind_=static_cast<Kind>((static_cast<unsigned>(kind_)+1)%4); Filter(); }
        else if (mods_ && (keys & Bit(Y))) return Event::Settings;
        // Navigation never also confirms an item in the same polling pass.
        else if (keys & Bit(A)) return mods_ ? Event::Toggle : count_ ? Event::Spawn : Event::None;
        return Event::None;
    }
};
}

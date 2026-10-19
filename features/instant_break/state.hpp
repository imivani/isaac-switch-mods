#pragma once
#include <cstddef>
#include <cstdint>

namespace isaac::instant_break {
inline constexpr std::size_t MaxGrid = 448;
struct RoomIdentity {
    std::uintptr_t game{}, room{}, descriptor{};
    std::uint32_t runSeed{}, entryFrame{};
    std::int32_t index{}, dimension{};
    bool operator==(const RoomIdentity&) const = default;
};
struct Poop {
    std::uintptr_t address{};
    std::uint32_t seed{};
    std::int32_t variant{}, state{};
};
class State {
    struct Slot {
        Poop previous{};
        bool known{};
    } slots_[MaxGrid]{};
    RoomIdentity room_{};
    std::uint32_t frame_{};
    bool active_{};
public:
    void Reset() {
        for (auto& slot : slots_) slot = {};
        room_ = {}; frame_ = 0; active_ = false;
    }
    bool Begin(const RoomIdentity& room, std::uint32_t frame) {
        if (!active_ || !(room_ == room) || frame < frame_) Reset();
        if (active_ && frame == frame_) return false;
        room_ = room; frame_ = frame; active_ = true;
        return true;
    }
    void Forget(std::size_t index) { if (index < MaxGrid) slots_[index] = {}; }
    bool Observe(std::size_t index, const Poop& value, bool clear) {
        if (index >= MaxGrid) return false;
        auto& slot = slots_[index];
        if (!value.address || value.state < 0 || value.state >= 1000) {
            slot = {}; return false;
        }
        const auto& old = slot.previous;
        if (!slot.known || old.address != value.address || old.seed != value.seed ||
            old.variant != value.variant || value.state < old.state) {
            slot.previous = value; slot.known = true; return false;
        }
        // The Lua baseline is updated in combat and held fixed while clear.
        // Keeping it fixed also repeats native Destroy for tough/giant poop.
        if (!clear) { slot.previous = value; return false; }
        return value.state > old.state;
    }
};
}

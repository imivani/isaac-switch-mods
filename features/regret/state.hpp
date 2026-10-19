#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

// Independent implementation of the observed Regret Pedestals behavior.
// Engine pointers are identities only; this class never dereferences them.
namespace isaac::regret {

inline constexpr std::size_t kTrackedCapacity = 128;
inline constexpr std::size_t kGhostCapacity = 128;
inline constexpr std::uint8_t kFadeFrames = 60;

enum class Animation : std::uint8_t { Other, Idle, ShopIdle };

struct Identity {
    std::uintptr_t address{};
    std::uint32_t generation{};
    constexpr bool operator==(const Identity& other) const {
        return address == other.address && generation == other.generation;
    }
};

struct Pickup {
    Identity identity{};
    std::int32_t item{};
    float worldX{}, worldY{};
    Animation animation{Animation::Other};
};

struct Afterimage {
    Identity identity{};
    std::int32_t item{};
    float worldX{}, worldY{};
    Animation animation{Animation::Idle};
    std::uint8_t age{};

    constexpr float Alpha() const { return 1.0f - static_cast<float>(age) / kFadeFrames; }
    constexpr float Rise() const { return static_cast<float>(age) / 10.0f; }
};

struct Statistics {
    std::uint32_t trackingOverflow{};
    std::uint32_t afterimageOverflow{};
    std::uint32_t invalidPositions{};
    std::uint32_t incompleteSnapshots{};
};

class State {
public:
    // Use a real room entry serial, or a descriptor identity plus an explicit
    // Reset on re-entry. A reused Room* alone does not identify a new room.
    void SetRoom(std::uint64_t room) {
        if (room != room_) {
            ClearItems();
            room_ = room;
        }
    }

    void Reset() {
        ClearItems();
        room_ = 0;
        statistics_ = {};
    }

    void SetStopWithHourglass(bool enabled) { stopWithHourglass_ = enabled; }

    // A partial scan must finish with complete=false. Otherwise missing entries
    // would look like removals. Existing entries still receive safe fresh data.
    void BeginSnapshot(std::uint64_t room, bool primaryHourglassHeld) {
        SetRoom(room);
        primaryHourglassHeld_ = primaryHourglassHeld;
        for (std::size_t i = 0; i < trackedCount_; ++i) tracked_[i].seen = false;
    }

    void Observe(const Pickup& pickup) { Observe(pickup, primaryHourglassHeld_); }

    // Suitable for an update hook as well as a complete room scan. Existing
    // tracked items continue updating while Hourglass is held, matching upstream.
    void Observe(const Pickup& pickup, bool primaryHourglassHeld) {
        if (!pickup.identity.address) return;
        for (std::size_t i = 0; i < trackedCount_; ++i) {
            if (tracked_[i].pickup.identity == pickup.identity) {
                Update(tracked_[i], pickup);
                tracked_[i].seen = true;
                return;
            }
        }
        if (!room_ || !pickup.item || !Eligible(pickup.animation) ||
            (stopWithHourglass_ && primaryHourglassHeld)) return;
        if (!ValidPosition(pickup)) {
            ++statistics_.invalidPositions;
            return;
        }
        if (trackedCount_ == kTrackedCapacity) {
            ++statistics_.trackingOverflow;
            return;
        }
        tracked_[trackedCount_++] = {pickup, true, true};
    }

    void EndSnapshot(bool complete) {
        if (!complete) {
            ++statistics_.incompleteSnapshots;
            return;
        }
        std::size_t destination = 0;
        for (std::size_t i = 0; i < trackedCount_; ++i) {
            if (tracked_[i].seen) {
                if (i != destination) tracked_[destination] = tracked_[i];
                ++destination;
            } else {
                Emit(tracked_[i]);
            }
        }
        trackedCount_ = destination;
    }

    // Call while the engine object is still valid, with its final item/position.
    // Item 0 means it was collected/emptied: forget it without an afterimage.
    // Never read the engine object after forwarding its destructor/removal call.
    void Removed(const Pickup& finalPickup) {
        for (std::size_t i = 0; i < trackedCount_; ++i) {
            if (tracked_[i].pickup.identity == finalPickup.identity) {
                Update(tracked_[i], finalPickup);
                Emit(tracked_[i]);
                for (std::size_t j = i + 1; j < trackedCount_; ++j) tracked_[j - 1] = tracked_[j];
                --trackedCount_;
                return;
            }
        }
        // The engine may mark removal and then empty the item in the same
        // update. A later post-update/destructor snapshot can correct the
        // captured values before rendering, without touching a dead object.
        for (std::size_t i = 0; i < afterimageCount_; ++i) {
            if (!(afterimages_[i].identity == finalPickup.identity)) continue;
            if (!finalPickup.item || !ValidPosition(finalPickup)) {
                for (std::size_t j = i + 1; j < afterimageCount_; ++j) afterimages_[j - 1] = afterimages_[j];
                --afterimageCount_;
            } else {
                afterimages_[i].item = finalPickup.item;
                afterimages_[i].worldX = finalPickup.worldX;
                afterimages_[i].worldY = finalPickup.worldY;
            }
            return;
        }
    }

    // Render all current Afterimages, then advance once per post-render call.
    // Ages 0..59 render exactly 60 times, even with simultaneous expirations.
    void AdvanceRender() {
        std::size_t destination = 0;
        for (std::size_t i = 0; i < afterimageCount_; ++i) {
            auto afterimage = afterimages_[i];
            if (++afterimage.age < kFadeFrames) afterimages_[destination++] = afterimage;
        }
        afterimageCount_ = destination;
    }

    std::size_t TrackedCount() const { return trackedCount_; }
    std::size_t AfterimageCount() const { return afterimageCount_; }
    const Afterimage* GetAfterimage(std::size_t index) const {
        return index < afterimageCount_ ? &afterimages_[index] : nullptr;
    }
    const Statistics& Stats() const { return statistics_; }

private:
    struct Tracked {
        Pickup pickup{};
        bool seen{};
        bool validPosition{};
    };

    static bool Eligible(Animation animation) {
        return animation == Animation::Idle || animation == Animation::ShopIdle;
    }
    static bool ValidPosition(const Pickup& pickup) {
        constexpr float max = std::numeric_limits<float>::max();
        return pickup.worldX >= -max && pickup.worldX <= max &&
               pickup.worldY >= -max && pickup.worldY <= max;
    }
    void Update(Tracked& tracked, const Pickup& pickup) {
        // The list chosen when first tracked controls the disappearance style.
        // A reroll changes the live item's ID; it never emits the old item.
        const auto animation = tracked.pickup.animation;
        tracked.pickup = pickup;
        tracked.pickup.animation = animation;
        tracked.validPosition = ValidPosition(pickup);
        if (!tracked.validPosition) ++statistics_.invalidPositions;
    }
    void Emit(const Tracked& tracked) {
        const auto& pickup = tracked.pickup;
        if (!pickup.item || !tracked.validPosition) return;
        if (afterimageCount_ == kGhostCapacity) {
            // Drop the oldest afterimage, keeping memory fixed during mass clears.
            for (std::size_t i = 1; i < afterimageCount_; ++i) afterimages_[i - 1] = afterimages_[i];
            --afterimageCount_;
            ++statistics_.afterimageOverflow;
        }
        afterimages_[afterimageCount_++] = {pickup.identity, pickup.item, pickup.worldX, pickup.worldY, pickup.animation, 0};
    }
    void ClearItems() {
        trackedCount_ = 0;
        afterimageCount_ = 0;
        primaryHourglassHeld_ = false;
    }

    std::array<Tracked, kTrackedCapacity> tracked_{};
    std::array<Afterimage, kGhostCapacity> afterimages_{};
    std::size_t trackedCount_{}, afterimageCount_{};
    std::uint64_t room_{};
    bool stopWithHourglass_{true};
    bool primaryHourglassHeld_{};
    Statistics statistics_{};
};

} // namespace isaac::regret

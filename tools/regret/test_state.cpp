#include "../../features/regret/state.hpp"

#include <cassert>
#include <cmath>
#include <iostream>
#include <limits>

using isaac::regret::Animation;
using isaac::regret::Pickup;
using isaac::regret::State;

static Pickup Item(std::uintptr_t address, std::int32_t item = 1,
                   Animation animation = Animation::Idle, std::uint32_t generation = 1) {
    return {{address, generation}, item, 100.0f, 200.0f, animation};
}

static void TestFadeAndFinalPosition() {
    State state;
    state.BeginSnapshot(1, false);
    state.Observe(Item(10, 101));
    state.EndSnapshot(true);
    auto final = Item(10, 101);
    final.worldX = 120;
    final.worldY = 210;
    state.Removed(final);
    assert(state.TrackedCount() == 0 && state.AfterimageCount() == 1);
    for (int frame = 0; frame < 60; ++frame) {
        const auto* image = state.GetAfterimage(0);
        assert(image && image->item == 101 && image->age == frame);
        assert(image->worldX == 120 && image->worldY == 210);
        assert(std::abs(image->Alpha() - (1.0f - frame / 60.0f)) < 0.00001f);
        assert(std::abs(image->Rise() - frame / 10.0f) < 0.00001f);
        state.AdvanceRender();
    }
    assert(state.AfterimageCount() == 0 && !state.GetAfterimage(0));
}

static void TestCollectedAndSameFrameRemoval() {
    State state;
    state.BeginSnapshot(1, false);
    state.Observe(Item(10));
    state.Observe(Item(20));
    state.EndSnapshot(true);
    state.Removed(Item(10, 0));
    assert(state.AfterimageCount() == 0 && state.TrackedCount() == 1);
    state.BeginSnapshot(1, false);
    state.Observe(Item(20, 0));
    state.EndSnapshot(true);
    state.BeginSnapshot(1, false);
    state.EndSnapshot(true);
    assert(state.AfterimageCount() == 0 && state.TrackedCount() == 0);
    state.Removed(Item(10, 1)); // Duplicate removal cannot create a new image.
    assert(state.AfterimageCount() == 0);
}

static void TestRerollsAndAdmissionAnimation() {
    State state;
    state.BeginSnapshot(1, false);
    state.Observe(Item(10, 10, Animation::ShopIdle));
    state.Observe(Item(20, 20, Animation::Other));
    state.EndSnapshot(true);
    assert(state.TrackedCount() == 1);
    state.BeginSnapshot(1, false);
    state.Observe(Item(10, 11, Animation::Other));
    state.EndSnapshot(true);
    assert(state.AfterimageCount() == 0);
    state.Removed(Item(10, 12, Animation::Other));
    const auto* image = state.GetAfterimage(0);
    assert(image && image->item == 12 && image->animation == Animation::ShopIdle);
}

static void TestHourglassOnlyStopsNewTracking() {
    State state;
    state.BeginSnapshot(1, false);
    state.Observe(Item(10));
    state.EndSnapshot(true);
    state.BeginSnapshot(1, true);
    state.Observe(Item(10, 2));
    state.Observe(Item(20));
    state.EndSnapshot(true);
    assert(state.TrackedCount() == 1);
    state.Removed(Item(10, 2));
    assert(state.AfterimageCount() == 1);
    state.SetStopWithHourglass(false);
    state.Observe(Item(20), true);
    assert(state.TrackedCount() == 1);
}

static void TestIncompleteSnapshotAndRoomReset() {
    State state;
    state.BeginSnapshot(1, false);
    state.Observe(Item(10));
    state.EndSnapshot(true);
    state.BeginSnapshot(1, false);
    state.EndSnapshot(false);
    assert(state.TrackedCount() == 1 && state.AfterimageCount() == 0);
    assert(state.Stats().incompleteSnapshots == 1);
    state.BeginSnapshot(1, false);
    state.EndSnapshot(true);
    assert(state.TrackedCount() == 0 && state.AfterimageCount() == 1);
    state.SetRoom(2);
    assert(state.AfterimageCount() == 0);
    state.Observe(Item(20));
    state.SetRoom(0);
    state.Observe(Item(30));
    assert(state.TrackedCount() == 0);
}

static void TestPointerReuseUsesGeneration() {
    State state;
    state.BeginSnapshot(1, false);
    state.Observe(Item(10, 10, Animation::Idle, 111));
    state.EndSnapshot(true);
    state.Removed(Item(10, 10, Animation::Idle, 111));
    state.Observe(Item(10, 20, Animation::Idle, 222));
    state.Removed(Item(10, 0, Animation::Idle, 111)); // Final correction cannot remove the reused object.
    assert(state.TrackedCount() == 1 && state.AfterimageCount() == 0);
    state.Removed(Item(10, 0, Animation::Idle, 222));
    assert(state.TrackedCount() == 0 && state.AfterimageCount() == 0);
}

static void TestSimultaneousExpirationAndBounds() {
    State state;
    state.BeginSnapshot(1, false);
    for (std::size_t i = 0; i < 160; ++i) state.Observe(Item(100 + i, 100 + i));
    state.EndSnapshot(true);
    assert(state.TrackedCount() == 128 && state.Stats().trackingOverflow == 32);
    state.BeginSnapshot(1, false);
    state.EndSnapshot(true);
    assert(state.AfterimageCount() == 128);
    for (int i = 0; i < 59; ++i) state.AdvanceRender();
    assert(state.AfterimageCount() == 128);
    state.Observe(Item(900, 900));
    state.Removed(Item(900, 900));
    assert(state.AfterimageCount() == 128 && state.Stats().afterimageOverflow == 1);
    assert(state.GetAfterimage(0)->item == 101);
    state.AdvanceRender();
    assert(state.AfterimageCount() == 1 && state.GetAfterimage(0)->item == 900);
}

static void TestInvalidPositionCannotLeakOrDrawGarbage() {
    State state;
    state.BeginSnapshot(1, false);
    auto bad = Item(10);
    bad.worldX = std::numeric_limits<float>::quiet_NaN();
    state.Observe(bad);
    assert(state.TrackedCount() == 0);
    state.Observe(Item(10));
    bad.worldX = std::numeric_limits<float>::infinity();
    state.Removed(bad);
    assert(state.TrackedCount() == 0 && state.AfterimageCount() == 0);
    assert(state.Stats().invalidPositions == 2);
}

static void TestProceduralItemIdentityIsSigned() {
    State state;
    state.BeginSnapshot(1, false);
    state.Observe(Item(10, -123));
    state.Removed(Item(10, -123));
    assert(state.AfterimageCount() == 1 && state.GetAfterimage(0)->item == -123);
}

static void TestFinalSnapshotCorrectsRemoval() {
    State state;
    state.BeginSnapshot(1, false);
    state.Observe(Item(10, 1));
    state.Removed(Item(10, 1));
    assert(state.AfterimageCount() == 1);
    state.Removed(Item(10, 0));
    assert(state.AfterimageCount() == 0);
    state.Observe(Item(20, 2));
    state.Removed(Item(20, 2));
    state.AdvanceRender();
    auto final = Item(20, 3);
    final.worldX = 999;
    state.Removed(final);
    assert(state.AfterimageCount() == 1 && state.GetAfterimage(0)->item == 3);
    assert(state.GetAfterimage(0)->worldX == 999 && state.GetAfterimage(0)->age == 1);
}

int main() {
    TestFadeAndFinalPosition();
    TestCollectedAndSameFrameRemoval();
    TestRerollsAndAdmissionAnimation();
    TestHourglassOnlyStopsNewTracking();
    TestIncompleteSnapshotAndRoomReset();
    TestPointerReuseUsesGeneration();
    TestSimultaneousExpirationAndBounds();
    TestInvalidPositionCannotLeakOrDrawGarbage();
    TestProceduralItemIdentityIsSigned();
    TestFinalSnapshotCorrectsRemoval();
    std::cout << "Regret state: 10 scenario tests passed (60-frame fade, removals, collection, rerolls, Hourglass, rooms, identity reuse, bounded overflow, signed procedural IDs, final snapshot correction).\n";
}

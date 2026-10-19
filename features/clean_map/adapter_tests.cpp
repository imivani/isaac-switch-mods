#include "../../native/source/program/clean_map.hpp"
#include "../../native/source/program/feature_api.hpp"
#include "../../native/source/program/mod_settings.hpp"
#include "../../native/source/program/generated/build_profile.hpp"
#include "generated/native.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
namespace api = isaac::clean_map::native;
unsigned checks{}, queries{}, setting_reads{}, drawn{}, hidden{};
bool enabled = true;
struct Block {
    std::vector<std::uint64_t> data;
    explicit Block(std::size_t size) : data((size + 7) / 8) {}
    std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(data.data()); }
    std::size_t Size() const { return data.size() * 8; }
};
Block module(0xac0000), game(0x250000), next_game(0x250000);
void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
void CurrentGame(std::uintptr_t value) {
    std::memcpy(reinterpret_cast<void*>(module.Address() + isaac::profile::GameGlobal), &value, sizeof(value));
}
void SetupGuards() {
    std::memcpy(reinterpret_cast<void*>(module.Address()+api::MinimapRender), api::MinimapRenderGuard, sizeof(api::MinimapRenderGuard));
    std::memcpy(reinterpret_cast<void*>(module.Address()+api::MinimapFrameSetup), api::MinimapFrameSetupGuard, sizeof(api::MinimapFrameSetupGuard));
}
void SharedRenderBoundary(std::uintptr_t sprite, std::uintptr_t caller) {
    // Only the shared callback's argument capture is modeled; the predicate
    // below is the real production implementation linked into this test.
    if (isaac::ShouldHideCleanMapFrame(sprite, caller)) { ++hidden; return; }
    ++drawn;
}
}

extern "C" std::uint32_t svcQueryMemory(MemoryInfo* info, std::uint32_t* page, std::uint64_t address) {
    ++queries; *page = 0;
    for (const Block* block : {&module,&game,&next_game}) {
        if (address >= block->Address() && address < block->Address()+block->Size()) {
            *info = {block->Address(),block->Size(),Perm_R}; return 0;
        }
    }
    *info = {}; return 1;
}
namespace isaac {
bool IsModEnabled(ModFeature feature) {
    ++setting_reads;
    Check(feature == ModFeature::CleanMap, "minimap setting is independent of item animation and other mod flags");
    return enabled;
}
}

int main() {
    const auto caller = module.Address()+api::FrameReturn;
    const auto sprite = game.Address()+api::GameMinimapSprite;
    Check(!isaac::ShouldHideCleanMapFrame(sprite,caller), "uninitialized predicate preserves native drawing");
    Check(!isaac::InitializeCleanMap(0), "null module rejected");
    Check(!isaac::InitializeCleanMap(UINTPTR_MAX-0x1000), "overflowing module rejected without memory reads");
    SetupGuards(); CurrentGame(game.Address());
    Check(isaac::InitializeCleanMap(module.Address()), "exact native frame guards initialize");
    Check(isaac::InitializeCleanMap(module.Address()), "repeat initialization does not add a hook");
    SharedRenderBoundary(sprite,caller);
    Check(hidden == 1 && drawn == 0, "only native small Frame call is hidden while enabled");
    enabled = false; SharedRenderBoundary(sprite,caller);
    Check(hidden == 1 && drawn == 1, "turning setting off immediately restores original frame draw");
    enabled = true; SharedRenderBoundary(sprite,caller);
    Check(hidden == 2 && drawn == 1, "turning setting on immediately hides original frame draw");
    for (int offset = -256; offset <= 256; offset += 4) if (offset) {
        const auto old_queries = queries, old_reads = setting_reads;
        SharedRenderBoundary(sprite, static_cast<std::uintptr_t>(static_cast<std::intptr_t>(caller)+offset));
        Check(queries == old_queries && setting_reads == old_reads, "unrelated sprite callers leave without memory or setting queries");
    }
    Check(hidden == 2 && drawn == 129, "all adjacent render call sites retain original drawing");
    for (int offset = -512; offset <= 512; offset += 8) if (offset)
        Check(!isaac::ShouldHideCleanMapFrame(static_cast<std::uintptr_t>(static_cast<std::intptr_t>(sprite)+offset),caller),
              "expanded-map and other sprites cannot match the small-frame object");
    for (const auto invalid : {std::uintptr_t(0),std::uintptr_t(0x10000),game.Address()+1,UINTPTR_MAX-0x1000}) {
        CurrentGame(invalid);
        Check(!isaac::ShouldHideCleanMapFrame(sprite,caller), "invalid current game fails closed");
    }
    CurrentGame(next_game.Address());
    Check(!isaac::ShouldHideCleanMapFrame(sprite,caller), "old game sprite is not hidden after a game change");
    Check(isaac::ShouldHideCleanMapFrame(next_game.Address()+api::GameMinimapSprite,caller), "new game's verified small frame is selected");
    CurrentGame(game.Address());
    isaac::ResetCleanMap();
    Check(isaac::ShouldHideCleanMapFrame(sprite,caller), "no-resource reset preserves predicate if SDK unload fails");
    const auto* before = reinterpret_cast<const unsigned char*>(game.Address());
    bool unchanged = true;
    for (std::size_t index = 0; index < game.Size(); ++index) unchanged = unchanged && before[index] == 0;
    Check(unchanged, "complete game/sprite memory remains byte-identical");
    for (const auto site : {api::MinimapRender,api::MinimapFrameSetup,api::FrameCall}) {
        SetupGuards();
        *reinterpret_cast<unsigned char*>(module.Address()+site) ^= 1;
        Check(!isaac::InitializeCleanMap(module.Address()), "changed native function/setup/call guard disables feature");
        Check(!isaac::ShouldHideCleanMapFrame(sprite,caller), "failed guard preserves the original frame");
    }
    SetupGuards(); Check(isaac::InitializeCleanMap(module.Address()), "fresh exact module can initialize again");
    Check(isaac::ShouldHideCleanMapFrame(sprite,caller), "fresh module uses validated caller again");
    std::printf("PASS: %u checks; live toggle, exact caller/sprite scope, expanded-map exclusion, guard failures and zero game writes.\n",checks);
}

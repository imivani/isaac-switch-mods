// Host-side boundary tests. NativeSprite is replaced with a recording backend;
// these tests do not claim that a Switch texture or hook has been exercised.
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../native/source/program/boss_ui.cpp"

namespace {
struct Region { std::uintptr_t begin; std::size_t size; };
std::vector<Region> regions;
struct Call { std::string animation; int frame; isaac::NativeVec2 position, crop; isaac::boss_ui_data::Color color; };
struct SpriteState { std::string file, animation; int frame{}; isaac::boss_ui_data::Color color{1,1,1,1}; };
std::unordered_map<const isaac::NativeSprite*, SpriteState> sprites;
std::vector<Call> calls;
std::string failPath;
unsigned destroyed{}, rectangles{}, labels{};
std::string lastLabel;

void RegionFor(void* pointer, std::size_t size) { regions.push_back({reinterpret_cast<std::uintptr_t>(pointer), size}); }
template<class T, class Buffer> void Put(Buffer& buffer, std::size_t offset, T value) { std::memcpy(buffer.data() + offset, &value, sizeof(value)); }
__attribute__((aligned(16))) bool Boss(const void*) { return true; }
void Rect(void*, float, float, float, float, isaac::FeatureColor) { ++rectangles; }
void Text(void*, const char* value, float, float, isaac::FeatureColor, float) { ++labels; lastLabel = value; }
}

extern "C" Result svcQueryMemory(MemoryInfo* info, u32* page, u64 address) {
    for (const auto& region : regions) if (address >= region.begin && address - region.begin < region.size) {
        *info = {}; info->addr = region.begin; info->size = region.size; info->perm = Perm_R; *page = 0; return 0;
    }
    return 1;
}

namespace isaac {
bool NativeSprite::Create(std::uintptr_t module) { module_ = module; created_ = module != 0; return created_; }
bool NativeSprite::Load(const char* path) {
    sprites[this].file = path;
    loaded_ = created_ && (failPath.empty() || (failPath != "*" && std::string(path).find(failPath) == std::string::npos));
    return loaded_;
}
bool NativeSprite::SetSpritesheet(unsigned, const char*) { return loaded_; }
bool NativeSprite::SetSpritesheetFromGameString(unsigned, std::uintptr_t) { return loaded_; }
void NativeSprite::LoadGraphics() {}
void NativeSprite::SetScale(float x, float y) { assert(x == 1 && y == 1); }
bool NativeSprite::HasAnimation(const char* name) const {
    for (const auto& animation : data::Animations) if (sprites[this].file == animation.path)
        for (unsigned i = 0; i < animation.sequenceCount; ++i)
            if (std::strcmp(data::Sequences[animation.sequenceStart + i].name, name) == 0) return true;
    return false;
}
bool NativeSprite::SetFrame(const char* name, int frame) {
    if (!loaded_ || !HasAnimation(name)) return false;
    for (const auto& animation : data::Animations) if (sprites[this].file == animation.path)
        for (unsigned i = 0; i < animation.sequenceCount; ++i) {
            const auto& sequence = data::Sequences[animation.sequenceStart + i];
            if (std::strcmp(sequence.name, name) == 0) assert(frame >= 0 && static_cast<unsigned>(frame) < sequence.frames);
        }
    sprites[this].animation = name; sprites[this].frame = frame; return true;
}
void NativeSprite::SetTint(float r, float g, float b, float a) { sprites[this].color = {r,g,b,a}; }
void NativeSprite::Render(NativeVec2 position, NativeVec2, NativeVec2 bottom) {
    assert(loaded_);
    const auto& state = sprites[this];
    calls.push_back({state.animation, state.frame, position, bottom, state.color});
}
void NativeSprite::Destroy() {
    if (created_) ++destroyed;
    sprites.erase(this); created_ = loaded_ = false; module_ = 0;
}
bool CopyNativeString(std::uintptr_t, char* out, std::size_t size) { if (size) out[0] = 0; return false; }
}

#include "test_segments.hpp"

int main() {
    using namespace isaac;
    unsigned cases = TestOwnership();
    alignas(8) std::array<std::byte, 0x2000> room{};
    alignas(8) std::array<std::byte, 0x400> entity{};
    alignas(8) std::array<std::byte, 0xa0> vtable{};
    std::uintptr_t entityList[] = {reinterpret_cast<std::uintptr_t>(entity.data())};
    RegionFor(room.data(), room.size()); RegionFor(entity.data(), entity.size());
    RegionFor(vtable.data(), vtable.size()); RegionFor(entityList, sizeof(entityList));
    Put(room, profile::RoomEntities, reinterpret_cast<std::uintptr_t>(entityList));
    Put(room, profile::RoomEntityCount, unsigned{1});
    Put(entity, 0, reinterpret_cast<std::uintptr_t>(vtable.data()));
    Put(entity, profile::EntityExists, std::uint8_t{1});
    Put(entity, profile::EntityType, unsigned{20});
    Put(entity, profile::EntityHp, 100.0f); Put(entity, profile::EntityMaxHp, 100.0f);
    Put(vtable, profile::EntityIsBossVtableSlot, reinterpret_cast<std::uintptr_t>(&Boss));
    FeatureFrame frame{reinterpret_cast<std::uintptr_t>(&Boss) - 0x1000, 0,
                       reinterpret_cast<std::uintptr_t>(room.data()), 480, 270, {nullptr, Text, Rect}};
    Snapshot snapshot[MaxBosses]{};
    bool gideon = false;
    // Invisible-but-alive bosses stay represented, matching the upstream mod.
    assert(SnapshotBosses(frame, snapshot, gideon) == 1 && !gideon); ++cases;
    Put(entity, EntityFlags, std::uint64_t{1} << 29);
    assert(SnapshotBosses(frame, snapshot, gideon) == 0);
    Put(entity, EntityFlags, std::uint64_t{1} << 31);
    assert(SnapshotBosses(frame, snapshot, gideon) == 0); ++cases;
    Put(entity, EntityFlags, std::uint64_t{0});
    Put(entity, profile::EntityHp, std::numeric_limits<float>::quiet_NaN());
    assert(SnapshotBosses(frame, snapshot, gideon) == 0);
    Put(entity, profile::EntityHp, std::numeric_limits<float>::infinity());
    assert(SnapshotBosses(frame, snapshot, gideon) == 0); ++cases;
    Put(entity, profile::EntityHp, 100.0f);
    Put(vtable, profile::EntityIsBossVtableSlot, std::uintptr_t{0x10000});
    assert(SnapshotBosses(frame, snapshot, gideon) == 0); ++cases;
    Put(vtable, profile::EntityIsBossVtableSlot, reinterpret_cast<std::uintptr_t>(&Boss));
    Put(room, profile::RoomEntityCount, unsigned{MaxEntities + 1});
    assert(SnapshotBosses(frame, snapshot, gideon) == 0); ++cases;
    Put(room, profile::RoomEntityCount, unsigned{1});
    Put(entity, profile::EntityType, unsigned{907});
    assert(SnapshotBosses(frame, snapshot, gideon) == 1 && gideon); ++cases;

    Snapshot value{}; value.hp = value.maximum = 100;
    assert(HpFrame(value) == 1);
    value.hp = 50; assert(HpFrame(value) == 50);
    value.hp = 0.01f; assert(HpFrame(value) == 99);
    value.hp = 200; assert(HpFrame(value) == 1); ++cases;
    const auto& mother = Definition(45, 10);
    assert(std::strstr(Icon(mother, value, 4, 6), "mausoleum_mom"));
    assert(std::strstr(Icon(mother, value, 0, 6), "/mom.png"));
    const auto& isaacBoss = Definition(102, 0);
    std::strcpy(value.animation, "3Attack");
    assert(std::strstr(Icon(isaacBoss, value, 0, 10), "isaac_phase3"));
    value.hp = 39;
    assert(std::strstr(Icon(Definition(917, 0), value, 0, 4), "colostomia_phase2")); ++cases;

    currentModule = 0x100000;
    Entry entry{};
    entry.definition = &Definition(20, 0); entry.value = value; entry.value.hp = 50;
    entry.changedFrame = -100; entry.startFrame = 0;
    LoadStyle(entry, 0); LoadIcon(entry, entry.definition->sprite);
    calls.clear();
    assert(DrawBar(entry, {100,200}, 120, "bg100", "charge100", "overlay100", 12));
    assert(calls.size() == 4 && calls[0].animation == "bg100" && calls[1].animation == "charge100");
    assert(calls[1].frame == 50 && std::fabs(calls[1].crop.x - 61) < 0.001f);
    assert(calls[2].animation == "endDeco" && std::fabs(calls[2].position.x - 159) < 0.001f);
    assert(calls[3].animation == "idle" && calls[3].position.x == 92);
    assert(labels == 0); ++cases;
    calls.clear(); entry.changedFrame = 11; entry.healing = false;
    assert(DrawBar(entry, {100,200}, 120, "bg100", "charge100", "overlay100", 13));
    assert(std::fabs(calls[1].color.r - .48f) < .001f); ++cases;
    Clear(entry);
    entry.definition = &Definition(950, 1); entry.value = value; entry.value.hp = 50;
    entry.startFrame = 0; LoadStyle(entry, entry.definition->style); LoadIcon(entry, entry.definition->sprite);
    calls.clear();
    assert(DrawBar(entry, {100,200}, 120, "bg100", "charge100", "overlay100", 7));
    assert(calls[0].frame == 0 && calls[1].frame == 3 && calls[3].frame == 3); ++cases;
    Clear(entry);

    failPath = "dogma_bosshp";
    entry.definition = &Definition(950, 1); const auto oldFailures = status.assetFailures;
    LoadStyle(entry, entry.definition->style);
    assert(entry.barReady && entry.actualStyle == 0 && status.assetFailures == oldFailures + 1); ++cases;
    Clear(entry); failPath = "*";
    entry.definition = &Definition(20, 0); LoadStyle(entry, 0);
    assert(!entry.barReady);
    value.hp = value.maximum;
    Fallback(frame, value, {100,200}, 120);
    assert(rectangles == 3 && labels == 1 && lastLabel == "100%"); ++cases;
    Clear(entry); assert(destroyed > 0); ++cases;
    assert(sprites.empty());
    std::printf("Boss UI boundary scenarios: %u PASS (recording backend; no hardware claim)\n", cases);
}

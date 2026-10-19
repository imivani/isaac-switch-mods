#include "boss_ui.hpp"
#include "native_sprite.hpp"
#include "generated/build_profile.hpp"
#include "generated/boss_ui_data.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace isaac {
namespace {
using namespace feature_memory;
namespace data = boss_ui_data;

constexpr unsigned MaxBosses = 28;
constexpr unsigned MaxEntities = 4096;
constexpr unsigned MaxParentDepth = 64;
constexpr unsigned BarsPerRow = 7;
constexpr std::uintptr_t EntityFlags = 0x1b8;
constexpr std::uintptr_t EntityInitSeed = 0x3e8;
constexpr std::uintptr_t EntityBirthFrame = 0x2f4;
constexpr std::uintptr_t EntityAnimation = 0x88;
constexpr std::uintptr_t EntityParent = 0x390;
constexpr std::uintptr_t EntityChild = 0x398;
constexpr std::uintptr_t GameFrame = 0x24f99c;
constexpr std::uintptr_t GetAbsoluteStage = 0x3e7f3c;

struct Snapshot {
    // address is only an identity token after the snapshot has been made.
    // Rendering never dereferences a previously retained entity pointer.
    std::uintptr_t address{};
    std::uint32_t seed{}, birth{}, type{}, variant{};
    float hp{}, maximum{};
    char animation[96]{};
};

struct Entry {
    NativeSprite bar, icon, overlay;
    Snapshot value{};
    bool active{}, barReady{}, iconReady{}, overlayReady{};
    unsigned requestedStyle{}, actualStyle{}, iconAnimation{};
    const char* requestedIcon{};
    const data::Boss* definition{};
    int startFrame{}, changedFrame{-100};
    bool healing{};
};

constinit Entry entries[MaxBosses]{};
constinit BossUiStatus status{};
std::uintptr_t currentModule{}, currentGame{}, currentRoom{};
int previousFrame{-1};
// Rebuilt on every snapshot. This is only a membership index, never a retained
// entity reference or a call to the game's unbounded GetLastParent traversal.
std::uintptr_t roomEntityIndex[MaxEntities]{};

template<class T> T Peek(std::uintptr_t address, std::uintptr_t offset = 0) {
    // The caller validated the containing object once, before taking a copy.
    T value{};
    std::memcpy(&value, reinterpret_cast<const void*>(address + offset), sizeof(T));
    return value;
}

bool Pointer(std::uintptr_t address) { return address > 0x10000 && !(address & 7); }
bool PositiveHealth(float hp) { return hp > 0.0f && hp <= 100000000.0f; }

bool Same(const Snapshot& a, const Snapshot& b) {
    return a.address == b.address && a.seed == b.seed && a.birth == b.birth &&
           a.type == b.type && a.variant == b.variant;
}

void Clear(Entry& entry) {
    entry.bar.Destroy();
    entry.icon.Destroy();
    entry.overlay.Destroy();
    entry.value = {};
    entry.active = entry.barReady = entry.iconReady = entry.overlayReady = false;
    entry.requestedStyle = entry.actualStyle = entry.iconAnimation = 0;
    entry.requestedIcon = nullptr;
    entry.definition = nullptr;
    entry.startFrame = 0;
    entry.changedFrame = -100;
    entry.healing = false;
}

const data::Boss& Definition(unsigned type, unsigned variant) {
    for (const auto& definition : data::Bosses)
        if (definition.type == type && definition.variant == variant) return definition;
    return data::DefaultBoss;
}

bool Ignored(const Snapshot& boss, std::uintptr_t parent, std::uintptr_t child) {
    for (const auto& value : data::Ignores)
        if (value.type == boss.type && value.variant == boss.variant) return true;
    // These exact upstream rules distinguish shared-health body pieces from
    // independently damageable bosses such as Larry Jr., Gemini and Steven.
    // Only the current entity's link is tested; no linked pointer is read here.
    // A new head after Turdlet splits must become eligible immediately.
    if (parent)
        for (const auto& value : data::IgnoreWhenParent)
            if (value.type == boss.type && value.variant == boss.variant) return true;
    if (!child)
        for (const auto& value : data::IgnoreWithoutChild)
            if (value.type == boss.type && value.variant == boss.variant) return true;
    // These upstream rules only depend on the already verified current
    // animation string. Other special-state fields are not guessed here.
    if (boss.type == 406 && (std::strcmp(boss.animation, "Final") == 0 ||
                            std::strcmp(boss.animation, "Death") == 0)) return true;
    if (boss.type == 903 && boss.variant == 1 && std::strstr(boss.animation, "Stun")) return true;
    if (boss.type == 912 && boss.variant == 0 && std::strcmp(boss.animation, "Transition") == 0) return true;
    if (boss.type == 950 && boss.variant == 1 && std::strstr(boss.animation, "Destroyed")) return true;
    return false;
}

bool HasDeliriumAncestor(std::uintptr_t parent, unsigned count) {
    std::uintptr_t visited[MaxParentDepth]{};
    unsigned depth = 0;
    while (parent && depth < MaxParentDepth) {
        // Being in readable memory alone does not prove this is an entity.
        // Follow only this frame's room members, and stop at cycles/stale links.
        if (!Pointer(parent) ||
            !std::binary_search(roomEntityIndex, roomEntityIndex + count, parent)) return false;
        for (unsigned i = 0; i < depth; ++i) if (visited[i] == parent) return false;
        if (!Readable(parent, EntityParent + sizeof(std::uintptr_t))) return false;
        visited[depth++] = parent;
        if (Peek<unsigned>(parent, profile::EntityType) == 412) return true;
        parent = Peek<std::uintptr_t>(parent, EntityParent);
    }
    return false;
}

unsigned SnapshotBosses(const FeatureFrame& frame, Snapshot (&out)[MaxBosses], bool& gideon) {
    std::uintptr_t entities{};
    std::uint32_t count{};
    if (!TryRead(frame.room + profile::RoomEntities, entities) ||
        !TryRead(frame.room + profile::RoomEntityCount, count) || count > MaxEntities ||
        (count && (!Pointer(entities) || !Readable(entities, std::size_t(count) * sizeof(std::uintptr_t))))) return 0;
    if (count) std::memcpy(roomEntityIndex, reinterpret_cast<const void*>(entities), std::size_t(count) * sizeof(std::uintptr_t));
    std::sort(roomEntityIndex, roomEntityIndex + count);
    unsigned found = 0;
    for (unsigned i = 0; i < count; ++i) {
        const auto address = Peek<std::uintptr_t>(entities + i * sizeof(std::uintptr_t));
        if (!Pointer(address) || !Readable(address, EntityInitSeed + 4) ||
            !Peek<std::uint8_t>(address, profile::EntityExists) || Peek<std::uint8_t>(address, profile::EntityDead)) continue;
        const auto type = Peek<unsigned>(address, profile::EntityType);
        if (type == 907) gideon = true;
        if (Peek<std::uint64_t>(address, EntityFlags) & ((1ull << 29) | (1ull << 31))) continue;
        const auto vtable = Peek<std::uintptr_t>(address);
        if (!Pointer(vtable) || !Readable(vtable, profile::EntityIsBossVtableSlot + 8)) continue;
        const auto isBoss = Peek<std::uintptr_t>(vtable, profile::EntityIsBossVtableSlot);
        if ((isBoss & 3) || isBoss < frame.module || isBoss >= frame.module + profile::TextSize ||
            !reinterpret_cast<bool (*)(const void*)>(isBoss)(reinterpret_cast<const void*>(address))) continue;
        Snapshot value{};
        value.address = address;
        value.seed = Peek<std::uint32_t>(address, EntityInitSeed);
        value.birth = Peek<std::uint32_t>(address, EntityBirthFrame);
        value.type = type;
        value.variant = Peek<unsigned>(address, profile::EntityVariant);
        value.hp = Peek<float>(address, profile::EntityHp);
        value.maximum = Peek<float>(address, profile::EntityMaxHp);
        if (!PositiveHealth(value.hp) || !PositiveHealth(value.maximum)) continue;
        const auto animation = Peek<std::uintptr_t>(address, EntityAnimation);
        if (Pointer(animation)) CopyNativeString(animation, value.animation, sizeof(value.animation));
        const auto parent = Peek<std::uintptr_t>(address, EntityParent);
        const auto child = Peek<std::uintptr_t>(address, EntityChild);
        if (Ignored(value, parent, child) || HasDeliriumAncestor(parent, count)) continue;
        if (found < MaxBosses) out[found++] = value;
        else ++status.omittedBosses;
    }
    return found;
}

bool Condition(const data::Rule& rule, const Snapshot& value, int stageType, int absoluteStage) {
    using Kind = data::Condition;
    switch (rule.condition) {
    case Kind::AnimationStarts: return std::strncmp(value.animation, rule.text, std::strlen(rule.text)) == 0;
    case Kind::AnimationEnds: {
        const auto length = std::strlen(value.animation), suffix = std::strlen(rule.text);
        return length >= suffix && std::strcmp(value.animation + length - suffix, rule.text) == 0;
    }
    case Kind::AnimationContains: return std::strstr(value.animation, rule.text) != nullptr;
    case Kind::AnimationEquals: return std::strcmp(value.animation, rule.text) == 0;
    case Kind::HpBelow: return value.hp < rule.value;
    case Kind::PercentBelow: return value.hp / value.maximum * 100.0f < rule.value;
    case Kind::StageType: return stageType == static_cast<int>(rule.value);
    case Kind::AbsoluteStage: return absoluteStage == static_cast<int>(rule.value);
    }
    return false;
}

const char* Icon(const data::Boss& definition, const Snapshot& value, int stageType, int absoluteStage) {
    for (unsigned i = 0; i < definition.ruleCount; ++i) {
        const auto& rule = data::Rules[definition.ruleStart + i];
        if (Condition(rule, value, stageType, absoluteStage)) return rule.sprite;
    }
    return definition.sprite;
}

bool Load(NativeSprite& sprite, unsigned animation, const char* image) {
    const auto& definition = data::Animations[animation];
    if (!sprite.Create(currentModule) || !sprite.Load(definition.path)) return false;
    if (image) {
        for (unsigned layer = 0; layer < definition.layers; ++layer)
            if (!sprite.SetSpritesheet(layer, image)) return false;
        sprite.LoadGraphics();
    }
    sprite.SetTint(1, 1, 1, 1);
    sprite.SetScale(1, 1);
    // Reject missing names at load time, including compact-row sequences.
    for (unsigned i = 0; i < definition.sequenceCount; ++i)
        if (!sprite.SetFrame(data::Sequences[definition.sequenceStart + i].name, 0)) return false;
    return true;
}

void LoadStyle(Entry& entry, unsigned requested) {
    entry.requestedStyle = requested;
    entry.actualStyle = requested;
    const auto& style = data::Styles[requested];
    entry.barReady = Load(entry.bar, style.animation, style.sprite);
    if (!entry.barReady) {
        ++status.assetFailures;
        if (requested != 0) {
            entry.actualStyle = 0;
            entry.barReady = Load(entry.bar, data::Styles[0].animation, data::Styles[0].sprite);
            if (!entry.barReady) ++status.assetFailures;
        }
    }
    entry.overlayReady = false;
    entry.overlay.Destroy();
    const auto& actual = data::Styles[entry.actualStyle];
    if (entry.barReady && actual.overlayAnimation >= 0) {
        entry.overlayReady = Load(entry.overlay, static_cast<unsigned>(actual.overlayAnimation), actual.overlaySprite);
        if (!entry.overlayReady) ++status.assetFailures;
    }
}

void LoadIcon(Entry& entry, const char* icon) {
    entry.requestedIcon = icon;
    entry.iconAnimation = entry.definition->animation;
    entry.iconReady = Load(entry.icon, entry.iconAnimation, icon);
    if (!entry.iconReady) {
        ++status.assetFailures;
        entry.iconAnimation = data::DefaultBoss.animation;
        entry.iconReady = Load(entry.icon, entry.iconAnimation, data::DefaultBoss.sprite);
        if (!entry.iconReady) ++status.assetFailures;
    }
}

bool SetFrame(NativeSprite& sprite, unsigned animation, const char* name, bool animated, unsigned elapsed, unsigned hpFrame) {
    const auto& asset = data::Animations[animation];
    for (unsigned i = 0; i < asset.sequenceCount; ++i) {
        const auto& sequence = data::Sequences[asset.sequenceStart + i];
        if (std::strcmp(sequence.name, name) != 0) continue;
        const unsigned frame = animated ? elapsed % sequence.frames :
                               hpFrame < sequence.frames ? hpFrame : sequence.frames - 1;
        return sprite.SetFrame(name, static_cast<int>(frame));
    }
    return false;
}

unsigned HpFrame(const Snapshot& value) {
    float percent = value.hp / value.maximum * 100.0f;
    if (percent >= 99.0f) return 1;
    if (percent < 0.0f) percent = 0.0f;
    const unsigned integer = static_cast<unsigned>(percent);
    const unsigned ceil = integer + (percent > static_cast<float>(integer) ? 1u : 0u);
    return 100 - ceil;
}

void Tint(NativeSprite& sprite, data::Color color) { sprite.SetTint(color.r, color.g, color.b, color.a); }

bool DrawBar(Entry& entry, NativeVec2 position, float width, const char* background,
             const char* charge, const char* overlay, int now) {
    if (!entry.barReady) return false;
    const auto& style = data::Styles[entry.actualStyle];
    const unsigned hpFrame = HpFrame(entry.value);
    const unsigned elapsed = now >= entry.startFrame ? static_cast<unsigned>(now - entry.startFrame) : 0;
    entry.bar.SetTint(1, 1, 1, 1);
    if (!SetFrame(entry.bar, style.animation, background, style.animated, elapsed, hpFrame)) return false;
    entry.bar.Render(position);
    auto tint = style.idle;
    if (now >= entry.changedFrame && now - entry.changedFrame <= 7 && (now & 1)) tint = entry.healing ? style.heal : style.hit;
    Tint(entry.bar, tint);
    if (!SetFrame(entry.bar, style.animation, charge, style.animated, elapsed, hpFrame)) return false;
    const float lostWidth = width / 100.0f * static_cast<float>(hpFrame);
    entry.bar.Render(position, {}, {1.0f + lostWidth, 0.0f});
    if (!SetFrame(entry.bar, style.animation, "endDeco", style.animated, elapsed, hpFrame)) return false;
    entry.bar.Render({position.x + width - 1.0f - lostWidth, position.y});
    entry.bar.SetTint(1, 1, 1, 1);
    if (entry.overlayReady) {
        if (SetFrame(entry.overlay, static_cast<unsigned>(style.overlayAnimation), overlay, style.overlayAnimated, elapsed, hpFrame))
            entry.overlay.Render(position);
        else { entry.overlayReady = false; ++status.assetFailures; }
    }
    if (entry.iconReady) {
        const bool animated = entry.iconAnimation == entry.definition->animation && entry.definition->animated;
        if (SetFrame(entry.icon, entry.iconAnimation, "idle", animated, elapsed, hpFrame))
            entry.icon.Render({position.x + entry.definition->x - 3.0f, position.y + entry.definition->y});
        else { entry.iconReady = false; ++status.assetFailures; }
    }
    return true;
}

void Fallback(const FeatureFrame& frame, const Snapshot& boss, NativeVec2 position, float width) {
    // A failed animation must not leave the player without a health display
    // after the ordinary vanilla textures have been replaced with transparency.
    float fraction = boss.hp / boss.maximum;
    if (fraction > 1.0f) fraction = 1.0f;
    frame.draw.Rect(position.x - 1, position.y - 5, position.x + width + 1, position.y + 3, {0.08f, 0.04f, 0.04f, 1});
    frame.draw.Rect(position.x, position.y - 4, position.x + width, position.y + 2, {0.25f, 0.08f, 0.08f, 1});
    frame.draw.Rect(position.x, position.y - 4, position.x + width * fraction, position.y + 2, {0.8f, 0.0f, 0.0f, 1});
    char text[12]{};
    const float percentage = fraction * 100.0f;
    const unsigned integer = static_cast<unsigned>(percentage);
    std::snprintf(text, sizeof(text), "%u%%", integer + (percentage > static_cast<float>(integer) ? 1u : 0u));
    frame.draw.Text(text, position.x + width * 0.5f - 5.0f, position.y - 5, {1, 1, 1, 1}, 0.45f);
}
} // namespace

void RenderBossUi(const FeatureFrame& frame) {
    status.renderedBars = status.fallbackBars = status.omittedBosses = 0;
    if (!frame.module || !Pointer(frame.game) || !Pointer(frame.room) ||
        !(frame.width >= 200 && frame.width <= 4096 && frame.height >= 120 && frame.height <= 4096)) return;
    // Teardown belongs to the module-unload hook while native code is alive.
    if (currentModule && currentModule != frame.module) return;
    currentModule = frame.module;
    int now{};
    if (!TryRead(frame.game + GameFrame, now) || now < 0) return;
    if (currentGame != frame.game || currentRoom != frame.room || now < previousFrame) {
        for (auto& entry : entries) Clear(entry);
        currentGame = frame.game;
        currentRoom = frame.room;
    }
    previousFrame = now;
    Snapshot snapshot[MaxBosses]{};
    bool gideon = false;
    unsigned count = SnapshotBosses(frame, snapshot, gideon);
    if (gideon) count = 0; // Keep the original special wave bar intact.

    // Drop disappeared identities without ever following their old pointers.
    for (auto& entry : entries) {
        if (!entry.active) continue;
        bool present = false;
        for (unsigned i = 0; i < count; ++i) if (Same(entry.value, snapshot[i])) { present = true; break; }
        if (!present) Clear(entry);
    }
    if (!count) return;
    status.attempted = true;
    status.nativeSpritesReady = true;
    const int stageType = Read<int>(frame.game, 4);
    // Verified getter accounts for Greed floors; the saved-stage-seed index
    // formula is not the same thing as Level::GetAbsoluteStage().
    const int absoluteStage = reinterpret_cast<int (*)(const void*)>(frame.module + GetAbsoluteStage)(reinterpret_cast<const void*>(frame.game));
    Entry* ordered[MaxBosses]{};
    for (unsigned i = 0; i < count; ++i) {
        Entry* selected = nullptr;
        for (auto& entry : entries) if (entry.active && Same(entry.value, snapshot[i])) { selected = &entry; break; }
        if (!selected) {
            for (auto& entry : entries) if (!entry.active) { selected = &entry; break; }
            if (!selected) continue;
            selected->active = true;
            selected->value = snapshot[i];
            selected->definition = &Definition(snapshot[i].type, snapshot[i].variant);
            selected->startFrame = now;
            selected->changedFrame = now - 8;
            LoadStyle(*selected, selected->definition->style);
        } else if (selected->value.hp != snapshot[i].hp) {
            selected->healing = snapshot[i].hp > selected->value.hp;
            selected->changedFrame = now;
        }
        selected->value = snapshot[i];
        const auto* icon = Icon(*selected->definition, selected->value, stageType, absoluteStage);
        if (selected->requestedIcon != icon) LoadIcon(*selected, icon);
        ordered[i] = selected;
    }

    // Exact upstream default sizes, 15-pixel half-spacing, 17-pixel bottom
    // padding and 20-pixel row steps. No extra names or diagnostic headings.
    const unsigned rows = (count + BarsPerRow - 1) / BarsPerRow;
    for (unsigned row = 0; row < rows; ++row) {
        unsigned inRow = count - row * BarsPerRow;
        if (inRow > BarsPerRow) inRow = BarsPerRow;
        const float width = inRow <= 2 ? 120.0f : inRow <= 4 ? 60.0f : 30.0f;
        const char* background = inRow <= 2 ? "bg100" : inRow <= 4 ? "bg50" : "bg25";
        const char* charge = inRow <= 2 ? "charge100" : inRow <= 4 ? "charge50" : "charge25";
        const char* overlay = inRow <= 2 ? "overlay100" : inRow <= 4 ? "overlay50" : "overlay25";
        const float left = frame.width * 0.5f - static_cast<float>(inRow) * width * 0.5f - static_cast<float>(inRow - 1) * 15.0f;
        const float top = frame.height - 17.0f - static_cast<float>(rows - 1 - row) * 20.0f;
        for (unsigned slot = 0; slot < inRow; ++slot) {
            Entry* entry = ordered[row * BarsPerRow + slot];
            if (!entry) continue;
            const NativeVec2 position{left + static_cast<float>(slot) * (width + 30.0f), top};
            if (!DrawBar(*entry, position, width, background, charge, overlay, now)) {
                if (entry->barReady) { entry->barReady = false; ++status.assetFailures; }
                Fallback(frame, entry->value, position, width);
                ++status.fallbackBars;
            }
            if (!entry->barReady || !entry->iconReady) status.nativeSpritesReady = false;
            ++status.renderedBars;
        }
    }
}

void ResetBossUi() {
    for (auto& entry : entries) Clear(entry);
    currentModule = currentGame = currentRoom = 0;
    previousFrame = -1;
    status = {};
}

BossUiStatus GetBossUiStatus() { return status; }

} // namespace isaac

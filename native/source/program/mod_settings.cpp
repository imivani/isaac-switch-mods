#include "mod_settings.hpp"
#include "specialist.hpp"
#include "instant_break.hpp"
namespace isaac {
namespace {
std::uint32_t s_Enabled = (1u << ModCount) - 1;
std::uint32_t s_Available = (1u << ModCount) - 1;
constexpr const char* Names[] = {
    "External Item Descriptions", "Hit Counter", "Enhanced Boss Bars", "Planetarium Chance",
    "Regret Pedestals", "Time Machine", "Machine Lock", "Greed Machine Never Jams",
    "Instant Fires and Poops", "Specialist Dance", "Animated Items", "Unique Birthright", "Clean Minimap",
    "Repentance+ Rules"
};
constexpr const char* Descriptions[] = {
    "Descriptions for nearby items and rooms. Hold map to browse your character and inventory.",
    "Count the hits you take this run and on this floor. Press Y here to move it and change its size and opacity.",
    "Show the enhanced boss portraits and health bars.",
    "Show your Planetarium chance beside the other stats.",
    "Leave a fading picture when a pedestal item disappears.",
    "Speed up repeated machine and beggar uses in safe, cleared rooms while you hold toward them.",
    "Hold toward a machine or beggar and you stay in place against it, even when not lined up exactly. Release, push away, shoot or get into danger to move again.",
    "The Greed Donation Machine never jams. A machine that jammed earlier in this run works again from the next floor.",
    "Break ordinary fires and poop with one successful hit.",
    "Dance and play the Specialist music beside a high-quality item. Turning this off ends the dance.",
    "Animate supported collectible pictures on pedestals and when held above your head.",
    "Give Birthright a different picture for each character.",
    "Remove the dark frame around the small map. Map rooms and icons stay visible.",
    "Repentance+ rules. With The Void unlocked, a Void portal always appears after ???, The Lamb, Mega Satan and Mother (once you have seen her ending). Counts Boss Rush waves."
};
static_assert(sizeof(Names)/sizeof(*Names)==ModCount && sizeof(Descriptions)/sizeof(*Descriptions)==ModCount);
constexpr unsigned Index(ModFeature feature) { return static_cast<unsigned>(feature); }
}
bool IsModEnabled(ModFeature feature) { return Index(feature) < ModCount && (s_Enabled & s_Available & (1u << Index(feature))); }
bool IsModAvailable(ModFeature feature) { return Index(feature) < ModCount && (s_Available & (1u << Index(feature))); }
void SetModAvailable(ModFeature feature, bool available) {
    if (Index(feature) >= ModCount) return;
    if (available) s_Available |= 1u << Index(feature);
    else s_Available &= ~(1u << Index(feature));
}
void ToggleMod(ModFeature feature) {
    if (!IsModAvailable(feature)) return;
    s_Enabled ^= 1u << Index(feature);
    if (feature == ModFeature::Specialist && !IsModEnabled(feature)) ResetSpecialist();
    if (feature == ModFeature::InstantBreak) ResetInstantBreakRoom();
}
const char* ModName(ModFeature feature) { return Index(feature) < ModCount ? Names[Index(feature)] : ""; }
const char* ModDescription(ModFeature feature) { return Index(feature) < ModCount ? Descriptions[Index(feature)] : ""; }
bool ModHasSettings(ModFeature feature) {
    return feature == ModFeature::HitCounter || feature == ModFeature::TimeMachine || feature == ModFeature::Eid ||
        feature == ModFeature::RepentancePlus;
}
std::uint32_t ModEnabledMask() { return s_Enabled & ((1u << ModCount) - 1); }
void SetModEnabledMask(std::uint32_t mask) {
    for (unsigned i = 0; i < ModCount; ++i) {
        const auto feature = static_cast<ModFeature>(i);
        if (((mask >> i) & 1) != ((s_Enabled >> i) & 1)) {
            s_Enabled ^= 1u << i;
            if (feature == ModFeature::Specialist && !(s_Enabled & (1u << i))) ResetSpecialist();
            if (feature == ModFeature::InstantBreak) ResetInstantBreakRoom();
        }
    }
}
void ResetModSettings() { s_Enabled = s_Available = (1u << ModCount) - 1; }
}

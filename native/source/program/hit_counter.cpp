#include "hit_counter.hpp"
#include "mod_menu.hpp"
#include "mod_settings.hpp"
#include "generated/build_profile.hpp"
#include "generated/iteration5_guards.hpp"
#include "../../../features/hit_counter/counter.hpp"
#include "lib.hpp"
#include <cstdio>
#include <cstring>

namespace isaac {
namespace {
using feature_memory::Readable;
using feature_memory::TryRead;
namespace api=iteration5::native;
namespace hc=hit_counter;
struct ColorRaw { float r,g,b,a; };
using DrawScaled=void (*)(const void*,const char*,float,float,float,float,const ColorRaw*,unsigned,bool);
using StringWidth=int (*)(const void*,const char*);
using LineHeight=std::uint16_t (*)(const void*);

std::uintptr_t s_Module{},s_DamageModule{},s_GreedModule{};
bool s_Ready{},s_GreedReady{};
unsigned s_Depth{};
std::uint8_t s_DamagePrologue[20]{},s_GreedPrologue[20]{};
constinit hc::Counter s_Counter;
constinit hc::Settings s_Settings;

template<std::size_t N> bool Matches(std::uintptr_t p,const std::uint8_t (&bytes)[N]) {
    return Readable(p,N) && std::memcmp(reinterpret_cast<const void*>(p),bytes,N)==0;
}
bool ReadStamp(hc::RunStamp& stamp) {
    std::uintptr_t game{};
    if (!s_Module || !TryRead(s_Module+profile::GameGlobal,game) || !Readable(game,0x25d48) ||
        !Readable(game+0x24f99c,4)) return false;
    return TryRead(game+0x25d44,stamp.seed) && TryRead(game+0x24f99c,stamp.frame) &&
        TryRead(game,stamp.stage) && TryRead(game+4,stamp.stageType);
}

HOOK_DEFINE_TRAMPOLINE(PlayerTakeDamage) {
    static bool Callback(void* player,float amount,std::uint64_t flags,void* source,int countdown) {
        if (!s_Ready) return Orig(player,amount,flags,source,countdown);
        const auto address=reinterpret_cast<std::uintptr_t>(player);
        int before{};
        const bool readable=TryRead(address+api::PlayerInvincibility,before);
        // Tainted Forgotten's soul forwards its damage to the body through
        // this same function. Count the outermost call only.
        ++s_Depth;
        const bool landed=Orig(player,amount,flags,source,countdown);
        --s_Depth;
        int after{};
        if (!s_Depth && readable && TryRead(address+api::PlayerInvincibility,after) &&
            hc::CountsAsHit(landed,before,after,flags)) {
            hc::RunStamp stamp;
            if (ReadStamp(stamp)) s_Counter.Hit(stamp);
        }
        return landed;
    }
};
HOOK_DEFINE_TRAMPOLINE(GreedBreakChance) {
    static float Callback(void* player) {
        // Slot::Update jams when Random()*100 < chance; 0 can never jam.
        // The HUD reads the same value, so it also shows the true 0 chance.
        if (s_GreedReady && IsModEnabled(ModFeature::GreedNoJam)) return 0.f;
        return Orig(player);
    }
};

struct Text {
    const void* font{};
    float scale{1};
    bool native{};
};
Text CounterFont(float scale) {
    std::uintptr_t manager{};
    std::uint8_t loaded{};
    // The HUD's coin/bomb/key counter font. Font::IsLoaded is LDRB W0,[X0].
    if (s_Module && TryRead(s_Module+profile::ManagerGlobal,manager) &&
        TryRead(manager+profile::ManagerFont,loaded) && loaded && Readable(manager+profile::ManagerFont,0x100))
        return {reinterpret_cast<const void*>(manager+profile::ManagerFont),scale,true};
    return {nullptr,scale,false};
}
float Width(const Text& text,const char* value) {
    if (text.native) return static_cast<float>(reinterpret_cast<StringWidth>(s_Module+api::FontStringWidth)(text.font,value))*text.scale;
    return static_cast<float>(std::strlen(value))*5.f*text.scale;
}
float Height(const Text& text) {
    if (text.native) return static_cast<float>(reinterpret_cast<LineHeight>(s_Module+api::FontLineHeight)(text.font))*text.scale;
    return 10.f*text.scale;
}
void Draw(const FeatureFrame& frame,const Text& text,const char* value,float x,float y,float alpha) {
    if (text.native) {
        const ColorRaw color{1,1,1,alpha};
        reinterpret_cast<DrawScaled>(s_Module+profile::DrawStringScaled)(text.font,value,x,y,text.scale,text.scale,&color,0,false);
    } else frame.draw.Text(value,x,y,{1,1,1,alpha},text.scale);
}
void Label(char* out,std::size_t size,unsigned total,unsigned floor) {
    if (s_Settings.floor) std::snprintf(out,size,"Hits: %u  Floor: %u",total,floor);
    else std::snprintf(out,size,"Hits: %u",total);
}
struct Drawn { float x,y,w,h; };
Drawn DrawCounter(const FeatureFrame& frame,const char* label,float alphaScale) {
    const auto text=CounterFont(hc::Scale(s_Settings));
    const float w=Width(text,label),h=Height(text);
    const auto at=hc::Place(s_Settings,frame.width,frame.height,w,h);
    Draw(frame,text,label,at.x,at.y,hc::Alpha(s_Settings)*alphaScale);
    return {at.x,at.y,w,h};
}
void Outline(const FeatureFrame& frame,const Drawn& d,FeatureColor color) {
    const float x1=d.x-3,y1=d.y-2,x2=d.x+d.w+3,y2=d.y+d.h+1;
    frame.draw.Rect(x1,y1,x2,y1+1,color); frame.draw.Rect(x1,y2-1,x2,y2,color);
    frame.draw.Rect(x1,y1,x1+1,y2,color); frame.draw.Rect(x2-1,y1,x2,y2,color);
}
}

bool InitializeHitCounter(std::uintptr_t module) {
    s_Module=module; s_Depth=0; s_Counter.Reset();
    if (s_Ready && module==s_DamageModule && Matches(module+api::PlayerTakeDamage,s_DamagePrologue)) return true;
    s_Ready=false;
    if (!module || !Matches(module+api::PlayerTakeDamage,api::PlayerTakeDamageGuard) ||
        !Matches(module+api::FontStringWidth,api::FontStringWidthGuard) ||
        !Matches(module+api::FontLineHeight,api::FontLineHeightGuard)) return false;
    PlayerTakeDamage::InstallAtPtr(module+api::PlayerTakeDamage);
    std::memcpy(s_DamagePrologue,reinterpret_cast<const void*>(module+api::PlayerTakeDamage),sizeof(s_DamagePrologue));
    s_DamageModule=module; s_Ready=true;
    return true;
}
bool InitializeGreedMachine(std::uintptr_t module) {
    if (s_GreedReady && module==s_GreedModule && Matches(module+api::GreedBreakChance,s_GreedPrologue)) return true;
    s_GreedReady=false;
    if (!module || !Matches(module+api::GreedBreakChance,api::GreedBreakChanceGuard) ||
        !Matches(module+api::JamDecision,api::JamDecisionGuard) ||
        !Matches(module+api::JamFlagWrite,api::JamFlagWriteGuard) ||
        !Matches(module+api::JamFlagInit,api::JamFlagInitGuard)) return false;
    GreedBreakChance::InstallAtPtr(module+api::GreedBreakChance);
    std::memcpy(s_GreedPrologue,reinterpret_cast<const void*>(module+api::GreedBreakChance),sizeof(s_GreedPrologue));
    s_GreedModule=module; s_GreedReady=true;
    return true;
}
void ResetHitCounter() { s_Counter.Reset(); s_Depth=0; }
void ResetGreedMachine() {}
void UpdateGreedMachine(std::uintptr_t game) {
    if (!s_GreedReady || !IsModEnabled(ModFeature::GreedNoJam)) return;
    std::uint64_t flags{};
    const auto address=game+api::GameStateFlags;
    if (!TryRead(address,flags) || !(flags & api::GreedJammedFlag)) return;
    flags&=~api::GreedJammedFlag;
    std::memcpy(reinterpret_cast<void*>(address),&flags,sizeof(flags));
}
void RenderHitCounter(const FeatureFrame& frame) {
    if (!s_Ready || !frame.game || IsModMenuOpen()) return;
    hc::RunStamp stamp;
    if (!ReadStamp(stamp)) return;
    s_Counter.Sync(stamp);
    char label[48];
    Label(label,sizeof(label),s_Counter.Total(),s_Counter.Floor());
    DrawCounter(frame,label,1.f);
}

void SetHitCounterLayout(int x, int y, unsigned size, unsigned opacity, bool floor) {
    s_Settings.x = static_cast<float>(x);
    s_Settings.y = static_cast<float>(y);
    s_Settings.size = size;
    s_Settings.opacity = opacity;
    s_Settings.floor = floor;
}
void HitCounterBox(float& width, float& height) {
    if (!s_Ready) { width=60; height=10; return; }
    char label[48];
    Label(label,sizeof(label),s_Counter.Total(),s_Counter.Floor());
    const auto text=CounterFont(hc::Scale(s_Settings));
    width=Width(text,label); height=Height(text);
}
void HitCounterCounts(unsigned& total, unsigned& floor) {
    hc::RunStamp stamp;
    if (s_Ready && ReadStamp(stamp)) s_Counter.Sync(stamp);
    total=s_Counter.Total(); floor=s_Counter.Floor();
}
void DrawHitCounterPreview(const FeatureFrame& frame, float alpha, bool outline) {
    if (!s_Ready || frame.module!=s_Module) return;
    if (frame.game) { hc::RunStamp stamp; if (ReadStamp(stamp)) s_Counter.Sync(stamp); }
    char label[48];
    Label(label,sizeof(label),s_Counter.Total(),s_Counter.Floor());
    const auto drawn=DrawCounter(frame,label,alpha);
    if (outline) Outline(frame,drawn,{.89f,.71f,.93f,.95f});
}
}

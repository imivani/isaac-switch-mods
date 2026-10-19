#include "../../native/source/program/boss_hud_gate.hpp"
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
unsigned checks{}, original_calls{}, stock_bars{}, other_hud_draws{};
bool enabled = true;
struct Block {
    std::vector<std::uint64_t> data;
    unsigned permission = 3;
    explicit Block(std::size_t size) : data((size + 7) / 8) {}
    std::uintptr_t Address() const { return reinterpret_cast<std::uintptr_t>(data.data()); }
    std::size_t Size() const { return data.size() * 8; }
};
Block module(0xac0000), game(0x200000), room(0x2100), descriptor(0x100), config(0x40);
std::uintptr_t Hud() { return game.Address() + api::GameHud; }
void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::fprintf(stderr,"FAIL: %s\n",message); std::exit(1); }
}
template<class T> void Put(std::uintptr_t p, T value) { std::memcpy(reinterpret_cast<void*>(p),&value,sizeof(value)); }
template<class T> T Get(std::uintptr_t p) { T value{}; std::memcpy(&value,reinterpret_cast<const void*>(p),sizeof(value)); return value; }
void Setup() {
    std::memcpy(reinterpret_cast<void*>(module.Address()+api::SetBossHealth),api::SetBossHealthGuard,sizeof(api::SetBossHealthGuard));
    std::memcpy(reinterpret_cast<void*>(module.Address()+api::BossHealthGuardSite),api::BossHealthGuardSiteGuard,sizeof(api::BossHealthGuardSiteGuard));
    std::memcpy(reinterpret_cast<void*>(module.Address()+api::BossHiddenReturn),api::BossHiddenReturnGuard,sizeof(api::BossHiddenReturnGuard));
    Put(module.Address()+isaac::profile::GameGlobal,game.Address());
    Put(game.Address()+isaac::profile::GameRoom,room.Address());
    Put(room.Address()+8,descriptor.Address());
    Put(descriptor.Address()+0x10,config.Address());
    Put<int>(config.Address()+8,5); Put<int>(config.Address(),0);
    Put<int>(room.Address()+api::RoomBossId,1);
    Put<std::uint32_t>(Hud()+api::HudBossHealth,0x3f000000u);
    Put<std::uint8_t>(Hud()+api::HudBossPosition,1);
    game.permission = 3; enabled = true;
}
// Only the boundary to the real game's HUD call is modeled. This reproduces
// the verified negative-health return: other HUD draws happen first, the
// position flag is cleared, then the boss section returns without drawing.
void OriginalHudBoundary() {
    ++original_calls; ++other_hud_draws;
    if (Get<float>(Hud()+api::HudBossHealth) < 0.f) Put<std::uint8_t>(Hud()+api::HudBossPosition,0);
    else ++stock_bars;
}
void Invoke() {
    auto restore = isaac::BeginBossHud(reinterpret_cast<void*>(Hud()));
    OriginalHudBoundary();
    isaac::EndBossHud(restore);
    Check(!restore.active,"restoration token is consumed");
}
}

extern "C" std::uint32_t svcQueryMemory(MemoryInfo* info, std::uint32_t* page, std::uint64_t address) {
    *page = 0;
    for (const Block* block : {&module,&game,&room,&descriptor,&config}) {
        if (address >= block->Address() && address < block->Address()+block->Size()) {
            *info = {block->Address(),block->Size(),block->permission}; return 0;
        }
    }
    *info = {}; return 1;
}
namespace isaac {
bool IsModEnabled(ModFeature feature) { Check(feature==ModFeature::BossBars,"only Enhanced Boss Bars setting controls this helper"); return enabled; }
}

int main() {
    Setup();
    Check(!isaac::BeginBossHud(reinterpret_cast<void*>(Hud())).active,"no suppression before exact-build verification");
    Check(!isaac::InitializeBossHudGate(0),"null module fails safely");
    Check(!isaac::InitializeBossHudGate(UINTPTR_MAX-0x1000),"overflowing module fails safely");
    Check(isaac::InitializeBossHudGate(module.Address()),"exact field/branch/return guards initialize");
    Check(isaac::InitializeBossHudGate(module.Address()),"repeat initialization installs no second hook");
    const auto baseline=game.data;
    Invoke();
    Check(original_calls==1 && other_hud_draws==1 && stock_bars==0,"enabled feature preserves all ordinary HUD work and suppresses stock boss draw");
    Check(game.data==baseline,"health bits and position flag are exactly restored after the native call");
    enabled=false; Invoke();
    Check(stock_bars==1 && game.data==baseline,"disabled feature immediately draws unchanged original boss bar");
    enabled=true; Invoke();
    Check(stock_bars==1 && other_hud_draws==3,"re-enabling immediately resumes suppression");

    auto outer=isaac::BeginBossHud(reinterpret_cast<void*>(Hud()));
    auto inner=isaac::BeginBossHud(reinterpret_cast<void*>(Hud()));
    Check(outer.active && !inner.active,"nested HUD entry cannot overwrite saved health with the hidden sentinel");
    OriginalHudBoundary(); isaac::EndBossHud(inner);
    enabled=false; isaac::EndBossHud(outer);
    Check(game.data==baseline,"End restores even if the toggle changes within the callback");
    isaac::EndBossHud(outer);
    Check(game.data==baseline,"End is idempotent");
    enabled=true;

    for (std::uint32_t bits : {0u,1u,0x3e800000u,0x3f800000u,0x40000000u,0x7f7fffffu}) {
        Put(Hud()+api::HudBossHealth,bits);
        for (std::uint8_t position : {std::uint8_t(0),std::uint8_t(1),std::uint8_t(255)}) {
            Put(Hud()+api::HudBossPosition,position);
            auto token=isaac::BeginBossHud(reinterpret_cast<void*>(Hud()));
            Check(token.active && Get<std::uint32_t>(Hud()+api::HudBossHealth)==0xbf800000u,"finite nonnegative health temporarily uses native hidden sentinel");
            Put<std::uint8_t>(Hud()+api::HudBossPosition,0);
            isaac::EndBossHud(token);
            Check(Get<std::uint32_t>(Hud()+api::HudBossHealth)==bits && Get<std::uint8_t>(Hud()+api::HudBossPosition)==position,"float bits and complete position byte survive restoration");
        }
    }
    for (std::uint32_t bits : {0xbf800000u,0xbe800000u,0x80000000u,0x7f800000u,0xff800000u,0x7fc00001u}) {
        Put(Hud()+api::HudBossHealth,bits);
        Check(!isaac::BeginBossHud(reinterpret_cast<void*>(Hud())).active,"negative/nonfinite native health is not changed");
        Check(Get<std::uint32_t>(Hud()+api::HudBossHealth)==bits,"rejected health retains exact bit pattern");
    }
    Setup();
    for (int special : {0x53,0x37,0x58}) {
        Put(room.Address()+api::RoomBossId,special);
        Check(!isaac::BeginBossHud(reinterpret_cast<void*>(Hud())).active,"Gideon and already-hidden native boss IDs preserve original rendering and side effects");
    }
    Put<int>(room.Address()+api::RoomBossId,1);
    Put<int>(config.Address()+8,16); Put<int>(config.Address(),35);
    Check(!isaac::BeginBossHud(reinterpret_cast<void*>(Hud())).active,"Beast-room native hiding logic is preserved");
    Put<int>(config.Address(),34);
    auto normal=isaac::BeginBossHud(reinterpret_cast<void*>(Hud()));
    Check(normal.active,"another variant of room type16 is not incorrectly excluded"); isaac::EndBossHud(normal);
    Setup();
    game.permission=1;
    Check(!isaac::BeginBossHud(reinterpret_cast<void*>(Hud())).active,"read-only HUD fields are never written");
    game.permission=2;
    Check(!isaac::BeginBossHud(reinterpret_cast<void*>(Hud())).active,"write-only fields are not read");
    game.permission=3;
    for (auto invalid : {std::uintptr_t(0),Hud()+1,Hud()+8,UINTPTR_MAX-0x1000})
        Check(!isaac::BeginBossHud(reinterpret_cast<void*>(invalid)).active,"invalid/foreign/overflowing HUD is rejected");
    for (auto location : {module.Address()+isaac::profile::GameGlobal,game.Address()+isaac::profile::GameRoom,
                          room.Address()+8,descriptor.Address()+0x10}) {
        const auto saved=Get<std::uintptr_t>(location);
        for (auto invalid : {std::uintptr_t(0),std::uintptr_t(0x10000),saved+1,UINTPTR_MAX-7}) {
            Put(location,invalid);
            Check(!isaac::BeginBossHud(reinterpret_cast<void*>(Hud())).active,"invalid room/config/game chain preserves stock behavior");
        }
        Put(location,saved);
    }
    Check(game.data==baseline,"all rejection paths leave original HUD and game memory unchanged");
    for (auto site : {api::SetBossHealth,api::BossHealthGuardSite,api::BossHiddenReturn}) {
        Setup(); *reinterpret_cast<unsigned char*>(module.Address()+site)^=1;
        Check(!isaac::InitializeBossHudGate(module.Address()),"any changed native field/branch/return guard rejects initialization");
        Check(!isaac::BeginBossHud(reinterpret_cast<void*>(Hud())).active,"failed guard leaves stock boss bar untouched");
    }
    Setup(); Check(isaac::InitializeBossHudGate(module.Address()),"fresh verified module can initialize after rejection");
    std::printf("PASS: %u checks; stock/enhanced toggle, exact restoration, Gideon/Beast exceptions, guards, permissions and nested calls.\n",checks);
}

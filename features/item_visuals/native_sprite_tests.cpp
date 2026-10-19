// Compile the ACTUAL shared native_sprite.cpp. Only svcQueryMemory is mocked;
// tests never jump into a game function or launch/modify the game.
#include <cstddef>
#include <cstdint>
#define private public
#include "../../native/source/program/native_sprite.hpp"
#undef private
#include "../../native/source/program/feature_api.hpp"
#include "exchange_scope.hpp"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
unsigned checks=0;
#define CHECK(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#condition); std::exit(1); } } while (false)
struct Region { std::uintptr_t address; std::size_t size; unsigned permissions; };
std::vector<Region> regions;
void Add(void* pointer,std::size_t size) { regions.push_back({reinterpret_cast<std::uintptr_t>(pointer),size,Perm_R|Perm_W}); }
template<class T> void Put(std::byte* bytes,std::size_t offset,T value) { std::memcpy(bytes+offset,&value,sizeof(value)); }
std::uintptr_t Word(const std::byte* bytes,std::size_t offset) { std::uintptr_t value{};std::memcpy(&value,bytes+offset,sizeof(value));return value; }
constexpr auto Pair=0xb0+0xa0;
struct Fixture {
    isaac::NativeSprite owned;
    alignas(8) std::byte other[0x158]{};
    alignas(8) std::byte own_layers[2*0xb0], other_layers[2*0xb0];
    std::uint64_t own_image=11,own_control=13,other_image=17,other_control=19;
    Fixture() {
        regions.clear();
        std::memset(own_layers,0xa5,sizeof(own_layers));std::memset(other_layers,0x5a,sizeof(other_layers));
        owned.loaded_=true;
        Put(owned.object_,0xa8,reinterpret_cast<std::uintptr_t>(own_layers));Put(owned.object_,0xb0,2u);
        Put(other,0xa8,reinterpret_cast<std::uintptr_t>(other_layers));Put(other,0xb0,2u);
        Put(own_layers,Pair,reinterpret_cast<std::uintptr_t>(&own_image));
        Put(own_layers,Pair+8,reinterpret_cast<std::uintptr_t>(&own_control));
        Put(other_layers,Pair,reinterpret_cast<std::uintptr_t>(&other_image));
        Put(other_layers,Pair+8,reinterpret_cast<std::uintptr_t>(&other_control));
        Add(&owned,sizeof(owned));Add(other,sizeof(other));Add(own_layers,sizeof(own_layers));Add(other_layers,sizeof(other_layers));
        Add(&own_image,8);Add(&own_control,8);Add(&other_image,8);Add(&other_control,8);
    }
    std::uintptr_t Other() { return reinterpret_cast<std::uintptr_t>(other); }
    bool Swapped() const { return Word(own_layers,Pair)==reinterpret_cast<std::uintptr_t>(&other_image) &&
        Word(own_layers,Pair+8)==reinterpret_cast<std::uintptr_t>(&other_control) &&
        Word(other_layers,Pair)==reinterpret_cast<std::uintptr_t>(&own_image) &&
        Word(other_layers,Pair+8)==reinterpret_cast<std::uintptr_t>(&own_control); }
};
using Bytes=std::array<std::byte,2*0xb0>;
Bytes Copy(const std::byte* bytes) { Bytes out;std::memcpy(out.data(),bytes,out.size());return out; }
bool Same(const Bytes& before,const std::byte* now) {return std::memcmp(before.data(),now,before.size())==0;}
void Invalid(unsigned scenario) {
    Fixture fixture;
    auto pointer=fixture.Other();unsigned own=1,other=1;
    if(scenario==0) fixture.owned.loaded_=false;
    if(scenario==1) pointer=0;
    if(scenario==2) own=2;
    if(scenario==3) other=2;
    if(scenario==4) Put(fixture.other,0xb0,513u);
    if(scenario==5) Put(fixture.other,0xb0,0u);
    if(scenario==6) regions[3].permissions=Perm_R;
    if(scenario==7) regions[2].permissions=Perm_R;
    if(scenario==8) regions[3].size-=8;
    if(scenario==9) Put(fixture.other_layers,Pair,std::uintptr_t{0});
    if(scenario==10) Put(fixture.other_layers,Pair+8,std::uintptr_t{0});
    if(scenario==11) Put(fixture.own_layers,Pair+8,std::uintptr_t{0xabcdef});
    if(scenario==12) pointer=reinterpret_cast<std::uintptr_t>(fixture.owned.object_);
    if(scenario==13) Put(fixture.other,0xa8,std::uintptr_t{UINTPTR_MAX-8});
    const auto own_before=Copy(fixture.own_layers),other_before=Copy(fixture.other_layers);
    CHECK(!fixture.owned.ExchangeLayerTexture(own,pointer,other));
    CHECK(Same(own_before,fixture.own_layers));CHECK(Same(other_before,fixture.other_layers));
}
void LayerReadiness(unsigned scenario) {
    Fixture fixture;
    Put(fixture.own_layers, 0xb0+0x38, std::uint8_t{1});
    CHECK(fixture.owned.HasLayerTexture(1));
    const auto own_before = Copy(fixture.own_layers);
    const auto other_before = Copy(fixture.other_layers);
    CHECK(!fixture.owned.HasLayerTexture(2));
    CHECK(Same(own_before, fixture.own_layers) && Same(other_before, fixture.other_layers));
    unsigned layer = 1;
    if (scenario == 0) fixture.owned.loaded_ = false;
    if (scenario == 1) Put(fixture.owned.object_, 0xb0, 0u);
    if (scenario == 2) Put(fixture.owned.object_, 0xb0, 513u);
    if (scenario == 3) layer = 512;
    if (scenario == 4) Put(fixture.owned.object_, 0xa8, std::uintptr_t{0});
    if (scenario == 5) Put(fixture.own_layers, 0xb0+0x38, std::uint8_t{0});
    if (scenario == 6) Put(fixture.own_layers, Pair, std::uintptr_t{0});
    if (scenario == 7) Put(fixture.own_layers, Pair+8, std::uintptr_t{0});
    if (scenario == 8) Put(fixture.own_layers, Pair, std::uintptr_t{0xabcdef});
    if (scenario == 9) Put(fixture.own_layers, Pair+8, std::uintptr_t{0xabcdef});
    if (scenario == 10) regions[2].size -= 1;
    if (scenario == 11) regions[2].permissions = 0;
    if (scenario == 12) regions[4].size = 7;
    if (scenario == 13) regions[5].permissions = 0;
    const auto changed = Copy(fixture.own_layers);
    CHECK(!fixture.owned.HasLayerTexture(layer));
    CHECK(Same(changed, fixture.own_layers));
}
}

extern "C" std::uint32_t svcQueryMemory(MemoryInfo* info,std::uint32_t* page,std::uint64_t pointer) {
    *info={};*page=0;
    for(const auto& region:regions) if(pointer>=region.address && pointer-region.address<region.size) {
        info->addr=region.address;info->size=region.size;info->perm=region.permissions;return 0;
    }
    return 1;
}

int main() {
    {
        Fixture fixture;
        const auto own_before=Copy(fixture.own_layers),other_before=Copy(fixture.other_layers);
        CHECK(fixture.owned.ExchangeLayerTexture(1,fixture.Other(),1));
        CHECK(fixture.Swapped());
        CHECK(std::memcmp(own_before.data(),fixture.own_layers,Pair)==0);
        CHECK(std::memcmp(other_before.data(),fixture.other_layers,Pair)==0);
        CHECK(fixture.own_image==11 && fixture.own_control==13 && fixture.other_image==17 && fixture.other_control==19);
        CHECK(fixture.owned.ExchangeLayerTexture(1,fixture.Other(),1));
        CHECK(Same(own_before,fixture.own_layers) && Same(other_before,fixture.other_layers));
        bool failure=false;
        const auto draw_with_early_return=[&]() {
            isaac_item_visuals::TextureDrawScope<isaac::NativeSprite> exchange(&fixture.owned,fixture.Other(),&failure);
            CHECK(exchange.Active() && fixture.Swapped());
            return 42;
        };
        for(unsigned frame=0;frame<1000;++frame) {
            CHECK(draw_with_early_return()==42);
            CHECK(!failure && Same(own_before,fixture.own_layers) && Same(other_before,fixture.other_layers));
        }
        {
            isaac_item_visuals::TextureDrawScope<isaac::NativeSprite> skipped(nullptr,fixture.Other(),&failure);
            CHECK(!skipped.Active());
        }
        CHECK(!failure && Same(own_before,fixture.own_layers) && Same(other_before,fixture.other_layers));
    }
    for(unsigned scenario=0;scenario<14;++scenario) Invalid(scenario);
    for(unsigned scenario=0;scenario<14;++scenario) LayerReadiness(scenario);
    std::printf("PASS: %u actual NativeSprite texture readiness, exchange, rejection and scoped-restoration checks\n",checks);
}

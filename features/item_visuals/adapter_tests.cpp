// Runs the ACTUAL item_visuals.cpp callbacks with bounded fake game services.
// NativeSprite's graphics backend is mocked here; its real texture-readiness
// and exchange implementations are tested separately in native_sprite_tests.
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>
#define private public
#include "../../native/source/program/native_sprite.hpp"
#undef private
#include "../../native/source/program/item_visuals.cpp"

namespace {
unsigned checks{}, originals{}, custom_draws{}, loads{}, frame_calls{}, texture_queries{}, exchanges{};
unsigned clean_queries{}, last_original_visibility{}, failed_item{}, failed_layer{~0u};
bool failed_parse{}, failed_frame{}, clean_hide{};
unsigned curses{};
bool enabled[isaac::ModCount]{};
std::uintptr_t expected_config{};
struct Region { std::uintptr_t address; std::size_t size; unsigned permissions; };
std::vector<Region> regions;
struct SpriteState { unsigned item{}, layers{}; isaac::NativeVec2 scale{}, position{}; unsigned frame{}; };
std::map<const isaac::NativeSprite*, SpriteState> sprite_state;
std::string last_file, last_animation;
std::vector<std::string> messages;
bool texture_exchanged{};
#define CHECK(condition) do { ++checks; if (!(condition)) { std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#condition); std::exit(1); } } while (false)
template<class T> void Put(void* bytes, std::size_t offset, T value) {
    std::memcpy(static_cast<std::byte*>(bytes)+offset, &value, sizeof(value));
}
template<class T> T Get(const void* bytes, std::size_t offset) {
    T value{}; std::memcpy(&value,static_cast<const std::byte*>(bytes)+offset,sizeof(value)); return value;
}
void Add(void* pointer, std::size_t size) { regions.push_back({reinterpret_cast<std::uintptr_t>(pointer),size,Perm_R|Perm_W}); }
void Counters() { originals=custom_draws=loads=frame_calls=texture_queries=exchanges=clean_queries=0; last_original_visibility=99; }
unsigned GetCurses(const void*) { return curses; }
int GetFrame(const void*) { return 0; }
std::uintptr_t GetItem(const void*, int id) {
    return expected_config && Get<unsigned>(reinterpret_cast<const void*>(expected_config),4)==static_cast<unsigned>(id) ? expected_config : 0;
}
void OriginalLayer(void*, unsigned, const isaac::NativeVec2*, const isaac::NativeVec2*, const isaac::NativeVec2*) { ++originals; }
void OriginalSprite(void* sprite, const isaac::NativeVec2*, const isaac::NativeVec2*, const isaac::NativeVec2*) {
    ++originals;
    const auto states=Get<std::uintptr_t>(sprite,0xa8);
    last_original_visibility=states ? Get<std::uint8_t>(reinterpret_cast<const void*>(states),0xb0+0x7c) : 99;
}
void OriginalPickup(void* pickup, const isaac::NativeVec2*) {
    isaac::NativeVec2 pos{100,100}, zero{};
    const auto registered=regions.size();Add(&pos,sizeof(pos));Add(&zero,sizeof(zero));
    if(isaac::IsModEnabled(isaac::ModFeature::AnimatedItems) || isaac::IsModEnabled(isaac::ModFeature::Birthright)) {
        CHECK(isaac::s_PickupPass.entity==reinterpret_cast<std::uintptr_t>(pickup));
    }
    isaac::DrawItemPickupLayer::Callback(static_cast<std::byte*>(pickup)+0x50,1,&pos,&zero,&zero);
    regions.resize(registered);
}
void OriginalPlayer(void* player, const isaac::NativeVec2*, bool) {
    isaac::NativeVec2 pos{100,100}, zero{};
    const auto registered=regions.size();Add(&pos,sizeof(pos));Add(&zero,sizeof(zero));
    isaac::DrawItemHeldSprite::Callback(static_cast<std::byte*>(player)+0x12d8,&pos,&zero,&zero);
    regions.resize(registered);
}
void Text(void*,const char* value,float,float,isaac::FeatureColor,float) { messages.emplace_back(value); }

struct Fixture {
    std::vector<std::byte> module=std::vector<std::byte>(0xacd000);
    std::vector<std::byte> game=std::vector<std::byte>(0x24f9a0);
    std::vector<std::byte> manager=std::vector<std::byte>(0x36580);
    alignas(8) std::array<std::byte,0x780> pickup{};
    alignas(8) std::array<std::byte,0x2850> player{};
    alignas(8) std::array<std::byte,2*0xb0> pickup_layers{}, held_layers{};
    alignas(8) std::array<std::byte,8> config{}, room{};
    alignas(8) std::array<std::uintptr_t,1> players{};
    Fixture() {
        regions.clear(); sprite_state.clear();
        for(auto& value:enabled) value=true;
        failed_item=0; failed_layer=~0u; failed_parse=failed_frame=clean_hide=false; curses=0; texture_exchanged=false;
        Add(module.data(),module.size());Add(game.data(),game.size());Add(manager.data(),manager.size());
        Add(pickup.data(),pickup.size());Add(player.data(),player.size());
        Add(pickup_layers.data(),pickup_layers.size());Add(held_layers.data(),held_layers.size());
        Add(config.data(),config.size());Add(room.data(),room.size());Add(players.data(),sizeof(players));
        for(unsigned i=0;i<4;++i) std::memcpy(module.data()+isaac::HookOffsets[i],isaac::OriginalPrologues[i],20);
        Put(module.data(),isaac::profile::GameGlobal,Game());
        Put(module.data(),isaac::profile::ManagerGlobal,reinterpret_cast<std::uintptr_t>(manager.data()));
        Put(game.data(),0x21550,reinterpret_cast<std::uintptr_t>(room.data()));
        Put(game.data(),0x25d44,123u);Put(game.data(),0x24f99c,100);
        players[0]=reinterpret_cast<std::uintptr_t>(player.data());
        Put(game.data(),isaac::profile::PlayersBegin,reinterpret_cast<std::uintptr_t>(players.data()));
        Put(game.data(),isaac::profile::PlayersEnd,reinterpret_cast<std::uintptr_t>(players.data()+1));
        Put(pickup.data(),0x38,5u);Put(pickup.data(),0x3c,100u);
        Put(player.data(),0x38,1u);Put(player.data(),0x3c,0u);
        for(auto* entity:{pickup.data(),player.data()}) {
            Put(entity,0x1c1,std::uint8_t{1});Put(entity,0x1c2,std::uint8_t{1});
            Put(entity,0x3e8,77u);Put(entity,0x2f4,90);Put(entity,0x310,100.f);Put(entity,0x314,100.f);
        }
        Sprite(pickup.data()+0x50,pickup_layers.data());
        Sprite(player.data()+0x12d8,held_layers.data());
        expected_config=reinterpret_cast<std::uintptr_t>(config.data());
        Put(player.data(),0x1c38,expected_config);
        isaac::ObserveItemPickupRender::original=reinterpret_cast<std::uintptr_t>(&OriginalPickup);
        isaac::ObserveItemPlayerRender::original=reinterpret_cast<std::uintptr_t>(&OriginalPlayer);
        isaac::DrawItemPickupLayer::original=reinterpret_cast<std::uintptr_t>(&OriginalLayer);
        isaac::DrawItemHeldSprite::original=reinterpret_cast<std::uintptr_t>(&OriginalSprite);
        isaac::ResetItemVisuals();
        CHECK(isaac::InitializeItemVisuals(Module()));
        CHECK(isaac::InitializeItemVisuals(Module()));
        SetItem(25); Counters();
    }
    ~Fixture() { isaac::ResetItemVisuals(); }
    std::uintptr_t Module() const { return reinterpret_cast<std::uintptr_t>(module.data()); }
    std::uintptr_t Game() const { return reinterpret_cast<std::uintptr_t>(game.data()); }
    void Sprite(void* sprite, void* layers) {
        Put(sprite,0xa8,reinterpret_cast<std::uintptr_t>(layers));Put(sprite,0xb0,2u);Put(sprite,0x149,std::uint8_t{1});
        Put(layers,0xb0+0x7c,std::uint8_t{1});
        Sheet(layers,"gfx/items/test.png");
    }
    void Sheet(void* layers, const char* name) {
        auto* string=static_cast<std::byte*>(layers)+0xb0+0x10;
        std::memset(string,0,24);const auto size=std::strlen(name);CHECK(size<=22);
        Put(string,0,static_cast<std::uint8_t>(size*2));std::memcpy(string+1,name,size+1);
    }
    void SetItem(unsigned item) { Put(pickup.data(),0x40,item);Put(config.data(),4,item); }
    void Ground() { const isaac::NativeVec2 zero{};isaac::ObserveItemPickupRender::Callback(pickup.data(),&zero); }
    void Held(bool main_body=true) { const isaac::NativeVec2 zero{};isaac::ObserveItemPlayerRender::Callback(player.data(),&zero,main_body); }
    void NextFrame() { Put(game.data(),0x24f99c,Get<int>(game.data(),0x24f99c)+1); }
    void Reset() { isaac::ResetItemVisuals();Counters(); }
};
}

extern "C" std::uint32_t svcQueryMemory(MemoryInfo* info,std::uint32_t* page,std::uint64_t pointer) {
    *info={};*page=0;
    for(const auto& region:regions) if(pointer>=region.address && pointer-region.address<region.size) {
        info->addr=region.address;info->size=region.size;info->perm=region.permissions;return 0;
    }
    return 1;
}
namespace isaac_item_visuals::test {
std::uintptr_t Resolve(std::uintptr_t module,std::uintptr_t offset) {
    CHECK(module==isaac::s_Module);
    if(offset==isaac::profile::GetCurses) return reinterpret_cast<std::uintptr_t>(&GetCurses);
    if(offset==isaac::GetSpriteFrame) return reinterpret_cast<std::uintptr_t>(&GetFrame);
    if(offset==isaac::GetCollectible) return reinterpret_cast<std::uintptr_t>(&GetItem);
    CHECK(false);return 0;
}
}
namespace isaac {
bool IsModEnabled(ModFeature feature) { return enabled[static_cast<unsigned>(feature)]; }
bool ShouldHideCleanMapFrame(std::uintptr_t, std::uintptr_t caller) { ++clean_queries; CHECK(caller!=0); return clean_hide; }
bool NativeSprite::Create(std::uintptr_t module) { module_=module;created_=true;return true; }
bool NativeSprite::Load(const char* file) {
    ++loads;last_file=file;auto& state=sprite_state[this];state={};
    for(const auto& source:isaac_item_visuals::data::Animations) if(std::strcmp(source.file,file)==0) {
        state.item=source.item;state.layers=source.layers;break;
    }
    if(std::strcmp(file,"gfx/005.100_collectible.anm2")==0) { state.item=619;state.layers=2; }
    CHECK(state.item!=0);loaded_=!(failed_parse && state.item==failed_item);return loaded_;
}
bool NativeSprite::SetFrame(const char* animation,int frame) {
    ++frame_calls;last_animation=animation;sprite_state[this].frame=static_cast<unsigned>(frame);
    return loaded_ && !(failed_frame && sprite_state[this].item==failed_item);
}
bool NativeSprite::HasLayerTexture(unsigned layer) const {
    ++texture_queries;const auto& state=sprite_state.at(this);
    return loaded_ && layer<state.layers && !(state.item==failed_item && layer==failed_layer);
}
bool NativeSprite::SetSpritesheet(unsigned layer,const char* filename) {
    CHECK(layer==1 && std::strstr(filename,"/birthright/"));return loaded_;
}
void NativeSprite::LoadGraphics() {}
void NativeSprite::SetScale(float x,float y) { sprite_state[this].scale={x,y}; }
void NativeSprite::Render(NativeVec2 position,NativeVec2,NativeVec2) { ++custom_draws;sprite_state[this].position=position; }
bool NativeSprite::ExchangeLayerTexture(unsigned own,std::uintptr_t other,unsigned layer) {
    CHECK(own==1 && layer==1 && other!=0);++exchanges;texture_exchanged=!texture_exchanged;return true;
}
void NativeSprite::Destroy() { created_=loaded_=false;module_=0; }
bool CopyNativeString(std::uintptr_t source,char* output,std::size_t capacity) {
    // Fixtures use bounded short-form native strings. The real string reader
    // is not replaced in the production build.
    if(!feature_memory::Readable(source,24)) return false;
    const auto size=static_cast<std::size_t>(feature_memory::Read<std::uint8_t>(source)>>1);
    if(size>=capacity || size>22) return false;
    std::memcpy(output,reinterpret_cast<const void*>(source+1),size);output[size]=0;return true;
}
}

int main() {
    {
        Fixture fixture;
        for(const auto& row:isaac_item_visuals::data::Animations) {
            fixture.SetItem(row.item);Counters();fixture.Ground();
            if(originals!=0 || custom_draws!=1 || loads!=1)
                std::fprintf(stderr,"item %u: original=%u custom=%u loads=%u ready=%d animation_failure=%d pass=%zx\n",row.item,originals,custom_draws,loads,isaac::s_HookReady,isaac::s_AnimationFailure,isaac::s_PickupPass.entity);
            CHECK(originals==0 && custom_draws==1 && loads==1);
            CHECK(last_file==row.file && last_animation=="AnimatedPedestal" && texture_queries==row.layers);
            const auto& sprite=isaac::s_Animations[isaac_item_visuals::AnimationIndex(row.item)];
            CHECK(sprite_state[&sprite].frame==0);
            fixture.NextFrame();Counters();fixture.Ground();
            CHECK(originals==0 && custom_draws==1 && loads==0 && texture_queries==0 && sprite_state[&sprite].frame==1);
            Counters();fixture.Held();
            CHECK(originals==1 && custom_draws==1 && loads==0 && last_original_visibility==0);
            CHECK(Get<std::uint8_t>(fixture.held_layers.data(),0xb0+0x7c)==1);
        }
    }
    for(unsigned item:{25u,581u}) for(unsigned bad=0;bad<isaac_item_visuals::data::Animations[isaac_item_visuals::AnimationIndex(item)].layers;++bad) {
        Fixture fixture;fixture.SetItem(item);failed_item=item;failed_layer=bad;
        fixture.Ground();CHECK(originals==1 && custom_draws==0 && isaac::s_AnimationFailure);
        Counters();fixture.Held();CHECK(originals==1 && custom_draws==0 && last_original_visibility==1);
        CHECK(Get<std::uint8_t>(fixture.held_layers.data(),0xb0+0x7c)==1);
    }
    for(bool parse:{false,true}) {
        Fixture fixture;failed_item=25;failed_parse=parse;failed_frame=!parse;
        fixture.Ground();CHECK(originals==1 && custom_draws==0);
        Counters();fixture.Held();CHECK(originals==1 && custom_draws==0 && last_original_visibility==1);
    }
    {
        Fixture fixture;
        enabled[static_cast<unsigned>(isaac::ModFeature::AnimatedItems)]=false;
        fixture.Ground();CHECK(originals==1 && custom_draws==0 && loads==0);
        Counters();fixture.Held();CHECK(originals==1 && custom_draws==0 && loads==0);
        enabled[static_cast<unsigned>(isaac::ModFeature::AnimatedItems)]=true;
        Counters();fixture.Ground();CHECK(originals==0 && custom_draws==1);
        enabled[static_cast<unsigned>(isaac::ModFeature::AnimatedItems)]=false;
        Counters();fixture.Ground();CHECK(originals==1 && custom_draws==0 && loads==0);
        fixture.SetItem(619);Counters();fixture.Ground();CHECK(originals==1 && exchanges==2 && !texture_exchanged);
        Counters();fixture.Held();CHECK(originals==1 && exchanges==2 && !texture_exchanged);
        enabled[static_cast<unsigned>(isaac::ModFeature::Birthright)]=false;
        Counters();fixture.Ground();CHECK(originals==1 && exchanges==0 && loads==0);
        Counters();fixture.Held();CHECK(originals==1 && exchanges==0 && loads==0);
        // Clean Map still sees its own shared hook when both item mods are off.
        clean_hide=true;Counters();fixture.Held();CHECK(originals==0 && custom_draws==0 && clean_queries==1);
        clean_hide=false;Counters();fixture.Held();CHECK(originals==1 && clean_queries==1);
    }
    {
        Fixture fixture;
        fixture.Sheet(fixture.pickup_layers.data(),"gfx/questionmark.png");fixture.Ground();CHECK(originals==1 && custom_draws==0 && loads==0);
        fixture.Sheet(fixture.pickup_layers.data(),"gfx/items/test.png");Put(fixture.pickup.data(),0x562,std::uint8_t{1});Counters();fixture.Ground();
        CHECK(originals==1 && custom_draws==0 && loads==0);
        Counters();fixture.Held(false);CHECK(originals==1 && custom_draws==0 && loads==0);
        curses=0x40;fixture.NextFrame();Counters();fixture.Held();CHECK(originals==1 && custom_draws==0 && loads==0);
        fixture.SetItem(619);Put(fixture.pickup.data(),0x562,std::uint8_t{0});Counters();fixture.Ground();CHECK(originals==1 && exchanges==0 && loads==0);
    }
    {
        Fixture fixture;fixture.SetItem(619);failed_item=619;failed_layer=1;fixture.Ground();
        CHECK(originals==1 && exchanges==0 && isaac::s_BirthrightFailure);
        enabled[static_cast<unsigned>(isaac::ModFeature::Birthright)]=false;
        isaac::FeatureFrame frame{fixture.Module(),fixture.Game(),reinterpret_cast<std::uintptr_t>(fixture.room.data()),640,480,{nullptr,&Text,nullptr}};
        messages.clear();isaac::RenderItemVisualsStatus(frame);CHECK(messages.empty());
    }
    std::printf("PASS: %u actual item-visual callbacks, all98 animation selections, missing-texture fallback, held restoration, Birthright and independent-toggle checks\n",checks);
}

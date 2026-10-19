// Compile the unchanged production eid_ui.cpp separately. This recording
// backend replaces only native sprite/config/memory boundaries, not layout.
#include "../../native/source/program/eid_ui.hpp"
#include "../../native/source/program/native_sprite.hpp"
#include "../../native/source/program/generated/build_profile.hpp"
#include "../../eid_ui/layout.hpp"
#include "menu_ui_calls.hpp"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>
#include <sys/mman.h>

namespace {
using isaac::NativeSprite;
using isaac::FeatureColor;
struct Sprite { std::string path, animation, sheet; int frame{}; FeatureColor tint{1,1,1,1}; float sx{1},sy{1}; };
struct Paint { std::string kind,path,animation,sheet; unsigned value{}; int frame{}; float x{},y{}; FeatureColor tint{}; };
struct Region { std::uintptr_t p; std::size_t size; };
std::unordered_map<const NativeSprite*,Sprite> sprites;
std::vector<Paint> paint;
std::vector<Region> regions;
std::vector<std::uintptr_t> queries;
unsigned checks{}, cases{}, config_calls{}, item_loads{}, item_destroys{}, fallback_calls{}, font_loads{};
bool fail_font{}, fail_icons{}, fallback_loaded{true};
std::uintptr_t module{};
std::vector<std::uint64_t> manager(0x37000/8), configs(1024*0x80/8);
std::string active_case;
void Check(bool condition,const char* reason) {
    ++checks;
    if (!condition) { std::fprintf(stderr,"FAIL [%s]: %s\n",active_case.c_str(),reason); std::exit(1); }
}
template<class T> void Put(std::uintptr_t p,T value) { std::memcpy(reinterpret_cast<void*>(p),&value,sizeof(value)); }
std::uintptr_t Manager() { return reinterpret_cast<std::uintptr_t>(manager.data()); }
std::uintptr_t Configs() { return reinterpret_cast<std::uintptr_t>(configs.data()); }
std::uintptr_t HostConfig(const void* config,int id) {
    ++config_calls;
    Check(reinterpret_cast<std::uintptr_t>(config)==Manager()+0x36538,"native item-config receiver is correct");
    return id>0 && id<1024 ? Configs()+static_cast<unsigned>(id)*0x80 : 0;
}
void Text(void*,const char* text,float x,float y,FeatureColor color,float) {
    ++fallback_calls;
    if (fallback_loaded) paint.push_back({"fallback","","","",static_cast<unsigned char>(*text),0,x,y,color});
}
void Rect(void*,float,float,float,float,FeatureColor) {}
isaac::FeatureFrame Frame(float width=480,float height=270) { return {module,0,0,width,height,{nullptr,Text,Rect}}; }
unsigned GlyphCode(int frame) {
    for (const auto& glyph:isaac_eid_ui::assets::Glyphs) if (glyph.frame==frame) return glyph.code;
    return 0;
}
void Start(const char* name,bool reset=true) {
    active_case=name; ++cases;
    if (reset) isaac::ResetEidUi();
    paint.clear(); queries.clear();
    fail_font=fail_icons=false; fallback_loaded=true;
}
bool Bounds(float x,float y,float width,float height) {
    for (const auto& p:paint) {
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.tint.a)) return false;
        // Native sprite anchors include the upstream inline-icon offsets.
        // Compare their logical cells after removing those exact offsets;
        // captured preview commands retain the real draw positions.
        float px=p.x,py=p.y;
        if(p.kind=="icon") {
            const auto& icon=isaac_eid_ui::assets::Icons[p.value];
            px-=icon.left; py-=icon.top;
            if(px+icon.width>x+width+.02f) return false;
        }
        if (px<x-4 || px>x+width+.02f || py<y-3 || py>y+height+.02f) {
            std::fprintf(stderr,"Position bounds: %s value=%u %s/%s pos=%.2f,%.2f box=%.2f,%.2f %.2f,%.2f\n",p.kind.c_str(),p.value,p.path.c_str(),p.animation.c_str(),p.x,p.y,x,y,width,height); return false;
        }
        if (p.kind=="glyph" || p.kind=="fallback") {
            const auto* glyph=isaac_eid_ui::Glyph(p.value);
            if (!glyph || p.x+glyph->advance>x+width+.02f || p.y+10>y+height+1.02f) {
                std::fprintf(stderr,"Glyph bounds: value=%u pos=%.2f,%.2f advance=%d box=%.2f,%.2f %.2f,%.2f\n",p.value,p.x,p.y,glyph?glyph->advance:-1,x,y,width,height); return false;
            }
        }
    }
    return true;
}
void NoRunReads() {
    bool permitted=true;
    for (auto address:queries) {
        // These are the only explicit memory reads the menu helpers need.
        // A normal EID panel would query 0x24f99c with frame.game==0.
        permitted = permitted && (address==module+isaac::profile::ManagerGlobal || address==Manager()+0x36538);
    }
    Check(permitted,"title rendering has no game/room/frame-counter reads");
}
void Trampoline(std::uintptr_t at,std::uintptr_t target) {
    // x86-64 jump at the native module-relative call site. The production
    // source remains unchanged and calls its real computed function pointer.
    const unsigned char prefix[]={0x48,0xb8};
    std::memcpy(reinterpret_cast<void*>(at),prefix,2); Put(at+2,target);
    const unsigned char suffix[]={0xff,0xe0}; std::memcpy(reinterpret_cast<void*>(at+10),suffix,2);
}
void Prepare() {
    void* allocated=mmap(nullptr,0xac0000,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    Check(allocated!=MAP_FAILED,"test module memory allocation"); module=reinterpret_cast<std::uintptr_t>(allocated);
    regions={{module,0xac0000},{Manager(),manager.size()*8}};
    Put(module+isaac::profile::ManagerGlobal,Manager());
    Trampoline(module+0x3c0c50,reinterpret_cast<std::uintptr_t>(&HostConfig));
    Trampoline(module+0x3c0c9c,reinterpret_cast<std::uintptr_t>(&HostConfig));
    Check(mprotect(reinterpret_cast<void*>(module+0x3c0000),4096,PROT_READ|PROT_EXEC)==0,"native config boundary becomes executable and read-only");
}
void JsonString(FILE* out,const std::string& text) {
    std::fputc('"',out);
    for (unsigned char ch:text) {
        if (ch=='"'||ch=='\\') std::fputc('\\',out);
        if (ch<32) std::fprintf(out,"\\u%04x",ch); else std::fputc(ch,out);
    }
    std::fputc('"',out);
}
void ExportPaint(FILE* out,unsigned screen,unsigned source_call,bool& first) {
    for(const auto& p:paint) {
        std::fprintf(out,"%s{\"screen\":%u,\"source_call\":%u,\"kind\":",first?"":",",screen,source_call); first=false;
        JsonString(out,p.kind); std::fputs(",\"path\":",out); JsonString(out,p.path);
        std::fputs(",\"animation\":",out); JsonString(out,p.animation);
        std::fputs(",\"sheet\":",out); JsonString(out,p.sheet);
        std::fprintf(out,",\"value\":%u,\"frame\":%d,\"x\":%.5g,\"y\":%.5g,\"rgba\":[%.5g,%.5g,%.5g,%.5g]}",p.value,p.frame,p.x,p.y,p.tint.r,p.tint.g,p.tint.b,p.tint.a);
    }
}
void Capture(const char* filename) {
    auto* out=std::fopen(filename,"wb"); Check(out!=nullptr,"paint capture file opens");
    std::fprintf(out,"{\"width\":%g,\"height\":%g,\"source\":\"actual eid_ui.cpp menu helpers; native sprite/config boundaries recorded\",\"paint\":[",capture::Width,capture::Height);
    unsigned previous=~0u; bool first=true;
    for(const auto& call:capture::Calls) {
        if(call.screen!=previous) { isaac::ResetEidUi(); previous=call.screen; }
        paint.clear(); queries.clear();
        auto frame=Frame(capture::Width,capture::Height);
        if(call.kind==1) isaac::DrawEidMenuText(frame,call.text,call.x,call.y,call.color,call.width);
        else if(call.kind==2) isaac::DrawEidMenuDescription(frame,call.item,call.name,call.markup,call.x,call.y,call.width,call.height,1000);
        else continue;
        Check(!paint.empty(),"actual menu call produces font/icon paint"); NoRunReads();
        const float width=call.kind==1?std::fmin(call.width,capture::Width-call.x):call.width;
        const float height=call.kind==1?12.f:call.height;
        Check(Bounds(call.x,call.y,width,height),"actual DrawMenu call stays inside its text/description rectangle");
        ExportPaint(out,call.screen,call.index,first);
    }
    std::fputs("]}\n",out); std::fclose(out);
}
}

extern "C" std::uint32_t svcQueryMemory(MemoryInfo* info,std::uint32_t* page,std::uint64_t address) {
    queries.push_back(address); *page=0;
    for(const auto& region:regions) if(address>=region.p && address-region.p<region.size) {
        *info={region.p,region.size,Perm_R}; return 0;
    }
    *info={}; return 1;
}
namespace isaac {
bool NativeSprite::Create(std::uintptr_t mod) { module_=mod; created_=mod!=0; return created_; }
bool NativeSprite::Load(const char* path) {
    sprites[this].path=path;
    const bool font=std::strstr(path,"eid_glyphs")!=nullptr;
    if(font) ++font_loads;
    else if(std::strstr(path,"item_icon")) ++item_loads;
    loaded_=created_ && !(font?fail_font:fail_icons); return loaded_;
}
bool NativeSprite::SetSpritesheet(unsigned layer,const char* path) { Check(layer==0,"item icon uses existing layer zero"); sprites[this].sheet=path; return loaded_; }
void NativeSprite::LoadGraphics() {}
void NativeSprite::SetScale(float x,float y) { sprites[this].sx=x; sprites[this].sy=y; }
void NativeSprite::SetTint(float r,float g,float b,float a) { sprites[this].tint={r,g,b,a}; }
bool NativeSprite::SetFrame(const char* animation,int value) {
    if(!loaded_) return false;
    sprites[this].animation=animation; sprites[this].frame=value;
    return value>=0;
}
void NativeSprite::Render(NativeVec2 position,NativeVec2,NativeVec2) {
    const auto& state=sprites[this];
    Check(loaded_ && state.sx==1 && state.sy==1,"renderer uses loaded sprite at authentic font scale");
    std::string kind="icon"; unsigned value{};
    if(state.animation=="Glyphs") { kind="glyph"; value=GlyphCode(state.frame); Check(value!=0,"font frame maps to a real glyph"); }
    else if(state.animation=="Item") { kind="item"; std::sscanf(state.sheet.c_str(),"mock-item-%u",&value); }
    else {
        const char* paths[]={"gfx/codex_eid_ui/eid_inline_icons.anm2","gfx/codex_eid_ui/eid_player_icons.anm2",
            "gfx/codex_eid_ui/eid_transform_icons.anm2","gfx/codex_eid_ui/eid_cardspills.anm2","gfx/ui/buttons.anm2"};
        bool found=false;
        for(const auto& icon:isaac_eid_ui::assets::Icons) {
            if(icon.animation==state.animation && icon.frame==state.frame && state.path==paths[icon.sheet]) { found=true; break; }
            ++value;
        }
        Check(found,"inline icon frame belongs to imported source assets");
    }
    paint.push_back({kind,state.path,state.animation,state.sheet,value,state.frame,position.x,position.y,state.tint});
}
void NativeSprite::Destroy() {
    if(created_ && sprites[this].path.find("item_icon")!=std::string::npos) ++item_destroys;
    sprites.erase(this); created_=loaded_=false; module_=0;
}
bool CopyNativeString(std::uintptr_t address,char* output,std::size_t capacity) {
    if(address<Configs()+0x38 || (address-Configs()-0x38)%0x80 || !capacity) return false;
    const auto id=(address-Configs()-0x38)/0x80;
    if(id>=1024) return false;
    return std::snprintf(output,capacity,"mock-item-%u",static_cast<unsigned>(id))<static_cast<int>(capacity);
}
}

int main(int argc,char** argv) {
    Prepare(); auto frame=Frame();
    Start("plain title text");
    const auto beforeFont=font_loads;
    isaac::DrawEidMenuText(frame,"External Item Descriptions",12,12,0xffffff,80);
    Check(!paint.empty() && Bounds(12,12,80,12),"long label clips by actual glyph advance");
    Check(queries.empty(),"plain title label requires no game memory");
    isaac::DrawEidMenuText(frame,"MODS",12,30,0xff00ff,80);
    Check(font_loads==beforeFont+1,"font sprite loads once across menu calls");
    Start("unicode and missing glyph");
    isaac::DrawEidMenuText(frame,"\xc3\xa9 \xe2\x86\x91 \xf0\x9f\x98\x80",10,10,0xabcdef,100);
    Check(Bounds(10,10,100,12),"UTF-8/missing glyph positions remain bounded"); NoRunReads();

    Start("font failure fallback"); fail_font=true;
    const auto beforeFallback=fallback_calls;
    isaac::DrawEidMenuText(frame,"Fallback",10,10,0xffffff,100);
    Check(fallback_calls==beforeFallback+8 && paint.size()==8,"missing sprite font delegates readable text to canvas");
    paint.clear(); fallback_loaded=false;
    isaac::DrawEidMenuText(frame,"No native font",10,10,0xffffff,100);
    Check(paint.empty(),"unloaded canvas fallback can safely decline drawing");
    frame.draw.text=nullptr;
    isaac::DrawEidMenuText(frame,"No callback",10,10,0xffffff,100);
    Check(paint.empty(),"null fallback callback is safe"); frame=Frame();

    Start("selection icon cache");
    const auto beforeItems=item_loads, beforeConfig=config_calls, beforeDestroy=item_destroys;
    const char* body="{{Collectible1}} First#{{Trinket2}} Second#{{Damage}} +1 damage";
    isaac::DrawEidMenuDescription(frame,118,"Brimstone",body,233.56f,50,219.44f,160,1000);
    Check(item_loads==beforeItems+3 && config_calls==beforeConfig+3,"heading and inline items use verified config boundary");
    Check(Bounds(233.56f,50,219.44f,160),"description glyphs and icons stay inside actual menu bounds"); NoRunReads();
    paint.clear();
    isaac::DrawEidMenuDescription(frame,118,"Brimstone",body,233.56f,50,219.44f,160,1100);
    Check(item_loads==beforeItems+3 && item_destroys==beforeDestroy,"unchanged selection reuses item sprites");
    isaac::DrawEidMenuDescription(frame,559,"120 Volt","Electricity",233.56f,50,219.44f,160,1200);
    Check(item_destroys==beforeDestroy+3 && item_loads==beforeItems+4,"changing selection destroys prior heading/inline icon cache");
    const auto changed=item_destroys;
    isaac::DrawEidMenuDescription(frame,559,"Changed title","New body",233.56f,50,219.44f,160,1300);
    Check(item_destroys==changed+1,"same ID with changed content rebuilds its icon/layout cache");

    Start("long descriptions, narrow screen and pages");
    std::string longBody;
    for(unsigned i=0;i<35;++i) longBody+="{{Damage}} Long description wraps without crowding the item list#";
    isaac::DrawEidMenuDescription(Frame(250,270),0,"A long heading that must wrap across several lines",longBody.c_str(),125.46f,50,97.54f,172,1000);
    Check(Bounds(125.46f,50,97.54f,172),"long names and first description page fit a narrow 270-high screen");
    const auto firstPage=paint;
    paint.clear();
    isaac::DrawEidMenuDescription(Frame(250,270),0,"A long heading that must wrap across several lines",longBody.c_str(),125.46f,50,97.54f,172,7500);
    Check(Bounds(125.46f,50,97.54f,172),"automatic later page fits the same bounded panel");
    bool changedPage=paint.size()!=firstPage.size();
    for(std::size_t i=0;i<paint.size() && i<firstPage.size();++i) changedPage=changedPage||paint[i].value!=firstPage[i].value;
    Check(changedPage,"page label/content advances after 6500 milliseconds"); NoRunReads();
    paint.clear();
    isaac::DrawEidMenuDescription(frame,0,"Ignored","body",10,10,39,100,1);
    isaac::DrawEidMenuDescription(frame,0,"Ignored","body",10,10,100,23,1);
    Check(paint.empty(),"invalid small panel sizes are rejected");

    Start("complete collectible catalogue in actual picker panel");
    const auto catalogue=isaac_eid::EnglishRepentance(); unsigned descriptions{};
    for(std::size_t i=0;i<catalogue.count;++i) {
        const auto& entry=catalogue.entries[i];
        if(entry.category!=static_cast<unsigned>(isaac_eid::Category::Collectible)) continue;
        isaac_eid::Description desc{};
        if(!catalogue.Find(isaac_eid::Category::Collectible,entry.lookup_id,desc)) continue;
        active_case=std::string("catalogue item ")+std::to_string(entry.lookup_id)+" "+desc.name;
        paint.clear(); queries.clear();
        isaac::DrawEidMenuDescription(frame,entry.lookup_id,desc.name,desc.markup,233.56f,50,219.44f,160,1000);
        Check(!paint.empty() && Bounds(233.56f,50,219.44f,160),"catalogue description first page has bounded actual paint commands");
        NoRunReads(); ++descriptions;
    }
    Check(descriptions>700,"complete current collectible catalogue exercised");
    // Iteration 7 paper-menu helpers.
    Start("paper ink");
    isaac::DrawEidMenuBody(frame,"Ink",10,10,100,20,1000);
    Check(!paint.empty() && paint[0].tint.r>.99f,"description text keeps its colour by default");
    paint.clear(); isaac::SetEidInk(true);
    isaac::DrawEidMenuBody(frame,"Ink",10,10,100,20,1000);
    Check(!paint.empty() && std::fabs(paint[0].tint.r-42/255.f)<.01f && std::fabs(paint[0].tint.b-29/255.f)<.01f,"white description text becomes dark ink on paper");
    paint.clear(); isaac::DrawEidMenuBody(frame,"{{ColorRed}}Ink",10,10,100,20,1000);
    Check(!paint.empty() && paint[0].tint.r<.6f && paint[0].tint.r>paint[0].tint.g,"coloured description text is darkened, keeping its hue");
    paint.clear(); isaac::DrawEidMenuText(frame,"Ink",10,10,0x2a1f1d,100);
    Check(!paint.empty() && std::fabs(paint[0].tint.r-42/255.f)<.01f,"menu labels keep the exact colour the menu asks for");
    isaac::SetEidInk(false);
    Start("menu text width");
    isaac::DrawEidMenuText(frame,"Hits: 12",10,10,0xffffff,400);
    const auto* last=isaac_eid_ui::Glyph('2');
    Check(last && std::fabs(paint.back().x+last->advance-10-isaac::EidMenuTextWidth("Hits: 12"))<.02f,"measured width matches drawn advances");
    Check(isaac::EidMenuTextWidth(nullptr)==0 && isaac::EidMenuTextWidth("")==0,"empty text has no width");
    Start("description body pages");
    std::string numbered;
    for(unsigned i=0;i<12;++i) numbered+="Line "+std::to_string(i)+" of the description#";
    isaac::DrawEidMenuBody(frame,numbered.c_str(),70,190,340,50,1000);
    Check(!paint.empty() && Bounds(70,190,340,50),"body text stays inside the item note"); NoRunReads();
    const auto bodyFirst=paint; paint.clear();
    isaac::DrawEidMenuBody(frame,numbered.c_str(),70,190,340,50,6100);
    bool bodyPaged=paint.size()!=bodyFirst.size();
    for(std::size_t i=0;i<paint.size() && i<bodyFirst.size();++i) bodyPaged=bodyPaged||paint[i].value!=bodyFirst[i].value;
    Check(bodyPaged && Bounds(70,190,340,50),"body turns its page after five seconds");
    paint.clear();
    isaac::DrawEidMenuBody(frame,"body",10,10,39,100,1); isaac::DrawEidMenuBody(frame,"body",10,10,100,9,1);
    isaac::DrawEidMenuBody(frame,nullptr,10,10,100,100,1);
    Check(paint.empty(),"invalid body requests are rejected");
    Start("standalone icon");
    isaac::DrawEidIcon(frame,"Damage",20,20);
    Check(paint.size()==1 && paint[0].kind=="icon","named EID icon draws one sprite");
    paint.clear(); isaac::DrawEidIcon(frame,"NoSuchIcon",20,20); isaac::DrawEidIcon(frame,nullptr,20,20);
    Check(paint.empty(),"unknown icon names draw nothing");
    if(argc==2) { Start("actual menu captures"); Capture(argv[1]); }
    isaac::ResetEidUi(); Check(sprites.empty(),"all owned font/icon sprites are released before module unload");
    munmap(reinterpret_cast<void*>(module),0xac0000);
    std::printf("PASS: %u checks, %u scenarios, %u collectible descriptions; actual eid_ui.cpp glyphs, cache, fallback, title-only reads and bounds.\n",checks,cases,descriptions);
}

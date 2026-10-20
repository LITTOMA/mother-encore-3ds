#include "encore/world_links.hpp"
#include "encore/content.hpp"
#include "encore/resource_catalog.hpp"
#include <set>
#include "encore/crc32.hpp"
#include <cmath>
#include <cstring>

namespace encore::upstream {
namespace {
constexpr size_t header_bytes=64,max_bytes=64*1024;
constexpr uint32_t strides[]={1,24,84,36,4,8,8};
constexpr char pin[]="7d9246600fffe518408f5830d4848635019005a3";
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint16_t u16(const uint8_t*p){return uint16_t(p[0]|p[1]<<8);}
float f32(const uint8_t*p){const uint32_t v=u32(p);float f;std::memcpy(&f,&v,4);return f;}
struct Layout {const uint8_t*p=nullptr;size_t n=0;
    uint32_t count(uint32_t k)const{return k>=1&&k<=u32(p+52)?u32(p+header_bytes+(k-1)*16+8):0;}
    const uint8_t*record(uint32_t k,uint32_t i)const{return p+u32(p+header_bytes+(k-1)*16+4)+size_t(i)*strides[k-1];}
    std::string_view text(const uint8_t*r)const{const auto off=u32(r),len=u32(r+4),pool=count(1);
        if(!len||off>pool||len>pool-off)return {};return std::string_view(reinterpret_cast<const char*>(record(1,0))+off,len);}
};
bool printable(std::string_view s){for(unsigned char c:s)if(c<32||c>126)return false;return true;}
}
uint32_t WorldLinksData::capability()const{return bytes_.empty()?0:u32(bytes_.data()+20);}
uint32_t WorldLinksData::scene_count()const{return bytes_.empty()?0:Layout{bytes_.data(),bytes_.size()}.count(2);}
uint32_t WorldLinksData::route_count()const{return bytes_.empty()?0:Layout{bytes_.data(),bytes_.size()}.count(3);}
WorldScene WorldLinksData::scene(uint32_t i)const{
    if(i>=scene_count())return {};const Layout l{bytes_.data(),bytes_.size()};const auto*r=l.record(2,i);
    return {u32(r),l.text(r+4),u32(r+12),u32(r+16)};
}
WorldRoute WorldLinksData::route(uint32_t i)const{
    if(i>=route_count())return {};const Layout l{bytes_.data(),bytes_.size()};const auto*r=l.record(3,i);
    WorldRoute w;w.id=u32(r);w.from=u32(r+4);w.to=u32(r+8);w.door=l.text(r+12);w.destination={f32(r+20),f32(r+24)};w.direction={f32(r+28),f32(r+32)};
    w.sound=l.text(r+36);w.end_sound=l.text(r+44);w.in_kind=u16(r+52);w.out_kind=u16(r+54);w.in_speed=f32(r+56);w.out_speed=f32(r+60);w.music_fade=f32(r+64);
    w.flag=l.text(r+68);w.flag_value=u32(r+76)!=0;w.flags=u32(r+80);return w;
}
uint32_t WorldLinksData::find_scene(uint32_t id)const{for(uint32_t i=0;i<scene_count();++i)if(scene(i).id==id)return i;return kNotFound;}
uint32_t WorldLinksData::find_route(uint32_t id)const{for(uint32_t i=0;i<route_count();++i)if(route(i).id==id)return i;return kNotFound;}
uint32_t WorldLinksData::find_route_from(uint32_t scene_id,std::string_view door)const{
    for(uint32_t i=0;i<route_count();++i){const auto r=route(i);if(r.from==scene_id&&r.door==door)return i;}return kNotFound;
}
bool WorldLinksData::load(const uint8_t*p,size_t n,std::string&error){
    auto fail=[&](const char*m){error=m;return false;};
    if(!p||n<header_bytes+kWorldLinkSectionCount*16||n>max_bytes)return fail("World links size rejected");
    if(std::memcmp(p,"ENCLNK01",8)||u32(p+8)!=1||u32(p+12)!=n||(u32(p+20)<1||u32(p+20)>5)||u32(p+24)||u32(p+28)||u32(p+52)!=(u32(p+20)==1?3u:u32(p+20)>=4?u32(p+20)+2:5u)||u32(p+56)||u32(p+60))
        return fail("World links schema/capability/reserved rejected");
    for(size_t i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail("World links source pin rejected");}
    if(encore::crc32(p+32,n-32)!=u32(p+16))return fail("World links checksum rejected");
    const auto sections=u32(p+52);size_t cursor=header_bytes+sections*16;if(n<cursor)return fail("World links truncated directory");
    for(uint32_t i=0;i<sections;++i){const auto*d=p+header_bytes+i*16;const auto off=u32(d+4),count=u32(d+8);
        if(u32(d)!=i+1||u32(d+12)!=strides[i])return fail("World links directory rejected");
        if(!count){if(off)return fail("World links empty section rejected");continue;}
        if(off%4||off<cursor||off>n||count>(n-off)/strides[i])return fail("World links section span rejected");
        for(size_t j=cursor;j<off;++j)if(p[j])return fail("World links padding rejected");
        cursor=size_t(off)+size_t(count)*strides[i];}
    if(cursor!=n)return fail("World links trailing bytes rejected");
    const Layout l{p,n};
    const auto pool=l.count(1),scenes=l.count(2),routes=l.count(3);
    if(!pool||pool>32768||scenes<2||scenes>64||!routes||routes>256)return fail("World links capacity rejected");
    auto text_ok=[&](const uint8_t*r,bool empty,size_t limit){const auto off=u32(r),len=u32(r+4);
        if(!len)return empty&&!off;return off<=pool&&len<=pool-off&&len<=limit&&printable(l.text(r));};
    for(uint32_t i=0;i<scenes;++i){const auto*r=l.record(2,i);
        if(!u32(r)||!text_ok(r+4,false,256)||l.text(r+4).substr(0,6)!="res://"||!u32(r+12)||u32(r+20))return fail("World links scene rejected");
        for(uint32_t k=0;k<i;++k)if(u32(l.record(2,k))==u32(r)||l.text(l.record(2,k)+4)==l.text(r+4))return fail("World links duplicate scene rejected");}
    auto scene_known=[&](uint32_t id){for(uint32_t k=0;k<scenes;++k)if(u32(l.record(2,k))==id)return true;return false;};
    for(uint32_t i=0;i<routes;++i){const auto*r=l.record(3,i);
        const float dx=f32(r+28),dy=f32(r+32);const float values[]={f32(r+20),f32(r+24),dx,dy,f32(r+56),f32(r+60),f32(r+64)};
        bool numbers=true;for(float x:values)numbers&=std::isfinite(x)&&std::fabs(x)<=1000000;
        const bool cardinal=(dx==0&&std::fabs(dy)==1)||(dy==0&&std::fabs(dx)==1)||(dx==0&&dy==0);
        auto audio=[&](const uint8_t*t){return text_ok(t,true,256)&&(!u32(t+4)||l.text(t).substr(0,6)=="res://");};
        if(!u32(r)||!scene_known(u32(r+4))||!scene_known(u32(r+8))||(u32(p+20)==1&&u32(r+4)==u32(r+8))||!text_ok(r+12,false,512)||!numbers||!cardinal||!audio(r+36)||!audio(r+44)||
           u16(r+52)>2||u16(r+54)>2||f32(r+56)<=0||f32(r+56)>16||f32(r+60)<=0||f32(r+60)>16||f32(r+64)<0||f32(r+64)>60||!text_ok(r+68,true,64)||u32(r+76)>1||
           u32(r+80)>1||(bool(u32(r+80)&1)!=(f32(r+64)>0)))return fail("World links route rejected");
        for(uint32_t k=0;k<i;++k){const auto*o=l.record(3,k);if(u32(o)==u32(r)||(u32(o+4)==u32(r+4)&&l.text(o+12)==l.text(r+12)))return fail("World links duplicate route rejected");}
    }
    if(u32(p+20)>=2){
        const auto assets=l.count(4),refs=l.count(5);if(assets>scenes||refs>4096)return fail("World scene resource capacity rejected");
        std::vector<uint8_t>used(refs,0);std::set<uint32_t>owners;bool has_steps=false;
        for(uint32_t i=0;i<assets;++i){const auto*r=l.record(4,i);const auto id=u32(r),first=u32(r+24),count=u32(r+28);bool field=false;
            for(uint32_t j=0;j<scenes;++j){const auto*scene=l.record(2,j);field|=u32(scene)==id&&u32(scene+16)!=0;}
            has_steps|=u32(r+32)!=0;const bool music=u32(r+8)!=0;if(!field||!owners.insert(id).second||(u32(p+20)==2&&u32(r+32))||count>128||first>refs||count>refs-first||(!count&&first)||music!=(u32(r+12)!=0)||music!=(u32(r+16)!=0))return fail("World scene asset row rejected");
            std::set<uint32_t>npc_ids;for(uint32_t j=0;j<count;++j){const auto at=first+j,id=u32(l.record(5,at));if(used[at]||!id||!npc_ids.insert(id).second)return fail("World scene NPC reference rejected");used[at]=1;}
        }
        if((u32(p+20)>=3)!=has_steps)return fail("World stepping capability must be used");
        for(auto value:used)if(!value)return fail("World scene NPC reference is unowned");
        for(uint32_t i=0;i<scenes;++i){const auto*r=l.record(2,i);if(u32(r+16)&&!owners.count(u32(r)))return fail("World field scene assets missing");}
    }
    std::set<uint32_t>owned_bindings;
    for(uint32_t section=6;section<=sections;++section){
        const auto count=l.count(section);if(!count||count>scenes)return fail("World object/prop owner capacity rejected");
        std::set<uint32_t>owners,bindings;
        for(uint32_t i=0;i<count;++i){const auto*r=l.record(section,i);const auto id=u32(r),binding=u32(r+4);bool field=false;
            for(uint32_t j=0;j<scenes;++j){const auto*s=l.record(2,j);field|=u32(s)==id&&u32(s+16)!=0;}
            if(!field||binding<1024||!owners.insert(id).second||!bindings.insert(binding).second||!owned_bindings.insert(binding).second)return fail("World object/prop owner identity rejected");
        }
    }
    bytes_.assign(p,p+n);error.clear();return true;
}
bool WorldLinksData::resolve_scene(uint32_t id,const ResourceCatalog&catalog,WorldSceneResources&out,std::string&error)const{
    auto fail=[&](const char*why){error=why;return false;};const auto index=find_scene(id);
    if(!valid()||!catalog.valid()||index==kNotFound)return fail("Scene resource lookup has no checked owner");
    WorldSceneResources next;next.scene=scene(index);
    const auto room_role=next.scene.map_role?ResourceRole::FieldRoom:ResourceRole::Room;
    if(catalog.path(room_role,next.scene.room_role).empty()||(next.scene.map_role&&catalog.path(ResourceRole::FieldMap,next.scene.map_role).empty()))return fail("Scene room/map binding is missing or has wrong type");
    if(!next.scene.map_role){out=std::move(next);error.clear();return true;}
    if(capability()==1){
        if(catalog.ids(ResourceRole::FieldRoom).size()!=1||catalog.ids(ResourceRole::FieldMap).size()!=1)return fail("Legacy links cannot select multiple field resource sets");
        bool ambiguous=false;auto singleton=[&](ResourceRole role){const auto ids=catalog.ids(role);ambiguous|=ids.size()>1;return ids.size()==1?ids.front():0u;};
        next.story=singleton(ResourceRole::FieldStory);next.music=singleton(ResourceRole::FieldMusic);next.audio=singleton(ResourceRole::FieldAudio);next.zones=singleton(ResourceRole::FieldMusicZones);next.mick=singleton(ResourceRole::MickTreats);next.npcs=catalog.ids(ResourceRole::FieldNpc);
        if(ambiguous)return fail("Legacy scene asset role is ambiguous");
    }else{
        const Layout l{bytes_.data(),bytes_.size()};const uint8_t*row=nullptr;for(uint32_t i=0;i<l.count(4);++i)if(u32(l.record(4,i))==id)row=l.record(4,i);
        if(!row)return fail("Scene asset owner is absent");next.story=u32(row+4);next.music=u32(row+8);next.audio=u32(row+12);next.zones=u32(row+16);next.mick=u32(row+20);next.steps=u32(row+32);
        if(capability()>=4)for(uint32_t i=0;i<l.count(6);++i){const auto*r=l.record(6,i);if(u32(r)==id)next.props=u32(r+4);}
        if(capability()>=5)for(uint32_t i=0;i<l.count(7);++i){const auto*r=l.record(7,i);if(u32(r)==id)next.objects=u32(r+4);}
        for(uint32_t i=0;i<u32(row+28);++i)next.npcs.push_back(u32(l.record(5,u32(row+24)+i)));
    }
    const std::pair<uint32_t,ResourceRole>refs[]={{next.story,ResourceRole::FieldStory},{next.music,ResourceRole::FieldMusic},{next.audio,ResourceRole::FieldAudio},{next.zones,ResourceRole::FieldMusicZones},{next.mick,ResourceRole::MickTreats},{next.steps,ResourceRole::FieldSteps},{next.props,ResourceRole::FieldProps},{next.objects,ResourceRole::FieldObjects}};
    for(const auto&ref:refs)if(ref.first&&catalog.path(ref.second,ref.first).empty())return fail("Scene asset binding is missing or has wrong type");
    for(auto npc:next.npcs)if(catalog.path(ResourceRole::FieldNpc,npc).empty())return fail("Scene NPC binding is missing or has wrong type");
    if(bool(next.music)!=bool(next.audio)||bool(next.music)!=bool(next.zones))return fail("Scene music group is incomplete");
    out=std::move(next);error.clear();return true;
}
bool WorldLinksData::bind_catalog(const ResourceCatalog&catalog,std::string&error)const{
    if(!valid()){error="World links are not loaded";return false;}
    std::set<uint32_t>used;
    for(uint32_t i=0;i<scene_count();++i){WorldSceneResources r;if(!resolve_scene(scene(i).id,catalog,r,error))return false;used.insert(r.scene.room_role);if(r.scene.map_role)used.insert(r.scene.map_role);
        for(auto id:{r.story,r.music,r.audio,r.zones,r.mick,r.steps,r.props,r.objects})if(id)used.insert(id);for(auto id:r.npcs)used.insert(id);}
    for(auto role:{ResourceRole::FieldRoom,ResourceRole::FieldMap,ResourceRole::FieldStory,ResourceRole::FieldMusic,ResourceRole::FieldAudio,ResourceRole::FieldMusicZones,ResourceRole::FieldNpc,ResourceRole::FieldSteps,ResourceRole::FieldProps,ResourceRole::FieldObjects})for(auto id:catalog.ids(role))if(!used.count(id)){error="Catalog scene resource has no world owner";return false;}
    error.clear();return true;
}
bool WorldLinksData::load_file(const char*path,std::string&error){
    if(!path){error="Missing world links path";return false;}
    std::vector<uint8_t> bytes;return encore::read_file(path,bytes,max_bytes,error)&&load(bytes.data(),bytes.size(),error);
}
}

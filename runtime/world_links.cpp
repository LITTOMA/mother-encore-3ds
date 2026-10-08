#include "encore/world_links.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cmath>
#include <cstring>

namespace encore::upstream {
namespace {
constexpr size_t header_bytes=64,max_bytes=64*1024;
constexpr uint32_t strides[kWorldLinkSectionCount]={1,24,84};
constexpr char pin[]="7d9246600fffe518408f5830d4848635019005a3";
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint16_t u16(const uint8_t*p){return uint16_t(p[0]|p[1]<<8);}
float f32(const uint8_t*p){const uint32_t v=u32(p);float f;std::memcpy(&f,&v,4);return f;}
struct Layout {const uint8_t*p=nullptr;size_t n=0;
    uint32_t count(uint32_t k)const{return u32(p+header_bytes+(k-1)*16+8);}
    const uint8_t*record(uint32_t k,uint32_t i)const{return p+u32(p+header_bytes+(k-1)*16+4)+size_t(i)*strides[k-1];}
    std::string_view text(const uint8_t*r)const{const auto off=u32(r),len=u32(r+4),pool=count(1);
        if(!len||off>pool||len>pool-off)return {};return std::string_view(reinterpret_cast<const char*>(record(1,0))+off,len);}
};
bool printable(std::string_view s){for(unsigned char c:s)if(c<32||c>126)return false;return true;}
}
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
    if(std::memcmp(p,"ENCLNK01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1||u32(p+24)||u32(p+28)||u32(p+52)!=kWorldLinkSectionCount||u32(p+56)||u32(p+60))
        return fail("World links schema/capability/reserved rejected");
    for(size_t i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail("World links source pin rejected");}
    if(encore::crc32(p+32,n-32)!=u32(p+16))return fail("World links checksum rejected");
    size_t cursor=header_bytes+kWorldLinkSectionCount*16;
    for(uint32_t i=0;i<kWorldLinkSectionCount;++i){const auto*d=p+header_bytes+i*16;const auto off=u32(d+4),count=u32(d+8);
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
        if(!u32(r)||!scene_known(u32(r+4))||!scene_known(u32(r+8))||u32(r+4)==u32(r+8)||!text_ok(r+12,false,512)||!numbers||!cardinal||!audio(r+36)||!audio(r+44)||
           u16(r+52)>2||u16(r+54)>2||f32(r+56)<=0||f32(r+56)>16||f32(r+60)<=0||f32(r+60)>16||f32(r+64)<0||f32(r+64)>60||!text_ok(r+68,true,64)||u32(r+76)>1||
           u32(r+80)>1||(bool(u32(r+80)&1)!=(f32(r+64)>0)))return fail("World links route rejected");
        for(uint32_t k=0;k<i;++k){const auto*o=l.record(3,k);if(u32(o)==u32(r)||(u32(o+4)==u32(r+4)&&l.text(o+12)==l.text(r+12)))return fail("World links duplicate route rejected");}
    }
    bytes_.assign(p,p+n);error.clear();return true;
}
bool WorldLinksData::load_file(const char*path,std::string&error){
    if(!path){error="Missing world links path";return false;}
    std::vector<uint8_t> bytes;return encore::read_file(path,bytes,max_bytes,error)&&load(bytes.data(),bytes.size(),error);
}
}

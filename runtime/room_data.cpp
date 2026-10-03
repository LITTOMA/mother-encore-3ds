#include "encore/room_data.hpp"
#include "encore/content.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <utility>

namespace encore::upstream {
namespace {
uint16_t u16(const uint8_t* p) { return static_cast<uint16_t>(p[0]|(uint16_t(p[1])<<8)); }
uint32_t u32(const uint8_t* p) { return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24); }
uint64_t u64(const uint8_t* p) { return uint64_t(u32(p))|(uint64_t(u32(p+4))<<32); }
int32_t i32(const uint8_t* p) { const uint32_t n=u32(p); int32_t v; std::memcpy(&v,&n,4); return v; }
float f32(const uint8_t* p) { const uint32_t n=u32(p); float v; std::memcpy(&v,&n,4); return v; }
double f64(const uint8_t* p) { const uint64_t n=u64(p); double v; std::memcpy(&v,&n,8); return v; }
Vec2 read_vec2(const uint8_t* p) { return {f32(p),f32(p+4)}; }
bool zero(const uint8_t* p,size_t n) { for(size_t i=0;i<n;++i) if(p[i]) return false; return true; }
bool finite(Vec2 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::abs(v.x)<=1000000&&std::abs(v.y)<=1000000; }
bool range(uint32_t first,uint32_t n,uint32_t total) { return first<=total&&n<=total-first; }
bool ref(uint32_t index,uint32_t total,bool optional=false) { return index<total||(optional&&index==kRoomNoIndex); }
const uint8_t* directory(const uint8_t* bytes,RoomSection section) {
    const auto index=static_cast<uint16_t>(section);
    return bytes&&index>=1&&index<=kRoomSectionCount?bytes+kRoomHeaderBytes+(index-1)*kRoomDirectoryEntryBytes:nullptr;
}
}
static_assert(sizeof(float)==4&&sizeof(double)==8,"ENCRMD01 requires IEEE-width floats");
static_assert(std::numeric_limits<float>::is_iec559&&std::numeric_limits<double>::is_iec559,"ENCRMD01 requires IEEE-754 floats");
uint32_t RoomView::count(RoomSection section) const { const auto* d=directory(bytes_,section); return d?u32(d+8):0; }
uint32_t RoomView::section_offset(RoomSection section) const { const auto* d=directory(bytes_,section); return d?u32(d+4):0; }
const uint8_t* RoomView::record(RoomSection section,uint32_t index) const {
    const auto* d=directory(bytes_,section);
    return d&&index<u32(d+8)?bytes_+u32(d+4)+size_t(index)*u32(d+12):nullptr;
}
std::string_view RoomView::string(uint32_t index) const {
    const auto* p=record(RoomSection::StringRef,index);
    // char is permitted to alias bytes; packed numeric objects are never cast.
    return p?std::string_view(reinterpret_cast<const char*>(bytes_+u32(p)),u32(p+4)):std::string_view{};
}
const char* RoomView::string_data(uint32_t index) const { auto s=string(index); return s.data()?s.data():""; }
RoomResource RoomView::resource(uint32_t index) const {
    RoomResource r; const auto* p=record(RoomSection::Resource,index); if(!p) return r;
    r.stable_id=u32(p+0);
    r.path_string=u32(p+4);
    r.width=u16(p+8);
    r.height=u16(p+10);
    r.columns=u16(p+12);
    r.rows=u16(p+14);
    r.kind=u16(p+16);
    r.flags=u16(p+18);
    std::copy(p+24,p+56,r.sha256.begin());
    return r;
}
RoomPolygon RoomView::polygon(uint32_t index) const {
    RoomPolygon r; const auto* p=record(RoomSection::Polygon,index); if(!p) return r;
    r.body_id=u32(p+0);
    r.owner_id=u32(p+4);
    r.first_vertex=u32(p+8);
    r.vertex_count=u16(p+12);
    r.flags=u16(p+14);
    r.minx=f32(p+16);
    r.miny=f32(p+20);
    r.maxx=f32(p+24);
    r.maxy=f32(p+28);
    return r;
}
RoomBodyRule RoomView::body_rule(uint32_t index) const {
    RoomBodyRule r; const auto* p=record(RoomSection::BodyRule,index); if(!p) return r;
    r.body_id=u32(p+0);
    r.source_path_string=u32(p+4);
    r.initially_enabled=p[8];
    r.flags=p[9];
    return r;
}
RoomOverlay RoomView::overlay(uint32_t index) const {
    RoomOverlay r; const auto* p=record(RoomSection::Overlay,index); if(!p) return r;
    r.stable_id=u32(p+0);
    r.resource_index=u32(p+4);
    r.x=f32(p+8);
    r.y=f32(p+12);
    r.sort_y=f32(p+16);
    r.u=u16(p+20);
    r.v=u16(p+22);
    r.w=u16(p+24);
    r.h=u16(p+26);
    r.flags=u16(p+28);
    return r;
}
RoomMapDraw RoomView::map_draw(uint32_t index) const {
    RoomMapDraw r; const auto* p=record(RoomSection::MapDraw,index); if(!p) return r;
    r.stable_id=u32(p+0);
    r.resource_index=u32(p+4);
    r.x=f32(p+8);
    r.y=f32(p+12);
    r.w=u16(p+16);
    r.h=u16(p+18);
    r.flags=u32(p+20);
    return r;
}
RoomClip RoomView::clip(uint32_t index) const {
    RoomClip r; const auto* p=record(RoomSection::Clip,index); if(!p) return r;
    r.stable_id=u32(p+0);
    r.length=f32(p+4);
    r.first_key=u32(p+8);
    r.key_count=u16(p+12);
    r.flags=p[14];
    r.visibility=p[15];
    r.frame_count=u16(p+16);
    r.channel=u16(p+18);
    return r;
}
RoomKey RoomView::key(uint32_t index) const {
    RoomKey r; const auto* p=record(RoomSection::Key,index); if(!p) return r;
    r.time=f32(p+0);
    r.frame=u16(p+4);
    return r;
}
RoomActorProfile RoomView::actor_profile(uint32_t index) const {
    RoomActorProfile r; const auto* p=record(RoomSection::ActorProfile,index); if(!p) return r;
    r.stable_id=u32(p+0);
    r.execution_kind=u16(p+4);
    r.flags=u16(p+6);
    r.primary_resource=u32(p+8);
    r.shadow_resource=u32(p+12);
    r.emote_resource=u32(p+16);
    r.animation_binding_first=u32(p+20);
    r.animation_binding_count=u16(p+24);
    r.initial_frame=u16(p+26);
    r.emote_initial_frame=u16(p+28);
    r.sprite_position=read_vec2(p+32);
    r.sprite_offset=read_vec2(p+40);
    r.emote_offset=read_vec2(p+48);
    r.shadow_offset=read_vec2(p+56);
    r.direction_first=u32(p+64);
    r.direction_count=u16(p+68);
    r.idle_clip=u32(p+72);
    r.emote_clip=u32(p+76);
    return r;
}
RoomActorInstance RoomView::actor_instance(uint32_t index) const {
    RoomActorInstance r; const auto* p=record(RoomSection::ActorInstance,index); if(!p) return r;
    r.stable_id=u32(p+0);
    r.profile_index=u32(p+4);
    r.binding_kind=u16(p+8);
    r.flags=u16(p+10);
    r.display_name_string=u32(p+12);
    r.position=read_vec2(p+16);
    r.direction=read_vec2(p+24);
    r.initial_clip=u32(p+32);
    return r;
}
RoomCameraArea RoomView::camera_area(uint32_t index) const {
    RoomCameraArea r; const auto* p=record(RoomSection::CameraArea,index); if(!p) return r;
    r.stable_id=u32(p+0);
    r.flags=u32(p+4);
    r.center=read_vec2(p+8);
    r.extents=read_vec2(p+16);
    return r;
}
RoomFlag RoomView::flag(uint32_t index) const {
    RoomFlag r; const auto* p=record(RoomSection::Flag,index); if(!p) return r;
    r.stable_id=u32(p+0);
    r.name_string=u32(p+4);
    r.default_value=p[8];
    r.flags=p[9];
    return r;
}
RoomInitialFlag RoomView::initial_flag(uint32_t index) const {
    RoomInitialFlag r; const auto* p=record(RoomSection::InitialFlag,index); if(!p) return r;
    r.flag_index=u32(p+0);
    r.value=p[4];
    return r;
}
RoomCondition RoomView::condition(uint32_t index) const {
    RoomCondition r; const auto* p=record(RoomSection::Condition,index); if(!p) return r;
    r.flag_index=u32(p+0);
    r.expected_value=p[4];
    r.domain=p[5];
    return r;
}
RoomTrigger RoomView::trigger(uint32_t index) const {
    RoomTrigger r; const auto* p=record(RoomSection::Trigger,index); if(!p) return r;
    r.stable_id=u32(p+0);
    r.first_vertex=u32(p+4);
    r.vertex_count=u16(p+8);
    r.flags=u16(p+10);
    r.condition_first=u32(p+12);
    r.condition_count=u32(p+16);
    r.program_index=u32(p+20);
    r.actor_instance_index=u32(p+24);
    return r;
}
RoomProgram RoomView::program(uint32_t index) const {
    RoomProgram r; const auto* p=record(RoomSection::Program,index); if(!p) return r;
    r.stable_id=u32(p+0);
    r.first_command=u32(p+4);
    r.command_count=u32(p+8);
    r.phrase_count=u32(p+12);
    r.source_path_string=u32(p+16);
    return r;
}
RoomCommand RoomView::command(uint32_t index) const {
    RoomCommand r; const auto* p=record(RoomSection::Command,index); if(!p) return r;
    r.opcode=u16(p+0);
    r.actor_index=u16(p+2);
    r.phrase=u32(p+4);
    r.target_index=u32(p+8);
    r.flags=u32(p+12);
    r.vector=read_vec2(p+16);
    r.value=f64(p+24);
    r.duration=f64(p+32);
    r.auxiliary_index=u32(p+40);
    return r;
}
RoomBinding RoomView::binding(uint32_t index) const {
    RoomBinding r; const auto* p=record(RoomSection::Binding,index); if(!p) return r;
    r.stable_id=u32(p+0);
    r.kind=u16(p+4);
    r.flags=u16(p+6);
    r.target_index=u32(p+8);
    r.auxiliary_index=u32(p+12);
    r.value=f64(p+16);
    r.duration=f64(p+24);
    return r;
}
RoomBattle RoomView::battle(uint32_t index) const {
    RoomBattle r; const auto* p=record(RoomSection::Battle,index); if(!p) return r;
    r.stable_id=u32(p+0);
    r.enemy_string=u32(p+4);
    r.actor_instance_index=u32(p+8);
    r.win_flag_index=u32(p+12);
    r.advantage=i32(p+16);
    r.flags=u32(p+20);
    r.win_cutscene_string=u32(p+24);
    r.battle_resource_index=u32(p+28);
    return r;
}
RoomScene RoomView::scene() const {
    RoomScene r; const auto* p=record(RoomSection::Scene,0); if(!p) return r;
    r.stable_id=u32(p+0);
    r.display_name_string=u32(p+4);
    r.version_string=u32(p+8);
    r.source_scene_string=u32(p+12);
    r.player_instance_index=u32(p+16);
    r.initial_program_index=u32(p+20);
    r.actor_hull_first=u32(p+24);
    r.actor_hull_count=u32(p+28);
    r.spawn=read_vec2(p+32);
    r.start_direction=read_vec2(p+40);
    r.initial_motion_state=u16(p+48);
    r.initial_frame=u16(p+50);
    r.initial_flag_first=u32(p+52);
    r.initial_flag_count=u32(p+56);
    r.body_rule_first=u32(p+60);
    r.body_rule_count=u32(p+64);
    r.default_camera_area=u32(p+68);
    r.rule_profile_id=u32(p+72);
    r.flags=u32(p+76);
    return r;
}
RoomAnimationBinding RoomView::animation_binding(uint32_t index) const {
    RoomAnimationBinding r; const auto* p=record(RoomSection::AnimationBinding,index); if(!p) return r;
    r.actor_profile_index=u16(p+0);
    r.motion_state=p[2];
    r.direction=p[3];
    r.clip_index=u32(p+4);
    return r;
}
RoomMovementPath RoomView::movement_path(uint32_t index) const {
    const auto* p=record(RoomSection::MovementPath,index);
    return p?RoomMovementPath{u32(p),u32(p+4),u16(p+8),u16(p+10),u16(p+12),f64(p+16)}:RoomMovementPath{};
}
RoomMovementPathEntry RoomView::movement_path_entry(uint32_t index) const {
    const auto* p=record(RoomSection::MovementPathEntry,index);
    return p?RoomMovementPathEntry{u16(p),read_vec2(p+8),f64(p+16)}:RoomMovementPathEntry{};
}
Vec2 RoomView::vertex(uint32_t index) const { const auto* p=record(RoomSection::Vertex,index); return p?read_vec2(p):Vec2{}; }
uint16_t RoomView::direction_frame(uint32_t index) const { const auto* p=record(RoomSection::DirectionFrame,index); return p?u16(p):0; }
uint32_t RoomView::experience(uint32_t index) const { const auto* p=record(RoomSection::Experience,index); return p?u32(p):0; }
RoomRule RoomView::rule(uint32_t index) const {
    RoomRule r; const auto* p=record(RoomSection::Rule,index); if(!p) return r;
    r.key=static_cast<RoomRuleKey>(u16(p)); r.scalar_type=static_cast<RoomScalarType>(u16(p+2));
    switch(r.scalar_type) { case RoomScalarType::F32:r.f32=f32(p+8);break; case RoomScalarType::F64:r.f64=f64(p+8);break; case RoomScalarType::U32:r.u32=u32(p+8);break; }
    return r;
}
float RoomView::rule_f32(RoomRuleKey key) const { for(uint32_t i=0;i<rule_count();++i) { auto r=rule(i); if(r.key==key&&r.scalar_type==RoomScalarType::F32) return r.f32; } return 0; }
double RoomView::rule_f64(RoomRuleKey key) const { for(uint32_t i=0;i<rule_count();++i) { auto r=rule(i); if(r.key==key&&r.scalar_type==RoomScalarType::F64) return r.f64; } return 0; }
uint32_t RoomView::rule_u32(RoomRuleKey key) const { for(uint32_t i=0;i<rule_count();++i) { auto r=rule(i); if(r.key==key&&r.scalar_type==RoomScalarType::U32) return r.u32; } return 0; }
RoomRule RoomView::rule(RoomRuleKey key) const { for(uint32_t i=0;i<rule_count();++i) { auto r=rule(i); if(r.key==key) return r; } return {}; }
bool RoomView::frame_clip(uint32_t index,FrameClip& result) const {
    if(index>=clip_count()) return false;
    const auto c=clip(index); if(c.key_count>16||(c.flags&16)) return false;
    FrameClip r{}; r.length=c.length; r.loop=c.loop(); r.count=static_cast<uint8_t>(c.key_count);
    r.main_visibility=static_cast<int8_t>((c.visibility&3)-1);
    r.special_visibility=static_cast<int8_t>(((c.visibility>>2)&3)-1);
    for(uint32_t i=0;i<c.key_count;++i) { const auto k=key(c.first_key+i); r.keys[i]={k.time,k.frame}; }
    result=r; return true;
}
namespace {
bool utf8(const uint8_t* p,size_t n) {
    size_t i=0;
    while(i<n) {
        uint32_t cp=p[i++]; unsigned extra=0;
        if(cp<0x80) continue;
        if(cp>=0xc2&&cp<=0xdf) { cp&=0x1f;extra=1; }
        else if(cp>=0xe0&&cp<=0xef) { cp&=0xf;extra=2; }
        else if(cp>=0xf0&&cp<=0xf4) { cp&=7;extra=3; }
        else return false;
        if(extra>n-i) return false;
        const unsigned length=extra;
        while(extra--) { const auto c=p[i++]; if((c&0xc0)!=0x80) return false;cp=(cp<<6)|(c&0x3f); }
        if((length==2&&cp<0x800)||(length==3&&cp<0x10000)||cp>0x10ffff||(cp>=0xd800&&cp<=0xdfff)) return false;
    }
    return true;
}
bool relative_path(std::string_view path,bool texture=true) {
    if(path.empty()||path.front()=='/'||path.back()=='/'||path.find('\\')!=path.npos||path.find(':')!=path.npos) return false;
    for(char c:path) if(static_cast<unsigned char>(c)<32||static_cast<unsigned char>(c)==127) return false;
    size_t at=0;
    while(at<path.size()) { auto end=path.find('/',at); if(end==path.npos) end=path.size(); const auto part=path.substr(at,end-at); if(part.empty()||part=="."||part=="..") return false; at=end+1; }
    return !texture||(path.size()>=4&&path.substr(path.size()-4)==".t3x");
}
uint32_t room_crc(const uint8_t* bytes,size_t size) {
    uint32_t c=0xffffffffu;
    for(size_t i=0;i<size;++i) { c^=(i>=52&&i<56)?0:bytes[i]; for(unsigned bit=0;bit<8;++bit)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1))); }
    return ~c;
}
bool convex(const RoomView& v,uint32_t first,uint32_t count) {
    if(count<3||count>64||!range(first,count,v.vertex_count())) return false;
    double sign=0;
    for(uint32_t i=0;i<count;++i) {
        const auto a=v.vertex(first+i),b=v.vertex(first+(i+1)%count),c=v.vertex(first+(i+2)%count);
        const double cross=(double(b.x)-a.x)*(double(c.y)-b.y)-(double(b.y)-a.y)*(double(c.x)-b.x);
        if(!std::isfinite(cross)||cross==0) return false;
        if(sign==0)sign=cross;else if((cross>0)!=(sign>0))return false;
    }
    // Same-turn polygons can still self-intersect (e.g. a pentagram). Strict
    // convexity requires every other vertex on the same side of every edge.
    for(uint32_t i=0;i<count;++i) {
        const auto a=v.vertex(first+i),b=v.vertex(first+(i+1)%count);
        for(uint32_t j=0;j<count;++j) {
            if(j==i||j==(i+1)%count) continue;
            const auto c=v.vertex(first+j);
            const double cross=(double(b.x)-a.x)*(double(c.y)-a.y)-(double(b.y)-a.y)*(double(c.x)-a.x);
            if(std::abs(cross)<=0.00001||(cross>0)!=(sign>0))return false;
        }
    }
    return true;
}
}
bool RoomData::validate(const uint8_t* bytes,size_t size,std::string& error) {
    const auto fail=[&](const char* text) { error=text; return false; };
    if(!bytes||size<kRoomHeaderBytes+kRoomSectionCount*kRoomDirectoryEntryBytes||size>kMaxRoomBytes) return fail("Room: invalid file size");
    if(std::memcmp(bytes,"ENCRMD01",8)!=0) return fail("Room: invalid magic");
    if(u16(bytes+8)!=1||u16(bytes+10)!=0||u32(bytes+12)!=kRoomHeaderBytes||u32(bytes+16)!=size||u32(bytes+20)!=0x01020304u) return fail("Room: unsupported format/header/endianness");
    if(u32(bytes+24)!=1||u32(bytes+28)!=0x454e0002u||(u32(bytes+32)!=4&&u32(bytes+32)!=5&&u32(bytes+32)!=6&&u32(bytes+32)!=7)||u32(bytes+36)!=u32(bytes+32)) return fail("Room: incompatible target/content/rules/capabilities");
    if(!u32(bytes+40)||u32(bytes+44)!=kRoomHeaderBytes||u16(bytes+48)!=kRoomSectionCount||u16(bytes+50)!=kRoomDirectoryEntryBytes) return fail("Room: invalid scene identity/directory");
    if(!u32(bytes+108)||!u32(bytes+112)||u32(bytes+116)||!zero(bytes+120,8)||zero(bytes+56,20)||zero(bytes+76,32)) return fail("Room: invalid provenance/header reserved fields");
    if(room_crc(bytes,size)!=u32(bytes+52)) return fail("Room: CRC mismatch");
    struct Interval { uint32_t begin=0,end=0; };
    std::array<Interval,kRoomSectionCount> intervals{}; size_t interval_count=0;
    std::array<bool,kRoomSectionCount> seen{};
    const uint32_t data_begin=kRoomHeaderBytes+kRoomSectionCount*kRoomDirectoryEntryBytes;
    for(uint32_t i=0;i<kRoomSectionCount;++i) {
        const auto* d=bytes+kRoomHeaderBytes+i*kRoomDirectoryEntryBytes;
        const uint32_t kind=u16(d),offset=u32(d+4),n=u32(d+8),stride=u32(d+12),byte_size=u32(d+16);
        if(kind!=i+1||seen[kind-1]||u16(d+2)!=1||u32(d+20)!=1) return fail("Room: unknown/duplicate/versioned section");
        seen[kind-1]=true;
        if(stride!=kRoomStrides[kind-1]||uint64_t(n)*stride!=byte_size) return fail("Room: invalid section stride/size");
        if(!n) { if(offset||byte_size)return fail("Room: noncanonical empty section");continue; }
        if((offset&7)||offset<data_begin||offset>size||byte_size>size-offset) return fail("Room: invalid section range/alignment");
        intervals[interval_count++]={offset,offset+byte_size};
    }
    std::sort(intervals.begin(),intervals.begin()+interval_count,[](Interval a,Interval b){return a.begin<b.begin;});
    uint32_t cursor=data_begin;
    for(size_t i=0;i<interval_count;++i) { const auto r=intervals[i];if(r.begin<cursor)return fail("Room: overlapping sections");if(!zero(bytes+cursor,r.begin-cursor))return fail("Room: nonzero section padding");cursor=r.end; }
    if(cursor!=size)return fail("Room: unexpected trailing bytes");
    const RoomView v(bytes,size);
    constexpr std::array<uint32_t,kRoomSectionCount> limits{{65536,kMaxRoomBytes,1024,65536,4096,16384,16384,4096,4096,32768,32768,64,64,64,4096,4096,8192,64,1024,65535,4096,1024,64,10000,1,8192,1024,8192}};
    for(uint32_t i=0;i<kRoomSectionCount;++i)if(v.count(static_cast<RoomSection>(i+1))>limits[i])return fail("Room: section count exceeds supported capacity");
    if(v.actor_instance_count()>64||v.actor_profile_count()>64||v.camera_area_count()>64||v.clip_count()>4096||v.flag_count()>4096||v.polygon_count()>4096||v.trigger_count()>64||v.command_count()>65535) return fail("Room: supported runtime capacity exceeded");
    if(v.count(RoomSection::Scene)!=1||!v.string_count()||!v.count(RoomSection::StringBytes)||v.rule_count()!=(u32(bytes+32)>=7?27u:25u)||!v.experience_count())return fail("Room: missing required records");
    const auto string_offset=v.section_offset(RoomSection::StringBytes),string_size=v.count(RoomSection::StringBytes);
    if(bytes[string_offset+string_size-1]!=0||!utf8(bytes+string_offset,string_size)) return fail("Room: invalid UTF-8 string pool");
    std::set<std::string_view> unique_strings;
    uint32_t string_cursor=string_offset;
    for(uint32_t i=0;i<v.string_count();++i) {
        const auto* p=v.record(RoomSection::StringRef,i);const auto offset=u32(p),length=u32(p+4);
        if(offset!=string_cursor||length>4096||offset<string_offset||offset>=string_offset+string_size||length>=string_offset+string_size-offset||bytes[offset+length]||(!i&&length)) return fail("Room: invalid string reference/empty string");
        if((offset>string_offset&&bytes[offset-1]!=0)||std::memchr(bytes+offset,0,length))return fail("Room: invalid string boundary/embedded NUL");
        if(!unique_strings.insert(v.string(i)).second)return fail("Room: duplicate string");
        string_cursor=offset+length+1;
    }
    if(string_cursor!=string_offset+string_size)return fail("Room: unreferenced string bytes");
    const RoomSection identities[]={RoomSection::Resource,RoomSection::Overlay,RoomSection::MapDraw,RoomSection::Clip,RoomSection::ActorProfile,RoomSection::ActorInstance,RoomSection::CameraArea,RoomSection::Flag,RoomSection::Trigger,RoomSection::Program,RoomSection::Binding,RoomSection::Battle,RoomSection::MovementPath};
    for(auto section:identities) {
        std::set<uint32_t> ids;
        for(uint32_t i=0;i<v.count(section);++i) { const auto id=u32(v.record(section,i));if(!id||!ids.insert(id).second)return fail("Room: zero/duplicate stable identity"); }
    }
    for(uint32_t i=0;i<v.resource_count();++i) {
        const auto r=v.resource(i);const auto* p=v.record(RoomSection::Resource,i);
        if(!ref(r.path_string,v.string_count())||r.flags||!zero(p+20,4)||zero(p+24,32)||v.string(r.path_string).empty())return fail("Room: invalid resource metadata");
        if(r.kind==1) { if(!relative_path(v.string(r.path_string))||!r.width||!r.height||r.width>1024||r.height>1024||!r.columns||!r.rows||r.width%r.columns||r.height%r.rows||uint32_t(r.columns)*r.rows>65535)return fail("Room: invalid texture path/grid"); }
        else if(r.kind==2) { if(r.width||r.height||r.columns||r.rows||v.string(r.path_string).substr(0,6)!="res://"||!relative_path(v.string(r.path_string).substr(6),false))return fail("Room: invalid audio request resource"); }
        else if((r.kind==uint16_t(RoomResourceKind::CheckedBattlePack)||r.kind==uint16_t(RoomResourceKind::CheckedWorldEffectPack))) { if(r.width||r.height||r.columns||r.rows||!relative_path(v.string(r.path_string),false))return fail("Room: invalid checked battle resource"); }
        else return fail("Room: unsupported resource kind");
    }
    for(uint32_t i=0;i<v.vertex_count();++i)if(!finite(v.vertex(i)))return fail("Room: nonfinite geometry");
    std::set<uint32_t> body_ids;
    for(uint32_t i=0;i<v.polygon_count();++i) {
        const auto r=v.polygon(i);
        if(!r.body_id||!r.owner_id||(r.flags&~1u)||!convex(v,r.first_vertex,r.vertex_count)||!std::isfinite(r.minx)||!std::isfinite(r.miny)||!std::isfinite(r.maxx)||!std::isfinite(r.maxy))return fail("Room: invalid convex polygon");
        auto a=v.vertex(r.first_vertex);float minx=a.x,miny=a.y,maxx=a.x,maxy=a.y;
        for(uint32_t j=1;j<r.vertex_count;++j) { a=v.vertex(r.first_vertex+j);minx=std::min(minx,a.x);miny=std::min(miny,a.y);maxx=std::max(maxx,a.x);maxy=std::max(maxy,a.y); }
        if(minx!=r.minx||miny!=r.miny||maxx!=r.maxx||maxy!=r.maxy)return fail("Room: polygon bounds mismatch");
        body_ids.insert(r.body_id);
    }
    std::set<uint32_t> rule_bodies;
    for(uint32_t i=0;i<v.body_rule_count();++i) {
        const auto r=v.body_rule(i);const auto* p=v.record(RoomSection::BodyRule,i);
        if(!r.body_id||!rule_bodies.insert(r.body_id).second||!ref(r.source_path_string,v.string_count())||v.string(r.source_path_string).empty()||p[8]>1||r.flags||!zero(p+10,6))return fail("Room: invalid body rule");
    }
    for(auto id:body_ids)if(!rule_bodies.count(id))return fail("Room: polygon references unknown body");
    for(uint32_t i=0;i<v.overlay_count();++i) {
        const auto r=v.overlay(i);const auto* p=v.record(RoomSection::Overlay,i);
        if(!ref(r.resource_index,v.resource_count())||v.resource(r.resource_index).kind!=1||!std::isfinite(r.x)||!std::isfinite(r.y)||!std::isfinite(r.sort_y)||!r.w||!r.h||(r.flags&~uint16_t(RoomOverlayFlag::Foreground))||u16(p+30))return fail("Room: invalid overlay");
        const auto res=v.resource(r.resource_index);if(uint32_t(r.u)+r.w>res.width||uint32_t(r.v)+r.h>res.height)return fail("Room: overlay outside texture");
    }
    for(uint32_t i=0;i<v.map_draw_count();++i) {
        const auto r=v.map_draw(i);
        if(!ref(r.resource_index,v.resource_count())||v.resource(r.resource_index).kind!=1||!std::isfinite(r.x)||!std::isfinite(r.y)||!r.w||!r.h||r.flags)return fail("Room: invalid map draw");
        const auto res=v.resource(r.resource_index);if(r.w>res.width||r.h>res.height)return fail("Room: map draw outside texture");
    }
    for(uint32_t i=0;i<v.key_count();++i) { const auto* p=v.record(RoomSection::Key,i);if(!std::isfinite(f32(p))||u16(p+6))return fail("Room: invalid animation key"); }
    for(uint32_t i=0;i<v.clip_count();++i) {
        const auto r=v.clip(i);
        if(!std::isfinite(r.length)||r.length<=0||!r.key_count||r.key_count>16||!range(r.first_key,r.key_count,v.key_count())||(r.flags&~31u)||(r.visibility&0xf0)||((r.visibility&3)==3)||(((r.visibility>>2)&3)==3)||!r.frame_count||r.channel>1)return fail("Room: invalid animation clip");
        if((r.flags&16?(v.key(r.first_key).time<=0||v.key(r.first_key).time>r.length):v.key(r.first_key).time!=0)||((r.flags&3)==3))return fail("Room: unsupported clip sampling/start");
        float previous=-1;
        for(uint32_t j=0;j<r.key_count;++j) { const auto k=v.key(r.first_key+j);if(k.time<0||k.time<=previous||k.frame>=r.frame_count)return fail("Room: invalid clip key order/frame");previous=k.time; }
    }
    const auto texture=[&](uint32_t index,bool optional) { return (optional&&index==kRoomNoIndex)||(index<v.resource_count()&&v.resource(index).kind==1); };
    const auto compatible_clip=[&](uint32_t index,uint32_t resource,uint16_t channel,bool optional) {
        if(optional&&index==kRoomNoIndex)return true;
        if(index>=v.clip_count()||resource>=v.resource_count())return false;
        const auto c=v.clip(index); const auto r=v.resource(resource);
        return r.kind==1&&c.channel==channel&&c.frame_count==uint32_t(r.columns)*r.rows;
    };
    for(uint32_t i=0;i<v.actor_profile_count();++i) {
        const auto r=v.actor_profile(i);const auto* p=v.record(RoomSection::ActorProfile,i);
        if((r.execution_kind<1||r.execution_kind>3)||(r.flags&~1u)||u16(p+30)||u16(p+70)||!texture(r.primary_resource,false)||!texture(r.shadow_resource,true)||!texture(r.emote_resource,true)||!range(r.animation_binding_first,r.animation_binding_count,v.animation_binding_count())||!range(r.direction_first,r.direction_count,v.direction_frame_count())||!finite(r.sprite_position)||!finite(r.sprite_offset)||!finite(r.emote_offset)||!finite(r.shadow_offset))return fail("Room: invalid actor profile");
        const auto res=v.resource(r.primary_resource);const auto frames=uint32_t(res.columns)*res.rows;
        if(r.initial_frame>=frames||(r.flags&&r.shadow_resource==kRoomNoIndex)||!compatible_clip(r.idle_clip,r.primary_resource,0,true)||!compatible_clip(r.emote_clip,r.emote_resource,1,true))return fail("Room: invalid actor profile frame/clip");
        if(r.emote_resource==kRoomNoIndex) { if(r.emote_initial_frame)return fail("Room: absent emote has initial frame"); }
        else { const auto e=v.resource(r.emote_resource);if(r.emote_initial_frame>=uint32_t(e.columns)*e.rows)return fail("Room: invalid emote initial frame"); }
        if((r.execution_kind==1&&r.direction_count!=8)||(r.execution_kind==3&&r.direction_count!=4)||(r.execution_kind==2&&r.direction_count!=0&&r.direction_count!=8))return fail("Room: invalid profile direction count");
        for(uint32_t j=0;j<r.direction_count;++j)if(v.direction_frame(r.direction_first+j)>=frames)return fail("Room: invalid direction frame");
        std::set<uint16_t> motions;
        for(uint32_t j=0;j<r.animation_binding_count;++j) {
            const auto a=v.animation_binding(r.animation_binding_first+j);
            if(a.actor_profile_index!=i||a.motion_state>5||a.direction>7||(r.execution_kind==1&&a.motion_state>4)||(r.execution_kind==2&&a.motion_state>3)||(r.execution_kind==3&&(a.direction>3||(a.motion_state!=0&&a.motion_state!=4&&a.motion_state!=5)))||!compatible_clip(a.clip_index,r.primary_resource,0,false)||!motions.insert(uint16_t(a.motion_state)*8+a.direction).second)return fail("Room: invalid animation binding ownership/clip");
        }
        if(r.execution_kind==1) {
            for(uint16_t motion=0;motion<4;++motion)for(uint16_t direction=0;direction<8;++direction)
                if(!motions.count(motion*8+direction))return fail("Room: incomplete player directional animations");
            if(motions.size()!=32&&motions.size()!=40)return fail("Room: incomplete actor walk animations");
        }
        if(r.execution_kind==3&&motions.size()!=12)return fail("Room: incomplete NPC directional animations");
    }
    for(uint32_t i=0;i<v.animation_binding_count();++i) {
        const auto r=v.animation_binding(i);if(r.actor_profile_index>=v.actor_profile_count()||r.motion_state>5||r.direction>7)return fail("Room: invalid animation binding");
        const auto p=v.actor_profile(r.actor_profile_index);if(i<p.animation_binding_first||i-p.animation_binding_first>=p.animation_binding_count)return fail("Room: orphan animation binding");
    }
    for(uint32_t i=0;i<v.actor_instance_count();++i) {
        const auto r=v.actor_instance(i);const auto* p=v.record(RoomSection::ActorInstance,i);
        if(!ref(r.profile_index,v.actor_profile_count())||(r.binding_kind!=1&&r.binding_kind!=2)||(r.flags&~1u)||!ref(r.display_name_string,v.string_count())||v.string(r.display_name_string).empty()||(!r.direction.x&&!r.direction.y)||!finite(r.position)||!finite(r.direction)||u32(p+36))return fail("Room: invalid actor instance");
        if(v.actor_profile(r.profile_index).execution_kind==3&&r.binding_kind!=2)return fail("Room: directional NPC binding rejected");
        if(!compatible_clip(r.initial_clip,v.actor_profile(r.profile_index).primary_resource,0,true))return fail("Room: incompatible initial actor clip");
    }
    for(uint32_t i=0;i<v.camera_area_count();++i) {
        const auto r=v.camera_area(i);if(r.flags||!finite(r.center)||!finite(r.extents)||r.extents.x<=0||r.extents.y<=0)return fail("Room: invalid camera area");
    }
    std::set<std::string_view> flag_names;
    for(uint32_t i=0;i<v.flag_count();++i) {
        const auto r=v.flag(i);const auto* p=v.record(RoomSection::Flag,i);
        if(!ref(r.name_string,v.string_count())||v.string(r.name_string).empty()||!flag_names.insert(v.string(r.name_string)).second||p[8]>1||(r.flags&~1u)||!zero(p+10,6))return fail("Room: invalid flag definition");
    }
    std::set<uint32_t> initial_flags;
    for(uint32_t i=0;i<v.initial_flag_count();++i) {
        const auto r=v.initial_flag(i);const auto* p=v.record(RoomSection::InitialFlag,i);
        if(!ref(r.flag_index,v.flag_count())||!initial_flags.insert(r.flag_index).second||!(v.flag(r.flag_index).flags&1)||p[4]>1||!zero(p+5,3))return fail("Room: invalid initial flag override");
    }
    for(uint32_t i=0;i<v.condition_count();++i) {
        const auto r=v.condition(i);const auto* p=v.record(RoomSection::Condition,i);
        if(!ref(r.flag_index,v.flag_count())||p[4]>1||r.domain||u16(p+6))return fail("Room: invalid condition");
    }
    for(uint32_t i=0;i<v.trigger_count();++i) {
        const auto r=v.trigger(i);const auto* p=v.record(RoomSection::Trigger,i);
        if(!convex(v,r.first_vertex,r.vertex_count)||r.flags||!range(r.condition_first,r.condition_count,v.condition_count())||!ref(r.program_index,v.program_count())||!ref(r.actor_instance_index,v.actor_instance_count())||u32(p+28))return fail("Room: invalid trigger");
    }
    for(uint32_t i=0;i<v.binding_count();++i) {
        const auto r=v.binding(i);
        if(r.flags||(r.kind!=uint16_t(RoomBindingKind::DeferredFlagBodyDeletion)&&r.auxiliary_index!=kRoomNoIndex)||!std::isfinite(r.value)||!std::isfinite(r.duration))return fail("Room: invalid binding fields");
        if(r.kind==1) { if(!ref(r.target_index,v.resource_count())||v.resource(r.target_index).kind!=2||r.value||r.duration)return fail("Room: invalid music binding"); }
        else if(r.kind==2) { if(r.target_index!=kRoomNoIndex||r.value||r.duration<=0||r.duration>3600)return fail("Room: invalid timed boundary binding"); }
        else if(r.kind==uint16_t(RoomBindingKind::PeriodicCameraShake)) { if(!ref(r.target_index,v.resource_count())||v.resource(r.target_index).kind!=uint16_t(RoomResourceKind::AudioRequestOnly)||r.value<=0||r.value>1000000||r.duration<=0||r.duration>3600)return fail("Room: invalid periodic camera shake binding"); }
        else if(r.kind==uint16_t(RoomBindingKind::StopRoomShaker)) { if(!ref(r.target_index,v.binding_count())||v.binding(r.target_index).kind!=uint16_t(RoomBindingKind::PeriodicCameraShake)||r.value||r.duration)return fail("Room: invalid stop room shaker binding"); }
        else if(r.kind==uint16_t(RoomBindingKind::StopMusicResource)){if(!ref(r.target_index,v.resource_count())||v.resource(r.target_index).kind!=2||r.value||r.duration)return fail("Room: invalid targeted music stop");}
        else if(r.kind==uint16_t(RoomBindingKind::WorldEffectAppear)||r.kind==uint16_t(RoomBindingKind::WorldEffectDisappear)){if(!ref(r.target_index,v.resource_count())||v.resource(r.target_index).kind!=4||r.value||r.duration)return fail("Room: invalid world effect binding");}
        else if(r.kind==uint16_t(RoomBindingKind::StartPhoneRing)){if(u32(bytes+32)<5||r.target_index>=64||r.value||r.duration)return fail("Room: invalid phone-ring binding");}
        else if(r.kind==uint16_t(RoomBindingKind::DeferredFlagBodyDeletion)){if(u32(bytes+32)<5||!rule_bodies.count(r.target_index)||!ref(r.auxiliary_index,v.flag_count())||(r.value!=0&&r.value!=1)||r.duration)return fail("Room: invalid deferred flag-body binding");}
        else return fail("Room: unsupported binding kind");
    }
    for(uint32_t i=0;i<v.battle_count();++i) {
        const auto r=v.battle(i);
        if(!ref(r.enemy_string,v.string_count())||v.string(r.enemy_string).empty()||!ref(r.actor_instance_index,v.actor_instance_count())||!ref(r.win_flag_index,v.flag_count(),true)||(r.flags&~15u)||((r.flags&8)&&(r.flags&2))||r.advantage < -100||r.advantage>100||!ref(r.win_cutscene_string,v.string_count())||!ref(r.battle_resource_index,v.resource_count())||v.resource(r.battle_resource_index).kind!=uint16_t(RoomResourceKind::CheckedBattlePack))return fail("Room: invalid battle request");
        if(!v.string(r.win_cutscene_string).empty()&&!relative_path(v.string(r.win_cutscene_string),false))return fail("Room: invalid battle continuation path");
    }
    std::vector<bool> path_entries_used(v.movement_path_entry_count(),false);
    for(uint32_t i=0;i<v.movement_path_count();++i) {
        const auto r=v.movement_path(i);const auto* p=v.record(RoomSection::MovementPath,i);
        if(!r.entry_count||r.entry_count>(u32(bytes+32)>=7?16:8)||!range(r.first_entry,r.entry_count,v.movement_path_entry_count())||(r.flags&~(u32(bytes+32)>=7?15u:3u))||(r.animation_motion!=kRoomNoActor&&r.animation_motion!=4)||u16(p+14)||!std::isfinite(r.speed)||r.speed<=0||r.speed>10000)return fail("Room: invalid movement path");
        for(uint32_t j=0;j<r.entry_count;++j) {
            const auto index=r.first_entry+j;
            if(path_entries_used[index])return fail("Room: overlapping movement path entries");
            path_entries_used[index]=true;
        }
    }
    if(std::find(path_entries_used.begin(),path_entries_used.end(),false)!=path_entries_used.end())return fail("Room: orphan movement path entries");
    for(uint32_t i=0;i<v.movement_path_entry_count();++i) {
        const auto r=v.movement_path_entry(i);const auto* p=v.record(RoomSection::MovementPathEntry,i);
        if(r.kind>1||!zero(p+2,6)||!finite(r.vector)||!std::isfinite(r.duration))return fail("Room: invalid movement path entry");
        if(r.kind==0) { if(r.duration)return fail("Room: movement path move duration unused"); }
        else if(r.vector.x||r.vector.y||r.duration<=0||r.duration>3600)return fail("Room: invalid movement path wait");
    }
    std::array<bool,27> rule_keys{};
    for(uint32_t i=0;i<v.rule_count();++i) {
        const auto* p=v.record(RoomSection::Rule,i);const auto key=u16(p),type=u16(p+2);
        if(key<1||key>(u32(bytes+32)>=7?27:25)||rule_keys[key-1]||u32(p+4))return fail("Room: invalid/duplicate rule key");
        rule_keys[key-1]=true;const auto expected=(key<=5||key==20)?1:2;
        if(type!=expected||(type==1&&u32(p+12)))return fail("Room: invalid rule scalar type/padding");
        const double value=type==1?f32(p+8):f64(p+8);
        if(!std::isfinite(value))return fail("Room: nonfinite rule value");
        if(key==24||key==25) { if(value<-1||value>1)return fail("Room: invalid shake direction"); }
        else if(key==22) { if(value<0||value>3600)return fail("Room: invalid shake margin"); }
        else if(value<=0||value>((key==21||key==23)?3600:1000000))return fail("Room: invalid positive rule value");
        if((key==6||key==7||key==14)&&value>1)return fail("Room: invalid fractional rule value");
    }
    if(std::abs(v.rule_f64(RoomRuleKey::ActorJumpAscentRatio)+v.rule_f64(RoomRuleKey::ActorJumpDescentRatio)-1)>=1e-12||v.rule_f64(RoomRuleKey::CameraZeroMagnitudeThreshold)>v.rule_f64(RoomRuleKey::CameraMinimumMagnitude))return fail("Room: inconsistent rule relationships");
    if(v.rule_f64(RoomRuleKey::RoomShakeWaitMarginSeconds)>=v.rule_f64(RoomRuleKey::RoomShakeWaitSeconds)||(!v.rule_f64(RoomRuleKey::RoomShakeDirectionX)&&!v.rule_f64(RoomRuleKey::RoomShakeDirectionY)))return fail("Room: invalid periodic shake rule relationships");
    if(v.experience(0)!=0)return fail("Room: experience table must begin at zero");
    for(uint32_t i=0;i<v.experience_count();++i)if(v.experience(i)>uint32_t(INT32_MAX)||(i&&v.experience(i)<=v.experience(i-1)))return fail("Room: invalid experience curve");
    const auto scene=v.scene();
    if(scene.stable_id!=u32(bytes+40)||!ref(scene.display_name_string,v.string_count())||!ref(scene.version_string,v.string_count())||!ref(scene.source_scene_string,v.string_count())||v.string(scene.display_name_string).empty()||v.string(scene.version_string).empty()||v.string(scene.source_scene_string).empty()||(!scene.start_direction.x&&!scene.start_direction.y)||!ref(scene.player_instance_index,v.actor_instance_count())||!ref(scene.initial_program_index,v.program_count(),true)||!convex(v,scene.actor_hull_first,scene.actor_hull_count)||!finite(scene.spawn)||!finite(scene.start_direction)||scene.initial_motion_state>3||!range(scene.initial_flag_first,scene.initial_flag_count,v.initial_flag_count())||!range(scene.body_rule_first,scene.body_rule_count,v.body_rule_count())||!ref(scene.default_camera_area,v.camera_area_count())||scene.rule_profile_id!=1||scene.flags)return fail("Room: invalid scene");
    const auto player=v.actor_instance(scene.player_instance_index);const auto profile=v.actor_profile(player.profile_index);const auto resource=v.resource(profile.primary_resource);
    if(player.binding_kind!=1||profile.execution_kind!=1||scene.initial_frame>=uint32_t(resource.columns)*resource.rows)return fail("Room: invalid scene player");
    // Commands are a bounded engine instruction set. Unused fields must remain
    // canonical zero/NONE so future behavior cannot hide in ignored data.
    for(uint32_t i=0;i<v.command_count();++i) {
        const auto c=v.command(i);const auto* p=v.record(RoomSection::Command,i);
        const bool extended=u32(bytes+32)>=5,branched=u32(bytes+32)>=6;
        const uint32_t allowed_flags=(u32(bytes+32)>=7&&c.opcode==12)?15u:extended?(c.opcode==10?15u:c.opcode==17?3u:c.opcode==33?7u:(c.opcode==16||c.opcode==19||c.opcode==32||c.opcode==35)?1u:0u):((c.opcode==16||c.opcode==19||c.opcode==32||c.opcode==33)?1u:0u);
        if(c.opcode>(u32(bytes+32)>=7?42:branched?41:extended?35:34)||(c.flags&~allowed_flags)||((c.opcode!=37&&c.opcode!=38)&&c.auxiliary_index!=kRoomNoIndex)||u32(p+44)||!finite(c.vector)||!std::isfinite(c.value)||!std::isfinite(c.duration))return fail("Room: unknown opcode/invalid command fields");
        const bool inherited_actor=(c.flags&1)&&(c.opcode==16||c.opcode==19||c.opcode==32||c.opcode==42);
        const bool actor_used=!inherited_actor&&(c.opcode==1||c.opcode==2||(c.opcode>=9&&c.opcode<=14)||c.opcode==16||(c.opcode>=18&&c.opcode<=21)||c.opcode==24||(c.opcode>=27&&c.opcode<=29)||c.opcode==32||c.opcode==42);
        if(actor_used) { if(c.actor_index>=v.actor_instance_count())return fail("Room: invalid command actor reference"); }
        else if(c.opcode==5) { if(c.actor_index!=kRoomNoActor&&c.actor_index>=v.actor_instance_count())return fail("Room: invalid talker reference"); }
        else if(c.actor_index!=kRoomNoActor)return fail("Room: unused command actor");
        if(c.opcode==20&&v.actor_instance(c.actor_index).binding_kind!=1&&v.actor_profile(v.actor_instance(c.actor_index).profile_index).execution_kind!=3&&v.actor_profile(v.actor_instance(c.actor_index).profile_index).execution_kind!=2)return fail("Room: restore requires a supported NPC replacement");
        bool target_used=true;
        switch(c.opcode) {
            case 6:if(!ref(c.target_index,v.binding_count()))return fail("Room: invalid command binding");break;
            case 8:case 34:if(!ref(c.target_index,v.resource_count())||v.resource(c.target_index).kind!=2)return fail("Room: invalid command audio request");break;
            case 13:case 14: {
                const auto actor=v.actor_instance(c.actor_index);const auto a=v.actor_profile(actor.profile_index);
                if(!compatible_clip(c.target_index,c.opcode==13?a.primary_resource:a.emote_resource,c.opcode==13?0:1,false))return fail("Room: incompatible command clip");
                break;
            }
            case 10:
                if(c.flags&2){if(!ref(c.target_index,v.actor_instance_count())||(!(c.flags&4)&&c.vector.x)||(!(c.flags&8)&&c.vector.y))return fail("Room: invalid relative turn target/axes");}
                else {target_used=false;if(c.flags&12)return fail("Room: turn override without actor");}
                break;
            case 17:if((c.flags==1&&c.vector.y)||(c.flags==2&&c.vector.x))return fail("Room: unused partial camera axis");if(c.target_index!=1)return fail("Room: unsupported camera easing");break;
            case 18:case 24:if(!ref(c.target_index,v.battle_count())||v.battle(c.target_index).actor_instance_index!=c.actor_index)return fail("Room: invalid command battle request");break;
            case 29: {
                if(!ref(c.target_index,v.movement_path_count()))return fail("Room: invalid command movement path");
                const auto path=v.movement_path(c.target_index);const auto profile=v.actor_profile(v.actor_instance(c.actor_index).profile_index);
                if(path.animation_motion!=kRoomNoActor) {
                    const auto directions=profile.direction_count;
                    if(!directions)return fail("Room: movement animation has no directions");
                    for(uint16_t direction=0;direction<directions;++direction) {
                        bool found=false;
                        for(uint32_t binding=0;binding<profile.animation_binding_count;++binding) {
                            const auto a=v.animation_binding(profile.animation_binding_first+binding);
                            if(a.motion_state==path.animation_motion&&a.direction==direction)found=true;
                        }
                        if(!found)return fail("Room: missing movement animation binding");
                    }
                }
                break;
            }
            case 37:case 31:if(!ref(c.target_index,v.flag_count()))return fail("Room: invalid command flag");break;
            case 36:if(c.target_index==kRoomNoIndex)return fail("Room: missing branch target");break;
            case 38:{if(!ref(c.target_index,v.string_count()))return fail("Room: missing leader identity");const auto id=v.string(c.target_index);if(id.empty()||id.size()>64||id[0]<'a'||id[0]>'z')return fail("Room: leader identity");for(char ch:id)if(!((ch>='a'&&ch<='z')||(ch>='0'&&ch<='9')||ch=='_'))return fail("Room: leader identity");break;}
            case 39:if(c.target_index>=32)return fail("Room: invalid choice group");break;
            case 32:if(!c.target_index||c.target_index==kRoomNoIndex)return fail("Room: invalid dialogue stable identity");break;
            default:target_used=false;break;
        }
        if(!target_used&&c.target_index!=kRoomNoIndex)return fail("Room: unused command target");
        const bool vector_used=c.opcode==9||c.opcode==10||c.opcode==11||c.opcode==15||c.opcode==17||c.opcode==27||c.opcode==28;
        if(!vector_used&&(c.vector.x||c.vector.y))return fail("Room: unused command vector");
        if(c.opcode==27&&(!c.vector.x&&!c.vector.y))return fail("Room: zero command direction");
        switch(c.opcode) {
            case 7:case 31:case 37:if(c.value!=0&&c.value!=1)return fail("Room: invalid command boolean");break;
            case 9:if(c.value<=0||c.value>10000)return fail("Room: invalid command move speed");break;
            case 12:if(c.value<0||c.value>1024)return fail("Room: invalid command jump height");break;
            case 15:if(c.value<0)return fail("Room: invalid command shake magnitude");break;
            default:if(c.value)return fail("Room: unused command value");break;
        }
        switch(c.opcode) {
            case 3:if(c.duration<=0||c.duration>3600)return fail("Room: invalid command wait duration");break;
            case 4:case 17:case 23:case 30:if(c.duration<0||c.duration>3600)return fail("Room: invalid command duration");break;
            case 11:if(u32(bytes+32)>=7&&c.duration==-1)break;[[fallthrough]];
            case 10:if(c.duration<=0||c.duration>10)return fail("Room: invalid command actor duration");break;
            case 12:case 15:if(c.duration<=0||c.duration>60)return fail("Room: invalid command action duration");break;
            default:if(c.duration)return fail("Room: unused command duration");break;
        }
    }
    std::vector<bool> command_used(v.command_count(),false);
    std::set<std::string_view> program_paths;
    for(uint32_t i=0;i<v.program_count();++i) {
        const auto r=v.program(i);
        if(!r.command_count||!r.phrase_count||r.phrase_count>65536||!range(r.first_command,r.command_count,v.command_count())||(v.command(r.first_command+r.command_count-1).opcode!=24&&v.command(r.first_command+r.command_count-1).opcode!=23))return fail("Room: invalid program/termination");
        if(v.command(r.first_command+r.command_count-1).opcode==23&&v.command(r.first_command+r.command_count-1).duration<=0)return fail("Room: world completion lacks camera timing");
        if(!ref(r.source_path_string,v.string_count())||!relative_path(v.string(r.source_path_string),false)||!program_paths.insert(v.string(r.source_path_string)).second)return fail("Room: invalid/duplicate program source path");
        bool branch_program=false;for(uint32_t j=0;j<r.command_count;++j){const auto op=v.command(r.first_command+j).opcode;if(op>=36&&op<=41)branch_program=true;}
        bool pending_timer=false;
        for(uint32_t j=0;j<r.command_count;++j) {
            const uint32_t index=r.first_command+j;
            const auto operation=v.command(index);
            if(operation.opcode>=36&&operation.opcode<=41&&pending_timer)return fail("Room: branch/submenu with active timer");
            if(operation.opcode==3){if(pending_timer)return fail("Room: overlapping phrase timers");pending_timer=true;}
            if(operation.opcode==26){if(!pending_timer)return fail("Room: timer wait without timer");pending_timer=false;}
            if(operation.opcode==33){if(bool(operation.flags&1)!=pending_timer)return fail("Room: dialogue timer mode mismatch");pending_timer=false;}
            if((operation.opcode==36&&operation.target_index>=r.command_count)||((operation.opcode==37||operation.opcode==38)&&operation.auxiliary_index>=r.command_count))return fail("Room: branch target out of program");
            if(operation.opcode>=36&&operation.opcode<=38){const uint32_t target=operation.opcode==36?operation.target_index:operation.auxiliary_index;if(target<=j||(target&&v.command(r.first_command+target-1).phrase==v.command(r.first_command+target).phrase))return fail("Room: branch must reach forward phrase entry");}
            if(operation.opcode==40&&(j+1>=r.command_count||v.command(index+1).opcode!=41))return fail("Room: save request lacks submenu wait");
            if(operation.opcode==41&&(!j||v.command(index-1).opcode!=40))return fail("Room: submenu wait lacks save request");
            if(branch_program&&operation.opcode==23&&operation.duration<=0)return fail("Room: branch completion lacks camera timing");
            if(!branch_program&&j+1<r.command_count&&v.command(index).opcode==23&&v.command(index).duration!=0)return fail("Room: nonterminal completion duration");
            if(command_used[index]||v.command(index).phrase>=r.phrase_count)return fail("Room: overlapping commands/invalid phrase");
            command_used[index]=true;
        }
        if(pending_timer)return fail("Room: unresolved phrase timer");
    }
    if(std::find(command_used.begin(),command_used.end(),false)!=command_used.end())return fail("Room: orphan commands");
    error.clear();return true;
}
bool RoomData::load(const uint8_t* bytes,size_t size,std::string& error) {
    if(!bytes||size>kMaxRoomBytes) { error="Room: null input or excessive size";return false; }
    // Validate before allocation and copy. Existing content and all its views
    // remain usable for every validation failure, including aliased reloads.
    if(!validate(bytes,size,error))return false;
    std::vector<uint8_t> next(bytes,bytes+size);
    bytes_.swap(next);error.clear();return true;
}
bool RoomData::load_file(const char* path,std::string& error) {
    if(!path) { error="Room: null file path";return false; }
    std::vector<uint8_t> next;
    if(!encore::read_file(path,next,kMaxRoomBytes,error)||!validate(next.data(),next.size(),error))return false;
    bytes_.swap(next);error.clear();return true;
}
}

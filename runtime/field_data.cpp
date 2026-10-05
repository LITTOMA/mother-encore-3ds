#include "encore/field_data.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <utility>

namespace encore::upstream {
namespace {
constexpr size_t header=128,directory=24,maximum=2*1024*1024;
uint32_t u32(const uint8_t* p) { return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24); }
uint16_t u16(const uint8_t* p) { return uint16_t(p[0]|(uint16_t(p[1])<<8)); }
float real(const uint8_t* p) { uint32_t n=u32(p);float f;std::memcpy(&f,&n,4);return f; }
double decimal(const uint8_t* p) { uint64_t n=uint64_t(u32(p))|(uint64_t(u32(p+4))<<32);double d;std::memcpy(&d,&n,8);return d; }
Vec2 vec(const uint8_t* p) { return {real(p),real(p+4)}; }
bool finite(Vec2 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::fabs(v.x)<=1000000&&std::fabs(v.y)<=1000000; }
bool zero(const uint8_t* p,size_t n) { for(size_t i=0;i<n;++i)if(p[i])return false;return true; }
uint32_t crc(const uint8_t* p,size_t n) { uint32_t c=0xffffffff;for(size_t i=0;i<n;++i){c^=p[i];for(int b=0;b<8;++b)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return c^0xffffffff; }
bool canonical(std::string_view s) {
    if(s.empty()||s.front()=='/'||s.find('\\')!=s.npos||s.find(':')!=s.npos)return false;
    size_t begin=0;while(begin<s.size()){auto end=s.find('/',begin);if(end==s.npos)end=s.size();auto part=s.substr(begin,end-begin);if(part.empty()||part=="."||part=="..")return false;begin=end+1;}return true;
}
bool script_identity(std::string_view s,uint32_t flags) {
    if(flags==0)return canonical(s);
    if(flags!=1)return false;
    const auto separator=s.find("::");if(separator==s.npos||!canonical(s.substr(0,separator)))return false;
    auto id=s.substr(separator+2);if(id.empty()||id.front()=='0')return false;
    for(char c:id)if(c<'0'||c>'9')return false;
    return true;
}
}
uint32_t FieldData::count(uint32_t k) const { return valid()&&k>=1&&k<=6?u32(bytes_.data()+header+(k-1)*directory+12):0; }
const uint8_t* FieldData::record(uint32_t k,uint32_t i) const {
    if(i>=count(k))return nullptr;
    const auto* d=bytes_.data()+header+(k-1)*directory;
    return bytes_.data()+u32(d+4)+size_t(i)*u32(d+16);
}
uint32_t FieldData::profile_count() const { return count(3); }
uint32_t FieldData::grass_count() const { return count(4); }
uint32_t FieldData::texture_count() const { return count(5); }
uint32_t FieldData::pending_count() const { return count(6); }
FieldIdentity FieldData::identity() const {
    FieldIdentity r;if(!valid())return r;r.scene_id=u32(bytes_.data()+36);
    std::copy_n(bytes_.data()+48,20,r.upstream_commit.begin());std::copy_n(bytes_.data()+68,32,r.source_sha256.begin());return r;
}
std::string_view FieldData::string(uint32_t i) const {
    const auto* p=record(1,i);if(!p)return {};const auto* d=bytes_.data()+header+directory;
    return {reinterpret_cast<const char*>(bytes_.data()+u32(d+4)+u32(p)),u32(p+4)};
}
FieldGrassProfile FieldData::profile(uint32_t i) const {
    FieldGrassProfile r;const auto* p=record(3,i);if(!p)return r;
    r.stable_id=u32(p);r.texture_first=u32(p+4);r.texture_count=u32(p+8);r.collision_layer=u32(p+12);r.collision_mask=u32(p+16);
    for(size_t j=0;j<4;++j)r.frames[j]=u16(p+20+j*2);
    r.sprite_offset=vec(p+28);r.collision_offset=vec(p+36);r.collision_extents=vec(p+44);
    r.idle_delay=real(p+52);r.enter_tween=decimal(p+56);r.exit_tween=decimal(p+64);r.squash=real(p+72);r.blend_divisor=real(p+76);r.flags=u32(p+80);
    for(size_t j=0;j<3;++j){r.blend_points[j]=vec(p+88+j*8);r.blend_frames[j]=u16(p+112+j*2);}
    return r;
}
FieldGrass FieldData::grass(uint32_t i) const {
    FieldGrass r;const auto* p=record(4,i);if(!p)return r;
    r.stable_id=u32(p);r.node_string=u32(p+4);r.name_string=u32(p+8);r.ready_ordinal=u32(p+12);r.seed=u32(p+16);r.profile_index=u32(p+20);r.grass_types=u32(p+24);r.flags=u32(p+28);
    r.position=vec(p+32);r.visibility_origin=vec(p+40);r.visibility_size=vec(p+48);return r;
}
FieldTexture FieldData::texture(uint32_t i) const {
    FieldTexture r;const auto* p=record(5,i);if(!p)return r;r.stable_id=u32(p);r.source_string=u32(p+4);r.path_string=u32(p+8);
    r.width=u16(p+12);r.height=u16(p+14);r.columns=u16(p+16);r.rows=u16(p+18);r.flags=u32(p+20);
    std::copy_n(p+24,32,r.source_sha256.begin());std::copy_n(p+56,32,r.output_sha256.begin());return r;
}
FieldPending FieldData::pending(uint32_t i) const {
    FieldPending r;const auto* p=record(6,i);if(!p)return r;r.stable_id=u32(p);r.node_string=u32(p+4);r.script_string=u32(p+8);r.ready_ordinal=u32(p+12);r.adapter_kind=u32(p+16);r.flags=u32(p+20);std::copy_n(p+24,32,r.source_sha256.begin());return r;
}
bool FieldData::load(const uint8_t* b,size_t n,const FieldIdentity& expected,std::string& error) {
    auto fail=[&](const char* s){error=s;return false;};
    if(!b||n<header+6*directory||n>maximum)return fail("Field: invalid resource size");
    if(std::memcmp(b,"ENCFILD1",8)||u32(b+8)!=1||u32(b+12)!=header||u32(b+16)!=n||u32(b+24)!=0x454e0017||u32(b+28)!=1||u32(b+32)!=1||u32(b+40)!=6||u32(b+44)!=directory||!zero(b+100,28))return fail("Field: unsupported format/capability/target");
    if(!expected.scene_id||u32(b+36)!=expected.scene_id||zero(expected.upstream_commit.data(),20)||zero(expected.source_sha256.data(),32)||std::memcmp(b+48,expected.upstream_commit.data(),20)||std::memcmp(b+68,expected.source_sha256.data(),32))return fail("Field: source identity rejected");
    if(crc(b+header,n-header)!=u32(b+20))return fail("Field: checksum mismatch");
    const uint32_t strides[]={8,1,120,56,88,56};const uint32_t limits[]={32768,maximum,16,8192,128,8192};
    size_t end=header+6*directory;
    for(uint32_t k=1;k<=6;++k){const auto* d=b+header+(k-1)*directory;const uint32_t off=u32(d+4),size=u32(d+8),cnt=u32(d+12),stride=u32(d+16);
        const size_t aligned=(end+3)&~size_t(3);
        if(u32(d)!=k||off!=aligned||off>n||size>n-off||cnt>limits[k-1]||stride!=strides[k-1]||size!=uint64_t(cnt)*stride||u32(d+20)||!zero(b+end,aligned-end))return fail("Field: invalid section bounds/order");
        end=off+size;
    }
    if(end!=n)return fail("Field: trailing bytes");
    FieldData next;next.bytes_.assign(b,b+n);
    const auto* sd=b+header+directory;const uint32_t string_bytes=u32(sd+12);
    for(uint32_t i=0;i<next.count(1);++i){const auto* p=next.record(1,i);const uint32_t off=u32(p),len=u32(p+4);if(len>1024||off>=string_bytes||len>=string_bytes-off||next.record(2,off)[len]!=0||std::memchr(next.record(2,off),0,len))return fail("Field: malformed string");}
    std::set<uint32_t> ids,ordinals;std::set<std::string_view> paths;
    auto id=[&](uint32_t v){return v&&ids.insert(v).second;};
    if(!next.profile_count()||!next.grass_count()||!next.texture_count())return fail("Field: missing grass dependency");
    for(uint32_t i=0;i<next.texture_count();++i){auto r=next.texture(i);if(!id(r.stable_id)||r.source_string>=next.count(1)||r.path_string>=next.count(1)||!canonical(next.string(r.source_string))||!canonical(next.string(r.path_string))||next.string(r.path_string).substr(0,9)!="graphics/"||!r.width||!r.height||r.width>1024||r.height>1024||!r.columns||!r.rows||r.width%r.columns||r.height%r.rows||r.flags||zero(r.source_sha256.data(),32)||zero(r.output_sha256.data(),32))return fail("Field: texture dependency rejected");}
    for(uint32_t i=0;i<next.profile_count();++i){auto r=next.profile(i);if(!id(r.stable_id)||!r.texture_count||r.texture_first>next.texture_count()||r.texture_count>next.texture_count()-r.texture_first||!r.collision_layer||!r.collision_mask||r.flags||!zero(next.record(3,i)+84,4)||!finite(r.sprite_offset)||!finite(r.collision_offset)||!finite(r.collision_extents)||r.collision_extents.x<=0||r.collision_extents.y<=0||!std::isfinite(r.idle_delay)||r.idle_delay<=0||r.idle_delay>3600||!std::isfinite(r.enter_tween)||r.enter_tween<=0||r.enter_tween>60||!std::isfinite(r.exit_tween)||r.exit_tween<=0||r.exit_tween>60||!std::isfinite(r.squash)||r.squash<=0||r.squash>1||!std::isfinite(r.blend_divisor)||r.blend_divisor<=0||r.blend_divisor>1024)return fail("Field: grass profile rejected");
        for(auto point:r.blend_points)if(!finite(point))return fail("Field: grass blend point rejected");
        if(!zero(next.record(3,i)+118,2))return fail("Field: grass blend padding rejected");
        for(uint32_t t=0;t<r.texture_count;++t){auto texture=next.texture(r.texture_first+t);for(auto frame:r.frames)if(frame>=uint32_t(texture.columns)*texture.rows)return fail("Field: grass frame outside texture");for(auto frame:r.blend_frames)if(frame>=uint32_t(texture.columns)*texture.rows)return fail("Field: grass blend frame outside texture");}
    }
    uint32_t previous=0;
    for(uint32_t i=0;i<next.grass_count();++i){auto r=next.grass(i);if(!id(r.stable_id)||r.node_string>=next.count(1)||r.name_string>=next.count(1)||!canonical(next.string(r.node_string))||next.string(r.name_string).empty()||!paths.insert(next.string(r.node_string)).second||r.ready_ordinal>=65536||(i&&r.ready_ordinal<=previous)||!ordinals.insert(r.ready_ordinal).second||r.profile_index>=next.profile_count()||!r.grass_types||r.grass_types>next.profile(r.profile_index).texture_count||r.flags||!finite(r.position)||!finite(r.visibility_origin)||!finite(r.visibility_size)||r.visibility_size.x<=0||r.visibility_size.y<=0)return fail("Field: grass instance rejected");previous=r.ready_ordinal;
        uint32_t hash=5381;for(unsigned char c:next.string(r.name_string)){if(c>=128)return fail("Field: grass name needs native Unicode hash adapter");hash=hash*33+c;}if(hash!=r.seed)return fail("Field: grass seed rejected");
    }
    previous=0;
    for(uint32_t i=0;i<next.pending_count();++i){auto r=next.pending(i);if(!id(r.stable_id)||r.node_string>=next.count(1)||r.script_string>=next.count(1)||(!canonical(next.string(r.node_string))&&next.string(r.node_string)!=".")||!script_identity(next.string(r.script_string),r.flags)||!paths.insert(next.string(r.node_string)).second||r.ready_ordinal>=65536||(i&&r.ready_ordinal<=previous)||!ordinals.insert(r.ready_ordinal).second||r.adapter_kind||zero(r.source_sha256.data(),32))return fail("Field: pending adapter identity rejected");previous=r.ready_ordinal;}
    bytes_=std::move(next.bytes_);error.clear();return true;
}
bool FieldData::load_file(const char* path,const FieldIdentity& expected,std::string& error) {
    FILE* f=path?std::fopen(path,"rb"):nullptr;if(!f){error="Field: cannot open resource";return false;}
    std::vector<uint8_t> bytes;uint8_t block[4096];bool bad=false;
    for(;;){size_t n=std::fread(block,1,sizeof(block),f);if(bytes.size()+n>maximum){bad=true;break;}bytes.insert(bytes.end(),block,block+n);if(n<sizeof(block)){bad=std::ferror(f)!=0;break;}}
    if(std::fclose(f)!=0)bad=true;
    if(bad){error="Field: failed/bounded read";return false;}
    return load(bytes.data(),bytes.size(),expected,error);
}
}

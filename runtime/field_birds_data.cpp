#include "encore/field_birds.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;while(n--){c^=*p++;for(unsigned i=0;i<8;++i)c=(c>>1)^((c&1)?0xedb88320u:0u);}return ~c;}
struct Reader {const uint8_t*p;size_t n,at=128;bool ok=true;
 uint32_t integer(){if(!ok||at>n||n-at<4){ok=false;return 0;}auto v=word(p+at);at+=4;return v;}
 int32_t signed_integer(){auto v=integer();int32_t out;std::memcpy(&out,&v,4);return out;}
 float scalar(){auto v=integer();float f;std::memcpy(&f,&v,4);if(!std::isfinite(f)||std::abs(f)>=1000000)ok=false;return f;}
 Vec2 vector(){float x=scalar(),y=scalar();return {x,y};}
 bool boolean(){auto v=integer();if(v>1)ok=false;return v!=0;}
 std::string text(){auto length=integer();if(!ok||length>65536||at>n||length>n-at){ok=false;return{};}std::string s(reinterpret_cast<const char*>(p+at),length);at+=length;size_t count=0;if(s.find('\0')!=s.npos||!utf8_count(s,count))ok=false;return s;}
 std::array<uint8_t,32>hash(){std::array<uint8_t,32>h{};if(!ok||at>n||n-at<32){ok=false;return h;}std::copy_n(p+at,32,h.begin());at+=32;if(std::all_of(h.begin(),h.end(),[](uint8_t v){return !v;}))ok=false;return h;}
};
bool path(std::string_view s){if(s.empty()||s.front()=='/'||s.back()=='/'||s.find('\\')!=s.npos||s.find(':')!=s.npos)return false;size_t b=0;while(b<s.size()){auto e=s.find('/',b);if(e==s.npos)e=s.size();auto v=s.substr(b,e-b);if(v.empty()||v=="."||v=="..")return false;b=e+1;}return true;}
}
const FieldBirdDescriptor*FieldBirdData::record(uint32_t id)const{for(const auto&r:records_)if(r.id==id)return &r;return nullptr;}
const FieldBirdClip*FieldBirdData::clip(uint32_t p,FieldBirdClipRole role)const{if(p>=profiles_.size())return nullptr;for(const auto&c:profiles_[p])if(c.role==role)return &c;return nullptr;}
const FieldBirdSkin*FieldBirdData::skin(uint32_t i)const{return i<skins_.size()?&skins_[i]:nullptr;}
bool FieldBirdData::source_hash(std::string_view p,std::array<uint8_t,32>&h)const{auto it=sources_.find(std::string(p));if(it==sources_.end())return false;h=it->second;return true;}
bool FieldBirdData::load_file(const char*p,const FieldIdentity&id,std::string&e){if(!p||!*p){e="Bird path rejected";return false;}FILE*f=std::fopen(p,"rb");if(!f){e="Cannot open Bird resource";return false;}if(std::fseek(f,0,SEEK_END)){std::fclose(f);e="Bird seek rejected";return false;}long n=std::ftell(f);if(n<128||n>4*1024*1024||std::fseek(f,0,SEEK_SET)){std::fclose(f);e="Bird file size rejected";return false;}std::vector<uint8_t>b(static_cast<size_t>(n));auto got=std::fread(b.data(),1,b.size(),f);bool closed=std::fclose(f)==0;if(got!=b.size()||!closed){e="Bird read rejected";return false;}return load(b.data(),b.size(),id,e);}
bool FieldBirdData::load(const uint8_t*p,size_t n,const FieldIdentity&id,std::string&e){
 auto reject=[&](const char*s){e=s;return false;};
 if(!p||n<128||n>4*1024*1024)return reject("Bird size rejected");
 if(std::memcmp(p,"ENCFBRD1",8)||word(p+8)!=2||word(p+12)!=128||word(p+16)!=n||word(p+24)!=0x454e0022||word(p+28)!=4||!word(p+32)||word(p+32)>10000||!id.scene_id||word(p+36)!=id.scene_id||std::memcmp(p+40,id.upstream_commit.data(),20)||std::memcmp(p+60,id.source_sha256.data(),32)||word(p+124))return reject("Bird identity/version/capability rejected");
 if(crc(p+128,n-128)!=word(p+20)||std::all_of(p+92,p+124,[](uint8_t v){return !v;}))return reject("Bird CRC/IR rejected");
 FieldBirdData d;d.identity_=id;Reader r{p,n};d.scene_=r.text();d.script_=r.text();auto&z=d.rules_;z.skin_mod=r.integer();z.facing_mod=r.integer();z.facing_value=r.integer();z.speed_mod=r.integer();bool always=r.boolean();z.speed_base=r.scalar();z.seek_factor=r.scalar();z.right=r.vector();z.left=r.vector();z.flight_z=r.signed_integer();d.columns_=r.integer();d.rows_=r.integer();d.pixel_snap_=r.boolean();auto profiles=r.integer();
 if(!r.ok||!path(d.scene_)||!path(d.script_)||!always||!z.skin_mod||z.skin_mod>256||!z.facing_mod||z.facing_value>=z.facing_mod||!z.speed_mod||z.speed_mod>65536||z.speed_base<=0||z.speed_base+z.speed_mod>=1000000||z.seek_factor<=0||z.seek_factor>65536||z.flight_z< -4096||z.flight_z>4096||!d.columns_||!d.rows_||d.columns_>256||d.rows_>256||!profiles||profiles>256)return reject("Bird rules/source grid rejected");
 const uint32_t frames=d.columns_*d.rows_;
 for(uint32_t j=0;j<profiles;++j){auto count=r.integer();if(!r.ok||count!=6)return reject("Bird native clip roster rejected");std::vector<FieldBirdClip>out;std::set<std::string>names;
  for(uint32_t i=0;i<count;++i){FieldBirdClip c;auto role=r.integer();c.role=static_cast<FieldBirdClipRole>(role);c.name=r.text();c.length=r.scalar();c.loop=r.boolean();auto tracks=r.integer();if(!r.ok||role!=i+1||c.name.empty()||!names.insert(c.name).second||c.length<=0||c.length>65536||!tracks||tracks>3)return reject("Bird clip rejected");std::set<uint32_t>properties;
   for(uint32_t k=0;k<tracks;++k){FieldBirdTrack t;auto property=r.integer();t.property=static_cast<FieldBirdProperty>(property);t.update=r.integer();auto keys=r.integer();if(!r.ok||property<1||property>3||!properties.insert(property).second||t.update>1||!keys||keys>4096)return reject("Bird track property/update rejected");float previous=-1;
    for(uint32_t a=0;a<keys;++a){FieldBirdKey key;key.time=r.scalar();key.transition=r.scalar();key.value=r.vector();if(!r.ok||key.time<0||key.time>c.length||key.time<=previous||(property!=2&&(key.value.y!=0||std::floor(key.value.x)!=key.value.x))||(property==1&&(key.value.x<0||key.value.x>=frames))||(property==3&&(key.value.x< -4096||key.value.x>4096)))return reject("Bird key range/order rejected");previous=key.time;t.keys.push_back(key);}c.tracks.push_back(std::move(t));}out.push_back(std::move(c));}d.profiles_.push_back(std::move(out));}
 auto skins=r.integer();if(!r.ok||skins!=z.skin_mod)return reject("Bird skin RNG binding rejected");
 for(uint32_t i=0;i<skins;++i){FieldBirdSkin s;s.index=r.integer();s.width=r.integer();s.height=r.integer();s.source=r.text();s.path=r.text();s.source_sha=r.hash();s.output_sha=r.hash();if(!r.ok||s.index!=i||!s.width||!s.height||s.width>1024||s.height>1024||s.width%d.columns_||s.height%d.rows_||!path(s.source)||!path(s.path)||s.path.substr(0,9)!="graphics/"||s.path.size()<5||s.path.substr(s.path.size()-4)!=".t3x")return reject("Bird atlas bounds/path rejected");d.skins_.push_back(std::move(s));}
 std::set<uint32_t>ids,children;std::set<std::string>nodes;uint32_t previous=0;
 for(uint32_t i=0;i<word(p+32);++i){FieldBirdDescriptor a;a.id=r.integer();a.ready=r.integer();a.parent_id=r.integer();a.profile=r.integer();for(auto&v:a.children)v=r.integer();a.animation_ready=r.integer();a.timer_ready=r.integer();a.frame=r.integer();a.flags=r.integer();a.body_layer=r.integer();a.body_mask=r.integer();a.area_layer=r.integer();a.area_mask=r.integer();for(auto&v:a.pause)v=r.integer();a.initial_z=r.signed_integer();for(auto&v:a.priority)v=r.signed_integer();a.body_radius=r.scalar();a.safe_margin=r.scalar();a.area_radius=r.scalar();a.timer_wait=r.scalar();a.animation_speed=r.scalar();a.position=r.vector();for(auto&v:a.parent)v=r.vector();a.sprite_position=r.vector();a.sprite_offset=r.vector();a.area_center=r.vector();for(auto&v:a.notifier)v=r.scalar();a.notifier_position=r.vector();a.notifier_scale=r.vector();for(auto*colors:{&a.modulate,&a.self_modulate,&a.sprite_modulate,&a.sprite_self_modulate})for(auto&v:*colors){v=r.scalar();if(v<0||v>1)r.ok=false;}a.node=r.text();
  if(!r.ok||!a.id||!a.parent_id||a.id==a.parent_id||!ids.insert(a.id).second||!nodes.insert(a.node).second||!path(a.node)||(i&&a.ready<=previous)||a.profile>=profiles||a.frame>=frames||a.flags>511||!(a.flags&32)||(a.flags&256)||a.body_radius<=0||a.safe_margin<0||a.area_radius<=0||a.timer_wait<=0||a.animation_speed<=0||a.animation_speed>1024||a.initial_z< -4096||a.initial_z>4096||a.animation_ready>=a.ready||a.timer_ready>=a.ready||a.animation_ready==a.timer_ready||a.parent[0].y||a.parent[1].x||a.parent[0].x<=0||a.parent[0].x!=a.parent[1].y||a.notifier[2]<=0||a.notifier[3]<=0||a.notifier_scale.x<=0||a.notifier_scale.y<=0)return reject("Bird instance/body/notifier/order rejected");
  for(auto v:a.pause){if(v>2)return reject("Bird pause mode rejected");}
  for(auto v:a.children){if(!v||v==a.id||v==a.parent_id||!children.insert(v).second)return reject("Bird duplicate source child rejected");}
  previous=a.ready;d.records_.push_back(std::move(a));}
 for(auto v:children){if(ids.count(v))return reject("Bird root/child alias rejected");}
 auto signals=r.integer();if(!r.ok||signals!=4)return reject("Bird source connection closure rejected");
 std::set<std::pair<uint32_t,std::string>>unique_connections;
 for(uint32_t i=0;i<signals;++i){FieldBirdConnection c;c.role=r.integer();c.child=r.integer();c.signal=r.text();c.method=r.text();
  const uint32_t expected_child=i==0?1:i==1?4:5;
  auto symbol=[](std::string_view value){return !value.empty()&&value.size()<256&&std::all_of(value.begin(),value.end(),[](char x){return (x>='a'&&x<='z')||(x>='A'&&x<='Z')||(x>='0'&&x<='9')||x=='_';});};
  if(!r.ok||c.role!=i+1||c.child!=expected_child||!symbol(c.signal)||!symbol(c.method)||!unique_connections.emplace(c.child,c.signal).second)return reject("Bird source connection role/signature rejected");
  d.connections_.push_back(std::move(c));}
 auto proofs=r.integer();if(!r.ok||!proofs||proofs>10000)return reject("Bird proof count rejected");for(uint32_t i=0;i<proofs;++i){auto name=r.text();auto h=r.hash();if(!r.ok||!path(name)||!d.sources_.emplace(name,h).second)return reject("Bird duplicate proof rejected");}
 std::array<uint8_t,32>h{};if(!r.ok||r.at!=n||!d.source_hash(d.scene_,h)||h!=id.source_sha256||!d.source_hash(d.script_,d.script_sha_))return reject("Bird source closure/trailing rejected");for(const auto&s:d.skins_)if(!d.source_hash(s.source,h)||h!=s.source_sha)return reject("Bird texture proof rejected");d.valid_=true;*this=std::move(d);e.clear();return true;
}
}

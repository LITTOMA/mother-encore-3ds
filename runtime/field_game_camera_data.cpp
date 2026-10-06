#include "encore/field_game_camera.hpp"
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
 double real(){if(!ok||at>n||n-at<8){ok=false;return 0;}uint64_t v=uint64_t(word(p+at))|uint64_t(word(p+at+4))<<32;at+=8;double out;std::memcpy(&out,&v,8);if(!std::isfinite(out)||std::abs(out)>=1000000)ok=false;return out;}
 Vec2 vector(){float x=scalar(),y=scalar();return {x,y};}
 bool boolean(){auto v=integer();if(v>1)ok=false;return v!=0;}
 std::string text(){auto length=integer();if(!ok||length>65536||at>n||length>n-at){ok=false;return{};}std::string s(reinterpret_cast<const char*>(p+at),length);at+=length;size_t count=0;if(s.find('\0')!=s.npos||!utf8_count(s,count))ok=false;return s;}
 std::array<uint8_t,32>hash(){std::array<uint8_t,32>h{};if(!ok||at>n||n-at<32){ok=false;return h;}std::copy_n(p+at,32,h.begin());at+=32;if(std::all_of(h.begin(),h.end(),[](uint8_t v){return !v;}))ok=false;return h;}
};
bool path(std::string_view s){if(s.empty()||s.front()=='/'||s.back()=='/'||s.find('\\')!=s.npos||s.find(':')!=s.npos)return false;size_t b=0;while(b<s.size()){auto e=s.find('/',b);if(e==s.npos)e=s.size();auto v=s.substr(b,e-b);if(v.empty()||v=="."||v=="..")return false;b=e+1;}return true;}
}
const FieldGameCameraDescriptor*FieldGameCameraData::record(uint32_t id)const{for(const auto&r:records_)if(r.id==id)return &r;return nullptr;}
bool FieldGameCameraData::source_hash(std::string_view p,std::array<uint8_t,32>&h)const{auto i=sources_.find(std::string(p));if(i==sources_.end())return false;h=i->second;return true;}
bool FieldGameCameraData::load_file(const char*p,const FieldIdentity&id,std::string&e){if(!p||!*p){e="GameCamera path rejected";return false;}FILE*f=std::fopen(p,"rb");if(!f){e="Cannot open GameCamera";return false;}if(std::fseek(f,0,SEEK_END)){std::fclose(f);e="GameCamera seek rejected";return false;}long n=std::ftell(f);if(n<128||n>4*1024*1024||std::fseek(f,0,SEEK_SET)){std::fclose(f);e="GameCamera size rejected";return false;}std::vector<uint8_t>b(static_cast<size_t>(n));auto got=std::fread(b.data(),1,b.size(),f);bool closed=std::fclose(f)==0;if(got!=b.size()||!closed){e="GameCamera read rejected";return false;}return load(b.data(),b.size(),id,e);}
bool FieldGameCameraData::load(const uint8_t*p,size_t n,const FieldIdentity&id,std::string&e){
 auto reject=[&](const char*s){e=s;return false;};
 if(!p||n<128||n>4*1024*1024)return reject("GameCamera size rejected");
 if(std::memcmp(p,"ENCFGCM1",8)||word(p+8)!=1||word(p+12)!=128||word(p+16)!=n||word(p+24)!=0x454e0031||word(p+28)!=3||!word(p+32)||word(p+32)>10000||word(p+36)!=id.scene_id||!id.scene_id||std::memcmp(p+40,id.upstream_commit.data(),20)||std::memcmp(p+60,id.source_sha256.data(),32)||word(p+124))return reject("GameCamera identity/version/capability rejected");
 if(crc(p+128,n-128)!=word(p+20)||std::all_of(p+92,p+124,[](uint8_t v){return !v;}))return reject("GameCamera CRC/IR rejected");
 FieldGameCameraData d;d.identity_=id;Reader r{p,n};d.scene_=r.text();d.script_=r.text();d.shaker_=r.text();d.scope_action_=r.text();for(auto&v:d.player_states_)v=r.integer();d.shake_direction_=r.vector();d.shake_side_amplitude_=r.vector();for(auto&v:d.tuning_)v=r.real();for(auto&v:d.lengths_)v=r.scalar();
 if(!r.ok||!path(d.scene_)||!path(d.script_)||!path(d.shaker_)||d.script_==d.shaker_||d.scope_action_.empty()||d.scope_action_.find_first_not_of("abcdefghijklmnopqrstuvwxyz_0123456789")!=d.scope_action_.npos||d.player_states_[0]==d.player_states_[1]||d.player_states_[0]>1000||d.player_states_[1]>1000||std::any_of(d.tuning_.begin(),d.tuning_.end(),[](double v){return v<=0;})||d.tuning_[10]>1||d.tuning_[12]>d.tuning_[11]||d.tuning_[13]!=std::floor(d.tuning_[13])||d.tuning_[15]!=1||std::any_of(d.lengths_.begin(),d.lengths_.end(),[](float v){return v<=0;}))return reject("GameCamera source/tuning rejected");
 std::set<uint32_t>ids,children;std::set<std::string>nodes;uint32_t previous=0;
 for(uint32_t j=0;j<word(p+32);++j){FieldGameCameraDescriptor a;a.id=r.integer();a.ready=r.integer();a.parent_id=r.integer();a.arrows_id=r.integer();a.animation_id=r.integer();a.animation_ready=r.integer();a.area_id=r.integer();a.shape_id=r.integer();a.flags=r.integer();a.pause=r.integer();a.physics_interpolation=r.integer();a.z=r.signed_integer();a.priority=r.signed_integer();for(auto&v:a.limits)v=r.signed_integer();a.position=r.vector();a.offset=r.vector();for(auto&v:a.world)v=r.vector();a.zoom=r.vector();a.rotation=r.scalar();a.area_position=r.vector();a.area_scale=r.vector();a.shape_position=r.vector();a.shape_scale=r.vector();a.shape_extents=r.vector();a.area_layer=r.integer();a.area_mask=r.integer();a.area_flags=r.integer();a.node=r.text();
  if(!r.ok||!a.id||!a.parent_id||a.id==a.parent_id||!ids.insert(a.id).second||!nodes.insert(a.node).second||!path(a.node)||(j&&a.ready<=previous)||a.animation_ready>=a.ready||a.flags>15||a.pause>2||a.physics_interpolation>2||a.zoom.x!=1||a.zoom.y!=1||a.rotation||a.limits[0]>=a.limits[3]||a.limits[1]>=a.limits[2]||a.area_scale.x<=0||a.area_scale.y<=0||a.shape_scale.x<=0||a.shape_scale.y<=0||a.shape_extents.x<=0||a.shape_extents.y<=0||a.area_flags>15)return reject("GameCamera instance/native geometry rejected");
  for(auto child:{a.arrows_id,a.animation_id,a.area_id,a.shape_id})if(!child||child==a.id||child==a.parent_id||!children.insert(child).second)return reject("GameCamera child identity rejected");
  previous=a.ready;d.records_.push_back(std::move(a));}
 for(auto v:children)if(ids.count(v))return reject("GameCamera root/child alias rejected");
 auto count=r.integer();if(!r.ok||!count||count>10000)return reject("GameCamera proof count rejected");for(uint32_t i=0;i<count;++i){auto name=r.text();auto h=r.hash();if(!r.ok||!path(name)||!d.sources_.emplace(name,h).second)return reject("GameCamera source proof rejected");}
 std::array<uint8_t,32>h{};if(!r.ok||r.at!=n||!d.source_hash(d.scene_,h)||h!=id.source_sha256||!d.source_hash(d.script_,d.script_sha_)||!d.source_hash(d.shaker_,d.shaker_sha_))return reject("GameCamera source/trailing rejected");d.valid_=true;*this=std::move(d);e.clear();return true;
}
}

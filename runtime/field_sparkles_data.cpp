#include "encore/field_sparkles.hpp"
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
const FieldSparklesDescriptor*FieldSparklesData::record(uint32_t id)const{for(const auto&r:records_)if(r.id==id)return &r;return nullptr;}
const FieldSparklesAnimation*FieldSparklesData::animation(uint32_t i)const{return i<animations_.size()?&animations_[i]:nullptr;}
bool FieldSparklesData::source_hash(std::string_view p,std::array<uint8_t,32>&h)const{auto it=sources_.find(std::string(p));if(it==sources_.end())return false;h=it->second;return true;}
bool FieldSparklesData::load_file(const char*p,const FieldIdentity&id,std::string&e){if(!p||!*p){e="Sparkles path rejected";return false;}FILE*f=std::fopen(p,"rb");if(!f){e="Cannot open Sparkles resource";return false;}if(std::fseek(f,0,SEEK_END)){std::fclose(f);e="Sparkles seek rejected";return false;}long n=std::ftell(f);if(n<128||n>4*1024*1024||std::fseek(f,0,SEEK_SET)){std::fclose(f);e="Sparkles file size rejected";return false;}std::vector<uint8_t>b(static_cast<size_t>(n));auto got=std::fread(b.data(),1,b.size(),f);bool closed=std::fclose(f)==0;if(got!=b.size()||!closed){e="Sparkles file read rejected";return false;}return load(b.data(),b.size(),id,e);}
bool FieldSparklesData::load(const uint8_t*p,size_t n,const FieldIdentity&id,std::string&e){
 auto reject=[&](const char*s){e=s;return false;};
 if(!p||n<128||n>4*1024*1024)return reject("Sparkles size rejected");
 if(std::memcmp(p,"ENCFSPL1",8)||word(p+8)!=1||word(p+12)!=128||word(p+16)!=n||word(p+24)!=0x454e0021||word(p+28)!=3||!word(p+32)||word(p+32)>10000||!id.scene_id||word(p+36)!=id.scene_id||std::memcmp(p+40,id.upstream_commit.data(),20)||std::memcmp(p+60,id.source_sha256.data(),32)||word(p+124))return reject("Sparkles identity/version/capability rejected");
 if(crc(p+128,n-128)!=word(p+20)||std::all_of(p+92,p+124,[](uint8_t v){return !v;}))return reject("Sparkles CRC/IR proof rejected");
 FieldSparklesData d;d.identity_=id;Reader r{p,n};d.scene_=r.text();d.script_=r.text();r.hash();d.low_=r.scalar();d.high_=r.scalar();d.image_=r.text();d.texture_=r.text();d.width_=r.integer();d.height_=r.integer();const auto image_sha=r.hash();d.output_sha_=r.hash();
 if(!r.ok||!path(d.scene_)||!path(d.script_)||!path(d.image_)||!path(d.texture_)||d.texture_.substr(0,9)!="graphics/"||d.texture_.size()<5||d.texture_.substr(d.texture_.size()-4)!=".t3x"||!d.width_||!d.height_||d.width_>1024||d.height_>1024||d.low_<0||d.high_<d.low_||d.high_>=65536)return reject("Sparkles source texture/RNG rejected");
 auto count=r.integer();if(!r.ok||!count||count>1000)return reject("Sparkles source animation count rejected");std::set<std::string>names;
 for(uint32_t i=0;i<count;++i){FieldSparklesAnimation a;a.name=r.text();a.speed=r.scalar();a.loop=r.boolean();auto frames=r.integer();if(!r.ok||a.name.empty()||!names.insert(a.name).second||a.speed<=0||a.speed>1024||!frames||frames>4096)return reject("Sparkles source animation rejected");for(uint32_t j=0;j<frames;++j){FieldSparklesFrame f{r.integer(),r.integer(),r.integer(),r.integer()};if(!r.ok||!f.width||!f.height||f.x>d.width_||f.y>d.height_||f.width>d.width_-f.x||f.height>d.height_-f.y)return reject("Sparkles atlas frame bounds rejected");a.frames.push_back(f);}d.animations_.push_back(std::move(a));}
 std::set<uint32_t>ids;std::set<std::string>nodes;uint32_t previous=0;
 for(uint32_t i=0;i<word(p+32);++i){FieldSparklesDescriptor a;a.id=r.integer();a.parent_id=r.integer();auto owner=r.integer();a.owner=static_cast<FieldSparklesOwner>(owner);a.ready=r.integer();a.profile=r.integer();a.frame=r.integer();a.flags=r.integer();a.z_index=r.signed_integer();a.pause_mode=r.integer();a.process_priority=r.signed_integer();a.speed_scale=r.scalar();a.position=r.vector();a.offset=r.vector();for(auto&v:a.world)v=r.vector();a.node=r.text();a.parent=r.text();if(!r.ok||!a.id||!a.parent_id||a.id==a.parent_id||owner>2||!ids.insert(a.id).second||!nodes.insert(a.node).second||!path(a.node)||!path(a.parent)||a.node.substr(0,a.parent.size()+1)!=a.parent+"/"||a.node.find('/',a.parent.size()+1)!=a.node.npos||(i&&a.ready<=previous)||a.profile>=count||a.frame>=d.animations_[a.profile].frames.size()||a.flags>31||a.pause_mode>2||a.speed_scale<=0||a.speed_scale>1024||a.z_index< -4096||a.z_index>4096||a.world[0].y!=0||a.world[1].x!=0||a.world[0].x==0||a.world[1].y==0)return reject("Sparkles source instance/order rejected");if(!std::isfinite(a.speed_scale*d.animations_[a.profile].speed)||a.speed_scale*d.animations_[a.profile].speed>1024)return reject("Sparkles clock rate rejected");previous=a.ready;d.records_.push_back(std::move(a));}
 auto proofs=r.integer();if(!r.ok||!proofs||proofs>10000)return reject("Sparkles source proof count rejected");for(uint32_t i=0;i<proofs;++i){auto name=r.text();auto h=r.hash();if(!r.ok||!path(name)||!d.sources_.emplace(name,h).second)return reject("Sparkles duplicate/invalid source proof rejected");}
 std::array<uint8_t,32>h{};if(!r.ok||r.at!=n||!d.source_hash(d.scene_,h)||h!=id.source_sha256||!d.source_hash(d.script_,d.script_sha_)||!d.source_hash(d.image_,h)||h!=image_sha)return reject("Sparkles truncated/trailing/source closure rejected");d.valid_=true;*this=std::move(d);e.clear();return true;
}
}

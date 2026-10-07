#include "encore/field_camera_area.hpp"
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
const FieldCameraAreaDescriptor*FieldCameraAreaData::record(uint32_t id)const{for(const auto&r:records_)if(r.id==id)return &r;return nullptr;}
bool FieldCameraAreaData::source_hash(std::string_view p,std::array<uint8_t,32>&h)const{auto it=sources_.find(std::string(p));if(it==sources_.end())return false;h=it->second;return true;}
bool FieldCameraAreaData::load_file(const char*p,const FieldIdentity&id,std::string&e){if(!p||!*p){e="Camarea path rejected";return false;}FILE*f=std::fopen(p,"rb");if(!f){e="Cannot open Camarea resource";return false;}if(std::fseek(f,0,SEEK_END)){std::fclose(f);e="Camarea seek rejected";return false;}long n=std::ftell(f);if(n<128||n>4*1024*1024||std::fseek(f,0,SEEK_SET)){std::fclose(f);e="Camarea size rejected";return false;}std::vector<uint8_t>b(static_cast<size_t>(n));auto got=std::fread(b.data(),1,b.size(),f);bool closed=std::fclose(f)==0;if(got!=b.size()||!closed){e="Camarea read rejected";return false;}return load(b.data(),b.size(),id,e);}
bool FieldCameraAreaData::load(const uint8_t*p,size_t n,const FieldIdentity&id,std::string&e){
 auto reject=[&](const char*s){e=s;return false;};
 if(!p||n<128||n>4*1024*1024)return reject("Camarea size rejected");
 if(std::memcmp(p,"ENCFCAA1",8)||word(p+8)!=2||word(p+12)!=128||word(p+16)!=n||word(p+24)!=0x454e0023||word(p+28)!=4||!word(p+32)||word(p+32)>10000||!id.scene_id||word(p+36)!=id.scene_id||std::memcmp(p+40,id.upstream_commit.data(),20)||std::memcmp(p+60,id.source_sha256.data(),32)||word(p+124))return reject("Camarea identity/version/capability rejected");
 if(crc(p+128,n-128)!=word(p+20)||std::all_of(p+92,p+124,[](uint8_t v){return !v;}))return reject("Camarea CRC/IR rejected");
 FieldCameraAreaData d;d.identity_=id;Reader r{p,n};d.scene_=r.text();d.script_=r.text();for(auto&v:d.reset_)v=r.signed_integer();if(!r.ok||!path(d.scene_)||!path(d.script_)||d.reset_[0]>=d.reset_[3]||d.reset_[1]>=d.reset_[2])return reject("Camarea source/reset limits rejected");
 std::set<uint32_t>ids,children;std::set<std::string>nodes;uint32_t previous=0;
 for(uint32_t i=0;i<word(p+32);++i){FieldCameraAreaDescriptor a;a.id=r.integer();a.ready=r.integer();a.parent_id=r.integer();a.shape_id=r.integer();a.reference_id=r.integer();a.reference_exists=r.boolean();a.flags=r.integer();a.layer=r.integer();a.mask=r.integer();a.pause=r.integer();a.priority=r.signed_integer();a.position=r.vector();a.scale=r.vector();for(auto&v:a.world)v=r.vector();a.shape_position=r.vector();a.shape_scale=r.vector();for(auto&v:a.shape_world)v=r.vector();a.extents=r.vector();a.camera_offset=r.vector();for(auto&v:a.reference_margins)v=r.scalar();a.node=r.text();a.reference_path=r.text();a.reference_node=r.text();
  if(!r.ok||!a.id||!a.parent_id||!a.shape_id||a.id==a.parent_id||a.id==a.shape_id||!ids.insert(a.id).second||!children.insert(a.shape_id).second||!nodes.insert(a.node).second||!path(a.node)||(i&&a.ready<=previous)||a.flags>15||a.pause>2||a.scale.x<=0||a.scale.y<=0||a.shape_scale.x<=0||a.shape_scale.y<=0||a.extents.x<=0||a.extents.y<=0||a.world[0].y||a.world[1].x||a.world[0].x<=0||a.world[1].y<=0||a.shape_world[0].y||a.shape_world[1].x||a.shape_world[0].x<=0||a.shape_world[1].y<=0||a.reference_exists!=(a.reference_id!=0)||a.reference_exists!=!a.reference_node.empty()||(a.reference_exists&&(!path(a.reference_node)||a.reference_path.empty()))||a.reference_path.find(':')!=a.reference_path.npos||a.reference_path.find('\\')!=a.reference_path.npos)return reject("Camarea instance/reference/geometry rejected");
  previous=a.ready;d.records_.push_back(std::move(a));}
 for(auto v:children){if(ids.count(v))return reject("Camarea root/shape alias rejected");}
 auto count=r.integer();if(!r.ok||count!=2)return reject("Camarea connection count rejected");
 std::set<std::string>signals,methods;
 for(uint32_t i=0;i<count;++i){FieldCameraAreaConnection c;c.role=r.integer();c.signal=r.text();c.method=r.text();
  auto symbol=[](const std::string &s){return !s.empty()&&s.size()<=256&&(s[0]=='_'||(s[0]>='a'&&s[0]<='z')||(s[0]>='A'&&s[0]<='Z'))&&std::all_of(s.begin(),s.end(),[](char x){return x=='_'||(x>='a'&&x<='z')||(x>='A'&&x<='Z')||(x>='0'&&x<='9');});};
  if(!r.ok||c.role!=i+1||!symbol(c.signal)||!symbol(c.method)||!signals.insert(c.signal).second||!methods.insert(c.method).second)return reject("Camarea source connection rejected");
  d.connections_.push_back(std::move(c));}
 auto proofs=r.integer();if(!r.ok||!proofs||proofs>10000)return reject("Camarea proof count rejected");for(uint32_t i=0;i<proofs;++i){auto name=r.text();auto h=r.hash();if(!r.ok||!path(name)||!d.sources_.emplace(name,h).second)return reject("Camarea source proof rejected");}
 std::array<uint8_t,32>h{};if(!r.ok||r.at!=n||!d.source_hash(d.scene_,h)||h!=id.source_sha256||!d.source_hash(d.script_,d.script_sha_))return reject("Camarea source/trailing rejected");d.valid_=true;*this=std::move(d);e.clear();return true;
}
}

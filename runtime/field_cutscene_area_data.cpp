#include "encore/field_cutscene_area.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <map>
#include <set>
namespace encore::upstream {namespace {
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
uint32_t crc(const uint8_t*p,size_t n){uint32_t c=~0u;for(size_t i=0;i<n;++i){c^=i>=16&&i<20?0:p[i];for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&uint32_t(-int32_t(c&1)));}return~c;}
struct Reader{
 const uint8_t*p;size_t n,at=64;bool ok=true;
 uint32_t integer(){if(!ok||at>n||n-at<4){ok=false;return 0;}auto v=u32(p+at);at+=4;return v;}
 bool boolean(){auto v=integer();if(v>1)ok=false;return v!=0;}
 float scalar(){auto u=integer();float v;std::memcpy(&v,&u,4);if(!std::isfinite(v)||std::abs(v)>1e6)ok=false;return v;}
 Vec2 point(){return{scalar(),scalar()};}
 std::string text(){auto len=integer();if(!ok||len>8192||at>n||len>n-at){ok=false;return{};}std::string v(reinterpret_cast<const char*>(p+at),len);at+=len;size_t i=0;uint32_t cp;while(i<v.size())if(!encore::utf8_next(v,i,cp)||cp<32){ok=false;break;}return v;}
 std::array<uint8_t,32>hash(){std::array<uint8_t,32>h{};if(!ok||at>n||n-at<h.size()){ok=false;return h;}std::copy(p+at,p+at+h.size(),h.begin());at+=h.size();if(std::all_of(h.begin(),h.end(),[](uint8_t b){return!b;}))ok=false;return h;}
};
bool path(std::string_view p){return!p.empty()&&p.front()!='/'&&p.back()!='/'&&p.find("..") ==p.npos&&p.find(':')==p.npos&&p.find('\\')==p.npos;}
bool symbol(std::string_view s){if(s.empty())return false;for(auto c:s)if(!(c=='_'||(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')))return false;return true;}
}
const FieldCutsceneAreaBinding*FieldCutsceneAreaData::binding(uint32_t id)const{for(const auto&v:bindings_)if(v.id==id)return&v;return nullptr;}
bool FieldCutsceneAreaData::load_file(const char*p,std::string&e){
 if(!p){e="CutsceneArea file path absent";return false;}
 std::ifstream f(p,std::ios::binary);if(!f){e="CutsceneArea file unavailable";return false;}
 f.seekg(0,std::ios::end);auto n=f.tellg();if(n<64||n>4*1024*1024){e="CutsceneArea file size";return false;}
 f.seekg(0);std::vector<uint8_t>b(static_cast<size_t>(n));if(!f.read(reinterpret_cast<char*>(b.data()),n)){e="CutsceneArea file read";return false;}
 return load(b.data(),b.size(),e);
}
bool FieldCutsceneAreaData::source_hash(std::string_view path_,std::array<uint8_t,32>&out)const{auto i=sources_.find(std::string(path_));if(i==sources_.end())return false;out=i->second;return true;}
bool FieldCutsceneAreaData::load(const uint8_t*p,size_t n,std::string&e){
 auto reject=[&](const char*m){e=m;return false;};if(!p||n<64||n>4*1024*1024)return reject("CutsceneArea pack size");
 if(std::memcmp(p,"ENCCSA01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1||u32(p+24)!=1||!u32(p+28)||u32(p+28)>4096||u32(p+16)!=crc(p,n))return reject("CutsceneArea header/version/capability/rules/CRC");
 for(size_t i=52;i<64;++i)if(p[i])return reject("CutsceneArea reserved header");
 FieldCutsceneAreaData d;std::copy(p+32,p+52,d.pin_.begin());if(std::all_of(d.pin_.begin(),d.pin_.end(),[](uint8_t b){return!b;}))return reject("CutsceneArea source pin absent");
 Reader r{p,n};d.scene_=r.text();d.script_=r.text();for(auto&v:d.policy_.close)v=r.boolean();for(auto&v:d.policy_.pause)v=r.boolean();d.policy_.completion=r.text();d.policy_.battle_signal=r.text();
 if(!r.ok||!path(d.scene_)||!path(d.script_)||!symbol(d.policy_.completion)||!symbol(d.policy_.battle_signal))return reject("CutsceneArea source policy/lifecycle");
 auto count=r.integer();if(count!=2)return reject("CutsceneArea connection capability");std::set<uint32_t>roles;std::set<std::string>signals;
 for(uint32_t i=0;i<count;++i){FieldCutsceneAreaConnection c;c.role=r.integer();c.signal=r.text();c.method=r.text();if(!r.ok||c.role!=i+1||!roles.insert(c.role).second||!signals.insert(c.signal).second||!symbol(c.signal)||!symbol(c.method))return reject("CutsceneArea source signal role/identity");d.policy_.connections.push_back(std::move(c));}
 count=r.integer();if(count!=u32(p+28))return reject("CutsceneArea source instance count");std::set<uint32_t>ids,ordinals;std::set<std::string>nodes;uint32_t previous=0;
 for(uint32_t i=0;i<count;++i){
  FieldCutsceneAreaBinding b;b.id=r.integer();b.ready_ordinal=r.integer();b.shape_id=r.integer();b.collision_layer=r.integer();b.collision_mask=r.integer();b.flags=r.integer();b.node=r.text();b.dialog=r.text();b.programme=r.text();b.appear=r.text();b.disappear=r.text();b.programme_sha=r.hash();b.centre=r.point();b.half_extents=r.point();
  if(!r.ok||!b.id||!b.shape_id||!ids.insert(b.id).second||!ids.insert(b.shape_id).second||!ordinals.insert(b.ready_ordinal).second||(i&&b.ready_ordinal<=previous)||b.flags>3||!path(b.node)||!nodes.insert(b.node).second||!path(b.dialog)||!path(b.programme)||b.half_extents.x<=0||b.half_extents.y<=0)return reject("CutsceneArea source geometry/identity/programme");
  previous=b.ready_ordinal;d.bindings_.push_back(std::move(b));
 }
 count=r.integer();if(!count||count>2048)return reject("CutsceneArea source closure count");std::map<std::string,std::array<uint8_t,32>>sources;
 for(uint32_t i=0;i<count;++i){auto name=r.text();auto hash=r.hash();if(!r.ok||!path(name)||!sources.emplace(std::move(name),hash).second)return reject("CutsceneArea source closure hash/identity");}
 if(!sources.count(d.scene_)||!sources.count(d.script_))return reject("CutsceneArea source closure absent");
 for(const auto&b:d.bindings_){auto it=sources.find(b.programme);if(it==sources.end()||it->second!=b.programme_sha)return reject("CutsceneArea programme/source hash mismatch");}
 if(!r.ok||r.at!=n)return reject("CutsceneArea trailing bytes");
 d.sources_=std::move(sources);d.valid_=true;*this=std::move(d);e.clear();return true;
}
}

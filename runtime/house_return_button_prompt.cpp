#include "encore/house_return_button_prompt.hpp"
#include "encore/crc32.hpp"
#include "encore/utf8.hpp"
#include "global_yaml_file_hash.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <set>
#include <cmath>
namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
bool nz(const std::array<uint8_t,32>&v){return std::any_of(v.begin(),v.end(),[](uint8_t x){return x!=0;});}
bool safe(const std::string&p){return !p.empty()&&p[0]!='/'&&p.find("..") ==p.npos&&p.find(':')==p.npos&&p.find('\\')==p.npos;}
struct Reader {
 const uint8_t*p;size_t n;
 bool raw(void*out,size_t z){if(z>n)return false;std::memcpy(out,p,z);p+=z;n-=z;return true;}
 bool u(uint32_t&v){if(n<4)return false;v=uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;p+=4;n-=4;return true;}
 bool s(std::string&v){uint32_t z=0;if(!u(z)||!z||z>2048||z>n)return false;v.assign(reinterpret_cast<const char*>(p),z);p+=z;n-=z;size_t c=0;return v.find('\0')==v.npos&&encore::utf8_count(v,c);}
 bool hash(std::array<uint8_t,32>&v){return raw(v.data(),v.size())&&nz(v);}
 bool f(float&v){uint32_t bits=0;if(!u(bits))return false;std::memcpy(&v,&bits,4);return std::isfinite(v);}
};
}
const HouseButtonPromptConnection*HouseReturnButtonPromptData::connection(uint32_t role)const{
 for(const auto&c:connections_)if(c.role==role)return &c;return nullptr;
}
bool HouseReturnButtonPromptData::source_hash(std::string_view p,std::array<uint8_t,32>&out)const{
 auto i=sources_.find(std::string(p));if(i==sources_.end())return false;out=i->second;return true;
}
bool HouseReturnButtonPromptData::load(const uint8_t*p,size_t n,
 const FieldNodeTreeData&t,const std::array<uint8_t,32>&tree_ir,std::string&e){
 if(valid_||!p||n<120||n>4*1024*1024||std::memcmp(p,"ENCHBPR1",8)||!t.valid()||!nz(tree_ir))
  return fail(e,"House ButtonPrompt format/tree rejected");
 HouseReturnButtonPromptData d;Reader r{p+8,n-8};uint32_t fmt=0,cap=0,rules=0,family=0,scene=0,bytes=0,crc=0;
 if(!r.u(fmt)||!r.u(cap)||!r.u(rules)||!r.u(family)||!r.u(scene)||!r.u(bytes)||!r.u(crc)||fmt!=1||cap!=1||rules!=1||family!=0x454e0080||bytes!=n-120||
  !r.raw(d.identity_.upstream_commit.data(),20)||!r.hash(d.identity_.source_sha256)||!r.hash(d.ir_)||encore::crc32(p+120,n-120)!=crc)
  return fail(e,"House ButtonPrompt header/version/CRC rejected");
 d.identity_.scene_id=scene;const auto actual=t.identity();
 if(scene!=actual.scene_id||d.identity_.upstream_commit!=actual.upstream_commit||d.identity_.source_sha256!=actual.source_sha256||!scene)
  return fail(e,"House ButtonPrompt full tree source identity differs");
 std::array<uint8_t,32>core_sha{},native_sha{};uint32_t method_count=0,local=0;
 if(!r.hash(d.tree_ir_)||d.tree_ir_!=tree_ir||!r.hash(core_sha)||!r.hash(native_sha)||!r.s(d.hide_)||!r.s(d.offset_)||!r.s(d.enabled_)||d.enabled_==d.offset_||!r.s(d.material_)||!r.s(d.shader_)||!safe(d.material_)||!safe(d.shader_)||
  !r.u(d.shader_declaration_)||!d.shader_declaration_||!r.u(local)||local!=1||!r.hash(d.shader_code_)||!r.f(d.glow_[0])||!r.f(d.glow_[1])||!r.f(d.glow_[2])||!r.f(d.glow_[3])||
  d.glow_[0]!=0||d.glow_[1]!=0||d.glow_[2]!=0||d.glow_[3]!=1||!r.u(method_count)||method_count!=5)
  return fail(e,"House ButtonPrompt source method proof rejected");
 for(uint32_t i=0;i<method_count;++i){std::string method;
  if(!r.s(method)||std::find(d.methods_.begin(),d.methods_.end(),method)!=d.methods_.end())return fail(e,"House ButtonPrompt duplicate source method");d.methods_.push_back(std::move(method));
 }
 d.material_local_=local!=0;
 if(!r.s(d.wait_.source)||!safe(d.wait_.source)||!r.hash(d.wait_.source_sha)||!r.u(d.wait_.id)||!d.wait_.id||!r.u(d.wait_.flags)||d.wait_.flags!=4||!r.s(d.wait_.native_class)||!r.s(d.wait_.method)||
  !r.s(d.font_.path)||!safe(d.font_.path)||!r.hash(d.font_.sha)||!r.u(d.font_.bytes)||!d.font_.bytes||d.font_.bytes>2*1024*1024||!r.hash(d.font_.receipt_sha)||!r.s(d.font_.source)||!safe(d.font_.source)||!r.hash(d.font_.source_sha))
  return fail(e,"House ButtonPrompt tree/wait/font proof rejected");
 uint32_t count=0;if(!r.u(count)||!count||count>1024)return fail(e,"House ButtonPrompt source count rejected");
 for(uint32_t i=0;i<count;++i){std::string path;std::array<uint8_t,32>h{},known{};
  if(!r.s(path)||!safe(path)||!r.hash(h)||!d.sources_.emplace(path,h).second||(t.source_hash(path,known)&&known!=h))return fail(e,"House ButtonPrompt source closure differs");
 }
 std::array<uint8_t,32>proof{};
 if(!d.source_hash(t.source_scene(),proof)||proof!=actual.source_sha256||!d.source_hash(d.wait_.source,proof)||proof!=d.wait_.source_sha||!t.source_hash(d.wait_.source,proof)||proof!=d.wait_.source_sha||
  !d.source_hash(d.material_,proof)||!d.source_hash(d.shader_,proof)||!d.source_hash(d.font_.source,proof)||proof!=d.font_.source_sha)
  return fail(e,"House ButtonPrompt source script/material binding rejected");
 if(!r.u(count)||count!=7)return fail(e,"House ButtonPrompt exact source connections rejected");
 for(uint32_t i=0;i<count;++i){HouseButtonPromptConnection c;
  if(!r.u(c.role)||c.role!=i+1||!r.u(c.emitter)||!r.s(c.signal)||!r.s(c.method)||!r.u(c.arguments)||!r.u(c.flags)||!r.u(c.bind)||
   c.emitter!=(i<2||i==4||i==5?1u:i==6?3u:2u)||c.arguments!=(i<2||i==6?1u:0u)||c.flags!=(i==6?2u:0u)||c.bind!=(i==0?1u:i==1?0u:2u))
   return fail(e,"House ButtonPrompt source callback signature rejected");
  if(std::any_of(d.connections_.begin(),d.connections_.end(),[&](const auto&old){return old.emitter==c.emitter&&old.signal==c.signal;}))return fail(e,"House ButtonPrompt duplicate source signal");
  d.connections_.push_back(std::move(c));
 }
 uint32_t length=0;if(!r.u(length)||length>r.n||length<80||
  global_yaml_bytes_sha256(std::string_view(reinterpret_cast<const char*>(r.p),length))!=core_sha||!d.core_.load(r.p,length,e))return fail(e,"House ButtonPrompt embedded core rejected");
 r.p+=length;r.n-=length;
 if(d.core_.source_pin()!=actual.upstream_commit||d.core_.scene_hash()!=actual.source_sha256||d.core_.scene_id()!=scene||d.core_.script_hash()!=d.wait_.source_sha)
  return fail(e,"House ButtonPrompt core source identity rejected");
 if(!r.u(length)||length!=r.n||length<88||global_yaml_bytes_sha256(std::string_view(reinterpret_cast<const char*>(r.p),length))!=native_sha||!d.native_.load(r.p,length,t,d.core_,e))return fail(e,"House ButtonPrompt embedded native leaves rejected");
 if(d.native_.finished_signal()!=d.connection(7)->signal)return fail(e,"House ButtonPrompt AP callback source differs");
 std::set<uint32_t>roots;
 for(const auto&node:t.records())if(node.script==d.wait_.source)roots.insert(node.id);
 if(roots.empty()||roots.size()!=d.core_.records().size()||d.native_.records().size()!=roots.size()*4)return fail(e,"House ButtonPrompt full source roster rejected");
 for(const auto&c:d.core_.records()){
  const auto*node=t.record(c.id);const auto*parent=t.record(c.parent_id);
  if(!node||!parent||!roots.erase(c.id)||node->path!=c.node||node->parent!=c.parent_id||node->ready!=c.ready_ordinal||node->native_class!="Node2D"||node->script!=d.wait_.source||node->script_sha!=d.wait_.source_sha)
   return fail(e,"House ButtonPrompt root/parent/source Ready binding rejected");
 }
 d.valid_=true;*this=std::move(d);e.clear();return true;
}
bool HouseReturnButtonPromptData::load_file(const char*path,const FieldNodeTreeData&t,
 const std::array<uint8_t,32>&tree_ir,std::string&e){
 if(!path)return fail(e,"House ButtonPrompt path missing");std::ifstream f(path,std::ios::binary|std::ios::ate);
 if(!f)return fail(e,"House ButtonPrompt file unavailable");const auto z=f.tellg();if(z<120||z>4*1024*1024)return fail(e,"House ButtonPrompt file extent rejected");
 std::vector<uint8_t>b(static_cast<size_t>(z));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),b.size()))return fail(e,"House ButtonPrompt file truncated");return load(b.data(),b.size(),t,tree_ir,e);
}
} // namespace encore::upstream

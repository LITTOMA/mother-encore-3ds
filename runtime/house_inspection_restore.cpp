#include "encore/house_inspection_restore.hpp"
#include "encore/crc32.hpp"
#include "encore/utf8.hpp"
#include "global_yaml_file_hash.hpp"
#include <algorithm>
#include <cstring>
#include <fstream>

namespace encore::upstream {
namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
bool nonzero(const std::array<uint8_t,32>&v){return std::any_of(v.begin(),v.end(),[](uint8_t x){return x!=0;});}
bool path(const std::string&p){
  if(p.empty()||p.front()=='/'||p.find(':')!=p.npos||p.find('\\')!=p.npos)return false;
  size_t at=0;
  while(at<p.size()){
    auto end=p.find('/',at);if(end==p.npos)end=p.size();const auto part=p.substr(at,end-at);
    if(part.empty()||part=="."||part=="..")return false;
    at=end+1;
  }
  return p.back()!='/';
}
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
std::array<uint8_t,32> hash(const uint8_t*p,size_t n){return global_yaml_bytes_sha256(std::string_view(reinterpret_cast<const char*>(p),n));}
struct Reader {
  const uint8_t*p;size_t n;
  bool raw(void*out,size_t z){if(z>n)return false;std::memcpy(out,p,z);p+=z;n-=z;return true;}
  bool u(uint32_t&v){if(n<4)return false;v=word(p);p+=4;n-=4;return true;}
  bool h(std::array<uint8_t,32>&v){return raw(v.data(),v.size())&&nonzero(v);}
  bool text(std::string&v){
    uint32_t z=0;if(!u(z)||!z||z>2048||z>n)return false;
    v.assign(reinterpret_cast<const char*>(p),z);p+=z;n-=z;size_t count=0;
    return v.find('\0')==v.npos&&encore::utf8_count(v,count);
  }
};
}
bool HouseInspectionRestoreData::source_hash(std::string_view p,
    std::array<uint8_t,32>&out)const{
  auto i=sources_.find(std::string(p));if(i==sources_.end())return false;
  out=i->second;return true;
}
bool HouseInspectionRestoreData::cross_bind(
    const HouseInspectionRestoreBindings&b,std::string&e)const{
  if(!b.room.valid()||!b.house.valid()||!b.doors||!b.doors->valid()||
     !b.reentry||!b.reentry->valid()||!b.tree||!b.tree->valid()||
     !b.reentry_bytes||!b.reentry_size||!nonzero(b.room_ir_sha)||
     !nonzero(b.tree_ir_sha))
    return fail(e,"Inspection Restore actual resource owners missing");
  if(room_.bytes!=b.room.byte_size()||house_.bytes!=b.house.byte_size()||
     reentry_.bytes!=b.reentry_size||room_.pack!=hash(b.room.bytes(),b.room.byte_size())||
     house_.pack!=hash(b.house.bytes(),b.house.byte_size())||
     reentry_.pack!=hash(b.reentry_bytes,b.reentry_size)||
     room_.ir!=b.room_ir_sha||tree_ir_!=b.tree_ir_sha||
     reentry_.ir!=b.reentry->ir_sha256())
    return fail(e,"Inspection Restore actual Room/House/Reentry fingerprints differ");
  if(b.room.byte_size()<128||std::memcmp(b.room.bytes(),"ENCRMD01",8)||
     word(b.room.bytes()+32)!=8||word(b.room.bytes()+36)!=10||
     !std::equal(identity_.upstream_commit.begin(),identity_.upstream_commit.end(),b.room.bytes()+56)||
     b.room.scene().stable_id!=room_scene_||
     b.room.string(b.room.scene().source_scene_string)!=std::string("res://")+scene_)
    return fail(e,"Inspection Restore complete Room source/schema differs");
  const auto ti=b.tree->identity(),ri=b.reentry->identity();
  if(ti.scene_id!=identity_.scene_id||ti.upstream_commit!=identity_.upstream_commit||
     ti.source_sha256!=identity_.source_sha256||b.tree->source_scene()!=scene_||
     ri.scene_id!=reentry_scene_||ri.upstream_commit!=identity_.upstream_commit||
     ri.source_sha256!=identity_.source_sha256||b.reentry->target_scene()!=scene_||
     b.reentry->door_id()!=door_||!b.reentry->matches(*b.doors,b.room,b.house,e))
    return fail(e,"Inspection Restore complete tree/Reentry source identity differs");
  // Check the exact supplied pack bytes through the real Reentry parser too.
  // This temporary data owner allocates no native nodes and is not published.
  HouseReentryData actual;
  if(!actual.load(b.reentry_bytes,b.reentry_size,*b.doors,b.room,b.house,e)||
     actual.ir_sha256()!=b.reentry->ir_sha256()||
     actual.identity().scene_id!=ri.scene_id||actual.door_id()!=door_)
    return fail(e,"Inspection Restore Reentry bytes/actual parsed owner differ");
  std::array<uint8_t,32>proof{};
  if(!source_hash(scene_,proof)||proof!=identity_.source_sha256||
     !b.tree->source_hash(scene_,proof)||proof!=identity_.source_sha256)
    return fail(e,"Inspection Restore target source proof differs");
  for(const auto&s:sources_){
    if((b.tree->source_hash(s.first,proof)&&proof!=s.second)||
       (b.reentry->source_hash(s.first,proof)&&proof!=s.second))
      return fail(e,"Inspection Restore shared source closure differs");
  }
  for(const auto&n:b.reentry->native_nodes()){
    const auto*t=b.tree->record(n.id);
    if(!t||t->path!=n.node||t->parent!=n.parent||t->native_class!=n.native_class||
       t->script!=n.script||t->script_sha!=n.script_sha||
       t->local[0].x!=n.local.x.x||t->local[0].y!=n.local.x.y||
       t->local[1].x!=n.local.y.x||t->local[1].y!=n.local.y.y||
       t->local[2].x!=n.local.origin.x||t->local[2].y!=n.local.origin.y)
      return fail(e,"Inspection Restore Reentry native source descriptor differs");
  }
  if(b.reentry->native_nodes().empty()||
     b.reentry->native_nodes().front().id!=identity_.scene_id)
    return fail(e,"Inspection Restore native source root differs");
  e.clear();return true;
}
bool HouseInspectionRestoreData::load(const uint8_t*p,size_t n,
    const HouseInspectionRestoreBindings&b,std::string&e){
  if(!p||n<128||n>1024*1024||std::memcmp(p,"ENCHRST1",8)||
     word(p+8)!=1||word(p+12)!=128||word(p+16)!=n||
     word(p+20)!=encore::crc32(p+128,n-128)||word(p+24)!=0x454e0082||
     word(p+28)!=1||word(p+32)!=1||!word(p+36)||word(p+124)!=0)
    return fail(e,"Inspection Restore format/version/capability/CRC rejected");
  HouseInspectionRestoreData next;next.identity_.scene_id=word(p+36);
  std::copy_n(p+40,20,next.identity_.upstream_commit.begin());
  std::copy_n(p+60,32,next.identity_.source_sha256.begin());
  std::copy_n(p+92,32,next.ir_.begin());
  if(!nonzero(next.identity_.source_sha256)||!nonzero(next.ir_))
    return fail(e,"Inspection Restore source/authoring proof missing");
  Reader r{p+128,n-128};
  if(!r.text(next.scene_)||!path(next.scene_)||!r.u(next.room_scene_)||
     !next.room_scene_||!r.u(next.reentry_scene_)||!next.reentry_scene_||
     !r.u(next.door_)||!next.door_||!r.h(next.tree_ir_))
    return fail(e,"Inspection Restore target binding malformed");
  for(auto*f:{&next.room_,&next.house_,&next.reentry_})
    if(!r.u(f->bytes)||!f->bytes||f->bytes>1024*1024||!r.h(f->pack)||!r.h(f->ir))
      return fail(e,"Inspection Restore dependency fingerprint malformed");
  uint32_t count=0;
  if(!r.h(next.original_ir_)||!r.h(next.semantics_)||!r.u(count)||!count||count>1024)
    return fail(e,"Inspection Restore source recipe proof malformed");
  for(uint32_t i=0;i<count;++i){std::string path_value;std::array<uint8_t,32>proof{};
    if(!r.text(path_value)||!path(path_value)||!r.h(proof)||
       !next.sources_.emplace(path_value,proof).second)
      return fail(e,"Inspection Restore source inventory malformed");
  }
  uint32_t size=0;
  if(!r.u(size)||size<96||size!=r.n||!next.cross_bind(b,e)||
     !next.restore_.load(r.p,size,b.room,b.house,e))
    return fail(e,"Inspection Restore embedded original Restore or cross-binding rejected");
  next.valid_=true;*this=std::move(next);e.clear();return true;
}
bool HouseInspectionRestoreData::matches(
    const HouseInspectionRestoreBindings&b,std::string&e)const{
  if(!valid_||!cross_bind(b,e)||!restore_.matches(b.room,b.house))
    return fail(e,"Inspection Restore retained owners changed");
  e.clear();return true;
}
bool HouseInspectionRestoreData::load_file(const char*path_value,
    const HouseInspectionRestoreBindings&b,std::string&e){
  if(!path_value)return fail(e,"Inspection Restore file path missing");
  std::ifstream f(path_value,std::ios::binary|std::ios::ate);
  if(!f)return fail(e,"Inspection Restore file unavailable");const auto z=f.tellg();
  if(z<128||z>1024*1024)return fail(e,"Inspection Restore file extent rejected");
  std::vector<uint8_t>raw(static_cast<size_t>(z));f.seekg(0);
  if(!f.read(reinterpret_cast<char*>(raw.data()),raw.size()))return fail(e,"Inspection Restore file truncated");
  return load(raw.data(),raw.size(),b,e);
}
} // namespace encore::upstream

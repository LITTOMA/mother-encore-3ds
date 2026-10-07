#include "encore/house_return_camera_control.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <set>

namespace encore::upstream {
namespace {
bool reject(std::string&e,const char*m){e=m;return false;}
bool same(const FieldIdentity&a,const FieldIdentity&b){return a.scene_id==b.scene_id&&a.source_sha256==b.source_sha256&&a.upstream_commit==b.upstream_commit;}
bool nonzero(const std::array<uint8_t,32>&a){return std::any_of(a.begin(),a.end(),[](auto x){return x!=0;});}
struct Reader {
 const uint8_t*p;size_t n,at=0;bool ok=true;
 bool bytes(void*out,size_t size){if(!ok||at>n||size>n-at){ok=false;return false;}std::memcpy(out,p+at,size);at+=size;return true;}
 uint32_t u(){uint8_t v[4]{};bytes(v,4);return uint32_t(v[0])|uint32_t(v[1])<<8|uint32_t(v[2])<<16|uint32_t(v[3])<<24;}
 double d(){uint8_t v[8]{};bytes(v,8);uint64_t b=0;for(size_t i=0;i<8;++i)b|=uint64_t(v[i])<<(8*i);double x=0;std::memcpy(&x,&b,8);if(!std::isfinite(x)||std::abs(x)>1000000)ok=false;return x;}
 std::string s(){const auto size=u();if(!ok||!size||size>4096||at>n||size>n-at){ok=false;return{};}std::string out(reinterpret_cast<const char*>(p+at),size);at+=size;if(out.find('\0')!=out.npos)ok=false;return out;}
};
} // namespace
bool HouseReturnCameraControlData::source_hash(std::string_view path,std::array<uint8_t,32>&out)const{
 auto i=sources_.find(std::string(path));if(!valid_||i==sources_.end())return false;out=i->second;return true;
}
bool HouseReturnCameraControlData::load_file(const char*path,const FieldNodeTreeData&t,
 const std::array<uint8_t,32>&tree_ir,std::string&e){
 if(!path)return reject(e,"RoomShaker pack filename missing");std::ifstream f(path,std::ios::binary);
 if(!f)return reject(e,"RoomShaker pack unavailable");std::vector<uint8_t>b((std::istreambuf_iterator<char>(f)),{});
 return load(b.data(),b.size(),t,tree_ir,e);
}
bool HouseReturnCameraControlData::load(const uint8_t*p,size_t size,const FieldNodeTreeData&t,
 const std::array<uint8_t,32>&tree_ir,std::string&e){
 if(!p||size<32||size>1048576||!t.valid()||!nonzero(tree_ir)||std::memcmp(p,"ENCHSHK1",8))return reject(e,"RoomShaker source pack header missing");
 Reader head{p,size};head.at=8;
 if(head.u()!=2||head.u()!=size||head.u()!=crc32(p+32,size-32)||head.u()!=0x48525331||head.u()!=2||head.u()!=1)return reject(e,"RoomShaker format/capability/rules/CRC rejected");
 Reader r{p,size};r.at=32;HouseReturnCameraControlData next;
 r.bytes(next.identity_.upstream_commit.data(),20);next.identity_.scene_id=r.u();r.bytes(next.identity_.source_sha256.data(),32);
 r.bytes(next.tree_ir_.data(),32);r.bytes(next.ir_.data(),32);
 for(auto&v:next.nodes_){v.id=r.u();v.ready=r.u();v.path=r.s();v.native_class=r.s();}
 const auto auto_start=r.u();next.auto_start_=auto_start;next.player_camera_=r.u();for(auto&v:next.values_)v=r.d();
 next.script_=r.s();next.prototype_=r.s();next.sound_=r.s();next.bus_=r.s();next.player_state_=r.s();
 next.ui_source_=r.s();next.ui_game_over_member_=r.s();const auto initial=r.u();next.ui_game_over_initial_=initial;r.bytes(next.ui_game_over_getter_.data(),32);r.bytes(next.input_source_.data(),32);r.bytes(next.input_start_.data(),32);r.bytes(next.input_engine_commit_.data(),20);
 if(initial>1||!nonzero(next.input_source_)||!nonzero(next.input_start_)||!nonzero(next.ui_game_over_getter_)||next.ui_game_over_member_!="_game_over")return reject(e,"RoomShaker actual UI constructor/getter proof rejected");
 const auto count=r.u();if(count!=7)return reject(e,"RoomShaker incomplete actual source closure");
 for(uint32_t i=0;i<count;++i){auto path=r.s();std::array<uint8_t,32>sha{};r.bytes(sha.data(),32);
  if(!nonzero(sha)||path.find("..")!=path.npos||!next.sources_.emplace(path,sha).second)return reject(e,"RoomShaker source SHA/duplicate path rejected");}
 const auto methods=r.u();if(methods!=6)return reject(e,"RoomShaker source function closure missing");
 for(uint32_t i=0;i<methods;++i){auto m=r.s();std::array<uint8_t,32>sha{};r.bytes(sha.data(),32);
  if(!nonzero(sha)||!next.methods_.emplace(m,sha).second)return reject(e,"RoomShaker source function proof rejected");}
 for(const auto*m:{"_ready","delayed_start","start_shake","stop_shake","vibrate","_on_Timer_timeout"})if(!next.methods_.count(m))return reject(e,"RoomShaker unimplemented source method");
 if(!r.ok||r.at!=size||auto_start>1||!same(next.identity_,t.identity())||next.tree_ir_!=tree_ir||!nonzero(next.ir_)||
  next.values_[0]<=0||next.values_[1]<0||next.values_[1]>=next.values_[0]||next.values_[3]<=0||next.values_[4]<0||
  next.values_[7]<0||next.values_[7]!=std::trunc(next.values_[7])||next.values_[8]<0||next.values_[8]>1||next.values_[9]<0||next.values_[9]>1)
  return reject(e,"RoomShaker payload/source identity/tuning rejected");
 std::array<uint8_t,32>sha{};
 if(!t.source_hash(t.source_scene(),sha)||next.sources_[t.source_scene()]!=sha||sha!=t.identity().source_sha256||
  !t.source_hash(next.script_,sha)||next.sources_[next.script_]!=sha||!next.sources_.count(next.prototype_)||!next.sources_.count(next.sound_)||!next.sources_.count(next.ui_source_))
  return reject(e,"RoomShaker source scene/script/resource closure differs");
 std::set<uint32_t>ids;size_t scripts=0;
 for(const auto&n:t.records())if(n.script==next.script_)++scripts;
 if(scripts!=1)return reject(e,"RoomShaker omits/adds an actual House source receiver");
 size_t children=0;for(const auto&n:t.records())if(n.parent==next.nodes_[0].id)++children;
 if(children!=2)return reject(e,"RoomShaker source native child closure differs");
 for(size_t i=0;i<3;++i){const auto&v=next.nodes_[i];const auto*n=t.record(v.id);
  const auto type=i==0?"Control":i==1?"Timer":"AudioStreamPlayer";
  if(!n||!ids.insert(v.id).second||v.ready!=n->ready||v.path!=n->path||v.native_class!=type||n->class_index>=t.classes().size()||t.classes()[n->class_index]!=type||
   (i==0?(n->script!=next.script_||n->script_sha!=next.sources_.at(next.script_)||n->script_methods!=1):(!n->script.empty()||n->parent!=next.nodes_[0].id)))
   return reject(e,"RoomShaker full Tree source/native/Ready descriptor differs");
 }
 next.valid_=true;*this=std::move(next);e.clear();return true;
}
} // namespace encore::upstream

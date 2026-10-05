#include "encore/field_npc.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include "encore/utf8.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <utility>
namespace encore::upstream {
namespace {
uint32_t word(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
bool path(const std::string&s){return !s.empty()&&s.front()!='/'&&s.find('\\')==s.npos&&s.find(':')==s.npos&&s.find("..") ==s.npos;}
struct Reader {
 const uint8_t*p;size_t size,at=64;bool good=true;
 uint32_t u(){if(at>size||size-at<4){good=false;return 0;}const auto v=word(p+at);at+=4;return v;}
 float f(){const auto bits=u();float v=0;std::memcpy(&v,&bits,4);if(!std::isfinite(v)||std::abs(v)>1000000)good=false;return v;}
 Vec2 v(){const auto x=f();const auto y=f();return {x,y};}
 uint32_t count(uint32_t max){const auto n=u();if(n>max)good=false;return good?n:0;}
 bool boolean(){const auto v=u();if(v>1)good=false;return v==1;}
 std::string s(){const auto n=count(4096);if(at>size||n>size-at){good=false;return {};}std::string out(reinterpret_cast<const char*>(p+at),n);at+=n;size_t cursor=0;uint32_t cp=0;while(cursor<out.size())if(!encore::utf8_next(out,cursor,cp)||cp<32){good=false;break;}return out;}
 void raw(uint8_t*out,size_t n){if(at>size||n>size-at){good=false;return;}std::memcpy(out,p+at,n);at+=n;}
};
bool fail(std::string&e,const char*s){e=s;return false;}
}
float FieldNpcData::parameter(FieldNpcParameter p)const{const auto i=uint32_t(p);return valid_&&i&&i<=parameters_.size()?parameters_[i-1]:0;}
bool FieldNpcData::load_file(const char*path,std::string&e){std::vector<uint8_t>bytes;if(!encore::read_file(path,bytes,8*1024*1024,e))return false;return load(bytes.data(),bytes.size(),e);}
bool FieldNpcData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<64||n>8*1024*1024||std::memcmp(p,"ENCNPC01",8)||word(p+8)!=1||word(p+12)!=n||word(p+20)!=1||word(p+24)!=1||!word(p+28)||word(p+28)>4096||!word(p+52)||!std::any_of(p+32,p+52,[](uint8_t b){return b!=0;}))return fail(e,"Field NPC header/version/capability/rules rejected");
 for(size_t i=56;i<64;++i)if(p[i])return fail(e,"Field NPC reserved header rejected");uint8_t zero[4]{};auto crc=encore::crc32_update(0xffffffffu,p,16);crc=encore::crc32_update(crc,zero,4);crc=encore::crc32_update(crc,p+20,n-20)^0xffffffffu;if(crc!=word(p+16))return fail(e,"Field NPC CRC rejected");
 FieldNpcData next;next.scene_=word(p+52);std::copy(p+32,p+52,next.pin_.begin());Reader r{p,n};const auto parameter_count=r.count(256);if(parameter_count!=uint32_t(FieldNpcParameter::MovementDifferenceFloor))return fail(e,"Field NPC parameter count rejected");
 for(uint32_t i=0;i<parameter_count;++i){if(r.u()!=i+1)return fail(e,"Field NPC parameter identity rejected");next.parameters_.push_back(r.f());}
 const auto parameter=[&](FieldNpcParameter id){return next.parameters_[uint32_t(id)-1];};
 for(const auto id:{FieldNpcParameter::ReturnDelay,FieldNpcParameter::ReadyTimerMin,FieldNpcParameter::AxisModulus,FieldNpcParameter::MinimumMove,FieldNpcParameter::MotionThreshold,FieldNpcParameter::LookThreshold,FieldNpcParameter::MovementDifferenceFloor})if(parameter(id)<=0)return fail(e,"Field NPC nonpositive parameter rejected");if(std::floor(parameter(FieldNpcParameter::AxisModulus))!=parameter(FieldNpcParameter::AxisModulus)||parameter(FieldNpcParameter::TimerDeviation)<0||parameter(FieldNpcParameter::ExtentVertical)<0||parameter(FieldNpcParameter::ExtentHorizontal)<0)return fail(e,"Field NPC RNG/extent parameter rejected");
 std::set<uint32_t>ids,ordinals;std::set<std::string>nodes;
 for(uint32_t i=0;i<word(p+28);++i){FieldNpcDescriptor v;v.id=r.u();v.ready_ordinal=r.u();v.flags=r.u();v.extended_interact=r.u();v.columns=r.u();v.rows=r.u();v.width=r.u();v.height=r.u();v.node=r.s();v.sprite=r.s();v.animation=r.s();v.texture=r.s();v.appear=r.s();v.disappear=r.s();v.idle=r.s();v.talk_idle=r.s();v.walk=r.s();v.talk=r.s();
  if(!r.good||!v.id||!ids.insert(v.id).second||!ordinals.insert(v.ready_ordinal).second||!path(v.node)||!nodes.insert(v.node).second||!path(v.sprite)||!path(v.animation)||!path(v.texture)||v.idle.empty()||v.talk_idle.empty()||v.walk.empty()||v.talk.empty()||v.flags>=4096||v.extended_interact>3||!v.columns||!v.rows||v.columns>256||v.rows>256||!v.width||!v.height||v.width>4096||v.height>4096||v.width%v.columns||v.height%v.rows)return fail(e,"Field NPC identity/sprite schema rejected");
  v.position=r.v();v.direction=r.v();v.sprite_offset=r.v();v.speed=r.f();v.walk_frequency=r.f();v.wander_radius=r.f();v.timer_wait=r.f();v.safe_margin=r.f();if(v.speed<=0||v.speed>1024||v.walk_frequency<=0||v.walk_frequency>86400||v.wander_radius<=0||v.timer_wait<=0||v.safe_margin<0)return fail(e,"Field NPC movement/timer bounds rejected");
  auto count=r.count(4096);bool have=false,last=false,thoughts=false;uint32_t group=0,ordinal=0;std::string condition;
  for(uint32_t j=0;j<count;++j){FieldNpcDialogue d;d.thoughts=r.boolean();d.group=r.u();d.ordinal=r.u();d.last=r.boolean();d.flag=r.s();d.program=r.s();d.source=r.s();
   const bool same=have&&d.thoughts==thoughts&&d.group==group;if(!path(d.program)||!path(d.source)||!d.ordinal||(same?(last||d.ordinal!=ordinal+1||d.flag!=condition):(!d.ordinal||d.ordinal!=1||(have&&!last)||(have&&(d.thoughts<thoughts||(d.thoughts==thoughts&&d.group!=group+1)))||(!have&&d.group!=0)||(have&&d.thoughts!=thoughts&&d.group!=0))))return fail(e,"Field NPC dialogue group/progression rejected");
   have=true;last=d.last;thoughts=d.thoughts;group=d.group;ordinal=d.ordinal;condition=d.flag;v.dialogues.push_back(std::move(d));}if(have&&!last)return fail(e,"Field NPC incomplete dialogue group");
  count=r.count(4096);for(uint32_t j=0;j<count;++j){FieldNpcEventPosition event;event.flag=r.s();event.position=r.v();v.event_positions.push_back(std::move(event));}
  count=r.count(256);for(uint32_t j=0;j<count;++j){FieldNpcConnection c;c.from=r.s();c.to=r.s();c.mode=r.u();if(c.from.empty()||c.to.empty()||c.mode>2)return fail(e,"Field NPC transition rejected");v.connections.push_back(std::move(c));}
  count=r.count(256);std::set<std::string>motions;for(uint32_t j=0;j<count;++j){FieldNpcMotion m;m.name=r.s();m.loop=r.boolean();const auto dirs=r.count(8);if(m.name.empty()||!motions.insert(m.name).second||(dirs!=1&&dirs!=2&&dirs!=4&&dirs!=8))return fail(e,"Field NPC directional state rejected");
   for(uint32_t k=0;k<dirs;++k){FieldNpcDirection dir;dir.vector=r.v();dir.duration=r.f();const auto keys=r.count(4096);if(!keys||dir.duration<=0||dir.duration>86400)return fail(e,"Field NPC clip key/length rejected");float previous=-1;for(uint32_t l=0;l<keys;++l){FieldNpcKey key;key.time=r.f();key.frame=r.u();if(key.time<0||key.time<previous||key.time>=dir.duration||key.frame>=uint64_t(v.columns)*v.rows)return fail(e,"Field NPC frame/time rejected");previous=key.time;dir.keys.push_back(key);}m.directions.push_back(std::move(dir));}v.motions.push_back(std::move(m));}
  if(!motions.count(v.idle)||!motions.count(v.talk_idle))return fail(e,"Field NPC initial state missing");for(const auto&c:v.connections)if(!motions.count(c.from)||!motions.count(c.to))return fail(e,"Field NPC dangling transition");
  count=r.count(9);if(count!=9)return fail(e,"Field NPC geometry count rejected");for(uint32_t j=0;j<count;++j){FieldNpcGeometry g;g.role=r.u();g.kind=r.u();g.layer=r.u();g.mask=r.u();g.offset=r.v();g.value=r.v();g.scale=r.v();g.rotation=r.f();if(g.role!=j+1||g.kind<1||g.kind>5||g.layer>65535||g.mask>65535||g.scale.x<=0||g.scale.y<=0||(g.kind==1&&(g.value.x<=0||g.value.y<=0))||(g.kind==2&&g.value.x<=0))return fail(e,"Field NPC geometry role/value rejected");v.geometry.push_back(g);}
  if(!r.good)return fail(e,"Field NPC truncated/nonfinite record rejected");next.npcs_.push_back(std::move(v));
 }
 const auto count=r.count(8192);std::set<std::string>source_paths;for(uint32_t i=0;i<count;++i){FieldNpcSource s;s.path=r.s();r.raw(s.sha256.data(),s.sha256.size());if(!path(s.path)||!source_paths.insert(s.path).second||!std::any_of(s.sha256.begin(),s.sha256.end(),[](uint8_t b){return b!=0;}))return fail(e,"Field NPC source reference rejected");next.sources_.push_back(std::move(s));}
 if(!r.good||r.at!=n||!count)return fail(e,"Field NPC truncation/trailing bytes rejected");for(const auto&v:next.npcs_){if(!source_paths.count(v.sprite)||!source_paths.count(v.animation))return fail(e,"Field NPC dangling sprite/animation source");for(const auto&d:v.dialogues)if(!source_paths.count(d.source))return fail(e,"Field NPC dangling programme source");}
 next.valid_=true;*this=std::move(next);e.clear();return true;
}
}

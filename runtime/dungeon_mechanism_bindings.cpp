#include "encore/dungeon_mechanism_bindings.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <cstring>
#include <set>

namespace encore::upstream {namespace {
bool bad(std::string&e,const char*s){e=s;return false;}
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
bool owner(DungeonSwitchesView g,uint32_t id,uint32_t&kind,uint32_t&family){
 for(uint32_t i=0;i<g.switch_count();++i){const auto n=g.switch_node(i);if(n.id==id){kind=1;family=uint32_t(n.kind);return true;}}
 for(uint32_t i=0;i<g.object_count();++i){const auto n=g.object(i);if(n.id==id){kind=2;family=uint32_t(n.kind)+2;return true;}}
 return false;
}
bool wall_owner(DungeonSwitchesView g,const DungeonWallTimelineData&w){
 if(w.scene_id()!=g.scene_id())return false;
 for(uint32_t i=0;i<g.object_count();++i){const auto n=g.object(i);if(n.id==w.owner_id())return n.kind==DungeonControlledKind::Wall&&g.text(n.path)==w.owner_path();}
 return false;
}
}
struct DungeonMechanismBindingsData::State {
 DungeonSwitchesData graph;std::vector<DungeonMechanismBinding>bindings;
 std::vector<DungeonMechanismProfile>profiles;
};
DungeonSwitchesView DungeonMechanismBindingsData::graph()const{return state_?state_->graph.view():DungeonSwitchesView{};}
const std::vector<DungeonMechanismBinding>&DungeonMechanismBindingsData::bindings()const{static const std::vector<DungeonMechanismBinding>empty;return state_?state_->bindings:empty;}
const DungeonMechanismProfile*DungeonMechanismBindingsData::profile(uint32_t id)const{if(state_)for(const auto&p:state_->profiles)if(p.id==id)return &p;return nullptr;}
bool DungeonMechanismBindingsData::load_file(const char*path,std::string&e){
 if(state_||!path)return bad(e,"Mechanism bindings require a fresh owner and file path");
 std::vector<uint8_t>b;return encore::read_file(path,b,256*1024,e)&&load(b.data(),b.size(),e);
}
bool DungeonMechanismBindingsData::load(const uint8_t*p,size_t n,std::string&e){
 if(state_||!p||n<128||n>256*1024)return bad(e,"Mechanism bindings size/owner rejected");
 if(std::memcmp(p,"ENCDMA01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1||!u32(p+24)||u32(p+24)>65535||u32(p+28)||u32(p+52)!=4||u32(p+56)||u32(p+60))return bad(e,"Mechanism bindings schema/capability/reserved rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return bad(e,"Mechanism bindings source pin rejected");}
 if(encore::crc32(p+32,n-32)!=u32(p+16))return bad(e,"Mechanism bindings checksum rejected");
 const uint32_t strides[]={40,16,1,1},limits[]={256,64,256*1024,65536};uint32_t offsets[4],counts[4];size_t cursor=128;
 for(unsigned i=0;i<4;++i){const auto*d=p+64+i*16;offsets[i]=u32(d+4);counts[i]=u32(d+8);
  if(u32(d)!=i+1||u32(d+12)!=strides[i]||!counts[i]||counts[i]>limits[i]||offsets[i]%4||offsets[i]<cursor||offsets[i]>n||counts[i]>(n-offsets[i])/strides[i])return bad(e,"Mechanism bindings section span rejected");
  for(size_t j=cursor;j<offsets[i];++j)if(p[j])return bad(e,"Mechanism bindings noncanonical padding");
  cursor=offsets[i]+counts[i]*strides[i];
 }
 if(cursor!=n)return bad(e,"Mechanism bindings unowned trailing bytes");
 auto v=std::make_shared<State>();if(!v->graph.load(p+offsets[3],counts[3],e))return false;
 const auto g=v->graph.view();if(g.scene_id()!=u32(p+24)||counts[0]!=g.switch_count()+g.object_count())return bad(e,"Mechanism bindings graph scene/owner coverage rejected");
 std::set<uint32_t>profile_ids;uint32_t payload=0;
 for(uint32_t i=0;i<counts[1];++i){const auto*q=p+offsets[1]+i*16;const auto id=u32(q),family=u32(q+4),start=u32(q+8),size=u32(q+12);
  if(!id||id>65535||!profile_ids.insert(id).second||family<1||family>4||start!=payload||!size||size>counts[2]-payload)return bad(e,"Mechanism bindings profile identity/span rejected");
  DungeonMechanismProfile row;row.id=id;row.family=DungeonMechanismFamily(family);
  if(family==4){if(!row.wall.load(p+offsets[2]+start,size,e))return false;if(!wall_owner(g,row.wall))return bad(e,"Mechanism bindings wall owner/path rejected");}
  else {
   if(!row.properties.load(p+offsets[2]+start,size,e))return false;
   if(row.properties.clips().empty()||row.properties.clips().size()>32||row.properties.initial().hitbox||row.properties.initial().interaction)return bad(e,"Mechanism bindings property scope rejected");
   for(const auto&key:row.properties.keys())if(key.track!=PropTrack::Frame&&key.track!=PropTrack::BodyDisabled)return bad(e,"Mechanism bindings unknown property track");
  }
  payload+=size;v->profiles.push_back(std::move(row));
 }
 if(payload!=counts[2])return bad(e,"Mechanism bindings unowned profile bytes");
 std::set<uint32_t>seen,used;
 for(uint32_t i=0;i<counts[0];++i){const auto*q=p+offsets[0]+i*40;DungeonMechanismBinding row;row.owner=u32(q);row.kind=DungeonMechanismOwner(u32(q+4));row.profile=u32(q+8);uint32_t kind=0,family=0;
  const DungeonMechanismProfile*profile=nullptr;for(const auto&vprofile:v->profiles)if(vprofile.id==row.profile)profile=&vprofile;
  if(!owner(g,row.owner,kind,family)||!seen.insert(row.owner).second||!profile||u32(q+4)!=kind||uint32_t(profile->family)!=family)return bad(e,"Mechanism bindings graph/profile identity rejected");
  std::set<uint32_t>clips;
  for(unsigned role=0;role<7;++role){row.roles[role]=u32(q+12+role*4);const bool required=kind==1?(role==1||role==2):family==4?(role>=4):(role>=3);
   if(required){const auto&props=family==4?profile->wall.properties():profile->properties;if(!row.roles[role]||!clips.insert(row.roles[role]).second||!props.clip(row.roles[role]))return bad(e,"Mechanism bindings role clip identity rejected");}
   else if(row.roles[role])return bad(e,"Mechanism bindings unsupported animation role");
  }
  if(family==4){const auto&ids=profile->wall.roles();if(profile->wall.owner_id()!=row.owner||row.roles[4]!=ids[0]||row.roles[5]!=ids[2]||row.roles[6]!=ids[3])return bad(e,"Mechanism bindings wall role mapping rejected");}
  used.insert(row.profile);v->bindings.push_back(row);
 }
 if(used!=profile_ids)return bad(e,"Mechanism bindings unused profile rejected");
 state_=std::move(v);e.clear();return true;
}
}

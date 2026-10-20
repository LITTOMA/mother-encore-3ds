#include "encore/dungeon_wall_timeline.hpp"
#include "encore/dungeon_switches.hpp"
#include "encore/audio_data.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
bool bad(std::string&e,const char*s){e=s;return false;}
uint32_t u32(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
float f32(const uint8_t*p){auto bits=u32(p);float v;std::memcpy(&v,&bits,4);return v;}
double f64(const uint8_t*p){uint64_t bits=u32(p)|(uint64_t(u32(p+4))<<32);double v;std::memcpy(&v,&bits,8);return v;}
bool real(double v,double low,double high){return std::isfinite(v)&&v>=low&&v<=high;}
bool path(std::string_view v){if(v.empty()||v.size()>512)return false;size_t at=0;while(at<v.size()){auto end=v.find('/',at);if(end==v.npos)end=v.size();auto part=v.substr(at,end-at);if(part.empty()||part=="."||part=="..")return false;for(unsigned char c:part)if(c<32||c>126||c=='\\'||c==':')return false;at=end+1;}return v.back()!='/';}
}
const DungeonWallCueClip*DungeonWallTimelineData::clip(uint32_t id)const{if(valid_)for(const auto&c:clips_)if(c.id==id)return &c;return nullptr;}
bool DungeonWallTimelineData::load_file(const char*path,std::string&e){if(!path)return bad(e,"Wall timeline file path absent");std::vector<uint8_t>b;return encore::read_file(path,b,65536,e)&&load(b.data(),b.size(),e);}
bool DungeonWallTimelineData::load(const uint8_t*p,size_t n,std::string&e){
 if(!p||n<144||n>65536)return bad(e,"Wall timeline size rejected");
 if(std::memcmp(p,"ENCDWT01",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1||!u32(p+24)||u32(p+24)>65535||u32(p+28)||u32(p+52)!=5||u32(p+56)||u32(p+60))return bad(e,"Wall timeline schema/capability/reserved rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return bad(e,"Wall timeline source pin rejected");}
 if(encore::crc32(p+32,n-32)!=u32(p+16))return bad(e,"Wall timeline checksum rejected");
 const uint32_t strides[]={1,128,1,16,16},limits[]={1030,1,65536,4,1024};uint32_t offset[5],count[5];size_t cursor=144;
 for(unsigned i=0;i<5;++i){auto*d=p+64+i*16;offset[i]=u32(d+4);count[i]=u32(d+8);
  if(u32(d)!=i+1||u32(d+12)!=strides[i]||!count[i]||count[i]>limits[i]||offset[i]%4||offset[i]<cursor||offset[i]>n||count[i]>(n-offset[i])/strides[i])return bad(e,"Wall timeline section span rejected");
  for(size_t j=cursor;j<offset[i];++j){if(p[j])return bad(e,"Wall timeline noncanonical padding");}
  cursor=offset[i]+count[i]*strides[i];
 }
 if(cursor!=n||count[1]!=1||count[3]!=4)return bad(e,"Wall timeline section coverage rejected");
 DungeonWallTimelineData v;v.scene_=u32(p+24);const auto*q=p+offset[1];v.owner_=u32(q);v.audio_.id=u32(q+4);std::set<uint32_t>ids;
 if(!v.owner_||!v.audio_.id)return bad(e,"Wall timeline owner/audio identity rejected");
 for(unsigned i=0;i<4;++i){v.roles_[i]=u32(q+8+i*4);if(!v.roles_[i]||v.roles_[i]>65535||!ids.insert(v.roles_[i]).second)return bad(e,"Wall timeline role identity rejected");}
 const auto po=u32(q+24),pl=u32(q+28),ao=u32(q+32),al=u32(q+36);
 if(po||!pl||pl>count[0]||ao!=pl||al<=6||al>count[0]-pl||al!=count[0]-pl)return bad(e,"Wall timeline string ownership rejected");
 v.path_.assign(reinterpret_cast<const char*>(p+offset[0]),pl);v.audio_.source.assign(reinterpret_cast<const char*>(p+offset[0]+ao),al);
 if(!path(v.path_)||v.audio_.source.compare(0,6,"res://")||!path(std::string_view(v.audio_.source).substr(6)))return bad(e,"Wall timeline unsafe source/owner path");
 std::copy_n(q+40,32,v.audio_.sha.begin());if(std::all_of(v.audio_.sha.begin(),v.audio_.sha.end(),[](uint8_t b){return b==0;}))return bad(e,"Wall timeline audio source hash absent");
 const auto flags=u32(q+76),diminish=u32(q+112);v.audio_.gain_db=f32(q+72);v.audio_.loop=flags!=0;v.audio_.bus=u32(q+116);v.audio_.pitch=f32(q+120);
 v.camera_.magnitude=f64(q+80);v.camera_.direction={f32(q+88),f32(q+92)};v.camera_.interval=f64(q+96);v.camera_.weight=f64(q+104);v.camera_.diminish=diminish!=0;
 if(flags>1||!real(v.audio_.gain_db,-120,24)||v.audio_.bus!=1||v.audio_.pitch!=1||u32(q+124)||diminish>1||!real(v.camera_.magnitude,0,1000000)||!real(v.camera_.direction.x,-1000000,1000000)||!real(v.camera_.direction.y,-1000000,1000000)||!real(v.camera_.interval,0,60)||v.camera_.interval<=0||!real(v.camera_.weight,0,1))return bad(e,"Wall timeline unsupported node/camera policy");
 if(!v.properties_.load(p+offset[2],count[2],e))return false;
 const auto&initial=v.properties_.initial();if(v.properties_.clips().size()!=4||initial.hitbox||initial.interaction)return bad(e,"Wall timeline nested property scope rejected");
 for(const auto&key:v.properties_.keys())if(key.track!=PropTrack::Frame&&key.track!=PropTrack::BodyDisabled)return bad(e,"Wall timeline unsupported nested track");
 for(uint32_t i=0;i<count[4];++i){auto*k=p+offset[4]+i*16;const auto kind=u32(k+4);DungeonWallCue row{f32(k),DungeonWallCueKind(kind),f64(k+8)};
  if(!real(row.time,0,60)||(kind!=1&&kind!=2)||(kind==1?(row.value!=0&&row.value!=1):(!real(row.value,v.camera_.interval,60)||row.value/v.camera_.interval>1000000)))return bad(e,"Wall timeline cue kind/value rejected");
  v.cues_.push_back(row);
 }
 uint32_t first=0;
 for(unsigned i=0;i<4;++i){auto*c=p+offset[3]+i*16;DungeonWallCueClip row{u32(c),u32(c+4),u32(c+8)};const auto&prop=v.properties_.clips()[i];
  if(prop.id!=v.roles_[i]||row.id!=prop.id||row.first!=first||row.count>count[4]-first||u32(c+12))return bad(e,"Wall timeline cue clip ownership rejected");
  uint32_t kind=0;float previous=-1;
  for(uint32_t j=0;j<row.count;++j){const auto&key=v.cues_[first+j];const auto actual=uint32_t(key.kind);if(key.time>prop.duration||actual<kind||(actual==kind&&key.time<=previous))return bad(e,"Wall timeline source track order/range rejected");kind=actual;previous=key.time;}
  first+=row.count;v.clips_.push_back(row);
 }
 if(first!=count[4])return bad(e,"Wall timeline unowned cue data");
 v.valid_=true;*this=std::move(v);e.clear();return true;
}
bool DungeonWallTimelineData::matches(const DungeonSwitchesView&graph,const AudioBank&bank,std::string&e)const{
 if(!valid_||!graph.valid()||graph.scene_id()!=scene_)return bad(e,"Wall timeline scene graph mismatch");
 bool found=false;for(uint32_t i=0;i<graph.object_count();++i){auto node=graph.object(i);if(node.id==owner_){if(node.kind!=DungeonControlledKind::Wall||graph.text(node.path)!=path_)return bad(e,"Wall timeline controlled owner mismatch");found=true;}}
 if(!found)return bad(e,"Wall timeline controlled owner missing");
 AudioAsset asset;if(!bank.find(audio_.id,asset)||asset.source_sha256!=audio_.sha||asset.source_path!=audio_.source||asset.loops()!=audio_.loop||asset.gain_db!=audio_.gain_db)return bad(e,"Wall timeline audio identity/policy mismatch");
 e.clear();return true;
}
}

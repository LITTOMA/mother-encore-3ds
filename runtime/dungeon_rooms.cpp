#include "encore/dungeon_rooms.hpp"
#include "encore/field_camera.hpp"
#include "encore/content.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
namespace encore::upstream {namespace {
bool fail(std::string&e,const char*s){e=s;return false;}
uint32_t u(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
int32_t integer(const uint8_t*p){const auto raw=u(p);int32_t v;std::memcpy(&v,&raw,4);return v;}
float real(const uint8_t*p){const auto raw=u(p);float v;std::memcpy(&v,&raw,4);return v;}
Vec2 vec(const uint8_t*p){return{real(p),real(p+4)};}
bool finite(Vec2 p){return std::isfinite(p.x)&&std::isfinite(p.y)&&std::abs(p.x)<=1000000&&std::abs(p.y)<=1000000;}
bool path(const std::string&s){
 if(s.empty()||s.size()>512||s.front()=='/'||s.back()=='/')return false;size_t start=0;
 for(size_t i=0;i<=s.size();++i){
  if(i<s.size()&&(static_cast<unsigned char>(s[i])<32||static_cast<unsigned char>(s[i])>=127||s[i]==':'||s[i]=='\\'))return false;
  if(i==s.size()||s[i]=='/'){const auto part=s.substr(start,i-start);if(part.empty()||part=="."||part=="..")return false;start=i+1;}
 }
 return true;
}
}
struct DungeonRoomsData::State {
 uint32_t scene=0;int32_t floor=0;std::string source,name,region;
 std::array<int32_t,4>reset{};std::vector<DungeonRoom>rooms;
};
uint32_t DungeonRoomsData::scene()const{return state_?state_->scene:0;}int32_t DungeonRoomsData::floor()const{return state_?state_->floor:0;}
const std::string&DungeonRoomsData::source()const{static const std::string empty;return state_?state_->source:empty;}
const std::string&DungeonRoomsData::name()const{static const std::string empty;return state_?state_->name:empty;}
const std::string&DungeonRoomsData::region()const{static const std::string empty;return state_?state_->region:empty;}
const std::array<int32_t,4>&DungeonRoomsData::reset_limits()const{static const std::array<int32_t,4>empty{};return state_?state_->reset:empty;}
const std::vector<DungeonRoom>&DungeonRoomsData::rooms()const{static const std::vector<DungeonRoom>empty;return state_?state_->rooms:empty;}
const DungeonRoom*DungeonRoomsData::room(uint32_t id)const{for(const auto&r:rooms())if(r.id==id)return&r;return nullptr;}
bool DungeonRoomsData::load_file(const char*path,std::string&e){
 if(valid()||!path||!*path)return fail(e,"Dungeon rooms require fresh owner and path");std::vector<uint8_t>bytes;return encore::read_file(path,bytes,65536,e)&&load(bytes.data(),bytes.size(),e);
}
bool DungeonRoomsData::load(const uint8_t*p,size_t size,std::string&e){
 if(valid()||!p||size<112||size>65536)return fail(e,"Dungeon room size/fresh owner rejected");
 if(std::memcmp(p,"ENCDRM01",8)||u(p+8)!=1||u(p+12)!=size||u(p+20)!=1||!u(p+24)||u(p+24)>65535||u(p+28)||u(p+52)!=3||u(p+56)||u(p+60))return fail(e,"Dungeon room schema/capability/scene rejected");
 const char*pin="7d9246600fffe518408f5830d4848635019005a3";
 for(unsigned i=0;i<20;++i){auto hex=[](char c){return c<='9'?c-'0':c-'a'+10;};if(p[32+i]!=uint8_t(hex(pin[i*2])*16+hex(pin[i*2+1])))return fail(e,"Dungeon room source pin rejected");}
 if(encore::crc32(p+32,size-32)!=u(p+16))return fail(e,"Dungeon room checksum rejected");
 const uint32_t strides[]={1,64,80},maximum[]={65536,1,128};uint32_t off[3]{},count[3]{};size_t cursor=112;
 for(unsigned i=0;i<3;++i){const auto q=p+64+i*16;off[i]=u(q+4);count[i]=u(q+8);
  if(u(q)!=i+1||u(q+12)!=strides[i]||!count[i]||count[i]>maximum[i]||off[i]<cursor||off[i]%4||off[i]>size||count[i]>(size-off[i])/strides[i])return fail(e,"Dungeon room directory span/count rejected");
  for(size_t at=cursor;at<off[i];++at)if(p[at])return fail(e,"Dungeon room nonzero padding rejected");cursor=off[i]+size_t(count[i])*strides[i];
 }
 if(cursor!=size)return fail(e,"Dungeon room trailing data rejected");auto next=std::make_shared<State>();std::vector<uint8_t>used(count[0]);
 auto text=[&](const uint8_t*q,std::string&out){const uint32_t first=u(q),n=u(q+4);if(!n||n>512||first>count[0]||n>count[0]-first)return false;
  for(uint32_t i=first;i<first+n;++i)if(used[i])return false;out.assign(reinterpret_cast<const char*>(p+off[0]+first),n);if(!path(out))return false;
  std::fill(used.begin()+first,used.begin()+first+n,1);return true;
 };
 const auto q=p+off[1];next->scene=u(q);next->floor=integer(q+4);
 if(next->scene!=u(p+24)||next->floor< -128||next->floor>127||!text(q+8,next->source)||!text(q+16,next->name)||!text(q+24,next->region)||next->name.size()>64||next->region.size()>64||next->source.rfind("Maps/",0)||next->source.size()<5||next->source.substr(next->source.size()-5)!=".tscn")return fail(e,"Dungeon room policy identity rejected");
 for(unsigned i=0;i<4;++i){next->reset[i]=integer(q+32+i*4);if(next->reset[i]<-100000000||next->reset[i]>100000000||u(q+48+i*4))return fail(e,"Dungeon room policy bounds/reserved rejected");}
 if(next->reset[0]>=next->reset[2]||next->reset[1]>=next->reset[3])return fail(e,"Dungeon room reset order rejected");
 std::set<uint32_t>ids;std::set<std::string>paths,flags;
 for(uint32_t i=0;i<count[2];++i){const auto r=p+off[2]+size_t(i)*80;DungeonRoom row;row.id=u(r);row.index=u(r+4);row.center=vec(r+24);row.extents=vec(r+32);row.size=vec(r+40);row.offset=vec(r+48);row.layer=u(r+56);row.mask=u(r+60);row.bits=u(r+64);
  if(!row.id||row.index!=i||!ids.insert(row.id).second||!text(r+8,row.path)||!text(r+16,row.flag)||row.path.size()>256||row.flag.size()>256||!paths.insert(row.path).second||!flags.insert(row.flag).second||row.flag.rfind(next->name+"/",0))return fail(e,"Dungeon room owner/flag/index rejected");
  if(!finite(row.center)||!finite(row.extents)||!finite(row.size)||!finite(row.offset)||row.extents.x<=0||row.extents.y<=0||row.size.x<=0||row.size.y<=0||row.size.x!=std::floor(double(row.extents.x)*2+0.5)||row.size.y!=std::floor(double(row.extents.y)*2+0.5)||row.layer>0xfffff||row.mask>0xfffff||row.bits>3||u(r+68)||u(r+72)||u(r+76))return fail(e,"Dungeon room geometry/filter/reserved rejected");next->rooms.push_back(std::move(row));
 }
 if(std::find(used.begin(),used.end(),uint8_t(0))!=used.end())return fail(e,"Dungeon room unowned text rejected");state_=std::move(next);e.clear();return true;
}
size_t DungeonRoomState::find(uint32_t id)const{for(size_t i=0;i<data_.rooms().size();++i)if(data_.rooms()[i].id==id)return i;return SIZE_MAX;}
bool DungeonRoomState::owner(uint64_t epoch,uint32_t id,size_t&i,std::string&e)const{if(!valid()||epoch!=epoch_||(i=find(id))==SIZE_MAX)return fail(e,"Dungeon room callback epoch/owner rejected");return true;}
bool DungeonRoomState::prepare(const DungeonRoomsData&data,uint64_t epoch,const std::vector<DungeonRoomFlag>&flags,std::string&e){
 if(valid()||!data.valid()||!epoch||epoch<=last_epoch_||flags.size()!=data.rooms().size())return fail(e,"Dungeon room prepare/flag coverage rejected");DungeonRoomState next;next.data_=data;next.epoch_=next.last_epoch_=epoch;next.inside_.resize(flags.size());next.visited_.resize(flags.size());std::vector<uint8_t>seen(flags.size());
 for(const auto&flag:flags){const auto i=next.find(flag.owner);if(i==SIZE_MAX||seen[i])return fail(e,"Dungeon room saved flag identity rejected");seen[i]=1;next.visited_[i]=uint8_t(flag.visited);}
 next.limits_=data.reset_limits();*this=std::move(next);e.clear();return true;
}
bool DungeonRoomState::emit(DungeonRoomEventKind kind,uint32_t id,std::string&e){if(events_.size()>=4096)return fail(e,"Dungeon room event capacity exceeded");events_.push_back({kind,id,epoch_,limits_,offset_});return true;}
bool DungeonRoomState::reset(uint32_t id,std::string&e){limits_=data_.reset_limits();offset_={};return emit(DungeonRoomEventKind::Limits,id,e)&&emit(DungeonRoomEventKind::Offset,id,e);}
bool DungeonRoomState::enter(uint64_t epoch,uint32_t id,bool player,std::string&e){
 size_t i;if(!owner(epoch,id,i,e))return false;if(!player){e.clear();return true;}if(inside_[i]||pending_.size()>=512)return fail(e,"Dungeon room duplicate entry/pending capacity rejected");auto next=*this;
 if(!next.visited_[i]){next.visited_[i]=1;if(!next.emit(DungeonRoomEventKind::ObjectFlag,id,e))return false;}
 next.current_=id;if(!next.emit(DungeonRoomEventKind::CurrentRoom,id,e))return false;++next.active_;next.inside_[i]=1;
 if(!next.reset(id,e))return false;next.pending_.push_back(i);*this=std::move(next);e.clear();return true;
}
bool DungeonRoomState::exit(uint64_t epoch,uint32_t id,bool player,std::string&e){
 size_t i;if(!owner(epoch,id,i,e))return false;if(!player){e.clear();return true;}if(!inside_[i])return fail(e,"Dungeon room unmatched exit rejected");auto next=*this;next.inside_[i]=0;--next.active_;
 if(!next.active_&&!next.reset(id,e))return false;*this=std::move(next);e.clear();return true;
}
bool DungeonRoomState::idle_frame(uint64_t epoch,Vec2 viewport,std::string&e){
 if(!valid()||epoch!=epoch_||!finite(viewport)||viewport.x<=0||viewport.y<=0)return fail(e,"Dungeon room idle epoch/viewport rejected");auto next=*this;
 for(const auto i:pending_){if(!next.inside_[i])continue;const auto&r=data_.rooms()[i];FieldCamera area;area.center=r.center;area.width=r.size.x;area.height=r.size.y;
  const auto bounds=field_camera_limits(area,viewport);for(size_t k=0;k<4;++k)next.limits_[k]=int32_t(bounds[k]);
  if(!next.emit(DungeonRoomEventKind::Limits,r.id,e))return false;next.offset_=r.offset;if(!next.emit(DungeonRoomEventKind::Offset,r.id,e))return false;
 }
 next.pending_.clear();*this=std::move(next);e.clear();return true;
}
bool DungeonRoomState::relative_position(Vec2 player,Vec2&out,std::string&e)const{const auto*r=data_.room(current_);if(!valid()||!r||!finite(player))return fail(e,"Dungeon room relative position has no current owner");out={player.x-r->center.x,player.y-r->center.y};e.clear();return true;}
bool DungeonRoomState::retire(uint64_t epoch,std::string&e){
 if(!valid()||epoch!=epoch_)return fail(e,"Dungeon room retirement epoch rejected");epoch_=0;current_=active_=0;std::fill(inside_.begin(),inside_.end(),uint8_t(0));pending_.clear();events_.clear();e.clear();return true;
}
int32_t DungeonRoomState::current_index()const{const auto*r=data_.room(current_);return r?int32_t(r->index):-1;}
bool DungeonRoomState::inside(uint32_t id)const{const auto i=find(id);return i<inside_.size()&&inside_[i];}
bool DungeonRoomState::visited(uint32_t id)const{const auto i=find(id);return i<visited_.size()&&visited_[i];}
}

#include "encore/dungeon_key_contacts.hpp"
namespace encore::upstream {namespace {bool fail(std::string&e,const char*why){e=why;return false;}}
bool DungeonKeyContacts::prepare(const DungeonKeyMediaData&data,uint64_t epoch,std::string&e){
 if(valid()||!data.valid()||!epoch||epoch<=last_epoch_)return fail(e,"Key contacts fresh media/epoch rejected");
 DungeonKeyContacts next;next.data_=data;next.epoch_=next.last_epoch_=epoch;std::vector<uint32_t>owners;
 for(size_t i=0;i<data.shapes().size();++i){const auto&s=data.shapes()[i];if(s.role!=KeyShapeRole::Pickup)continue;
  const auto*o=data.owner(s.owner);const auto*p=o?data.clocks().profile(o->profile):nullptr;
  if(!p||p->kind!=KeyClockKind::Key)return fail(e,"Key contact shape/method owner rejected");next.indices_.push_back(i);owners.push_back(s.owner);
 }
 size_t expected=0;for(const auto&o:data.owners())if(data.clocks().profile(o.profile)->kind==KeyClockKind::Key)++expected;
 if(owners.size()!=expected||!next.pairs_.initialize(owners,e))return fail(e,"Key pickup contact coverage rejected");
 *this=std::move(next);e.clear();return true;
}
bool DungeonKeyContacts::enabled(uint64_t epoch,const DungeonKeyContactBody&body,const DungeonKeyClocks&world,std::array<uint8_t,128>&bits,std::string&e)const{
 if(!valid()||epoch!=epoch_||world.epoch()!=epoch_||body.layer>0xfffff||body.mask>0xfffff||!data_.matches(world,e))return fail(e,"Key contact scene/filter/resource owner rejected");
 for(size_t i=0;i<count();++i){const auto&s=data_.shapes()[indices_[i]];DungeonKeyClockView state;if(!world.state(epoch_,s.owner,state,e))return false;
  const bool filters=(s.mask&body.layer)||(s.layer&body.mask);
  bits[i]=uint8_t(body.enabled&&state.object.alive&&!state.object.collision_disabled&&!(s.bits&1)&&(s.bits&2)&&filters);
 }e.clear();return true;
}
bool DungeonKeyContacts::begin(uint64_t epoch,const DungeonKeyContactBody&body,DungeonKeyClocks&world,bool paused,std::string&e){
 std::array<uint8_t,128>bits{};if(!enabled(epoch,body,world,bits,e))return false;auto next=*this;FieldAreaEvents events;
 if(!next.pairs_.begin(bits.data(),count(),events,e))return false;
 bool entered=false;for(size_t i=0;i<events.count;++i)entered|=events.values[i].entered;
 if(!entered){*this=std::move(next);e.clear();return true;}
 auto candidate=world;
 for(size_t i=0;i<events.count;++i)if(events.values[i].entered&&!candidate.pickup(epoch_,events.values[i].owner,true,paused,e))return false;
 *this=std::move(next);world=std::move(candidate);e.clear();return true;
}
bool DungeonKeyContacts::end(uint64_t epoch,const DungeonKeyContactBody&body,const DungeonKeyClocks&world,const uint8_t*overlap,size_t n,std::string&e){
 std::array<uint8_t,128>bits{};return enabled(epoch,body,world,bits,e)&&pairs_.end(bits.data(),overlap,n,e);
}
bool DungeonKeyContacts::retire(uint64_t epoch,std::string&e){
 if(!valid()||!epoch||epoch!=epoch_)return fail(e,"Key contact retirement epoch rejected");data_={};pairs_={};indices_.clear();epoch_=0;e.clear();return true;
}
}

#pragma once
#include "encore/poison_damage.hpp"
#include "encore/field_area_contacts.hpp"
namespace encore::upstream {
struct PoisonContactArea {uint32_t id=0,first=0,count=0;};
struct PoisonContactPart {uint32_t area=0,first=0,count=0;};
// ENCPCN01 format1/capability1:existing poison metadata,source-constrained
// trigger convex partitions and translated Player footprint. No save changes.
class PoisonContactsData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}const PoisonHazardsData&poison()const;
 const std::vector<Vec2>&body()const;const std::vector<Vec2>&points()const;
 const std::vector<PoisonContactArea>&areas()const;const std::vector<PoisonContactPart>&parts()const;
 uint32_t layer()const;uint32_t mask()const;
private:struct State;std::shared_ptr<const State>state_;
};
struct PoisonBodySample {Vec2 position{};uint32_t layer=0,mask=0;bool enabled=true;};
struct PoisonPlayerContact {uint64_t epoch=0,instance=0;uint32_t area=0;bool entered=false;};
// Source automatic contact pair sampling for one bound Player or Other body.
// Publish previous physics sample before node processing;sample after movement.
// Multiple simultaneous area transitions are outside the audited profile and
// reject without mutation;this does not implement arbitrary broadphase order.
class PoisonPlayerContacts {
public:
 bool prepare(const PoisonContactsData&,uint64_t epoch,uint64_t instance,FieldStepBody,std::string&);
 bool begin(uint64_t,const PoisonBodySample&,bool player_paused,WorldDamageActor&,SessionCharacter&,FieldRunLoop&,FieldStepShadowState*,std::vector<WorldDamageEvent>&,std::string&);
 // A physics owner may supply its complete ordered event stream. Every event
 // must match the pending sampled pair set and this body/epoch exactly. This
 // API consumes that order;it does not infer a broadphase order from geometry.
 bool begin_ordered(uint64_t,const PoisonBodySample&,const std::vector<PoisonPlayerContact>&,bool player_paused,WorldDamageActor&,SessionCharacter&,FieldRunLoop&,FieldStepShadowState*,std::vector<WorldDamageEvent>&,std::string&);
 bool end(uint64_t,const PoisonBodySample&,std::string&);
 bool retire(uint64_t,std::string&);
 bool valid()const{return data_.valid()&&epoch_!=0;}bool sampling()const{return valid()&&pairs_.sampling();}
 bool inside(size_t n)const{return valid()&&pairs_.inside(n);}
 const std::vector<PoisonPlayerContact>&events()const{return events_;}
private:
 bool owner(uint64_t,const PoisonBodySample&,std::string&)const;
 bool begin_impl(uint64_t,const PoisonBodySample&,const std::vector<PoisonPlayerContact>*,bool,WorldDamageActor&,SessionCharacter&,FieldRunLoop&,FieldStepShadowState*,std::vector<WorldDamageEvent>&,std::string&);
 void enabled(const PoisonBodySample&,std::array<uint8_t,128>&)const;
 PoisonContactsData data_;FieldAreaContacts pairs_;uint64_t epoch_=0,last_epoch_=0,instance_=0;FieldStepBody body_=FieldStepBody::Other;
 std::vector<PoisonPlayerContact>events_;
};
}

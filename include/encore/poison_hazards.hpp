#pragma once
#include "encore/field_steps.hpp"
#include <memory>
namespace encore::upstream {
struct PoisonPolicy {
 uint32_t scene=0;std::string source,name,status;
 int32_t area_damage=0,area_variance=0,geyser_damage=0,geyser_variance=0;
 Vec2 hit{},spawn_offset{};
};
struct PoisonHole {uint32_t id=0;std::string path;Vec2 position{};double interval=0,delay=0;};
struct PoisonArea {uint32_t id=0,step=0,layer=0,mask=0,bits=0,first=0,count=0;std::string path;};
struct PoisonShape {uint32_t owner=0,role=0,first=0,count=0,layer=0,mask=0;Vec2 position{};};
// ENCPSN01 f1/cap1: selected contact policies, original polygons and source
// Timer/spawn bindings. No HP, ailment, animation, physics or scene admission.
class PoisonHazardsData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}const PoisonPolicy&policy()const;
 const std::vector<PoisonHole>&holes()const;const std::vector<PoisonArea>&areas()const;
 const std::vector<PoisonShape>&shapes()const;const std::vector<Vec2>&points()const;
 bool matches(FieldStepsView,std::string&)const;
private:struct State;std::shared_ptr<const State>state_;
};
struct PoisonDamageState {
 bool continuous=false;int32_t damage=0,variance=0;
 // Ordered calls to Character.add_status, not implemented Character effects.
 std::vector<std::string>status_requests;
};
enum class PoisonContactKind:uint8_t {Area=1,Geyser=2};
// Manual source callback semantics. For a geyser the area argument is zero;
// source instance/body lifetime and physical delivery belong to the caller.
// An area exit stops continuous damage even when paused or another area still
// overlaps. It does not remove the status or clear remembered damage values.
bool apply_poison_callback(const PoisonHazardsData&,PoisonContactKind,uint32_t area,
 bool entered,FieldStepBody,bool player_paused,PoisonDamageState&,FieldRunLoop&,
 FieldStepShadowState*,std::string&);
struct PoisonHoleClock {double remaining=0;float wait=0;bool paused=false;};
struct PoisonSpawnRequest {uint64_t epoch=0,ticket=0;uint32_t hole=0;Vec2 position{};};
class PoisonHoleTimers {
public:
 bool prepare(const PoisonHazardsData&,uint64_t epoch,std::string&);
 bool pause(uint64_t,uint32_t hole,bool,std::string&);
 bool advance_idle(uint64_t,double delta,bool process_enabled,std::string&);
 bool retire(uint64_t,std::string&);
 bool valid()const{return data_.valid()&&epoch_!=0;}uint64_t epoch()const{return epoch_;}
 bool inspect(uint64_t,uint32_t,PoisonHoleClock&,std::string&)const;
 const std::vector<PoisonSpawnRequest>&requests()const{return requests_;}
 std::vector<PoisonSpawnRequest>take_requests(){auto result=std::move(requests_);requests_.clear();return result;}
private:
 size_t find(uint32_t)const;PoisonHazardsData data_;uint64_t epoch_=0,last_epoch_=0,ticket_=0;
 std::vector<PoisonHoleClock>clocks_;std::vector<PoisonSpawnRequest>requests_;
};
}

#pragma once
#include "encore/session_save.hpp"
#include "encore/source_random.hpp"
#include <array>
#include <memory>
namespace encore::upstream {
struct DamageVector {float x=0,y=0;};
// Field indices describe the schema,not content values. All values and identities
// are loaded from the independent checked resource.
enum DamageNumber:size_t {ShakeMagnitude,ShakeDuration,ShakeX,ShakeY,VibrationWeak,VibrationStrong,VibrationDuration,NumberOffsetX,NumberOffsetY,NumberMin,NumberMax,NumberUp,NumberDown,NumberXDuration,NumberUpDuration,NumberDownDuration,KnockbackForce,KnockbackDecay};
struct WorldDamagePolicy {std::string source,hurt,slot,unconscious;uint32_t flash=0,reset=0,floor=0,device=0,modulus=0,negative=0,flags=0;std::array<double,18>number{};};
struct WorldDamageClip {uint32_t id=0,first=0,count=0;std::string name;float duration=0;};
struct WorldDamageKey {uint32_t target=0,mode=0,type=0;float time=0;std::array<float,4>value{};};
struct WorldDamageStatus {std::string id;uint32_t flags=0,priority=0,plate=0,probability=0;float scroll=0;};
class WorldDamageData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}const WorldDamagePolicy&policy()const;
 const std::vector<WorldDamageClip>&clips()const;const std::vector<WorldDamageKey>&keys()const;
 const std::vector<WorldDamageStatus>&statuses()const;
 const WorldDamageClip*clip(uint32_t)const;const WorldDamageStatus*status(const std::string&)const;
private:struct State;std::shared_ptr<const State>state_;
};
enum class WorldDamageEventKind:uint8_t {Camera=1,Vibration,Flash,Hurt,HpChanged,PlatesShow,PlatesUpdate,FlyingNumber,StatusChanged,GameOver};
struct WorldDamageEvent {WorldDamageEventKind kind{};std::array<double,8>value{};};
struct WorldDamagePose {uint32_t assigned=0;float time=0;bool playing=false;std::array<std::array<float,4>,9>property{};};
enum class WorldDamageLethality:uint8_t {Unspecified=0,Nonlethal=1,Lethal=2};
struct WorldDamageRequest {int64_t damage=0,variance=0;DamageVector hit{};WorldDamageLethality lethality=WorldDamageLethality::Unspecified;};
// Scoped party-member world-damage consumer. It mutates the real session model,
// with explicit source max HP and other-conscious-party facts supplied by the
// owner. It does not implement complete Character,Player,UI/audio/GPU or physics.
// prepare_after_reset is the audited boundary after original RESET has ended.
class WorldDamageActor {
public:
 bool prepare_after_reset(const WorldDamageData&,uint64_t epoch,uint64_t instance,const SessionCharacter&,std::string&);
 bool valid()const{return epoch_!=0;}bool invulnerable()const{return valid()&&pose_.playing&&pose_.assigned==data_.policy().flash;}
 const WorldDamageData&content()const{return data_;}
 const WorldDamageRequest&remembered_request()const{return remembered_;}
 bool validates(uint64_t epoch,uint64_t instance,const SessionCharacter&c,std::string&e)const{return owner(epoch,instance,e)&&member(c,e);}
 const WorldDamagePose&pose()const{return pose_;}bool continuous()const{return continuous_;}DamageVector knockback()const{return knockback_;}
 bool play(uint64_t,uint64_t,uint32_t,std::string&);bool pause_flash(uint64_t,uint64_t,std::string&);bool resume_flash(uint64_t,uint64_t,std::string&);
 bool manual_advance(uint64_t,uint64_t,double,std::string&);bool advance_idle(uint64_t,uint64_t,double,bool process_enabled,std::string&);
 bool add_status(uint64_t,uint64_t,SessionCharacter&,const std::string&,std::vector<WorldDamageEvent>&,std::string&);
 bool remove_status(uint64_t,uint64_t,SessionCharacter&,const std::string&,std::vector<WorldDamageEvent>&,std::string&);
 bool begin_continuous(uint64_t,uint64_t,SessionCharacter&,const WorldDamageRequest&,const std::string&status,std::vector<WorldDamageEvent>&,std::string&);
 bool stop_continuous(uint64_t,uint64_t,std::string&);
 bool damage(uint64_t,uint64_t,SessionCharacter&,int64_t maxhp,bool other_conscious,DamageVector position,const WorldDamageRequest&,SourceRandom&,std::vector<WorldDamageEvent>&,std::string&);
 bool physics_tail(uint64_t,uint64_t,SessionCharacter&,int64_t maxhp,bool other_conscious,bool paused,DamageVector position,SourceRandom&,std::vector<WorldDamageEvent>&,std::string&);
 bool retire(uint64_t,uint64_t,std::string&);
private:
 bool owner(uint64_t,uint64_t,std::string&)const;bool member(const SessionCharacter&,std::string&)const;bool request(const WorldDamageRequest&,bool require_lethality,std::string&)const;
 void add(SessionCharacter&,const std::string&,std::vector<WorldDamageEvent>&)const;
 WorldDamageData data_;WorldDamagePose pose_;std::string character_;uint64_t epoch_=0,last_epoch_=0,instance_=0;bool continuous_=false;WorldDamageRequest remembered_;DamageVector knockback_{};
};
}

#pragma once
#include "encore/movement.hpp"
#include "encore/circle_motion.hpp"
#include "encore/session_save.hpp"
#include "encore/source_random.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <map>
#include <memory>
#include <set>
#include <vector>
namespace encore::upstream {
enum class CoinScalar:uint32_t {MinimumSpeed,MaximumSpeed,AirMultiplier,MinimumHeight,MaximumHeight,Deceleration,SparkleMinimum,SparkleMaximum,SpawnInterval};
struct CoinPolicy {
 std::array<double,9>scalar{};
 float body_radius=0,pickup_radius=0,margin=0,delta=0,sparkle_fps=0,panning=0;
 Vec2 sprite_offset{},sparkle_offset{};
 double value(CoinScalar key)const{return scalar.at(size_t(key));}
};
struct CoinInfo {
 uint32_t cash=0,body_layer=0,body_mask=0,pickup_layer=0,pickup_mask=0,frames=0,initial_frame=0;
 uint32_t flipping=0,grounded=0,collect=0,reset=0,direction_count=0,sparkle_count=0,sparkle_initial=0;
 bool sparkle_playing=false,sparkle_loop=false;uint32_t default_coins=0,pickup_area_bit=0;
 std::string drop_audio,collect_audio,voice,scene,script;
};
enum class CoinProperty:uint32_t {Frame=1,Glow=2};
struct CoinKey {CoinProperty target=CoinProperty::Frame;float time=0;double value=0;uint32_t update=0;};
struct CoinClip {uint32_t id=0;float duration=0;bool loop=false;std::vector<CoinKey>keys;};
class CoinsData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}const CoinPolicy&policy()const{return policy_;}const CoinInfo&info()const{return info_;}
 uint64_t generation()const{return generation_;}
 const std::vector<CoinClip>&clips()const{return clips_;}const CoinClip*clip(uint32_t)const;
 const std::vector<Vec2>&directions()const{return directions_;}
 const std::vector<std::array<float,4>>&atlas()const{return atlas_;}
 const std::array<float,8>&colors()const{return colors_;}
 const std::array<double,2>&initial_modifiers()const{return modifiers_;}
 const std::array<std::string,6>&assets()const{return assets_;}
private:
 bool valid_=false;uint64_t generation_=0;CoinPolicy policy_;CoinInfo info_;
 std::vector<CoinClip>clips_;std::vector<Vec2>directions_;
 std::vector<std::array<float,4>>atlas_;std::array<float,8>colors_{};
 std::array<double,2>modifiers_{};std::array<std::string,6>assets_;
};
enum class CoinBodyKind:uint32_t {Other=1,PartyObject=2};
enum class CoinRequestKind:uint32_t {DropAudio=1,CashHud=2,CollectAudio=3};
struct CoinRequest {CoinRequestKind kind=CoinRequestKind::DropAudio;uint64_t epoch=0,instance=0,frame=0;};
struct CoinParent {uint64_t epoch=0,token=0;Vec2 position{},scale{1,1};float rotation=0;};
// Prepared scene services. Requests describe original effects; a scene sink
// must deliver the actual HUD and voices. cash commits to the real snapshot.
struct CoinServices {
 uint64_t epoch=0,frame=0,physics_step=0;SessionSnapshot*session=nullptr;SourceRandom*random=nullptr;
 bool hud_ready=false;std::set<std::string>sounds;std::set<uint64_t>owners;
 uint64_t next_owner=1,last_assigned_owner=0;std::set<uint64_t>factories;std::map<std::string,CoinParent>parents;
 std::map<uint64_t,CoinBodyKind>bodies;std::map<uint64_t,uint32_t>areas;
 std::vector<CoinRequest>requests;
};
struct CoinSnapshot {
 Vec2 position{},direction{},offset{};double speed=0,air_time=0,glow=0;
 uint32_t frame=0,sparkle=0,animation=0;float animation_time=0;
 bool grounded=false,physics=true,body_disabled=false,pickup_disabled=false,playing=false,queued=false,removed=false;
};
class CoinActor {
public:
 bool prepare(std::shared_ptr<const CoinsData>,uint64_t epoch,uint64_t instance,Vec2,CoinServices&,std::string&);
 // SceneTree idle_frame continuations precede internal animations/tweens.
 // The caller supplies separate original idle and physics clock identities.
 bool advance_idle(uint64_t epoch,double delta,CoinServices&,std::string&);
 bool advance_physics(uint64_t epoch,const CircleMotionSolver&,CoinServices&,std::string&);
 bool body_entered(uint64_t epoch,uint64_t body,CoinServices&,std::string&);
 bool area_entered(uint64_t epoch,uint64_t area,CoinServices&,std::string&);
 bool flush_deferred(uint64_t epoch,CoinServices&,std::string&);
 bool valid()const{return valid_;}uint64_t instance()const{return instance_;}
 const CoinSnapshot&snapshot()const{return state_;}
private:
 friend class CoinFactory;
 bool prepare_ready(std::shared_ptr<const CoinsData>,uint64_t,uint64_t,Vec2,CoinServices&,SourceRandom&,bool,std::string&);
 bool context(uint64_t,const CoinServices&,std::string&)const;
 bool collect(CoinServices&,std::string&);bool sample(float,bool,float,std::string&);
 bool play(uint32_t,std::string&);bool tick(float,SourceRandom&,bool&,std::string&);
 std::shared_ptr<const CoinsData>data_;CoinSnapshot state_;
 SessionSnapshot*session_=nullptr;SourceRandom*random_=nullptr;
 uint64_t epoch_=0,instance_=0,generation_=0,last_idle_=0,last_physics_=0,landed_idle_=0;
 bool valid_=false,idle_started_=false,pending_seek_=false,pending_pickup_=false,pickup_value_=false,pending_body_=false;
 unsigned jump_step_=0;float tween_time_=0,tween_duration_=0,tween_start_=0,tween_target_=0,sparkle_left_=0;
 double jump_height_=0;
};
// The original CoinSpawner timer plus AbstractSpawner's immediate child Ready.
// The scene owner dispatches existing children in source tree order and flushes
// deferred work. This class does not guess an arbitrary scene's process order.
class CoinFactory {
public:
 bool prepare(std::shared_ptr<const CoinsData>,uint64_t epoch,uint64_t instance,Vec2,CoinServices&,std::string&);
 bool set_position(uint64_t epoch,Vec2,CoinServices&,std::string&);
 bool spawn(uint64_t epoch,uint32_t amount,CoinServices&,std::string&);
 bool spawn_default(uint64_t epoch,CoinServices&s,std::string&e){return spawn(epoch,remaining_,s,e);}
 bool advance_physics(uint64_t epoch,CoinServices&,std::string&);
 bool retire_removed(uint64_t epoch,uint64_t instance,CoinServices&,std::string&);
 bool leave(uint64_t epoch,CoinServices&,std::string&);
 bool valid()const{return valid_;}bool spawning()const{return spawning_;}
 uint32_t remaining()const{return remaining_;}double timer()const{return timer_;}
 Vec2 position()const{return position_;}
 const std::map<uint64_t,CoinActor>&instances()const{return instances_;}
 CoinActor*actor(uint64_t id){auto found=instances_.find(id);return found==instances_.end()?nullptr:&found->second;}
private:
 friend class CoinSceneFactories;
 bool context(uint64_t,const CoinServices&,std::string&)const;
 std::shared_ptr<const CoinsData>data_;SessionSnapshot*session_=nullptr;SourceRandom*random_=nullptr;
 uint64_t epoch_=0,instance_=0,generation_=0,parent_=0,last_physics_=0;
 Vec2 position_{};uint32_t remaining_=0;double timer_=0;bool valid_=false,spawning_=false;
 std::map<uint64_t,CoinActor>instances_;
};
}

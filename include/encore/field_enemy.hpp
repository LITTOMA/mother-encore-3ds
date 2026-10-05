#pragma once
#include "encore/source_random.hpp"
#include <cstdint>
#include <array>
#include <functional>
#include <string>
#include <vector>
namespace encore::upstream {
struct FieldEnemyVector {float x=0,y=0;};
enum class FieldEnemyParameter:uint32_t {AppearanceModulus=1,WanderAxisModulus,ReadyDirectionMin,ReadyDirectionMax,ReadyTimerMin,WanderTimerDeviation,MinMovementLength,MinDisinterestTime,Knockback,KnockbackDeceleration,UnderlevelGap,ExplosionBase,ExplosionModulus,IncapacitatedDivisor,UnderlevelFlashLength,UnderlevelFlashInterval,UnderlevelFlashDelay,DamageFlashLength,StunLength,BashPower,BashVariance,AdvantageEnemy,AdvantageNeutral,AdvantagePlayer,SafeMargin};
struct FieldEnemyProfile {
 uint32_t id=0;std::string enemy,source,sprite,animation;
 int32_t level=0,hp=0,max_hp=0,pp=0,max_pp=0,defense=0,experience=0,cash=0;
 FieldEnemyVector sprite_offset;bool shadow=true,returning=true;
 float max_distance=0,max_speed=0,acceleration=0,friction=0,walk_frequency=0,wander_radius=0,chase_delay=0,return_delay=0;
};
struct FieldEnemySpawner {
 // flags: effective Ready returning, perma-death, source direction-property guard.
 uint32_t id=0,ready_ordinal=0,profile=0,flags=0,appearance_rate=0;std::string node;
 FieldEnemyVector position,initial_direction;float cooldown=0;float visibility_rect[4]{};
};
enum class FieldEnemyAnimation:uint32_t {None=0,Flash=1,Stun=2,Reset=3};
struct FieldEnemyAnimationBinding {FieldEnemyAnimation kind=FieldEnemyAnimation::None;std::string name,source,node,signal,handler;float duration=0;};
struct FieldEnemySource {std::string path;uint8_t sha256[32]{};};
struct FieldEnemyGeometry {uint32_t id=0,role=0,ordinal=0,mask=0,layer=0,flags=0;FieldEnemyVector offset;float rotation=0;FieldEnemyVector value;};
class FieldEnemyData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}uint32_t scene_id()const{return scene_id_;}uint32_t capabilities()const{return capabilities_;}
 const std::array<uint8_t,20>& source_pin()const{return source_pin_;}
 float parameter(FieldEnemyParameter p)const;
 const std::vector<FieldEnemyProfile>& profiles()const{return profiles_;}
 const std::vector<FieldEnemySpawner>& spawners()const{return spawners_;}
 const std::vector<FieldEnemySource>& sources()const{return sources_;}
 const std::vector<FieldEnemyGeometry>& geometry()const{return geometry_;}
 const std::vector<FieldEnemyAnimationBinding>& animations()const{return animations_;}
 const FieldEnemyAnimationBinding* animation(FieldEnemyAnimation)const;
private:
 bool valid_=false;uint32_t scene_id_=0,capabilities_=0;std::array<uint8_t,20>source_pin_{};std::vector<float>parameters_;std::vector<FieldEnemyProfile>profiles_;std::vector<FieldEnemySpawner>spawners_;std::vector<FieldEnemySource>sources_;std::vector<FieldEnemyGeometry>geometry_;std::vector<FieldEnemyAnimationBinding>animations_;
};
enum class FieldEnemyState:uint8_t {Wander,Return,Chase,Stunned};
enum class FieldEnemyPresentation:uint8_t {Idle,Walk,Jump,Exclamation,BlueExclamation,DamageFlash,Stun,UnderlevelKnockout,Restore,AnimationEnabled,AnimationDisabled,Erase};
enum class FieldEnemyRoster:uint8_t {Add,Remove,MoveToFront,AddToFront};
enum class FieldEnemyHurt:uint8_t {Explosion,Bat,Fire,StunGun};
struct FieldEnemyContext {
 bool paused=false,entering_door=false,cutscene=false,battle_queued=false,in_battle=false,can_pause=false,running=false,substantial_movement=false,player_can_see_enemy=false,event_ray_on_player=false,emote_playing=false;
 uint32_t highest_party_level=0;int32_t leader_offense=0;bool leader_incapacitated=false;
 FieldEnemyVector player_ground,party_midpoint,player_direction;
};
struct FieldEnemyInstance {
 uint64_t id=0,animation_generation=0;FieldEnemyAnimation current_animation=FieldEnemyAnimation::None;std::vector<uint64_t> damage_waiters;uint32_t spawner=0,profile=0;FieldEnemyState state=FieldEnemyState::Wander;
 FieldEnemyVector position,start_position,new_position,input,direction,velocity,knockback;
 int32_t hp=0,pp=0;float wander_time=0,return_time=0,chase_time=0,disinterest_time=0,flash_time=0;
 bool returning=true,seeing=false,blind=false,underlevel=false,drafted=false,on_screen=false,physics=false,visible=true,contact_enabled=true,wander_timer_running=false,roster_member=false,await_unpaused_contact=false,pending_damage=false,underlevel_flash=false,animation_running=false,queued_free=false,erased=false;
};
// Host operations are exact native adapters. Missing operations reject admission;
// collision, visibility, battle ordering and presentation may never be assumed.
struct FieldEnemyHost {
 std::function<bool(uint64_t,FieldEnemyContext&,std::string&)> context;
 std::function<bool(uint64_t,const FieldEnemyProfile&,const std::vector<FieldEnemyGeometry>&,FieldEnemyVector,std::string&)> create;
 // Set the source RayCast2D.cast_to, then return its current cached collider.
 // Basic Enemy does not call force_raycast_update inside choose_movement.
 std::function<bool(uint64_t,FieldEnemyVector,bool&,std::string&)> wander_raycast;
 std::function<bool(uint64_t,FieldEnemyVector,float,FieldEnemyVector&,FieldEnemyVector&,std::string&)> move_and_slide;
 std::function<bool(uint64_t,FieldEnemyRoster,const FieldEnemyInstance&,std::string&)> roster;
 std::function<bool(uint64_t,int32_t,const FieldEnemyInstance&,std::string&)> battle;
 // Erase emits source enemy_erased and queues deletion; the host must retain
 // the object until end_frame, including repeated same-frame Erase events.
 std::function<bool(uint64_t,FieldEnemyPresentation,const FieldEnemyInstance&,std::string&)> present;
 // Stop the old DamageAnimation and play this checked source clip. The host
 // dispatches animation_finished only after real idle-clock playback completes,
 // carrying its generation; interrupted clips do not emit a completion.
 std::function<bool(uint64_t,const FieldEnemyAnimationBinding&,uint64_t,const FieldEnemyInstance&,std::string&)> animation;
 std::function<bool(bool,std::string&)> queue_battle;
 std::function<bool(uint64_t,int32_t,std::string&)> damage_number;
 std::function<bool(int32_t,int32_t,std::string&)> reward;
};
class FieldEnemyRuntime {
public:
 bool initialize(const FieldEnemyData*,SourceRandom*,FieldEnemyHost,std::string&);
 bool ready(uint32_t spawner_id);bool spawner_screen_entered(uint32_t spawner_id);bool spawner_tree_exited(uint32_t spawner_id);bool end_frame();
 bool tree_exiting(uint64_t,bool changing_parents=false);bool screen_entered(uint64_t);bool screen_exited(uint64_t);bool view_entered(uint64_t,bool player);bool view_exited(uint64_t,bool player);bool blind_entered(uint64_t,bool player);bool blind_exited(uint64_t,bool player);
 // Connected source handler runs before Hurt coroutine subscriptions. Tree
 // exit cancels subscriptions; queue_free is deferred to end_frame.
 bool animation_finished(uint64_t,FieldEnemyAnimation,uint64_t generation);
 bool physics_step(uint64_t,float);bool idle_step(float);bool wander_timeout(uint64_t);bool contact(uint64_t,bool party_object);bool hurt(uint64_t,FieldEnemyHurt);bool stun(uint64_t);bool die(uint64_t,bool in_battle=true);bool activate(uint64_t);
 const std::vector<FieldEnemyInstance>& enemies()const{return enemies_;}const std::string& error()const{return error_;}
private:
 struct SpawnerState {bool ready=false,alive=true,queued_free=false;uint64_t attached=0;float cooldown=0;};
 const FieldEnemyData*data_=nullptr;SourceRandom*random_=nullptr;FieldEnemyHost host_;std::vector<SpawnerState>spawners_;std::vector<FieldEnemyInstance>enemies_;uint64_t next_id_=1,next_waiter_=1;std::string error_;
 bool fail(const char*);size_t spawner_index(uint32_t)const;FieldEnemyInstance*enemy(uint64_t);bool context(uint64_t,FieldEnemyContext&);float parameter(FieldEnemyParameter)const;
 bool choose(FieldEnemyInstance&);bool start_wander(FieldEnemyInstance&);bool start_chase(FieldEnemyInstance&,const FieldEnemyContext&);bool chase_stop(FieldEnemyInstance&);bool start_battle(FieldEnemyInstance&,int32_t);bool finish_damage(FieldEnemyInstance&);bool play_animation(FieldEnemyInstance&,FieldEnemyAnimation);bool present(FieldEnemyInstance&,FieldEnemyPresentation);
};
}

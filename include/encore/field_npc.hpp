#pragma once
#include "encore/source_random.hpp"
#include "encore/movement.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
namespace encore::upstream {
enum class FieldNpcParameter:uint32_t {InitialDownX=1,InitialDownY,ReturnDelay,ReadyTimerMin,TimerDeviation,AxisModulus,MinimumMove,MotionThreshold,LookThreshold,ExtentVertical,ExtentHorizontal,MovementDifferenceFloor};
enum class FieldNpcFlag:uint32_t {TurnInteract=1,TurnTelepathy=2,NoProblemThoughts=4,AutomaticShadow=8,NoShadow=16,NoCollision=32,Staring=64,Wander=128,Debug=256,PlayerTurnX=512,PlayerTurnY=1024,PartySprite=2048};
struct FieldNpcDialogue {bool thoughts=false,last=false;uint32_t group=0,ordinal=0;std::string flag,program,source;};
struct FieldNpcEventPosition {std::string flag;Vec2 position;};
struct FieldNpcConnection {std::string from,to;uint32_t mode=0;};
struct FieldNpcKey {float time=0;uint32_t frame=0;};
struct FieldNpcDirection {Vec2 vector;float duration=0;std::vector<FieldNpcKey>keys;};
struct FieldNpcMotion {std::string name;bool loop=false;std::vector<FieldNpcDirection>directions;};
struct FieldNpcGeometry {uint32_t role=0,kind=0,layer=0,mask=0;Vec2 offset,value,scale;float rotation=0;};
struct FieldNpcDescriptor {
 uint32_t id=0,ready_ordinal=0,flags=0,extended_interact=0,columns=0,rows=0,width=0,height=0;
 std::string node,sprite,animation,texture,appear,disappear,idle,talk_idle,walk,talk;
 Vec2 position,direction,sprite_offset;float speed=0,walk_frequency=0,wander_radius=0,timer_wait=0,safe_margin=0;
 std::vector<FieldNpcDialogue>dialogues;std::vector<FieldNpcEventPosition>event_positions;std::vector<FieldNpcConnection>connections;std::vector<FieldNpcMotion>motions;std::vector<FieldNpcGeometry>geometry;
 bool has(FieldNpcFlag f)const{return (flags&uint32_t(f))!=0;}
};
struct FieldNpcSource {std::string path;std::array<uint8_t,32>sha256{};};
class FieldNpcData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}uint32_t scene_id()const{return scene_;}const std::array<uint8_t,20>&source_pin()const{return pin_;}
 const std::vector<FieldNpcDescriptor>&npcs()const{return npcs_;}const std::vector<FieldNpcSource>&sources()const{return sources_;}float parameter(FieldNpcParameter)const;
private:
 bool valid_=false;uint32_t scene_=0;std::array<uint8_t,20>pin_{};std::vector<float>parameters_;std::vector<FieldNpcDescriptor>npcs_;std::vector<FieldNpcSource>sources_;
};
struct FieldNpcContext {Vec2 player;bool player_paused=false,in_battle=false,cutscene=false,stack_empty=true,current_talker=false,ancestor_visible=true,debug_build=false;};
struct FieldNpcInstance {
 uint32_t id=0,index=0,current_motion=UINT32_MAX,pending_motion=UINT32_MAX,frame=0,direction_index=0;Vec2 position,start_position,new_position,input,blend,velocity;
 double animation_time=0;bool ready=false,visible=true,physics=false,collision_enabled=false,interact_enabled=false,looking=false,player_nearby=false,pause_for_interact=false,mute=false,talking=false,wander_running=false,queued_free=false,destroyed=false;
 std::vector<uint64_t>return_waiters;std::vector<FieldNpcGeometry>geometry;
};
enum class FieldNpcTimer:uint8_t {SetWanderWait,StartWander,StopWander,ReturnDirection};
enum class FieldNpcPresentation:uint8_t {Pose,Visibility,Blend,Motion,Frame,Create,QueueFree};
struct FieldNpcHost {
 std::function<bool(uint32_t,FieldNpcContext&,std::string&)>context;
 std::function<bool(const std::string&,bool&,std::string&)>flag;
 // Stable identity is the typed equivalent of source get_path: flag:j:program.
 std::function<bool(uint32_t,const FieldNpcDialogue&,bool&,std::string&)>seen;
 std::function<bool(uint32_t,const FieldNpcDialogue&,std::string&)>mark_seen;
 // Admission uses the existing checked Room Programme, never a second VM.
 std::function<bool(const std::string&,std::string&)>admit_program;
 std::function<bool(uint32_t,const std::string&,bool,const FieldNpcDescriptor&,std::string&)>open_program;
 std::function<bool(uint32_t,bool,std::string&)>telepathy_effect;
 std::function<bool(uint32_t,std::string&)>begin_talker;
 std::function<bool(std::string&)>close_commands;
 std::function<bool(uint32_t,Vec2,bool&,std::string&)>cached_raycast;
 std::function<bool(uint32_t,Vec2,float,Vec2&,Vec2&,std::string&)>move_and_slide;
 std::function<bool(uint32_t,FieldNpcPresentation,const FieldNpcDescriptor&,const FieldNpcInstance&,std::string&)>present;
 // SetWanderWait changes wait_time without restarting time_left; Start/Stop
 // match the native Timer. ReturnDirection creates a separate SceneTreeTimer.
 std::function<bool(uint32_t,FieldNpcTimer,double,uint64_t,std::string&)>timer;
};
class FieldNpcRuntime {
public:
 const FieldNpcData*data()const{return data_;}
 bool initialize(const FieldNpcData*,SourceRandom*,FieldNpcHost,std::string&);
 bool ready(uint32_t);bool recheck_flags(uint32_t);bool visibility_changed(uint32_t);bool screen_entered(uint32_t);bool screen_exited(uint32_t);
 bool has_dialog(uint32_t,bool thoughts,bool&);bool interact(uint32_t);bool telepathy(uint32_t);bool stop_interaction(uint32_t);
 bool view_entered(uint32_t,bool player);bool view_exited(uint32_t,bool player);bool near_entered(uint32_t,bool party_player);bool near_exited(uint32_t,bool party_player);
 bool physics_step(uint32_t,float);bool idle_animations(uint32_t,double);bool wander_timeout(uint32_t);bool return_direction_timeout(uint32_t,uint64_t);
 bool set_talking(uint32_t,bool);bool set_mute(uint32_t,bool);bool set_direction(uint32_t,Vec2);bool teleport(uint32_t,Vec2);bool tree_exiting(uint32_t,bool persistent);bool destroy(uint32_t);
 const std::vector<FieldNpcInstance>&npcs()const{return npcs_;}const std::string&error()const{return error_;}
private:
 const FieldNpcData*data_=nullptr;SourceRandom*random_=nullptr;FieldNpcHost host_;std::vector<FieldNpcInstance>npcs_;uint64_t next_waiter_=1;uint32_t last_ready_=0;bool had_ready_=false;std::string error_;
 bool fail(const char*);FieldNpcInstance*instance(uint32_t);const FieldNpcDescriptor&descriptor(const FieldNpcInstance&)const;bool context(uint32_t,FieldNpcContext&);bool flag(const std::string&,bool&);bool appear(const FieldNpcDescriptor&,bool&);bool select(FieldNpcInstance&,bool,const FieldNpcDialogue*&);bool investigate(uint32_t,bool);bool move(FieldNpcInstance&);bool motion(FieldNpcInstance&,const std::string&);bool present(FieldNpcInstance&,FieldNpcPresentation);bool return_timer(FieldNpcInstance&);float parameter(FieldNpcParameter)const;
};
}

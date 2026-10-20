#pragma once
#include "encore/dungeon_switches.hpp"
namespace encore::upstream {
struct DungeonSwitchFlag {uint32_t id=0;bool value=false;};
struct DungeonSwitchHandle {uint32_t id=0;uint64_t generation=0;};
struct DungeonSwitchToken {uint64_t value=0,generation=0;};
enum class DungeonSwitchAnimation:uint8_t {None=0,TurnOff=1,TurnOn=2,Off=3,On=4,StayOff=5,StayOn=6};
struct DungeonSwitchState {bool on=false,disabled=false,ready=false;DungeonSwitchAnimation animation=DungeonSwitchAnimation::None;};
struct DungeonControlledState {bool on=false;DungeonSwitchAnimation animation=DungeonSwitchAnimation::None;int32_t order=0;uint32_t registrations=0;};
enum class DungeonSwitchMethod:uint8_t {Interact=1,Hit=2,PlayerEntered=3};
enum class DungeonSwitchEventKind:uint8_t {ReadyTimer=1,SwitchAnimation=2,ObjectAnimation=3,SwitchAudio=4,SwitchHit=5,DeniedAudio=6,FlagChanged=7,FlagsUpdated=8,Pause=9,Unpause=10,Cutscene=11,MusicFade=12,MusicResume=13,CameraMove=14,CameraReturn=15,ObjectStateChanged=16,WallTimer=17};
struct DungeonSwitchEvent {
 DungeonSwitchEventKind kind;DungeonSwitchHandle target;DungeonSwitchToken token;
 bool value=false,silent=false;float x=0,y=0,duration=0;
 DungeonSwitchAnimation animation=DungeonSwitchAnimation::None;DungeonSwitchState switch_snapshot;
};
// Source methods only. All timers, animation clocks, player/UI/audio and camera
// are explicit consumers of ordered requests. This cannot activate a scene.
class DungeonSwitchRuntime {
public:
 bool prepare(DungeonSwitchesView,uint32_t scene,std::string_view region,const std::vector<DungeonSwitchFlag>&,std::string&);
 bool ready()const{return data_.valid()&&generation_!=0;}uint64_t generation()const{return generation_;}
 DungeonSwitchHandle handle(uint32_t)const;
 bool switch_state(DungeonSwitchHandle,DungeonSwitchState&,std::string&)const;
 bool object_state(DungeonSwitchHandle,DungeonControlledState&,std::string&)const;
 bool operate(DungeonSwitchHandle,DungeonSwitchMethod,bool player_contact,bool player_paused,bool can_switch,std::string&);
 bool timer_finished(DungeonSwitchToken,std::string&);
 bool object_animation_finished(DungeonSwitchHandle,std::string&);
 bool camera_finished(DungeonSwitchToken,std::string&);
 bool idle_frame(std::string&);bool leave_area(bool region_changed,std::string&);
 const std::vector<DungeonSwitchEvent>&events()const{return events_;}void clear_events(){events_.clear();}
private:
 enum class Phase:uint8_t {Waiting,FocusCamera,Idle,ReturnCamera};
 struct Timer {uint64_t token=0;uint32_t index=0;bool wall=false;};
 struct Waiter {uint32_t object=0;uint64_t operation=0;bool on=false,silent=false;};
 struct Operation {uint64_t token=0;uint32_t source=0;Phase phase=Phase::Waiting;std::vector<uint32_t>waiting;};
 uint32_t find_switch(uint32_t)const;uint32_t find_object(uint32_t)const;bool object_on(uint32_t)const;
 bool validate(DungeonSwitchHandle,bool object,uint32_t&,std::string&)const;
 bool emit(DungeonSwitchEventKind,uint32_t id,DungeonSwitchToken,bool value,bool silent,float x,float y,float duration,DungeonSwitchAnimation,std::string&);
 bool emit(DungeonSwitchEventKind,uint32_t id,std::string&);
 bool timer(uint32_t index,bool wall,std::string&);bool flag(uint32_t,bool,std::string&);
 bool request_object(uint32_t object,uint64_t operation,bool silent,std::string&);
 bool begin_control(uint32_t,std::string&);bool settle(std::string&);bool finish_operation(size_t,std::string&);
 DungeonSwitchesView data_;uint64_t generation_=0,next_token_=0;
 std::vector<DungeonSwitchState>switches_;std::vector<DungeonSwitchAnimation>animations_;
 std::vector<Timer>timers_;std::vector<Waiter>waiters_;std::vector<Operation>operations_;
 std::vector<DungeonSwitchEvent>events_;
};
}

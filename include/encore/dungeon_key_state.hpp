#pragma once
#include "encore/dungeon_keys.hpp"
namespace encore::upstream {
struct DungeonKeyFlagValue {uint32_t id=0;bool value=false;};
struct DungeonKeyHandle {uint32_t id=0;uint64_t generation=0;};
enum class DungeonLockAnimation:uint8_t {None=0,Opening=1,Opened=2};
struct DungeonKeyState {
 bool alive=false,queued=false,visible=true,collision_disabled=false,prompt_enabled=false,opened=false;
 DungeonLockAnimation requested_animation=DungeonLockAnimation::None;
};
enum class DungeonKeyEventKind:uint8_t {
 CounterChanged=1,FlagChanged=2,FlagsUpdated=3,IndicatorRequested=4,
 PickupAudioRequested=5,Hidden=6,CollisionDisableDeferred=7,CollisionDisabled=8,
 DeleteQueued=9,Deleted=10,LockAnimationRequested=11,PromptDisabled=12,LockedDialogue=13
};
struct DungeonKeyEvent {
 DungeonKeyEventKind kind;DungeonKeyHandle target;int32_t counter=0;
 bool flag=false;DungeonKeyState state;
};
// Pure original-method state component. Audio occupancy is an explicit service
// input; presentation/UI/flag observers consume ordered requests and snapshots.
// This class cannot admit a map, audio stream, animation timeline or save.
class DungeonKeyRuntime {
public:
 bool prepare(DungeonKeysView,uint32_t scene,std::string_view region,int32_t counter,
              const std::vector<DungeonKeyFlagValue>&,std::string&);
 bool ready()const{return data_.valid()&&generation_!=0;}
 DungeonKeysView content()const{return data_;}uint64_t generation()const{return generation_;}
 int32_t counter()const{return counter_;}
 DungeonKeyHandle handle(uint32_t id)const;
 bool state(DungeonKeyHandle,DungeonKeyState&,std::string&)const;
 bool flag(DungeonKeyHandle,bool&,std::string&)const;
 bool pickup_body_entered(DungeonKeyHandle,bool is_player,bool paused,bool audio_playing,std::string&);
 bool pickup_finished(DungeonKeyHandle,std::string&);
 bool lock_interact(DungeonKeyHandle,std::string&);
 bool lock_open_method(DungeonKeyHandle,std::string&);
 bool end_scene_frame(std::string&);
 const std::vector<DungeonKeyEvent>&events()const{return events_;}void clear_events(){events_.clear();}
private:
 uint32_t find(uint32_t)const;
 bool validate(DungeonKeyHandle,uint32_t&,std::string&)const;
 bool live(DungeonKeyHandle,DungeonKeyKind,uint32_t&,std::string&)const;
 bool emit(DungeonKeyEventKind,uint32_t,std::string&);
 bool write_flag(uint32_t,std::string&);
 bool queue_delete(uint32_t,std::string&);
 bool open(uint32_t,std::string&);
 DungeonKeysView data_;uint64_t generation_=0;int32_t counter_=0;
 std::vector<DungeonKeyState>states_;std::vector<bool>flags_,disable_pending_;
 std::vector<DungeonKeyEvent>events_;
};
}

#pragma once
#include "encore/dungeon_mechanism_bindings.hpp"
#include "encore/dungeon_switch_state.hpp"
#include "encore/dungeon_wall_state.hpp"
namespace encore::upstream {
enum class DungeonMechanismEventKind:uint32_t {Request=1,AnimationFinished=2,WallAudio=3,WallCamera=4};
struct DungeonMechanismEvent {
 DungeonMechanismEventKind kind=DungeonMechanismEventKind::Request;
 uint32_t owner=0,clip=0;uint64_t epoch=0,callback_ticket=0;
 DungeonSwitchEvent request{};bool playing=false;double duration=0;
};
struct DungeonMechanismCallback {uint64_t epoch=0,ticket=0;};
struct DungeonMechanismPose {uint32_t owner=0,clip=UINT32_MAX;PropPose pose;double position=0;bool playing=false,on=false;};
// Source graph methods and source-ordered animation ownership. Requests still
// require prepared scene/audio/camera consumers; this class activates no Room.
class DungeonMechanismState {
public:
 bool prepare(const DungeonMechanismBindingsData&,uint64_t epoch,const std::vector<DungeonSwitchFlag>&,std::string&);
 bool valid()const{return data_.valid()&&epoch_!=0;}uint64_t epoch()const{return epoch_;}
 bool paused()const{return paused_;}bool cutscene()const{return cutscene_;}
 size_t event_count()const{return events_.size();}size_t pending_callbacks()const{return callbacks_.size();}
 bool operate(uint32_t,DungeonSwitchMethod,bool player_contact,bool external_paused,bool can_switch,std::string&);
 bool advance(double,std::string&);bool flush_deferred(std::string&);bool idle_frame(std::string&);
 bool timer_finished(DungeonMechanismCallback,std::string&);bool camera_finished(DungeonMechanismCallback,std::string&);
 bool leave_area(bool region_changed,std::string&);
 bool inspect(uint32_t,DungeonMechanismPose&,std::string&)const;
 std::vector<DungeonMechanismEvent>take_events(){auto v=std::move(events_);events_.clear();return v;}
 const DungeonMechanismBindingsData&data()const{return data_;}
private:
 struct Player {PropClipPlayer properties;DungeonWallState wall;};
 struct Callback {uint64_t ticket=0;DungeonSwitchToken token;bool camera=false;};
 size_t find(uint32_t)const;bool apply_requests(std::string&);bool emit(DungeonMechanismEvent,std::string&);
 bool finish(size_t,uint32_t,std::string&);bool wall_events(size_t,std::string&);
 bool advance_owned(double,std::string&);
 bool complete_callback(DungeonMechanismCallback,bool camera,std::string&);
 DungeonMechanismBindingsData data_;uint64_t epoch_=0;bool paused_=false,cutscene_=false;
 DungeonSwitchRuntime graph_;std::vector<Player>players_;std::vector<DungeonMechanismEvent>events_;
 uint64_t next_ticket_=0;std::vector<Callback>callbacks_;
};
}

#pragma once
#include "encore/dungeon_wall_timeline.hpp"
namespace encore::upstream {
enum class DungeonWallEventKind:uint32_t {AudioPlaying=1,CameraShake=2,Finished=3};
struct DungeonWallEvent {
 DungeonWallEventKind kind=DungeonWallEventKind::AudioPlaying;
 uint32_t owner=0,clip=0;uint64_t generation=0;bool playing=false;double duration=0;
};
// Typed command state only. The scene owns actual audio/camera/flag consumers
// and validates the instance generation before executing a command.
class DungeonWallState {
public:
 bool initialize(const DungeonWallTimelineData&,uint64_t generation,std::string&);
 bool play(uint32_t clip,std::string&);bool play_role(DungeonWallRole,std::string&);
 // Explicit native AnimationPlayer.advance semantics, including property
 // sampling after completion. Automatic idle processing uses process instead.
 bool advance_manual(double,std::string&);bool process(double,std::string&);
 bool flush_deferred(std::string&);
 bool valid()const{return valid_;}bool playing()const{return player_.playing();}
 uint32_t assigned_clip()const{return player_.assigned_clip();}double position()const{return player_.position();}
 const PropPose&pose()const{return player_.pose();}bool audio_request()const{return audio_request_;}
 size_t pending_count()const{return pending_.size();}
 std::vector<DungeonWallEvent>take_events(){auto v=std::move(events_);events_.clear();return v;}
 const DungeonWallTimelineData&data()const{return data_;}
private:
 bool valid_=false,audio_request_=false;uint64_t generation_=0;
 DungeonWallTimelineData data_;PropClipPlayer player_;
 std::vector<DungeonWallEvent>events_,pending_;
};
}

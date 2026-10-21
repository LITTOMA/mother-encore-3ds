#pragma once
#include "encore/movement.hpp"
#include <memory>
#include <string>
#include <vector>
namespace encore::upstream {
struct GeyserPolicy {
 std::string source,script,slot,bus;uint32_t spewing=0,reset=0,frames=0,initial_frame=0,layer=0,mask=0,hframes=0,vframes=0;
 bool monitoring=false,disabled=false,monitorable=false,centered=false;
 Vec2 root{},sprite{},offset{},size{},shape{},extents{};
 float rotation=0,max_distance=0,attenuation=0,pitch=0,normal_gain=0,muted_gain=0,notify[4]{},panning=0;
};
struct GeyserClip {uint32_t id=0,first=0,count=0,cue_first=0,cue_count=0;float duration=0;};
struct GeyserKey {float time=0;uint32_t kind=0,value=0,constant=0;};
struct GeyserCue {float time=0,start=0,end=0;uint32_t kind=0,sound=0,track=0;};
struct GeyserSound {uint32_t id=0,role=0;float length=0;std::string source;};
// ENCGYS01 f1/cap1. No full scene,physics,Character,NDSP or GPU admission.
class GeyserTimelineData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return bool(state_);}const GeyserPolicy&policy()const;
 bool same_owner(const GeyserTimelineData&other)const{return valid()&&state_==other.state_;}
 const std::vector<GeyserClip>&clips()const;const std::vector<GeyserKey>&keys()const;
 const std::vector<GeyserCue>&cues()const;const std::vector<GeyserSound>&sounds()const;
 const GeyserClip*clip(uint32_t)const;const GeyserSound*sound(uint32_t)const;
private:struct State;std::shared_ptr<const State>state_;
};
enum class GeyserCommandKind:uint8_t {SetStream=1,Play=2,Stop=3,GlobalRumble=4};
struct GeyserCommand {GeyserCommandKind kind{};uint32_t sound=0;float offset=0;};
enum class GeyserNamedVoice:uint8_t {Absent=1,Idle=2,Playing=3};
struct GeyserContext {bool on_screen=false,battle=false,cutscene=false,game_over=false;};
struct GeyserSnapshot {
 uint32_t clip=0,frame=0,pending_rumbles=0;float time=0,gain=0;
 bool monitoring=false,disabled=false,active=true,stream_paused=false,playing=true,queued_free=false;
 // AnimationPlayer cache,not AudioStreamPlayer2D.playing/natural completion.
 bool audio_cached=false;uint32_t audio_sound=0;float audio_start=0,audio_length=0;
};
class GeyserTimeline {
public:
 bool prepare(const GeyserTimelineData&,uint64_t epoch,uint64_t instance,std::string&);
 bool manual_advance(uint64_t,uint64_t,double,std::string&);
 bool advance_idle(uint64_t,uint64_t,double,bool process_enabled,std::string&);
 bool player_signal(uint64_t,uint64_t,bool paused,std::string&);
 // Native AudioStreamPlayer2D notifications are assignments,not a combined
 // player/tree pause mask. UNPAUSED resets the stream flag even if the source
 // Geyser animation remains inactive after a separate player pause signal.
 bool tree_notification(uint64_t,uint64_t,bool paused,bool audio_can_process,std::string&);
 bool process_ui(uint64_t,uint64_t,GeyserContext,std::string&);
 // Resolve source deferred call against CURRENT visibility/UI/global voice.
 // A queued-free actor remains callable until its explicit deferred retirement.
 bool flush_deferred(uint64_t,uint64_t,GeyserContext,GeyserNamedVoice&,std::string&);
 bool retire(uint64_t,uint64_t,std::string&);
 bool inspect(uint64_t,uint64_t,GeyserSnapshot&,std::string&)const;
 const std::vector<GeyserCommand>&commands()const{return commands_;}
 std::vector<GeyserCommand>take_commands(){auto out=std::move(commands_);commands_.clear();return out;}
 bool valid()const{return data_.valid()&&epoch_&&instance_;}
 const GeyserTimelineData&content()const{return data_;}
private:
 bool owner(uint64_t,uint64_t,std::string&)const;
 GeyserTimelineData data_;GeyserSnapshot pose_;uint64_t epoch_=0,instance_=0,last_epoch_=0;
 std::vector<GeyserCommand>commands_;
};
}

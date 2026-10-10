#pragma once
#include "encore/present_data.hpp"
#include "encore/source_random.hpp"
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
// Validators are read-only and run before any present executes. HouseRuntime
// owns text and flags; the platform owns key-item grants and audio.
class PresentHost {
public:
    virtual ~PresentHost()=default;
    virtual bool validate_text(uint32_t dialogue_id,std::string_view source_path,std::string&)=0;
    virtual bool validate_flag(std::string_view,std::string&)=0;
    virtual bool validate_item(PresentTemplate,std::string_view item,std::string&)=0;
    virtual bool validate_sound(std::string_view,std::string&)=0;
    virtual bool show_text(uint32_t dialogue_id,std::string&)=0;
    virtual bool flag(std::string_view,bool&,std::string&)=0;
    virtual bool set_flag(std::string_view,bool,std::string&)=0;
    virtual bool grant_item(PresentTemplate,std::string_view item,std::string&)=0;
    virtual bool play_sound(std::string_view,std::string&)=0;
};
// Platform-owned effects behind HouseRuntime's PresentHost: inventory and audio.
class PresentEffects {
public:
    virtual ~PresentEffects()=default;
    virtual bool validate_item(PresentTemplate,std::string_view item,std::string&)=0;
    virtual bool validate_sound(std::string_view,std::string&)=0;
    virtual bool grant_item(PresentTemplate,std::string_view item,std::string&)=0;
    virtual bool play_sound(std::string_view,std::string&)=0;
};
struct PresentPose {
    uint32_t texture=kPresentNone,frame=0;Vec2 position{};
    bool sparkles=false;uint32_t sparkle_texture=kPresentNone;PresentRegion sparkle_region{};Vec2 sparkle_position{};
};
// The Present's own AudioStreamPlayer: Play restarts it, Stop ends it.
enum class PresentAudioKind:uint8_t {Play,Stop};
struct PresentAudioRequest {PresentAudioKind kind=PresentAudioKind::Stop;uint32_t object=kPresentNone;std::string_view sound;};
enum class PresentProgramState:uint8_t {Idle,WaitingText,Complete,Failed};

class PresentRuntime {
public:
    // Opened state comes from the source flag (object flags are never set in
    // the supported session scope). Consumes no RNG.
    bool initialize(PresentView,PresentHost&,std::string&);
    // Each Sparkles _ready: frame = int(rand_range(0, 47)) in scene tree order.
    bool scene_ready(SourceRandom&,std::string&);
    bool ready()const{return ready_;}
    // Idle-process AnimatedSprite and AnimationPlayer time, also while paused.
    bool advance(double delta,std::string&);
    std::vector<PresentAudioRequest> take_audio(){auto out=std::move(audio_);audio_.clear();return out;}
    uint32_t count()const{return uint32_t(objects_.size());}
    PresentPose pose(uint32_t)const;
    bool supported(uint32_t object)const{return view_&&object<objects_.size()&&view_.object(object).program!=kPresentNone;}
    // Runs the object's programme until text needs acknowledgement or End.
    bool start(uint32_t object,std::string&);
    bool advance_text(std::string&);
    PresentProgramState state()const{return state_;}
    // True while the acknowledged text will be followed by more programme text.
    bool continues()const;
    PresentView view()const{return view_;}
private:
    struct Object {bool sparkles_visible=true,sparkles_playing=true;uint32_t frame=0,sparkle_frame=0;float sparkle_timeout=0;
        uint32_t clip=kPresentNone;float clip_time=0;bool clip_started=false;};
    bool step(std::string&);bool fail(std::string&,const char*);
    void play_clip(uint32_t object,uint32_t clip);
    PresentView view_;PresentHost* host_=nullptr;std::vector<Object>objects_;std::vector<PresentAudioRequest>audio_;
    uint32_t active_=kPresentNone,pc_=0;PresentProgramState state_=PresentProgramState::Idle;bool ready_=false;
};
}

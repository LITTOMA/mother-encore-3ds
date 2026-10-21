#pragma once
#include "encore/field_data.hpp"
#include "encore/audio_data.hpp"
#include <array>
namespace encore::upstream {
// ENCSTEP1 format1: capability1 rectangles; capability2 checked convex parts and
// declared Shadow animation policies. Neither capability activates a scene.
struct FieldStepPolicy {uint32_t default_sound=0;FieldText shadow,slot;};
struct FieldStepSound {FieldText name,source;std::array<uint8_t,32>sha256{};uint32_t id=0;};
struct FieldStepArea {FieldText path;uint32_t enter=0,exit=kFieldNone;Vec2 center{},extents{};FieldText enter_shadow,exit_shadow;uint32_t layer=0,mask=0;bool enabled=false;};
struct FieldStepPart {uint32_t area=0,first=0,count=0;};
struct FieldStepShadow {FieldText name;float fps=0;uint32_t frames=0;bool front=false,loop=false;};
class FieldStepsView {
public:
 bool valid()const{return p_!=nullptr;}uint32_t scene_id()const;uint32_t capability()const;
 uint32_t sound_count()const;uint32_t area_count()const;
 FieldStepPolicy policy()const;FieldStepSound sound(uint32_t)const;FieldStepArea area(uint32_t)const;
 std::string_view text(FieldText)const;uint32_t find_sound(std::string_view)const;
 uint32_t area_id(uint32_t)const;uint32_t first_part(uint32_t)const;uint32_t part_count(uint32_t)const;
 uint32_t part_count()const;FieldStepPart part(uint32_t)const;uint32_t point_count()const;Vec2 point(uint32_t)const;
 uint32_t shadow_count()const;FieldStepShadow shadow(uint32_t)const;uint32_t find_shadow(std::string_view)const;bool shadow_initial_playing()const;
 bool matches(const AudioBank&,std::string&)const;
 const void*bytes_identity()const{return p_;}
private:
 friend class FieldStepsData;const uint8_t*record(uint32_t,uint32_t)const;uint32_t count(uint32_t)const;
 const uint8_t*p_=nullptr;
};
enum class FieldRunEventKind:uint8_t {Play=1,ReplacePlay=2,Stop=3};
struct FieldRunEvent {FieldRunEventKind kind;uint32_t sound=0;};
// Pure source call semantics. MOVE decides when to invoke these operations;
// this controller does not infer movement, poll input or own a DSP stream.
class FieldRunLoop final:public WalkAudioSink {
public:
 bool initialize(FieldStepsView,std::string_view inherited_sound,std::string&);
 bool set_running(bool enabled,bool paused,bool move_state,bool climbing,std::string&);
 bool stop_playback(std::string&);
 // Checked MOVE call-site instructions; unknown operations reject atomically.
 bool apply_walk_calls(const WalkAudioCalls&,std::string&)override;
 bool walk_audio_ready()const override{return prepared();}
 bool area_event(uint32_t area,bool entered,bool is_player,std::string&);
 bool set_area_enabled(uint32_t area,bool enabled,std::string&);
 FieldStepsView content()const{return data_;}
 bool prepared()const{return data_.valid();}bool running()const{return running_;}bool playing()const{return playing_;}
 uint32_t wanted_sound()const{return wanted_;}uint32_t registered_sound()const{return registered_;}
 const std::vector<FieldRunEvent>&events()const{return events_;}void clear_events(){events_.clear();}
private:
 FieldStepsView data_;std::vector<bool>enabled_;std::vector<FieldRunEvent>events_;
 uint32_t wanted_=kFieldNone,registered_=kFieldNone;bool running_=false,playing_=false;
};
// Selected source set_shadow/get_shadow and AnimatedSprite mode/frame state.
// No elapsed animation clock or graphics service is inferred here.
class FieldStepShadowState {
public:
 bool initialize(FieldStepsView,std::string&);bool set_animation(std::string_view,std::string&);bool set_frame(uint32_t,std::string&);
 bool prepared()const{return data_.valid();}FieldStepsView content()const{return data_;}
 uint32_t animation()const{return animation_;}uint32_t frame()const{return frame_;}
 bool playing()const{return playing_;}bool behind()const{return prepared()&&!data_.shadow(animation_).front;}
private:FieldStepsView data_;uint32_t animation_=kFieldNone,frame_=0;bool playing_=false;
};
enum class FieldStepBody:uint8_t {Player=1,Party=2,Other=3};
// The caller owns body classification and the matching body Shadow state.
// Unsupported classification/ownership rejects before either state changes.
bool apply_step_callback(FieldRunLoop&,FieldStepShadowState*,uint32_t area,bool entered,FieldStepBody,std::string&);
// Ideal convex geometry query, including touching edges. The caller owns
// physics sampling, body filters and any engine-specific contact margin.
bool step_polygon_overlap(FieldStepsView,uint32_t area,const Vec2*world_body,size_t count,bool&overlap,std::string&);
class FieldStepsData {
public:
 FieldStepsData()=default;FieldStepsData(const FieldStepsData&)=delete;FieldStepsData&operator=(const FieldStepsData&)=delete;
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 FieldStepsView view()const{FieldStepsView v;if(!bytes_.empty())v.p_=bytes_.data();return v;}
private:std::vector<uint8_t>bytes_;
};
}

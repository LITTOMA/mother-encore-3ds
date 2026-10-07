#pragma once
#include "encore/source_random.hpp"
#include "encore/blackbars.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
namespace encore::upstream {
struct IntroRect {float x=0,y=0,width=0,height=0;};
struct IntroImage {std::string path;IntroRect rect;float alpha=1;bool visible=true;uint32_t frame=0,columns=1,rows=1;};
struct IntroMask {IntroRect rect;uint32_t color=0x000000ff;}; // RRGGBBAA, not native C2D byte order.
enum class IntroPhase:uint32_t {Inactive,DoorIn,DoorOut,Waiting,Playing,TextTail,Complete};
enum class IntroAudioKind:uint32_t {Play,FadeMusic,StopNamed};
enum class IntroAudioLane:uint32_t {Music,Text,Effect};
struct IntroAudio {IntroAudioKind kind=IntroAudioKind::Play;IntroAudioLane lane=IntroAudioLane::Effect;uint32_t effect_slot=0;std::string source_path,name;double pitch=1,gain_db=0,seconds=0;};
struct IntroDestination {std::string scene;float x=0,y=0,dx=0,dy=0;bool set_respawn=false,unpause=false;};
struct IntroDoorPose {uint32_t index=0,kind=0;float cut=1;bool incoming=false;IntroDestination destination;std::vector<IntroMask>masks;}; // kind: Fade=0, Circle Focus=1, Circle Pop=2.
struct IntroductionPose {
 // All coordinates are native viewport coordinates. Art remains 1:1; the
 // source composition is centered, and black border masks reach the viewport.
 IntroPhase phase=IntroPhase::Inactive;uint32_t scene=0;bool scene_visible=false;
 // Image rect already includes reviewed trim offset and cropped frame size.
 std::vector<IntroImage> images;std::vector<IntroMask> masks;
 std::vector<IntroMask> backgrounds;uint32_t background_color=0,text_color=0,hint_color=0;float line_spacing=0;IntroRect hint_rect;
 float text_character_spacing=0,hint_character_spacing=0;
 std::string text,font_source,hint_font_source,skip_text,font_catalog;
 uint32_t visible_characters=0;IntroRect text_clip;float text_y=0,hint_alpha=0,text_center_weight=1;bool center_text_vertically=false;
 IntroDoorPose door;
};
struct IntroKey {float time=0;std::array<float,4> value{};float transition=1;};
struct IntroTrack {uint32_t target=0,index=0,property=0,discrete=0;std::vector<IntroKey>keys;};
std::array<float,4> sample_intro_track(const IntroTrack&,double);
struct IntroClip {float length=0;std::vector<IntroTrack>tracks;};
struct IntroEvent {float time=0;uint32_t kind=0,arg=0;std::string source,name;};
struct IntroResource {std::string path,source,sha256;uint32_t width=0,height=0,columns=0,rows=0,frame_count=0,source_width=0,source_height=0,trim_x=0,trim_y=0,bytes=0,crc32=0;};
struct IntroLocale {std::string code,old_font,now_font,hint_font,punctuation,skip;std::array<float,3>character_spacing{};std::vector<std::string>texts;};
struct IntroScene {double delay=0,length=0,speed=0,slow=0,pause=0,hide=0,hide_y=0,pitch_min=0,pitch_max=0;uint32_t text_first=0,text_count=0,round_images=0,text_color=0,hint_color=0;float line_spacing=0;std::vector<IntroImage>images;std::vector<uint32_t>resources;std::vector<IntroTrack>tracks;std::vector<IntroEvent>events;IntroRect text_clip,hint_rect;IntroMask background;};
struct IntroDoor {IntroDestination destination;uint32_t in_kind=0,out_kind=0;float in_speed=0,out_speed=0;};
class IntroductionData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);bool valid()const{return valid_;}
 const IntroDestination& house_destination()const{return doors.back().destination;}
 const IntroLocale*locale(const std::string&)const;
 std::vector<IntroResource>resources;std::vector<IntroLocale>locales;std::vector<IntroScene>scenes;
 std::vector<IntroClip>borders;std::vector<IntroMask>border_rects;IntroClip cloud;
 std::vector<IntroClip>fades;std::vector<float>fade_mostly;
 std::vector<IntroDoor>doors;std::string font_catalog,music;std::array<std::string,2>text_sounds;float music_gain=0,canvas_width=0,canvas_height=0,music_fade=0;
 std::array<float,3>hint_timing{};std::array<float,2>hint_curve{};
 std::array<float,7>fade_shader{}; // Size, screenWidth, screenHeight, rectWidth, rectHeight, followX, followY.
 uint32_t background_color=0,finish_stop_slot=0;Blackbars blackbars;
private:bool valid_=false;
};
class Introduction {
public:
 bool begin(const IntroductionData&,SourceRandom&,const std::string&locale,float viewport_width,float viewport_height,std::string&);
 bool step(double delta,bool accept_hint,bool skip,std::string&);
 IntroductionPose pose()const;std::vector<IntroAudio>take_audio();
 IntroPhase phase()const{return phase_;}bool active()const{return phase_!=IntroPhase::Inactive&&phase_!=IntroPhase::Complete;}
 bool complete()const{return phase_==IntroPhase::Complete;}bool playtime_started()const{return playtime_;}
 bool house_ready()const{return data_&&door_+1==data_->doors.size()&&(phase_==IntroPhase::DoorOut||phase_==IntroPhase::Complete);}
 bool house_unpaused()const{return house_ready()&&(complete()||phase_time_>data_->fade_mostly[data_->doors.back().out_kind*2+1]);}
 void rebind_random(SourceRandom&random){random_=&random;}
 void set_focus(float x,float y){focus_x_=x;focus_y_=y;}
 void close(){*this=Introduction{};}
private:
 void enter_scene(uint32_t);void enter_door(uint32_t);void event(const IntroEvent&);void next_text();void hide_text();void finish_now();
 const IntroductionData*data_=nullptr;const IntroLocale*locale_=nullptr;SourceRandom*random_=nullptr;
 IntroPhase phase_=IntroPhase::Inactive;uint32_t scene_=0,door_=0,next_=0,visible_=0,event_=0,border_=0;
 double phase_time_=0,time_=0,text_time_=0,pause_=0,hide_time_=-1,border_time_=0,cloud_time_=-1,hint_time_=-1;
 double speed_=0;float width_=0,height_=0,text_origin_=0,focus_x_=0,focus_y_=0;bool finished_=true,process_=true,playtime_=false,now_finishing_=false;
 std::string text_;std::vector<IntroAudio>audio_;
 Blackbars blackbars_;
};
}

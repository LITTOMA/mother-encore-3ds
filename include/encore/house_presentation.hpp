#pragma once
#include "encore/house_data.hpp"
#include "encore/source_random.hpp"
#include <string>
#include <vector>
namespace encore::upstream {
struct HouseNpcPose {uint32_t primary_resource=house_no_index,shadow_resource=house_no_index,frame=0;Vec2 position{},sprite_offset{},shadow_offset{};bool visible=false;};
struct HouseDoorPose {uint32_t resource=house_no_index;Vec2 position{},offset{};bool visible=false;};
struct LocalizedHouseToken {uint32_t kind=0;std::string text;};
struct LocalizedHouseSegment {uint32_t flags=0;std::vector<LocalizedHouseToken>tokens;};
struct LocalizedHouseSpan {std::string speaker,word_separator,bullet;uint32_t max_name_length=0;std::vector<LocalizedHouseSegment>segments;};
using HouseLocaleResolver=bool(*)(void*,HouseView,uint32_t,uint32_t,std::string_view,LocalizedHouseSpan&,std::string&);
using HouseGlyphAdvance=bool(*)(void*,uint32_t,float&);
struct HouseTextLine {std::string text;float y=0;bool bullet=false;std::vector<uint32_t>colors;};
// Read-only state from the actual current source printer. Invisible delay cells
// remain logical U32 cells and are never printed as zero-byte font glyphs.
struct HouseSourceTextLine { std::u32string cells; std::vector<uint32_t>colors; bool bullet=false,wait=false; };
struct HouseSourceTextState { bool active=false; uint32_t visible_characters=0,choice_rows=0; std::vector<HouseSourceTextLine>lines; };
struct WorldDialoguePose {
 bool visible=false,name_visible=false,cursor_visible=false,text_visible=false;uint32_t box_resource=house_no_index,name_resource=house_no_index,cursor_resource=house_no_index,cursor_frame=0;
 BattleValue box{},name{},clip{},text_layout{},bullet_layout{},name_label{},cursor{};Vec2 anchor{};float cursor_rotation=0;std::string speaker,bullet;std::vector<HouseTextLine>lines;
};
enum class HouseAudioKind:uint32_t {MenuOpen=1,MenuClose,Confirm,VoiceStart,VoiceStop};
struct HouseAudioEvent {HouseAudioKind kind=HouseAudioKind::VoiceStop;std::string voice;double pitch=1;};
// Native AnimationTree subset: data-selected one/four-direction source
// states, including immediate Idle->Talk and at-end Talk->Idle edges.
class HouseNpcAnimation {
public:
 bool begin(HouseView,Vec2,uint32_t profile);void request(bool talking,Vec2 direction);bool idle_frame(double);
 uint32_t frame()const{return frame_;}bool talking()const{return talk_;}double time()const{return time_;}
 uint64_t frame_revision()const{return frame_revision_;}
private:
 HouseView content_;Vec2 direction_{};uint32_t profile_=house_no_index,dir_=0,frame_=0;float time_=0;bool talk_=false,requested_=false;
 uint64_t frame_revision_=0;
};
// A bounded adaptation of the original world DialogueBox and standing NPC
// animation mechanisms. Geometry, script tuning, text and clips are external.
class HousePresentation {
public:
 bool begin(HouseView,BattleView,SourceRandom&);
 void set_locale_resolver(HouseLocaleResolver resolver,void*state){locale_resolver_=resolver;locale_state_=state;}
 void set_font_line_height(float value){font_line_height_=value;}
 void set_glyph_advance(HouseGlyphAdvance callback,void*state){glyph_advance_=callback;glyph_state_=state;}
 // Apply a checked session choice after begin(), which restores pack defaults.
 // Changing the base period preserves the current clock/input acceleration;
 // CHAR_DELAY cells are expanded using this value when a phrase is prepared.
 bool set_text_speed(double seconds);double text_speed()const{return text_seconds_;}
 void rebind_random(SourceRandom&random){random_=&random;}
 bool begin_dialogue(uint32_t npc,std::string_view player_name,Vec2 player_position);
 bool begin_npc_dialogue(uint32_t npc,uint32_t first,uint32_t count,std::string_view player_name,Vec2 player_position);
 bool begin_dialogue(uint32_t first_segment,uint32_t segment_count,std::string_view player_name);
 void input(bool accept,bool cancel,bool defer_close=false,bool confirm_next=false);
 bool present_story_dialogue(uint32_t first,uint32_t count,std::string_view player_name);
 // Source options append blank layout rows after text completion. They own
 // input until selection clears the text; the existing box stays open.
 void set_choice_rows(uint32_t rows){choice_rows_=rows;}
 void clear_story_text();
 bool take_dialogue_advance(){const bool result=advance_requested_;advance_requested_=false;return result;}
 void close_story_dialogue(){if(active_&&!closing_)close();}
 void hide_story_dialogue(bool sound);
 void set_text_value_callback(bool(*callback)(void*,HouseTokenKind,std::string&),void*state){text_value_=callback;text_value_state_=state;}
 void set_text_completion_callback(bool(*callback)(void*),void* state){text_completion_=callback;text_completion_state_=state;}
 bool physics_frame(double,Vec2 player_position);bool idle_frame(double);
 bool set_npc_looking(uint32_t,bool);
 bool begin_npc_interaction(uint32_t,Vec2);
 bool set_npc_talking(uint32_t,bool);
 bool stop_npc_interaction(uint32_t);
 bool set_npc_replaced(uint32_t,bool);
 bool set_npc_visible(uint32_t,bool);
 bool restore_npc_pose(uint32_t,Vec2 position,Vec2 direction);
 uint64_t npc_frame_revision(uint32_t i)const{return i<npcs_.size()?npcs_[i].animation.frame_revision():0;}
 bool dialogue_active()const{return active_;}bool dialogue_done()const{return done_;}
 bool dialogue_stopped()const{return stopped_;}bool dialogue_finished()const{return finished_;}
 bool dialogue_closing()const{return closing_;}uint32_t visible_characters()const{return visible_;}
 uint32_t current_segment()const{return segment_;}bool talking()const{return talking_;}
 HouseNpcPose npc_pose(uint32_t)const;HouseDoorPose openable_door_pose(uint32_t,bool visible)const;WorldDialoguePose dialogue_pose()const;
 HouseSourceTextState source_text_state() const;
 std::vector<HouseAudioEvent>take_audio_events();
 HouseKey sample(HouseClipRole,double,uint32_t previous_frame=0)const;
 const char*error()const{return error_;}
private:
 struct Npc {Vec2 position{},direction{},blend{};bool replaced=false,visible=true,story_talking=false;HouseNpcAnimation animation;double return_timer=0;bool looking=false,return_pending=false;};
 // Zero bytes are internal invisible CHAR_DELAY cells, never font glyphs.
 struct Line {std::u32string text;uint32_t first=0,segment=0;bool bullet=false,wait=false;std::vector<uint32_t>colors;};
 float font_line_height_=0;HouseLocaleResolver locale_resolver_=nullptr;void*locale_state_=nullptr;HouseGlyphAdvance glyph_advance_=nullptr;void*glyph_state_=nullptr;LocalizedHouseSpan localized_;std::string locale_error_;
 bool fail(const char*);bool append_segment(uint32_t);void close(bool sound=true);float width(std::string_view)const;float width(std::u32string_view)const;bool valid_text(std::string_view)const;
 HouseClip clip(HouseClipRole)const;void voice(HouseAudioKind,double=1);
 HouseView content_;BattleView font_;SourceRandom*random_=nullptr;std::vector<Npc>npcs_;std::vector<Line>lines_;std::vector<HouseAudioEvent>audio_;
 std::string player_name_,speaker_,voice_,bullet_;uint32_t npc_=house_no_index,first_segment_=0,segment_count_=0,segment_=0,loaded_line_=0,loaded_count_=0,visible_=0;
 uint32_t choice_rows_=0;
 double box_time_=0,name_time_=0,name_size_time_=0,cursor_time_=0,text_time_=0,multiplier_=1,text_seconds_=0;float name_width_=0;
 bool advance_requested_=false;
 bool(*text_value_)(void*,HouseTokenKind,std::string&)=nullptr;void*text_value_state_=nullptr;
 bool(*text_completion_)(void*)=nullptr;void*text_completion_state_=nullptr;
 bool active_=false,done_=true,closing_=false,finished_=false,stopped_=false,talking_=false,voice_playing_=false;const char*error_="";
};
}

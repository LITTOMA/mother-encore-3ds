#pragma once
#include "encore/battle_data.hpp"
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>
namespace encore::upstream {
constexpr uint32_t house_no_index=UINT32_MAX;
enum class HouseSection:uint16_t {Strings=1,Doors,Npcs,Segments,Tokens,Interaction,Boundaries,Resources,Clips,Keys,Parameters,Overrides,Profiles,Dialogues,OpenableDoors,StoryTriggers,StoryConditions};
enum class HouseTokenKind:uint32_t {Literal=1,PlayerName=2,Color=3,ColorReset=4,EarnedCash=5,BankCash=6,CurrentCash=7,SourceDelay=8,ForcedNewline=9,SourceInlineWait=10,FavoriteFood=11};
enum class HouseSegmentFlag:uint32_t {Bullet=1,Wait=2,End=4};
enum class HouseBoundaryKind:uint32_t {UnsupportedDoor=1,UnsupportedScene=2};
enum class HouseClipRole:uint32_t {NpcIdleDown=1,NpcIdleLeft,NpcIdleRight,NpcIdleUp,NpcTalkDown,NpcTalkLeft,NpcTalkRight,NpcTalkUp,DialogueOpen,DialogueClose,NameOpen,NameClose,Cursor,Count};
enum class HouseParameter:uint32_t {DialogueRect=1,DialogueMargins,DialogueClip,DialogueText,DialogueBullet,NameRect,NameMargins,NameLabel,NameSizing,CursorGeometry,CursorRotation,TextTiming,TextTagSpeeds,VoicePitch,DisplayReference,FontMetrics,FadeCuts,FadeShader,NpcInteractionReturn,Count};
// All positions are source world coordinates. Fade lengths and alpha key times
// are source animation seconds, divided by the per-door playback speeds.
struct HouseDoor {
 uint32_t id=0,source_path=0,start_sound=0,end_sound=0;
 Vec2 center{},extents{},destination{},direction{};
 double fade_in_length=0,fade_in_opaque=0,fade_out_length=0,fade_out_mostly=0,fade_in_speed=0,fade_out_speed=0;
 BattleValue color{};
};
struct HouseNpc {
 uint32_t id=0,source_path=0,body_id=0,primary_resource=0,shadow_resource=0,first_segment=0,segment_count=0,seen_key=0;
 Vec2 position{},interact_center{},interact_extents{},default_direction{},view_center{};
 float view_radius=0;double return_delay=0;
 uint32_t flags=0; // staring=1, turn on interaction=2, player turns x/y=4/8
 // room_actor_index is an optional cross-pack binding; validate against the selected Room before use.
 uint32_t profile=house_no_index,dialogue_path=0,room_actor_index=house_no_index,program_index=house_no_index; // Empty segment span is an explicit unimplemented source dialogue
};
struct HouseSegment {uint32_t id=0,speaker=0,voice=0,first_token=0,token_count=0,flags=0;};
struct HouseToken {uint32_t kind=0,text=0;};
struct HouseInteraction {Vec2 ray_origin{};float ray_length=0;double text_seconds=0,accept_multiplier=0,cancel_multiplier=0;uint32_t collision_mask=0,bullet_string=0,word_separator=0,max_player_name_length=0;double voice_pitch_min=0,voice_pitch_max=0;};
struct HouseBoundary {uint32_t id=0,source_path=0,kind=0;Vec2 center{},extents{};};
struct HouseClip {uint32_t id=0,role=0,resource=house_no_index,first_key=0,key_count=0;double duration=0;uint32_t loop=0,interpolation=0,profile=house_no_index;};
struct HouseKey {double time=0;float ease=0;Vec2 position{};uint32_t frame=0;};
struct HouseOverride {uint32_t npc=0,flag=0,dialogue=0,dialogue_index=house_no_index,seen_key=0;};
struct HouseProfile {uint32_t id=0,resource=house_no_index;Vec2 sprite_offset{},shadow_offset{};uint32_t directions=0,flags=0;}; // visible=1,shadow=2,talk=4
struct HouseDialogue {uint32_t id=0,source_path=0,first_segment=0,segment_count=0;};
enum class HouseDoorPolicy:uint32_t {Blocked=1,Locked=2,RemoveKey=4,OneWay=8};
enum class HouseDoorStateField:uint32_t {SpriteVisible=1,PlayerDisabled=2,NonPlayerDisabled=4};
struct HouseOpenableDoor {
 uint32_t id=0,source_path=0,player_body_id=0,nonplayer_body_id=0,sprite_resource=house_no_index;
 // blocked_dialogue is a HouseDialogue index; locked/opened are retained source-path string offsets.
 uint32_t flag=0,key=0,activates_flag=0,deactivates_flag=0,blocked_dialogue=house_no_index,locked_dialogue=0,opened_dialogue=0;
 uint32_t start_sound=0,end_sound=0,ram_sound=0,policy=0,normal_mask=0,normal_values=0,action_mask=0,action_values=0,timer_flags=0;
 Vec2 position{},trigger_center{},trigger_extents{},interact_center{},interact_extents{},collision_center{},collision_extents{},sprite_position{},sprite_offset{},ram_direction{};
 // timer_flags bit1 means source one-shot idle Timer; ram_required_y compares only player Y direction.
 float ram_required_y=0;
 double close_delay=0,action_length=0,normal_length=0,ram_strength=0,ram_duration=0;
};
enum class HouseStoryConditionKind:uint32_t {ParentPresence=1,TriggerFlag=2};
struct HouseStoryCondition {uint32_t kind=0,flag=0,value=0;};
// ExecuteProgram requires a cross-pack Room program index checked before use.
enum class HouseStoryDisposition:uint32_t {DevelopmentBoundary=1,ExecuteProgram=2};
struct HouseStoryTrigger {uint32_t id=0,source_path=0,dialogue=0;Vec2 center{},extents{};uint32_t first_condition=0,condition_count=0,disposition=0,program_index=house_no_index;};

class HouseView {
public:
 bool valid()const{return bytes_!=nullptr;}explicit operator bool()const{return valid();}
 const uint8_t*bytes()const{return bytes_;}size_t byte_size()const{return size_;}
 uint32_t count(HouseSection)const;std::string_view string(uint32_t)const;
 HouseDoor door(uint32_t)const;HouseNpc npc(uint32_t)const;HouseSegment segment(uint32_t)const;HouseToken token(uint32_t)const;
 HouseInteraction interaction()const;HouseBoundary boundary(uint32_t)const;
 BattleResource resource(uint32_t)const;HouseClip clip(uint32_t)const;HouseKey key(uint32_t)const;
 BattleValue parameter(HouseParameter)const;uint32_t clip_for(HouseClipRole,uint32_t profile=house_no_index)const;HouseOverride override_dialogue(uint32_t)const;
 HouseProfile profile(uint32_t)const;HouseDialogue dialogue(uint32_t)const;HouseOpenableDoor openable_door(uint32_t)const;
 HouseStoryTrigger story_trigger(uint32_t)const;HouseStoryCondition story_condition(uint32_t)const;
private:friend class HouseData;const uint8_t*bytes_=nullptr;size_t size_=0;const uint8_t*record(HouseSection,uint32_t)const;
};
class HouseData {
public:
 HouseData()=default;HouseData(const HouseData&)=delete;HouseData&operator=(const HouseData&)=delete;
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 HouseView view()const{HouseView v;if(!bytes_.empty()){v.bytes_=bytes_.data();v.size_=bytes_.size();}return v;}
private:std::vector<uint8_t>bytes_;
};
}

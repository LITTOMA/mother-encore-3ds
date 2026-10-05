#pragma once
#include "encore/field_present.hpp"
namespace encore::upstream {
struct FieldDroppedAsset{uint32_t id=0,width=0,height=0,bytes=0,crc=0;std::string source,path;};
struct FieldDroppedBinding{
 uint32_t id=0,ready_ordinal=0,sparkles_id=0,sparkles_ready=0,prompt_id=0,tween_id=0,timer_id=0,animation_id=0,tween_ordinal=0,timer_ordinal=0,animation_ordinal=0,asset_id=0,flags=0,collision_layer=0,interaction_layer=0;
 std::string node,flag,item,dialogue,full,empty;Vec2 position{},scale{},local_position{},sparkles_offset{},prompt_offset{};BattleValue collision{},interaction{};
 bool object_flag()const{return flags&1;}bool emit()const{return flags&2;}bool reset_area()const{return flags&4;}bool reset_consumed()const{return flags&8;}bool can_pickup()const{return flags&16;}
};
struct FieldDroppedBlinkKey{float time=0;bool visible=false;};
struct FieldDroppedRules{std::array<float,4>timers{};std::array<float,2>speeds{};float initial_speed=0,position_duration=0,position_delay=0,scale_duration=0;Vec2 scale_from{},scale_to{};uint32_t position_trans=0,position_ease=0,scale_trans=0,scale_ease=0;float rotation_from=0,rotation_to=0,rotation_duration=0;uint32_t rotation_trans=0,rotation_ease=0;float blink_length=0;std::vector<FieldDroppedBlinkKey>blink_keys;};
class FieldDroppedData{
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);bool valid()const{return valid_;}const std::array<uint8_t,20>&source_pin()const{return pin_;}
 const std::vector<FieldDroppedBinding>&bindings()const{return bindings_;}const FieldDroppedBinding*binding(uint32_t)const;const FieldPresentItem*item(std::string_view)const;const FieldDroppedAsset*asset(uint32_t)const;const std::vector<FieldDroppedAsset>&assets()const{return assets_;}const FieldDroppedRules&rules()const{return rules_;}
 const std::string&scene()const{return scene_;}const std::string&script()const{return script_;}const std::string&empty_message()const{return empty_;}const std::string&sparkles_path()const{return sparkles_path_;}uint32_t sparkles_width()const{return sparkles_width_;}uint32_t sparkles_height()const{return sparkles_height_;}uint32_t sparkles_bytes()const{return sparkles_bytes_;}uint32_t sparkles_crc()const{return sparkles_crc_;}float sparkles_speed()const{return sparkles_speed_;}float sparkles_scale()const{return sparkles_scale_;}float random_low()const{return random_low_;}float random_high()const{return random_high_;}const std::vector<PresentSparklesFrame>&sparkles_frames()const{return sparkles_frames_;}
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 std::map<std::string,std::array<uint8_t,32>>sources_;
 bool valid_=false;std::array<uint8_t,20>pin_{};std::string scene_,script_,empty_,sparkles_path_;FieldDroppedRules rules_{};uint32_t sparkles_width_=0,sparkles_height_=0,sparkles_bytes_=0,sparkles_crc_=0;float sparkles_speed_=0,sparkles_scale_=0,random_low_=0,random_high_=0;std::vector<PresentSparklesFrame>sparkles_frames_;std::vector<FieldPresentItem>items_;std::vector<FieldDroppedAsset>assets_;std::vector<FieldDroppedBinding>bindings_;
};
bool validate_dropped_present_bridge(const FieldPresentData&,const FieldDroppedData&,std::string&);
struct FieldDroppedHost{
 std::function<bool(const FieldDroppedBinding&,std::string&)>admit_ready,admit_interaction;
 std::function<bool(const FieldDroppedBinding&,bool&,std::string&)>read_flag;
 std::function<bool(const FieldDroppedBinding&,bool,std::string&)>write_flag;
 std::function<bool(bool&,std::string&)>inventory_space,player_paused;
 std::function<bool(const FieldPresentItem&,bool give,std::string&)>select_item;
 std::function<bool(uint32_t,bool,std::string&)>prompt_enabled;
 std::function<bool(uint32_t,std::string&)>prompt_force_show,prompt_press,prompt_connect_hide_free,prompt_queue_free;
 std::function<bool(uint32_t,bool&,std::string&)>prompt_visible;
 std::function<bool(const FieldDroppedBinding&,std::string&)>prompt_reparent;
 std::function<bool(uint32_t,Vec2,std::string&)>prompt_position;
 std::function<bool(Vec2&,std::string&)>player_position;
 std::function<bool(uint32_t,Vec2,std::string&)>publish_position,publish_scale;
 // Register the collection tween in the one scene-wide creation-order ledger.
 std::function<bool(uint32_t,uint64_t&,std::string&)>register_collect_tween;
 std::function<bool(std::string_view,std::string&)>dialogue;
 std::function<bool(uint32_t,std::string&)>queue_free;
};
struct FieldDroppedRotation{float elapsed=0;bool finished=false;};
struct FieldDroppedCollect{uint64_t order=0;float elapsed=0;Vec2 from{},target{};bool started=false,position_done=false,scale_done=false,finished=false;};
struct FieldDroppedState{
 uint32_t id=0,sparkles_frame=0;bool child_ready=false,parent_ready=false,alive=true,visible=false,sprite_visible=true,show_wait=false,queued=false,timer_paused=false,timer_running=false,blink_playing=false,tween_active=false;
 Vec2 position{},scale{};float sprite_rotation=0,sparkles_timeout=0,timer_wait=0,blink_time=0,blink_speed=0;double timer_left=0;
 std::vector<uint32_t>disappear_waiters;std::vector<FieldDroppedRotation>rotations;std::vector<FieldDroppedCollect>collects;
};
class FieldDroppedRuntime{
public:
 bool initialize(const FieldDroppedData&,FieldDroppedHost,std::string&);
 bool ready_sparkles(uint32_t,SourceRandom&,std::string&);bool ready_item(uint32_t,std::string&);bool interact(uint32_t,std::string&);bool disappear(uint32_t,std::string&);bool player_pause(bool,std::string&);
 // Exact phase endpoints. Tree idle signal precedes native node processing.
 // Call idle_node for each source leaf in the scene-wide actual node order.
 // SceneTreeTween collection steps follow nodes in global tween creation order.
 bool idle_signal(std::string&);bool idle_node(uint32_t leaf,double delta,bool processing,std::string&);bool collect_step(uint32_t,uint64_t order,double delta,bool processing,std::string&);
 bool area_left(bool region_changed,std::string&);bool exit_tree(uint32_t,std::string&);
 const FieldDroppedState*state(uint32_t)const;const FieldDroppedData*content()const{return data_;}
private:
 FieldDroppedState*mutable_state(uint32_t);bool queue(FieldDroppedState&,std::string&);bool collect(FieldDroppedState&,const FieldDroppedBinding&,std::string&);bool timer(FieldDroppedState&,std::string&);
 const FieldDroppedData*data_=nullptr;FieldDroppedHost host_;std::vector<FieldDroppedState>states_;
};
}

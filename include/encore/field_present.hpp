#pragma once
#include "encore/present_sparkles.hpp"
#include "encore/battle_data.hpp"
#include <functional>
#include <map>
namespace encore::upstream {
struct FieldPresentItem {std::string key,source;bool keyitem=false;uint32_t doses=0;};
struct FieldPresentKey {float time=0;uint32_t role=0,value=0;};
struct FieldPresentClip {std::string name;float length=0;std::vector<FieldPresentKey>keys;};
struct FieldPresentBinding {
 uint32_t id=0,ready_ordinal=0,sparkles_id=0,sparkles_ready=0,prompt_id=0,tint_id=0,fetcher_id=0,flags=0,initial_frame=0,opened_frame=0,collision_layer=0,interaction_layer=0;
 std::string node,flag,item,dialogue,full,empty;Vec2 position{},scale{},sprite_position{},sparkles_position{};BattleValue collision{},interaction{};
 bool object_flag()const{return flags&1;}bool emit()const{return flags&2;}bool reset_area()const{return flags&4;}bool can_pickup()const{return flags&8;}bool serialized_visible()const{return flags&16;}
};
struct FieldPresentPending {uint32_t id=0,ready_ordinal=0;std::string node,script;};
class FieldPresentData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);bool valid()const{return valid_;}
 const std::array<uint8_t,20>&source_pin()const{return pin_;}const std::vector<FieldPresentBinding>&bindings()const{return bindings_;}const FieldPresentBinding*binding(uint32_t)const;const FieldPresentItem*item(std::string_view)const;
 const std::vector<FieldPresentPending>&pending()const{return pending_;}const FieldPresentClip&clip(bool wrapped)const{return clips_[wrapped?1:0];}
 const std::string&scene()const{return scene_;}const std::string&scene_name()const{return scene_name_;}const std::string&empty_message()const{return empty_message_;}const std::string&box_path()const{return box_path_;}const std::string&box_source()const{return box_source_;}const std::string&sound()const{return sound_;}const std::string&sparkles_path()const{return sparkle_path_;}
 uint32_t box_bytes()const{return box_bytes_;}uint32_t box_crc()const{return box_crc_;}uint32_t sparkles_bytes()const{return sparkle_bytes_;}uint32_t sparkles_crc()const{return sparkle_crc_;}uint32_t sparkles_width()const{return sparkle_width_;}uint32_t sparkles_height()const{return sparkle_height_;}
 uint32_t width()const{return width_;}uint32_t height()const{return height_;}uint32_t frames()const{return frames_;}float sparkle_speed()const{return sparkle_speed_;}float sparkle_scale()const{return sparkle_scale_;}float random_low()const{return low_;}float random_high()const{return high_;}const std::vector<PresentSparklesFrame>&sparkle_frames()const{return sparkle_frames_;}
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 std::map<std::string,std::array<uint8_t,32>>sources_;
 bool valid_=false;std::array<uint8_t,20>pin_{};std::string scene_,scene_name_,empty_message_,box_source_,box_path_,sound_,sparkle_path_;uint32_t width_=0,height_=0,frames_=0,box_bytes_=0,box_crc_=0,sparkle_bytes_=0,sparkle_crc_=0,sparkle_width_=0,sparkle_height_=0;float sparkle_speed_=0,sparkle_scale_=0,low_=0,high_=0;
 std::vector<FieldPresentBinding>bindings_;std::vector<FieldPresentItem>items_;std::array<FieldPresentClip,2>clips_;std::vector<PresentSparklesFrame>sparkle_frames_;std::vector<FieldPresentPending>pending_;
};
struct FieldPresentHost {
 // Cross-source child and interaction admission: no missing inventory, Room
 // Programme, audio voice, prompt, Tint or Fetcher may be silently ignored.
 std::function<bool(const FieldPresentBinding&,std::string&)>admit_ready,admit_interaction;
 std::function<bool(const FieldPresentBinding&,bool&,std::string&)>read_flag;
 std::function<bool(const FieldPresentBinding&,bool,std::string&)>write_flag;
 std::function<bool(bool&,std::string&)>inventory_space;
 // Uses the real shared UID/RNG/clock ledger and publishes global.item context.
 // give=true follows Inventory.add_item_available; false is Item.new with no add.
 std::function<bool(const FieldPresentItem&,bool give,std::string&)>select_item;
 std::function<bool(uint32_t,bool,std::string&)>prompt_enabled,sound;
 std::function<bool(std::string_view,std::string&)>dialogue;
};
struct FieldPresentState {
 uint32_t id=0,frame=0,sparkle_frame=0;bool child_ready=false,parent_ready=false,opened=false,visible=true,sparkle_visible=true,sparkle_playing=true,animation_playing=false,audio_playing=false,wrapped=false,alive=true;
 float sparkle_timeout=0,animation_time=0;uint32_t next_key=0;uint64_t wait_serial=0;std::vector<uint64_t>revert_waiters;
};
// Scene scheduler invokes child/parent Ready at the actual published postorder,
// never an eager bundle that changes interleaved NPC/Enemy shared RNG draws.
enum class FieldPresentSparklesEvent { FrameChanged, AnimationFinished };
class FieldPresentSparklesLeafOwner {
public:
 virtual ~FieldPresentSparklesLeafOwner()=default;
 virtual bool admit(const FieldPresentData&,const FieldPresentBinding&,std::string&)const=0;
 virtual bool signal(uint32_t child,FieldPresentSparklesEvent,std::string&)=0;
};
class FieldPresentRuntime {
public:
 bool initialize(const FieldPresentData&,FieldPresentHost,std::string&);
 bool ready_sparkles(uint32_t child,SourceRandom&,std::string&);bool ready_present(uint32_t parent,std::string&);
 bool interact(uint32_t,std::string&);bool area_left(bool region_changed,std::string&);bool exit_tree(uint32_t,std::string&);
 bool idle_frame(double delta,bool tree_idle_processing,std::string&);
 bool bind_sparkles_leaf_owner(FieldPresentSparklesLeafOwner&,std::string&);
 bool idle_sparkles_leaf(uint32_t child,double delta,bool can_process,std::string&);
 const FieldPresentState*state(uint32_t)const;const FieldPresentData*content()const{return data_;}
private:
 FieldPresentState*mutable_state(uint32_t);bool update_state(FieldPresentState&,const FieldPresentBinding&,std::string&);bool play(FieldPresentState&,bool wrapped,std::string&);bool animate(FieldPresentState&,const FieldPresentBinding&,float,std::string&);
 FieldPresentSparklesLeafOwner*sparkles_owner_=nullptr;
 const FieldPresentData*data_=nullptr;FieldPresentHost host_;std::vector<FieldPresentState>states_;
};
}

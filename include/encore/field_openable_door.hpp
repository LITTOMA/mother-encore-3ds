#pragma once
#include "encore/field_data.hpp"
#include <functional>
#include <map>
namespace encore::upstream {
enum class FieldOpenableClipRole:uint32_t {Action=1,Normal,Reset};
enum class FieldOpenableProperty:uint32_t {SpriteVisible=1,BodyDisabled,NonPlayerDisabled,AudioPlaying};
struct FieldOpenableTrack {FieldOpenableProperty property{};uint32_t update=0;bool value=false;};
struct FieldOpenableClip {FieldOpenableClipRole role{};std::string name;float length=0;bool loop=false;std::vector<FieldOpenableTrack>tracks;};
struct FieldOpenableTexture {uint32_t id=0;std::string source,path;std::array<float,4>region{};std::array<uint8_t,32>source_sha{},output_sha{};};
struct FieldOpenableSound {uint32_t id=0;std::string source,pcm;std::array<uint8_t,32>sha{};};
struct FieldOpenableDialogue {std::string program,source,key,en,zh_cn;std::array<uint8_t,32>source_sha{};};
// Child indices are schema roles: sprite, area, interact, body, nonplayer,
// bodyShape, nonplayerShape, areaShape, interactShape, prompt, audio, timer, anim.
struct FieldOpenableDescriptor {
 uint32_t id=0,ready=0,texture=0,profile=0,flags=0;std::string node,bus,key,flag,activate,deactivate,sound,end_sound;
 std::array<uint32_t,13>children{};std::array<std::string,3>dialogs{};
 Vec2 offset{},initial_sprite_position{},sprite_offset{},sprite_scale{};std::array<Vec2,4>positions{};float timer_wait=0,gain_db=0;
};
class FieldOpenableDoorData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);bool load_file(const char*,const FieldIdentity&,std::string&);bool valid()const{return valid_;}bool scene_admitted()const{return false;}
 FieldIdentity identity()const{return identity_;}const std::string&source_scene()const{return scene_;}const std::string&script()const{return script_;}const std::array<uint8_t,32>&script_sha()const{return script_sha_;}
 const std::vector<FieldOpenableDescriptor>&records()const{return records_;}const FieldOpenableDescriptor*record(uint32_t)const;
 uint32_t texture_count()const{return static_cast<uint32_t>(textures_.size());}
 const FieldOpenableClip*clip(uint32_t profile,FieldOpenableClipRole)const;const FieldOpenableTexture*texture(uint32_t)const;const FieldOpenableSound*sound(std::string_view)const;const FieldOpenableDialogue*dialogue(std::string_view)const;
 const std::string&none()const{return none_;}const std::string&bash()const{return bash_;}bool initial_unlocked()const{return initial_unlocked_;}
 float child_height()const{return child_height_;}const std::array<float,4>&shake()const{return shake_;}float run_y()const{return run_y_;}
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 bool valid_=false,initial_unlocked_=false;FieldIdentity identity_{};std::string scene_,script_,none_,bash_;std::array<uint8_t,32>script_sha_{};float child_height_=0,run_y_=0;std::array<float,4>shake_{};
 std::vector<FieldOpenableDescriptor>records_;std::vector<std::vector<FieldOpenableClip>>profiles_;std::vector<FieldOpenableTexture>textures_;std::vector<FieldOpenableSound>sounds_;std::vector<FieldOpenableDialogue>dialogues_;std::map<std::string,std::array<uint8_t,32>>sources_;
};
struct FieldOpenableInstance {
 uint32_t id=0;bool ready=false,unlocked=false,blocked=false,locked=false,sprite_visible=false,body_disabled=false,nonplayer_disabled=false,playing=false,started=false,timer_running=false;
 FieldOpenableClipRole clip{};float elapsed=0,timer_left=0;Vec2 sprite_position{};std::array<Vec2,4>positions{};
};
struct FieldOpenableBody {uint64_t id=0;bool actual_party_object=false;};
struct FieldOpenableObservation {bool player_paused=false,player_running=false,entering_door=false,animation_process=false,timer_process=false;Vec2 player_direction{};};
struct FieldOpenableHost {
 // All source nodes/art/material/geometry/audio/Prompt identities must resolve.
 std::function<bool(const FieldOpenableDoorData&,std::string&)>bind;
 std::function<bool(uint32_t,std::function<bool(uint64_t)>,std::function<bool(uint64_t)>,std::string&)>connect_area;
 std::function<bool(uint32_t,std::function<bool()>,std::string&)>connect_flags;
 std::function<bool(std::string_view,bool&present,bool&value,std::string&)>read_flag;
 std::function<bool(std::string_view,bool,std::string&)>write_flag;
 std::function<bool(std::string&)>emit_flags;
 std::function<bool(uint32_t,FieldOpenableObservation&,std::string&)>observe;
 std::function<bool(uint32_t,std::vector<FieldOpenableBody>&,std::string&)>overlapping_bodies;
 std::function<bool(uint32_t,const FieldOpenableTexture&,std::string&)>sprite_texture;
 std::function<bool(uint32_t,const FieldOpenableInstance&,std::string&)>publish;
 std::function<bool(uint32_t,FieldOpenableProperty,bool,std::string&)>property;
 // Append to the actual shared SceneTree deferred queue in source order. Invoke
 // the slot only when this property message commits; disconnect on scene free.
 std::function<bool(uint32_t,bool,std::function<bool()>,std::string&)>defer_disabled;
 // Plain source export-variable assignment, distinct from set_enabled method.
 std::function<bool(uint32_t,bool,std::string&)>prompt_assign_enabled;
 std::function<bool(uint32_t,std::string&)>prompt_hide;
 std::function<bool(uint32_t,const FieldOpenableSound&,float,std::string_view,std::string&)>audio_stream;
 std::function<bool(uint32_t,bool,std::string&)>audio_playing;
 std::function<bool(float,float,Vec2,std::string&)>camera_shake;
 // Original search order: actual party member inventories, then key inventory.
 std::function<bool(std::vector<uint64_t>&,uint64_t&key_inventory,std::string&)>party_inventories;
 std::function<bool(uint64_t,std::string_view,uint64_t&item,std::string&)>inventory_find;
 std::function<bool(uint64_t,std::string&,std::string&)>item_name;
 // Source get_item_owner checks key-items first then all character inventories,
 // not only current party. Returns real owner inventory; 0 means no owner.
 std::function<bool(uint64_t,uint64_t&inventory,std::string&)>item_owner;
 std::function<bool(uint64_t,uint64_t,bool&removed,std::string&)>inventory_drop;
 std::function<bool(uint64_t,std::string&)>set_global_item;
 std::function<bool(uint32_t,const FieldOpenableDialogue&,std::string&)>open_dialogue;
};
class FieldOpenableDoorRuntime {
public:
 bool initialize(const FieldOpenableDoorData&,FieldOpenableHost,std::string&);bool create(uint32_t);bool ready(uint32_t);bool update_state(uint32_t);
 bool body_entered(uint32_t,uint64_t);bool body_exited(uint32_t,uint64_t);bool timer_timeout(uint32_t);bool idle_frame(uint32_t,float);
 bool lock(uint32_t);bool unlock(uint32_t);bool open(uint32_t);bool close(uint32_t);bool interact(uint32_t);bool interact_item(uint32_t,uint64_t);
 const FieldOpenableInstance*instance(uint32_t)const;const std::string&error()const{return error_;}
private:
 const FieldOpenableDoorData*data_=nullptr;FieldOpenableHost host_;std::map<uint32_t,FieldOpenableInstance>instances_;std::string error_;bool poisoned_=false,had_ready_=false;uint32_t last_ready_=0;
 bool fail(const char*);bool callback(bool);FieldOpenableInstance*get(uint32_t,bool ready=true);bool publish(FieldOpenableInstance&);bool play(FieldOpenableInstance&,FieldOpenableClipRole);bool valid_body(uint32_t,uint64_t,bool&);bool assign_audio(FieldOpenableInstance&,std::string_view,bool);bool use_key(FieldOpenableInstance&,uint64_t);bool dialogue(FieldOpenableInstance&,uint32_t);bool apply(FieldOpenableInstance&,FieldOpenableProperty,bool);bool deferred(FieldOpenableInstance&,bool);
};
}

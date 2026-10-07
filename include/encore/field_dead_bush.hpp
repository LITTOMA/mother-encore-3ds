#pragma once
#include "encore/movement.hpp"
#include <array>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <vector>
namespace encore::upstream {
enum class FieldBushClipRole:uint32_t {Break=1,Grow,Hidden,Idle,Reset};
enum class FieldBushProperty:uint32_t {Frame=1,SpriteVisible,BodyDisabled,HitDisabled,InteractDisabled};
enum class FieldBushDeferredKind:uint32_t {None=0,CutsceneCheckStart=1};
struct FieldBushKey {float time=0;uint32_t value=0;};
struct FieldBushTrack {FieldBushProperty property{};uint32_t update=0;std::vector<FieldBushKey>keys;};
struct FieldBushClip {FieldBushClipRole role{};float length=0;std::string name;std::vector<FieldBushTrack>tracks;};
struct FieldBushDialogue {std::string program,key,en,zh_cn;};
struct FieldBushSound {uint32_t id=0;float gain_db=0;std::string source,pcm,bus;};
struct FieldBushDescriptor {
 uint32_t id=0,parent_id=0,ready_ordinal=0,sprite_id=0,prompt_id=0,body_shape_id=0,hit_shape_id=0,interact_shape_id=0,new_parent_id=0,called_id=0,flags=0,columns=0,rows=0,frame=0,hit_layer=0,hit_mask=0,notifier_id=0;
 bool hit_monitorable=false;FieldBushDeferredKind call_kind{};Vec2 sprite_position{},sprite_offset{},sprite_scale{},notifier_rect_position{},notifier_rect_size{},notifier_position{},notifier_scale{};std::string node,new_parent_path,called_path,call_method,flag,disappear_flag;
};
class FieldBushData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);bool valid()const{return valid_;}
 uint32_t scene_id()const{return scene_id_;}const std::array<uint8_t,20>&source_pin()const{return pin_;}const std::array<uint8_t,32>&scene_hash()const{return scene_;}const std::array<uint8_t,32>&script_hash()const{return script_;}
 const std::vector<FieldBushDescriptor>&records()const{return records_;}const FieldBushDescriptor*record(uint32_t)const;const FieldBushClip*clip(FieldBushClipRole)const;
 const FieldBushDialogue&dialogue(bool bat)const{return dialogues_.at(bat?0:1);}const std::string&bat_flag()const{return bat_flag_;}const FieldBushSound&sound()const{return sound_;}
 uint32_t roots_frame()const{return roots_frame_;}uint32_t vibration_device()const{return vibration_device_;}const std::string&vibration_gate()const{return vibration_gate_;}bool vibration_default()const{return vibration_default_;}const std::array<float,3>&vibration()const{return vibration_;}
 uint32_t width()const{return width_;}uint32_t height()const{return height_;}const std::string&texture_source()const{return texture_source_;}const std::string&texture_path()const{return texture_path_;}const std::array<uint8_t,32>&texture_hash()const{return texture_;}
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 bool valid_=false;uint32_t scene_id_=0,width_=0,height_=0,roots_frame_=0,vibration_device_=0;std::array<float,3>vibration_{};std::array<uint8_t,20>pin_{};std::array<uint8_t,32>scene_{},script_{},texture_{};
 bool vibration_default_=false;std::string texture_source_,texture_path_,bat_flag_,vibration_gate_;FieldBushSound sound_;std::vector<FieldBushDescriptor>records_;std::vector<FieldBushClip>clips_;std::vector<FieldBushDialogue>dialogues_;std::map<std::string,std::array<uint8_t,32>>sources_;
};
struct FieldBushInstance {
 uint32_t id=0,frame=0,new_parent=0,called=0,grow_waiters=0;FieldBushClipRole clip{};float elapsed=0;bool ready=false,visible=true,sprite_visible=true,body_disabled=false,hit_disabled=false,interact_disabled=false,playing=false,queued=false,deleted=false;
 std::vector<uint64_t>roots;
};
struct FieldBushHost {
 // Invoked for every source frame setter, including equal values.
 std::function<bool(uint32_t,uint32_t,std::string&)>native_frame;
 // Native events: play=1, finished=2, stopped=3; clips remain typed core-owned.
 std::function<bool(uint32_t,uint32_t,uint32_t,std::string&)>native_animation;
 // Bind actual source geometry, texture, audio, prompt and deferred receiver
 // identity. Unknown dispatch remains rejected, never a generic approved bit.
 std::function<bool(const FieldBushData&,std::string&)>bind;
 std::function<bool(uint32_t,bool&,std::string&)>resolve_node;
 std::function<bool(std::string_view,bool&present,bool&value,std::string&)>read_flag;
 std::function<bool(uint32_t,std::function<bool(bool)>,std::string&)>connect_viewport;
 std::function<bool(uint32_t,std::function<bool(uint32_t)>,std::string&)>connect_hitbox;
 std::function<bool(uint32_t,const FieldBushInstance&,std::string&)>publish;
 std::function<bool(uint32_t,uint32_t,uint64_t&,std::string&)>duplicate_sprite;
 std::function<bool(uint64_t,uint32_t,std::string&)>roots_frame;
 std::function<bool(uint64_t,uint32_t,std::string&)>roots_add_child;
 std::function<bool(uint32_t,Vec2&,std::string&)>sprite_global_position;
 // Source assigns this value to LOCAL Roots.position after add_child.
 std::function<bool(uint64_t,Vec2,std::string&)>roots_local_position;
 std::function<bool(uint32_t,const FieldBushSound&,std::string&)>audio_play;
 std::function<bool(uint32_t,bool,std::string&)>prompt_enabled;
 // Read the actual global Boolean field named by the checked source gate.
 std::function<bool(std::string_view,bool&,std::string&)>boolean_setting;
 std::function<bool(uint32_t,float,float,float,std::string&)>vibrate;
 std::function<bool(uint32_t,const FieldBushDialogue&,std::string&)>open_dialogue;
 std::function<bool(const FieldBushDescriptor&,FieldBushDeferredKind,std::string&)>call_deferred;
 std::function<bool(uint32_t,std::string&)>queue_free;
};
class FieldBushRuntime {
public:
 const FieldBushData*data()const{return data_;}
 bool initialize(const FieldBushData&,FieldBushHost,std::string&);bool create(uint32_t,bool source_constructor=false);bool ready(uint32_t);bool viewport(uint32_t,bool);bool grow(uint32_t);bool interact(uint32_t);bool hitbox_entered(uint32_t,uint32_t);bool idle_frame(uint32_t,float);bool seek(uint32_t,float,bool);bool commit_deleted(uint32_t);
 const FieldBushInstance*instance(uint32_t)const;const std::string&error()const{return error_;}
private:
 std::set<uint32_t> pending_source_constructor_;
 const FieldBushData*data_=nullptr;FieldBushHost host_;std::map<uint32_t,FieldBushInstance>instances_;std::string error_;uint32_t last_ready_=0;bool had_ready_=false,poisoned_=false;
 bool fail(const char*);bool callback(bool);FieldBushInstance*get(uint32_t,bool ready=true);bool publish(FieldBushInstance&);bool play(FieldBushInstance&,FieldBushClipRole);bool apply(FieldBushInstance&,FieldBushProperty,uint32_t);bool finished(FieldBushInstance&,FieldBushClipRole);
};
}

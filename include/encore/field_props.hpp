#pragma once
#include "encore/field_data.hpp"
#include "encore/prop_clips.hpp"
#include "encore/room_data.hpp"
#include <array>
namespace encore::upstream {
class FieldSwingData;class AudioBank;struct AudioAsset;
enum class FieldPropSoundBus:uint32_t {Sfx=1};
struct FieldPropContactPolicy {
 uint32_t target_layer=0,target_mask=0,target_flags=0,emitter_layer=0,emitter_mask=0,emitter_flags=0;
 float gain_db=0,pitch=0;uint32_t bus=0,pause_mode=0,swing_bytes=0,swing_crc=0;
 std::array<uint8_t,32>sound_sha256{};
};
// ENCPRP01 owns source-local breakable objects, not arbitrary node methods.
struct FieldPropPolicy {
 uint32_t scene=0,hidden=0,idle=0,grow=0,broken=0,clone_frame=0,bat_flag=kFieldNone;
 uint32_t width=0,height=0;std::string hard,easy,sound,texture,clone_parent;
 uint32_t clone_after=kFieldNone;
 std::array<float,4>rumble{};
 FieldPropContactPolicy contact;
};
struct FieldProp {
 uint32_t id=0,sprite=0,body=0,clone_item=0,appear=kFieldNone,finish=kFieldNone,callback=kFieldNone;
 std::string callback_path;
 uint32_t notice=kFieldNone;std::string path;
 Vec2 position{},sprite_center{},view_center{},view_extents{},hit_center{},hit_extents{},interact_center{},interact_extents{},body_extents{};
};
class FieldPropsData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool bind(RoomView,FieldMapView,std::string&)const;
 bool matches_action(const FieldSwingData&,std::string&)const;
 bool matches_audio(const AudioBank&,AudioAsset&,std::string&)const;
 bool valid()const{return valid_;}
 uint32_t capability()const{return capability_;}bool contact_bound()const{return valid_&&capability_==3;}
 uint32_t encoded_bytes()const{return encoded_bytes_;}uint32_t encoded_crc()const{return encoded_crc_;}
 bool rendering_bound()const{return valid_&&policy_.clone_after!=kFieldNone;}
 const FieldPropPolicy&policy()const{return policy_;}
 const PropClipsData&clips()const{return clips_;}
 const std::vector<FieldProp>&props()const{return props_;}
 const std::vector<std::string>&flags()const{return flags_;}
private:
 bool valid_=false;uint32_t capability_=0,encoded_bytes_=0,encoded_crc_=0;FieldPropPolicy policy_;PropClipsData clips_;
 std::vector<FieldProp>props_;std::vector<std::string>flags_;
};
enum class FieldPropEventKind:uint8_t {Rumble=1,Sound,HardDialogue,EasyDialogue};
struct FieldPropEvent {FieldPropEventKind kind;uint32_t prop=0;};
struct FieldPropClone {uint32_t prop=0,frame=0;Vec2 center{};bool visible=false;};
struct FieldPropState {
 PropClipPlayer animation;bool alive=true,visible=false,prompt=true,queued=false,deferred=false;
 uint32_t grow_waiters=0;
};
class FieldPropHost {
public:
 virtual ~FieldPropHost()=default;
 virtual bool can_call(uint32_t callback,std::string&)=0;
 // Called before this object's queued deletion is committed.
 virtual bool call(uint32_t callback,uint32_t prop,std::string&)=0;
};
// A prepared scene owns each source AudioStreamPlayer independently. Reserve
// the complete contact batch before any prop mutation; commits perform no IO
// or fallible allocation. Deletion retires only that source node's voice.
class FieldPropContactEffects {
public:
 virtual ~FieldPropContactEffects()=default;
 virtual bool matches(const FieldPropsData&,std::string&)const=0;
 virtual bool prepare_hits(const FieldPropsData&,const std::vector<uint32_t>&,std::string&)=0;
 virtual void commit_hit(uint32_t)=0;
 virtual void stop_deleted(uint32_t)=0;
};
class FieldProps {
public:
 bool initialize(const FieldPropsData&,const std::vector<bool>&,std::string&);
 bool ready()const{return data_.valid();}
 const FieldPropsData&data()const{return data_;}
 const std::vector<FieldPropState>&states()const{return states_;}
 const std::vector<FieldPropClone>&clones()const{return clones_;}
 bool set_visible(uint32_t,bool,std::string&);
 // Source methods. Contact admission belongs to the scene's typed shapes.
 bool grow(uint32_t,std::string&);bool hit(uint32_t,std::string&);
 bool can_hit(const std::vector<uint32_t>&,std::string&)const;
 bool interact(uint32_t,const std::vector<bool>&,std::string&);
 bool advance(double,const std::vector<bool>&,std::string&);
 bool flush_deferred(FieldPropHost&,std::string&);
 std::vector<FieldPropEvent>take_events(){auto v=std::move(events_);events_.clear();return v;}
private:
 bool available(uint32_t,std::string&)const;
 FieldPropsData data_;std::vector<FieldPropState>states_;std::vector<FieldPropClone>clones_;
 std::vector<FieldPropEvent>events_;
};
}

#pragma once
#include "encore/field_data.hpp"
#include "encore/prop_clips.hpp"
#include "encore/room_data.hpp"
#include <array>
namespace encore::upstream {
// ENCPRP01 owns source-local breakable objects, not arbitrary node methods.
struct FieldPropPolicy {
 uint32_t scene=0,hidden=0,idle=0,grow=0,broken=0,clone_frame=0,bat_flag=kFieldNone;
 uint32_t width=0,height=0;std::string hard,easy,sound,texture,clone_parent;
 std::array<float,4>rumble{};
};
struct FieldProp {
 uint32_t id=0,sprite=0,body=0,clone_item=0,appear=kFieldNone,finish=kFieldNone,callback=kFieldNone;
 Vec2 position{},sprite_center{},view_center{},view_extents{},hit_center{},hit_extents{},interact_center{},interact_extents{},body_extents{};
};
class FieldPropsData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool bind(RoomView,FieldMapView,std::string&)const;
 bool valid()const{return valid_;}
 const FieldPropPolicy&policy()const{return policy_;}
 const PropClipsData&clips()const{return clips_;}
 const std::vector<FieldProp>&props()const{return props_;}
 const std::vector<std::string>&flags()const{return flags_;}
private:
 bool valid_=false;FieldPropPolicy policy_;PropClipsData clips_;
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

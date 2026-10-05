#pragma once
#include "encore/field_data.hpp"
#include <functional>
#include <map>
namespace encore::upstream {
enum class FieldCameraLimit:uint32_t {Top,Left,Right,Bottom};
struct FieldCameraAreaDescriptor {
 uint32_t id=0,ready=0,parent_id=0,shape_id=0,reference_id=0,flags=0,layer=0,mask=0,pause=0;int32_t priority=0;bool reference_exists=false;
 Vec2 position{},scale{},shape_position{},shape_scale{},extents{},camera_offset{};std::array<Vec2,3>world{},shape_world{};std::array<float,4>reference_margins{};std::string node,reference_path,reference_node;
};
class FieldCameraAreaData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);bool load_file(const char*,const FieldIdentity&,std::string&);bool valid()const{return valid_;}bool scene_admitted()const{return false;}
 FieldIdentity identity()const{return identity_;}const std::string&source_scene()const{return scene_;}const std::string&script()const{return script_;}const std::array<uint8_t,32>&script_sha()const{return script_sha_;}const std::array<int32_t,4>&reset_limits()const{return reset_;}
 const std::vector<FieldCameraAreaDescriptor>&records()const{return records_;}const FieldCameraAreaDescriptor*record(uint32_t)const;bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 bool valid_=false;FieldIdentity identity_{};std::string scene_,script_;std::array<uint8_t,32>script_sha_{};std::array<int32_t,4>reset_{};std::vector<FieldCameraAreaDescriptor>records_;std::map<std::string,std::array<uint8_t,32>>sources_;
};
struct FieldCameraAreaObservation {
 bool alive=false,ancestors_admitted=false,signals_admitted=false,shape_admitted=false,reference_alive=false;
 uint32_t reference_id=0;Vec2 local_scale{},shape_extents{},shape_global_position{},reference_size{},viewport_size{};std::array<float,4>reference_margins{};
};
struct FieldCameraAreaState {uint32_t id=0,cached_reference_id=0;bool alive=true,ready=false,inside=false;};
struct FieldCameraAreaHost {
 std::function<bool(const FieldCameraAreaData&,std::string&)>bind;
 std::function<bool(const FieldCameraAreaDescriptor&,std::function<bool(uint64_t)>,std::function<bool(uint64_t)>,std::string&)>connect;
 // Real get_node_or_null executes ONCE during source onready initialization.
 std::function<bool(uint32_t,std::string_view,uint32_t&,std::string&)>resolve_reference;
 std::function<bool(uint32_t,uint32_t cached_reference,FieldCameraAreaObservation&,std::string&)>observe;
 std::function<bool(uint64_t,bool&,std::string&)>is_global_player;
 // Each operation addresses the actual global.currentCamera at call time.
 std::function<bool(int64_t delta,int64_t&result,std::string&)>adjust_camareas;
 std::function<bool(FieldCameraLimit,int32_t,std::string&)>set_limit;
 std::function<bool(std::array<int32_t,4>&,std::string&)>current_limits;
 std::function<bool(Vec2,std::string&)>set_camarea_offset;
 std::function<bool(uint64_t,std::function<bool()>,std::string&)>await_idle_frame;
 std::function<bool(uint64_t,std::string&)>cancel_idle_frame;
};
class FieldCameraAreaRuntime {
public:
 bool initialize(const FieldCameraAreaData&,FieldCameraAreaHost,std::string&);bool create(uint32_t);bool ready(uint32_t);bool body_enter(uint32_t,uint64_t);bool body_exit(uint32_t,uint64_t);bool resume_idle(uint64_t);bool get_size(uint32_t,Vec2&);bool get_area_global_position(uint32_t,Vec2&);bool exit_tree(uint32_t);
 const FieldCameraAreaState*state(uint32_t)const;const FieldCameraAreaData*data()const{return data_;}const std::string&error()const{return error_;}
private:
 const FieldCameraAreaData*data_=nullptr;FieldCameraAreaHost host_;std::map<uint32_t,FieldCameraAreaState>states_;std::map<uint64_t,uint32_t>awaits_;uint64_t next_await_=1;uint32_t last_ready_=0;bool had_ready_=false,poisoned_=false;std::string error_;
 FieldCameraAreaState*get(uint32_t,bool ready=true);bool fail(const char*);bool observe(FieldCameraAreaState&,FieldCameraAreaObservation&);bool reset();bool limit(FieldCameraLimit,double);bool size(const FieldCameraAreaObservation&,Vec2&);
};
}

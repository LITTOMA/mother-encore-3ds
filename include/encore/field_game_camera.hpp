#pragma once
#include "encore/field_camera_area.hpp"
#include "encore/source_random.hpp"
#include <functional>
#include <map>
namespace encore::upstream {
enum class FieldCameraTuning:size_t {Limit,ScopeSpeed,ScopeVertical,ScopeHorizontal,ShakeInterval,ScopeReturn,ReturnTime,ShakeFinalTime,ShakeMagnitude,ShakeLength,ShakeWeight,ShakeMinimum,ShakeSmall,ShakeLast,ShakeDirect,ShakeSide};
enum class FieldCameraProperty:uint32_t {Position,GlobalPosition,BaseOffset,ShakeOffset};
enum class FieldScopeOperation:uint32_t {Show,Hide,GlobalPosition,Input};
struct FieldGameCameraDescriptor {
 uint32_t id=0,ready=0,parent_id=0,arrows_id=0,animation_id=0,animation_ready=0,area_id=0,shape_id=0,flags=0,pause=0,physics_interpolation=0,area_layer=0,area_mask=0,area_flags=0;int32_t z=0,priority=0;std::array<int32_t,4>limits{};Vec2 position{},offset{},zoom{},area_position{},area_scale{},shape_position{},shape_scale{},shape_extents{};std::array<Vec2,3>world{};float rotation=0;std::string node;
};
class FieldGameCameraData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);bool load_file(const char*,const FieldIdentity&,std::string&);bool valid()const{return valid_;}bool scene_admitted()const{return false;}FieldIdentity identity()const{return identity_;}const std::string&source_scene()const{return scene_;}const std::string&script()const{return script_;}const std::string&shaker_script()const{return shaker_;}const std::string&scope_action()const{return scope_action_;}const std::array<uint8_t,32>&script_sha()const{return script_sha_;}const std::array<uint8_t,32>&shaker_sha()const{return shaker_sha_;}bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
 double tuning(FieldCameraTuning k)const{return tuning_[size_t(k)];}const std::array<uint32_t,2>&player_states()const{return player_states_;}Vec2 shake_direction()const{return shake_direction_;}Vec2 shake_side_amplitude()const{return shake_side_amplitude_;}const std::array<float,3>&animation_lengths()const{return lengths_;}const std::vector<FieldGameCameraDescriptor>&records()const{return records_;}const FieldGameCameraDescriptor*record(uint32_t)const;
private:
 bool valid_=false;FieldIdentity identity_{};std::string scene_,script_,shaker_,scope_action_;std::array<uint8_t,32>script_sha_{},shaker_sha_{};std::array<double,16>tuning_{};std::array<uint32_t,2>player_states_{};std::array<float,3>lengths_{};Vec2 shake_direction_{},shake_side_amplitude_{};std::vector<FieldGameCameraDescriptor>records_;std::map<std::string,std::array<uint8_t,32>>sources_;
};
struct FieldGameCameraState {uint32_t id=0;bool alive=true,ready=false,current=false,shaking=false;Vec2 position{},offset{},base{},shake{},camarea{},screen_center{},canvas_origin{};int64_t camareas=0;std::array<int32_t,4>limits{};uint64_t shared_tween=0;uint32_t animation_role=0;float animation_time=0;bool animation_playing=false;};
struct FieldGameCameraObservation {bool alive=false,ancestors_admitted=false,native_children_ready=false,listeners_admitted=false,native_geometry_admitted=false,physics_interpolation_enabled=false,parent_is_global_player=false,can_process=false,scope_visible=false;Vec2 viewport{},controls{};std::array<Vec2,3>parent_world{};uint64_t global_player=0,current_camera=0;bool in_battle=false,player_damaged=false;uint32_t player_state=0;bool scope_pressed=false,scope_just_pressed=false,scope_just_released=false;};
struct FieldCameraTweenTrack {FieldCameraProperty property=FieldCameraProperty::Position;Vec2 target{},initial{};bool explicit_initial=false;};
struct FieldCameraTweenState {uint64_t token=0,continuation_owner=0;uint32_t camera=0,transition=0,ease=0,continuation=0;float duration=0,elapsed=0;bool started=false,alive=true,running=true;std::vector<FieldCameraTweenTrack>tracks;};
struct FieldCameraShakerState {uint64_t token=0;uint32_t camera=0;bool alive=true,child_alive=true,stopped=false,playing=true,diminish=true,awaiting_idle=false;int64_t shakes_left=0;double timer=0,magnitude=0,length=0,interval=0,weight=0,reduction=0,side=0;Vec2 direction{},dir{},old_offset{},side_amplitude{};};
struct FieldGameCameraHost {
 std::function<bool(const FieldGameCameraData&,std::string&)>bind;
 std::function<bool(uint32_t,FieldGameCameraObservation&,std::string&)>observe;
 std::function<bool(uint32_t,const FieldGameCameraState&,std::string&)>publish;
 // Publish native Camera2D canvas transform only for the actual current camera.
 std::function<bool(uint32_t,Vec2 origin,Vec2 screen_center,std::string&)>publish_canvas;
 std::function<bool(uint32_t,std::function<bool()>,std::function<bool()>,std::string&)>connect_player;
 std::function<bool(uint32_t arrows,FieldScopeOperation,Vec2,std::string&)>scope;
 std::function<bool(uint32_t arrows,Vec2 direction,bool,std::string&)>arrow_visible;
 std::function<bool(std::string&)>info_plates_hide,player_exit_camera;
 std::function<bool(uint32_t,uint32_t role,bool started,std::string&)>animation_signal;
 std::function<bool(uint32_t,FieldGameCameraState&,std::string&)>current_camera_snapshot;
 std::function<bool(uint32_t,std::string&)>make_current,set_global_current;
 // Real global SceneTree creation order, bound node pause mode. Callbacks step
 // ONLY at actual idle tween traversal; newly added jobs respect snapshot tail.
 std::function<bool(uint32_t,uint64_t,std::function<bool(float)>,std::string&)>register_tween;
 std::function<bool(uint64_t,std::string&)>kill_tween;
 std::function<bool(uint64_t,uint32_t property_index,bool tween_finished,std::string&)>tween_signal;
 std::function<bool(uint64_t,std::string&)>coroutine_completed;
 // Construct/enter actual source Shaker child at call time, keeping global
 // physics order and one shared random stream; free is deferred after signals.
 std::function<bool(uint32_t,uint64_t,std::function<bool(float)>,std::string&)>create_shaker;
 std::function<bool(uint64_t,std::string&)>shaker_finished,queue_free_shaker;
 std::function<bool(uint64_t,std::function<bool()>,std::string&)>await_idle_frame;
 std::function<bool(uint64_t,std::string&)>cancel_idle_frame;
 std::function<bool(uint32_t,std::string&)>stopped_shaking;
};
class FieldGameCameraRuntime {
public:
 bool initialize(const FieldGameCameraData&,SourceRandom&,FieldGameCameraHost,std::string&);bool create(uint32_t);bool ready(uint32_t);bool idle(uint32_t,float);bool physics(uint32_t,float);bool input(uint32_t);bool animation_idle(uint32_t,float);bool player_pause(uint32_t);bool scoping_start(uint32_t);bool scoping_stop(uint32_t);bool scoping_process(uint32_t);bool native_update(uint32_t);
 bool set_position(uint32_t,Vec2,bool global=false);bool set_limit(uint32_t,FieldCameraLimit,int32_t);bool adjust_camareas(uint32_t,int64_t,int64_t&);bool set_camarea_offset(uint32_t,Vec2);bool get_offset_with_camerea_offset(uint32_t,Vec2&);bool reset(uint32_t);bool set_current(uint32_t);
 bool move_camera(uint32_t,Vec2,float,uint64_t&,uint32_t transition=1,uint32_t ease=1);bool move_offset(uint32_t,Vec2,float,uint64_t&);bool return_camera(uint32_t,float,uint64_t&);bool return_offset(uint32_t,float,uint64_t&);bool step_tween(uint64_t,float);bool return_camera(uint32_t id,uint64_t&token){return return_camera(id,float(tuning(FieldCameraTuning::ReturnTime)),token);}bool return_offset(uint32_t id,uint64_t&token){return return_offset(id,float(tuning(FieldCameraTuning::ReturnTime)),token);}
 bool shake_camera(uint32_t,double magnitude,double length,Vec2 direction,double interval,double weight,bool diminish,uint64_t&);bool shake_camera(uint32_t,uint64_t&);bool shaker_physics(uint64_t,float);bool pause_shaker(uint64_t);bool stop_shaker(uint64_t);bool shaker_deleted(uint64_t);bool set_shake_side_amplitude(uint64_t,Vec2);bool resume_shake_idle(uint64_t);bool exit_tree(uint32_t);
 const FieldGameCameraState*state(uint32_t)const;const std::map<uint64_t,FieldCameraTweenState>&tweens()const{return tweens_;}const std::map<uint64_t,FieldCameraShakerState>&shakers()const{return shakers_;}const std::string&error()const{return error_;}
 const FieldGameCameraData*data()const{return data_;}
private:
 const FieldGameCameraData*data_=nullptr;SourceRandom*random_=nullptr;FieldGameCameraHost host_;std::map<uint32_t,FieldGameCameraState>states_;std::map<uint64_t,FieldCameraTweenState>tweens_;std::map<uint64_t,FieldCameraShakerState>shakers_;uint64_t next_=1;uint32_t last_ready_=0;bool had_ready_=false,poisoned_=false;std::string error_;
 FieldGameCameraState*get(uint32_t,bool ready=true);bool fail(const char*);bool observe(uint32_t,FieldGameCameraObservation&);bool publish(FieldGameCameraState&);bool kill_shared(FieldGameCameraState&,bool running_only);bool tween(FieldGameCameraState&,std::vector<FieldCameraTweenTrack>,float,uint32_t,uint32_t,uint32_t,uint64_t&);bool property(FieldGameCameraState&,FieldCameraProperty,Vec2);bool read_property(FieldGameCameraState&,FieldCameraProperty,Vec2&);bool play_animation(FieldGameCameraState&,uint32_t);bool complete_tween(FieldCameraTweenState&);double tuning(FieldCameraTuning k)const{return data_->tuning(k);}
};
}

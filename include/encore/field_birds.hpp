#pragma once
#include "encore/field_data.hpp"
#include "encore/source_random.hpp"
#include <functional>
#include <map>
#include <set>
namespace encore::upstream {
enum class FieldBirdClipRole:uint32_t {Idle=1,Fly,Hop,HopRight,Reset,Prepare};
enum class FieldBirdProperty:uint32_t {Frame=1,Offset,RootZ};
struct FieldBirdKey {float time=0,transition=0;Vec2 value{};};
struct FieldBirdTrack {FieldBirdProperty property{};uint32_t update=0;std::vector<FieldBirdKey>keys;};
struct FieldBirdClip {FieldBirdClipRole role{};std::string name;float length=0;bool loop=false;std::vector<FieldBirdTrack>tracks;};
struct FieldBirdSkin {uint32_t index=0,width=0,height=0;std::string source,path;std::array<uint8_t,32>source_sha{},output_sha{};};
struct FieldBirdDescriptor {
 // Child order: Sprite, Area, areaShape, disabledBodyShape, AnimationPlayer,
 // VisibilityNotifier2D, Timer. Their native Ready ordinals are independent.
 uint32_t id=0,ready=0,parent_id=0,profile=0,animation_ready=0,timer_ready=0,frame=0,flags=0,body_layer=0,body_mask=0,area_layer=0,area_mask=0;
 std::array<uint32_t,7>children{};std::array<uint32_t,3>pause{};std::array<int32_t,3>priority{};int32_t initial_z=0;
 float body_radius=0,safe_margin=0,area_radius=0,timer_wait=0,animation_speed=0;Vec2 position{},sprite_position{},sprite_offset{},area_center{},notifier_position{},notifier_scale{};
 std::array<Vec2,3>parent{};std::array<float,4>notifier{},modulate{},self_modulate{},sprite_modulate{},sprite_self_modulate{};std::string node;
};
struct FieldBirdRules {uint32_t skin_mod=0,facing_mod=0,facing_value=0,speed_mod=0;float speed_base=0,seek_factor=0;Vec2 right{},left{};int32_t flight_z=0;};
class FieldBirdData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);bool load_file(const char*,const FieldIdentity&,std::string&);bool valid()const{return valid_;}bool scene_admitted()const{return false;}
 FieldIdentity identity()const{return identity_;}const std::string&source_scene()const{return scene_;}const std::string&script()const{return script_;}const std::array<uint8_t,32>&script_sha()const{return script_sha_;}
 uint32_t columns()const{return columns_;}uint32_t rows()const{return rows_;}bool pixel_snap()const{return pixel_snap_;}const FieldBirdRules&rules()const{return rules_;}
 const std::vector<FieldBirdDescriptor>&records()const{return records_;}const FieldBirdDescriptor*record(uint32_t)const;const FieldBirdClip*clip(uint32_t profile,FieldBirdClipRole)const;const FieldBirdSkin*skin(uint32_t)const;const std::vector<FieldBirdSkin>&skins()const{return skins_;}
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 bool valid_=false,pixel_snap_=false;FieldIdentity identity_{};std::string scene_,script_;std::array<uint8_t,32>script_sha_{};uint32_t columns_=0,rows_=0;FieldBirdRules rules_{};std::vector<FieldBirdDescriptor>records_;std::vector<std::vector<FieldBirdClip>>profiles_;std::vector<FieldBirdSkin>skins_;std::map<std::string,std::array<uint8_t,32>>sources_;
};
struct FieldBirdState {
 uint32_t id=0,skin=0,frame=0;int32_t z_index=0;bool ready=false,alive=true,visible=false,process=false,flip=false,animation_playing=false,started=false,seeked=false,timer_running=false;
 float speed=0,animation_time=0;double timer_left=-1;FieldBirdClipRole clip{};Vec2 position{},start{},velocity{},input{},sprite_offset{};
};
struct FieldBirdBody {bool global_player=false;Vec2 local_position{};};
struct FieldBirdObservation {
 bool alive=false,ancestors_admitted=false,source_signals_admitted=false,disabled_body_shape=false,clear_floor_state=false,ancestors_visible=false,notifier_on_screen=false,material_admitted=false;
 // Current admitted ancestor transform/color; excludes this bird root/Sprite.
 std::array<Vec2,3>parent{};std::array<float,4>canvas_color{};
};
struct FieldBirdHost {
 std::function<bool(const FieldBirdData&,std::string&)>bind;
 std::function<bool(uint32_t,FieldBirdObservation&,std::string&)>observe;
 // Source connections are installed before Ready. Animation finished must
 // dispatch the registered slot once through the actual synchronous signal bus.
 std::function<bool(const FieldBirdDescriptor&,std::function<bool(uint64_t)>,std::function<bool()>,std::function<bool()>,std::function<bool(FieldBirdClipRole)>,std::string&)>connect;
 std::function<bool(uint64_t,FieldBirdBody&,std::string&)>body_snapshot;
 std::function<bool(uint32_t,const FieldBirdSkin&,std::string&)>texture;
 std::function<bool(uint32_t,const FieldBirdState&,std::string&)>publish;
 std::function<bool(uint32_t,bool started,FieldBirdClipRole,std::string&)>animation_signal;
 std::function<bool(uint32_t,std::string&)>timer_timeout;
};
struct FieldBirdDraw {uint32_t id=0,skin=0,frame=0;int32_t z_index=0;bool visible=false,flip=false,pixel_snap=false;Vec2 origin{},scale{};std::array<float,4>color{};};
class FieldBirdRuntime {
public:
 bool initialize(const FieldBirdData&,SourceRandom&,FieldBirdHost,std::string&);bool create(uint32_t,bool source_constructor=false);bool ready(uint32_t);bool screen_enter(uint32_t);bool screen_exit(uint32_t);bool body_enter(uint32_t,uint64_t);
 bool animation_finished(uint32_t,FieldBirdClipRole);bool play(uint32_t,FieldBirdClipRole);bool seek(uint32_t,float,bool update=true);
 // Native Timer/AnimationPlayer internal leaves and script normal process
 // are separate source schedule entries. Offscreen hiding never stops children.
 bool idle_leaf(uint32_t,double,bool can_process,bool timer_paused=false);bool idle_process(uint32_t,double,bool can_process);bool exit_tree(uint32_t);bool draw(uint32_t,FieldBirdDraw&);
 const FieldBirdState*state(uint32_t)const;const FieldBirdData*data()const{return data_;}const std::string&error()const{return error_;}
private:
 std::set<uint32_t> pending_source_constructor_;
 const FieldBirdData*data_=nullptr;SourceRandom*random_=nullptr;FieldBirdHost host_;std::map<uint32_t,FieldBirdState>states_;uint32_t last_ready_=0;bool had_ready_=false,poisoned_=false;std::string error_;
 bool fail(const char*);FieldBirdState*get(uint32_t,bool ready=true);bool observe(uint32_t,FieldBirdObservation&);bool publish(FieldBirdState&);bool prepare(FieldBirdState&);bool animate(FieldBirdState&,float);bool apply(FieldBirdState&,const FieldBirdClip&,const FieldBirdTrack&,float,float);bool write(FieldBirdState&,FieldBirdProperty,Vec2);
};
}

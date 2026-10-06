#pragma once
#include "encore/field_data.hpp"
#include <functional>
#include <map>
namespace encore::upstream {
enum class FieldArrowProperty:uint32_t {Frame=1,Offset,Playing,Position,Visible};
enum class FieldArrowSignal:uint32_t {FrameChanged=1,AnimationFinished};
struct FieldArrowKey {float time=0,transition=0;Vec2 value{};};
struct FieldArrowTrack {uint32_t target=0,update=0;FieldArrowProperty property{};std::vector<FieldArrowKey>keys;};
struct FieldArrowClip {uint32_t role=0;std::string name;float length=0;bool loop=false;std::vector<FieldArrowTrack>tracks;};
struct FieldArrowRoot {uint32_t id=0,ready=0,parent_id=0,pause=0;int32_t z=0,priority=0;bool visible=false,z_relative=false;std::array<uint32_t,4>arrows{};Vec2 position{};std::array<float,4>modulate{},self_modulate{};std::string node;};
struct FieldArrowSprite {uint32_t id=0,root_id=0,ready=0,direction=0,frame=0,flags=0,pause=0;int32_t priority=0;Vec2 position{},offset{},scale{};float rotation=0,speed_scale=0,animation_speed=0;std::array<float,4>modulate{},self_modulate{};};
struct FieldArrowPlayer {uint32_t id=0,ready=0,target_root=0,kind=0,profile=0,pause=0;int32_t priority=0;float speed=0;};
struct FieldArrowAsset {std::string source,path;uint32_t width=0,height=0,bytes=0,crc=0;std::array<uint8_t,32>source_sha{},output_sha{};};
class FieldCameraArrowsData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);bool load_file(const char*,const FieldIdentity&,std::string&);bool valid()const{return valid_;}bool scene_admitted()const{return false;}
 FieldIdentity identity()const{return identity_;}const std::string&source_scene()const{return scene_;}const std::string&script()const{return script_;}const std::array<uint8_t,32>&script_sha()const{return script_sha_;}bool pixel_snap()const{return pixel_snap_;}const FieldArrowAsset&asset()const{return asset_;}const FieldArrowAsset&program()const{return program_;}const std::array<Vec2,4>&directions()const{return directions_;}
 const std::vector<FieldArrowRoot>&records()const{return roots_;}const std::vector<FieldArrowSprite>&sprites()const{return sprites_;}const std::vector<FieldArrowPlayer>&players()const{return players_;}const std::vector<std::array<float,4>>&frames()const{return frames_;}
 const FieldArrowRoot*record(uint32_t)const;const FieldArrowSprite*sprite(uint32_t)const;const FieldArrowPlayer*player(uint32_t)const;const FieldArrowPlayer*player_for_target(uint32_t)const;const FieldArrowClip*clip(uint32_t,uint32_t)const;bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 bool valid_=false,pixel_snap_=false;FieldIdentity identity_{};std::string scene_,script_;std::array<uint8_t,32>script_sha_{};FieldArrowAsset asset_,program_;std::array<Vec2,4>directions_{};std::vector<FieldArrowRoot>roots_;std::vector<FieldArrowSprite>sprites_;std::vector<FieldArrowPlayer>players_;std::vector<std::vector<FieldArrowClip>>profiles_;std::vector<std::array<float,4>>frames_;std::map<std::string,std::array<uint8_t,32>>sources_;
};
struct FieldArrowRootState {uint32_t id=0;bool ready=false,alive=true,visible=false,show_arrows=false;Vec2 position{},offset{},bounds{};};
struct FieldArrowSpriteState {uint32_t id=0,frame=0;bool alive=true,playing=false,visible=false,is_over=false;float timeout=0;Vec2 position{},offset{};};
struct FieldArrowPlayerState {uint32_t id=0,assigned=0;float time=0,scale=1;bool alive=true,playing=false;};
struct FieldArrowObservation {
 bool alive=false,ancestors_admitted=false,descendants_ready=false,listeners_admitted=false,can_process=false,update_pending=false,visible_in_tree=false,material_admitted=false;
 std::array<Vec2,3>world{},parent{};std::array<float,4>canvas_color{};
};
struct FieldCameraArrowsHost {
 std::function<bool(const FieldCameraArrowsData&,std::string&)>bind;
 std::function<bool(uint32_t,FieldArrowObservation&,std::string&)>observe;
 std::function<bool(uint32_t,std::function<bool(uint32_t)>,std::string&)>connect_animation_finished;
 std::function<bool(uint32_t,const FieldArrowRootState&,std::string&)>publish_root;
 std::function<bool(uint32_t,const FieldArrowSpriteState&,std::string&)>publish_sprite;
 std::function<bool(uint32_t,bool started,uint32_t clip,std::string&)>animation_signal;
 std::function<bool(uint32_t,FieldArrowSignal,std::string&)>sprite_signal;
 std::function<bool(uint32_t,uint64_t,std::function<bool()>,std::string&)>await_frame_changed;
 std::function<bool(uint32_t,uint64_t,std::string&)>cancel_frame_changed;
 std::function<bool(std::vector<Vec2>&pressed,std::vector<Vec2>&released,std::string&)>control_directions;
};
struct FieldArrowDraw {uint32_t id=0,frame=0;bool visible=false,pixel_snap=false;std::array<Vec2,4>world_vertices{};std::array<float,4>color{};};
class FieldCameraArrowsRuntime {
public:
 bool initialize(const FieldCameraArrowsData&,FieldCameraArrowsHost,std::string&);bool create(uint32_t);bool ready(uint32_t);bool show(uint32_t);bool hide(uint32_t);bool handle_input_events(uint32_t);bool point_directions(uint32_t,const std::vector<Vec2>&);bool unpoint_directions(uint32_t,const std::vector<Vec2>&);bool point_dir_sum(uint32_t,Vec2);bool set_offset(uint32_t,Vec2);bool set_bounds(uint32_t,Vec2);bool set_arrow_visible(uint32_t,Vec2,bool);bool global_position(uint32_t,Vec2);bool on_animation_finished(uint32_t,uint32_t);bool resume_frame(uint64_t);bool idle_leaf(uint32_t,float);bool exit_tree(uint32_t);bool draw(uint32_t,FieldArrowDraw&);
 const FieldArrowRootState*root_state(uint32_t)const;const FieldArrowSpriteState*sprite_state(uint32_t)const;const FieldArrowPlayerState*player_state(uint32_t)const;const FieldCameraArrowsData*data()const{return data_;}const std::string&error()const{return error_;}
private:
 struct Wait {uint32_t sender=0,receiver=0,root=0;};
 const FieldCameraArrowsData*data_=nullptr;FieldCameraArrowsHost host_;std::map<uint32_t,FieldArrowRootState>roots_;std::map<uint32_t,FieldArrowSpriteState>sprites_;std::map<uint32_t,FieldArrowPlayerState>players_;std::map<uint64_t,Wait>waits_;uint64_t next_wait_=1;uint32_t last_ready_=0;bool had_ready_=false,poisoned_=false;std::string error_;
 bool fail(const char*);FieldArrowRootState*get(uint32_t,bool ready=true);bool observe(uint32_t,FieldArrowObservation&);bool publish(FieldArrowRootState&);bool publish(FieldArrowSpriteState&);bool refresh(FieldArrowRootState&,bool);bool visibility(FieldArrowRootState&);bool directions(uint32_t,const std::vector<Vec2>&,uint32_t);bool play(uint32_t,uint32_t,float scale=1,bool from_end=false);bool animate(FieldArrowPlayerState&,float);bool apply(FieldArrowPlayerState&,const FieldArrowClip&,const FieldArrowTrack&,float,float);bool write(FieldArrowPlayerState&,uint32_t,FieldArrowProperty,Vec2);bool frame(uint32_t,int32_t);bool playing(uint32_t,bool);bool sprite_idle(FieldArrowSpriteState&,float);float duration(uint32_t)const;
};
}

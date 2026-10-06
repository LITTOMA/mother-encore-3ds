#pragma once
#include "encore/field_geometry.hpp"
#include <functional>
#include <utility>

namespace encore::upstream {
struct FieldDoorDescriptor {
 uint32_t id=0,ready=0,node=0,marker=0,audio=0,shape=0,layer=0,mask=0,flags=0,pause_mode=0;
 uint32_t target_name=0,target_path=0,sound=0,end_sound=0,in_anim=0,out_anim=0,flag=0;
 Vec2 target{},direction{};float in_speed=0,out_speed=0,music_fade=0;
 std::array<float,4>in_color{},out_color{};FieldGeometryTransform body_transform{},marker_transform{};Vec2 shape_offset{},extents{};
};
struct FieldDoorAudio {uint32_t id=0,source=0,bus=0;std::array<uint8_t,32>sha{};};
class FieldDoorData {
public:
 bool load(const uint8_t*,size_t,const FieldIdentity&,std::string&);bool load_file(const char*,const FieldIdentity&,std::string&);
 bool valid()const{return !bytes_.empty();}bool scene_admitted()const{return false;}
 FieldIdentity identity()const;std::string_view source_scene()const;std::string_view string(uint32_t)const;
 uint32_t door_count()const;FieldDoorDescriptor door(uint32_t)const;bool find(uint32_t,FieldDoorDescriptor&)const;
 FieldDoorAudio audio(uint32_t)const;bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
 std::string_view script()const;std::array<uint8_t,32>script_sha()const;std::string_view none()const;float ground_offset()const;
private:
 bool admit(std::vector<uint8_t>&&,const FieldIdentity&,std::string&);uint32_t count(uint32_t)const;const uint8_t*record(uint32_t,uint32_t)const;std::vector<uint8_t>bytes_;
};
enum class FieldDoorSignal:uint32_t {Entered,MovedPlayer,Done};
// Each step maps to the original deferred SceneTransition method. The backend
// owns real nodes and must fail on any pending mechanism. Preflight parses and
// admits all required destination consumers without calling Ready or using RNG.
enum class FieldDoorSceneStep:uint32_t {
 DetachPlayer,DisablePlayerCollisions,DetachPersistent,InstanceDestination,
 LeaveOldArea,FreeOldScene,AssignCurrentScene,InitParameters,AddRootAndReady,
 AddPlayer,CreateFollowers,SetPartyPosition,ReparentPersistent,SetTreeCurrent,
 UpdateKeyIndicator,EnablePlayerCollisions
};
struct FieldDoorCandidate {uint64_t token=0;std::string path;std::array<uint8_t,20>pin{};std::array<uint8_t,32>source_sha{};};
struct FieldDoorContext {uint64_t player=0;bool entering=false,in_cutscene=false;std::string current_scene_name;};
struct FieldDoorHost {
 std::function<bool(FieldDoorContext&,std::string&)>observe;
 // Resolve actual source Position2D/AudioStreamPlayer and bind actual body bus.
 std::function<bool(const FieldDoorDescriptor&,const FieldDoorAudio&,std::string&)>resolve_onready;
 std::function<bool(uint32_t,std::function<bool(uint64_t,std::string&)>,std::string&)>connect_body;
 std::function<bool(const FieldDoorDescriptor&,FieldDoorCandidate&,std::string&)>prepare_destination;
 std::function<bool(const FieldDoorCandidate&,bool committed,std::string&)>release_candidate;
 std::function<bool(uint64_t,bool,bool,std::string&)>pause_player;
 std::function<bool(bool,std::string&)>set_entering;
 std::function<bool(std::string_view,bool&,std::string&)>flag_exists;
 std::function<bool(std::string_view,bool,std::string&)>write_flag;
 std::function<bool(std::vector<uint64_t>&,std::string&)>music_changers;
 std::function<bool(uint64_t,float,std::string&)>stop_music;
 std::function<bool(uint32_t,std::string_view,const std::array<uint8_t,32>&,std::string&)>play_audio;
 std::function<bool(bool in,std::string_view,const std::array<float,4>&,float,std::string&)>fade;
 std::function<bool(uint32_t,FieldDoorSignal,std::string&)>emit;
 std::function<bool(uint32_t,Vec2&,std::string&)>marker_world;
 std::function<bool(uint64_t,Vec2,std::string&)>set_player_global_position;
 std::function<bool(uint32_t,bool add,std::string&)>persistent;
 std::function<bool(uint32_t,std::string&)>schedule_deferred;
 std::function<bool(std::string&)>clear_enemies;
 std::function<bool(FieldDoorSceneStep,const FieldDoorCandidate&,uint64_t,Vec2,Vec2,std::string&)>scene_step;
 std::function<bool(std::string&)>emit_scene_changed;
 std::function<bool(uint64_t,std::string&)>camera_current_and_visible;
 std::function<bool(std::string&)>fade_cut;
 std::function<bool(uint64_t,Vec2,std::string&)>direction_and_input;
 // Update every actual breadcrumb in source order, then each real follower's
 // position/reinit/disappear. This must use local player position after warp.
 std::function<bool(uint64_t,std::string&)>update_party;
 std::function<bool(uint64_t,std::string&)>unpause_player;
 std::function<bool(uint32_t,std::string&)>queue_free;
 // Source respawn only snapshots position/filename/run_sound/shadow; no disk IO.
 std::function<bool(std::string&)>set_respawn;
};
enum class FieldDoorPhase:uint32_t {Idle,BodyIdle,FadeIn,Deferred,TreeChanged,SceneIdle,PartyIdle,FadeMostly,Done,Poisoned};
class FieldDoorRuntime {
public:
 bool initialize(const FieldDoorData&,FieldDoorHost,std::string&);bool ready(uint32_t,std::string&);
 bool body_entered(uint32_t,uint64_t,std::string&);bool enter(uint32_t,uint64_t,std::string&);
 bool idle_frame(std::string&);bool fade_in_done(std::string&);bool deferred_commit(std::string&);bool tree_changed(std::string&);bool fade_out_mostly_done(std::string&);
 // Source helper is uncalled in the pinned project; explicitly unavailable.
 bool special_guest(uint32_t,std::string&);
 const FieldDoorData*data()const{return data_;}
 bool source_ready(uint32_t id)const{for(auto ready:ready_)if(ready==id)return phase_!=FieldDoorPhase::Poisoned;return false;}
 FieldDoorPhase phase()const{return phase_;}uint32_t active_door()const{return active_.id;}
private:
 const FieldDoorData*data_=nullptr;FieldDoorHost host_;std::vector<uint32_t>ready_;FieldDoorDescriptor active_{};FieldDoorCandidate candidate_{};
 std::vector<std::pair<uint32_t,uint64_t>>body_waiters_;
 uint64_t player_=0;bool same_=true,had_ready_=false;uint32_t last_ready_=0;FieldDoorPhase phase_=FieldDoorPhase::Idle;
 bool select(uint32_t,uint64_t,const FieldDoorContext&,std::string&);bool prepare(const FieldDoorContext&,std::string&);bool start(std::string&);bool after_warp(std::string&);bool audio(uint32_t,std::string&);bool poison(std::string&);
};
}

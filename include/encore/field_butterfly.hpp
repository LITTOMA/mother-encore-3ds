#pragma once
#include "encore/battle_data.hpp"
#include "encore/source_random.hpp"
#include <functional>
#include <map>
#include <array>
namespace encore::upstream {
struct FieldButterflyConnection{uint32_t role=0;std::string signal,method;};
struct FieldButterflyKey{float time=0,transition=0;Vec2 value{};};
struct FieldButterflyTrack{uint32_t role=0;bool discrete=false;std::vector<FieldButterflyKey>keys;};
struct FieldButterflyClip{uint32_t role=0;std::string name;float length=0;std::vector<FieldButterflyTrack>tracks;};
struct FieldButterflyAsset{uint32_t index=0,width=0,height=0,bytes=0,crc=0;std::string source,path;};
struct FieldButterflyBinding{
 uint32_t id=0,ready_ordinal=0,timer_id=0,timer_ordinal=0,fly_id=0,fly_ordinal=0,orbit_id=0,orbit_ordinal=0,area_id=0,notifier_id=0,enabler_id=0,frame=0,area_layer=0,area_mask=0,area_flags=0;bool flip=false;std::string node;float parent_scale=0,timer_wait=0,fly_speed=0,orbit_speed=0,area_radius=0;Vec2 position{},parent_origin{},sprite_position{},sprite_offset{},area_center{};BattleValue color{},notifier{},enabler{};
};
class FieldButterflyData{
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);bool valid()const{return valid_;}const std::array<uint8_t,20>&source_pin()const{return pin_;}const std::string&scene()const{return scene_;}const std::string&script()const{return script_;}
 const std::vector<FieldButterflyBinding>&bindings()const{return bindings_;}const FieldButterflyBinding*binding(uint32_t)const;const std::vector<FieldButterflyAsset>&assets()const{return assets_;}const FieldButterflyAsset*asset(uint32_t)const;const FieldButterflyClip*clip(uint32_t role)const;
 bool pixel_snap()const{return pixel_snap_;}uint32_t modulus()const{return modulus_;}uint32_t frames()const{return frames_;}float speed()const{return speed_;}double fly_seek()const{return fly_seek_;}double orbit_seek()const{return orbit_seek_;}
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
 const std::vector<FieldButterflyConnection>&connections()const{return connections_;}
private:
 std::vector<FieldButterflyConnection>connections_;
 std::map<std::string,std::array<uint8_t,32>>sources_;
 bool valid_=false,pixel_snap_=false;std::array<uint8_t,20>pin_{};std::string scene_,script_;uint32_t modulus_=0,frames_=0;float speed_=0;double fly_seek_=0,orbit_seek_=0;std::vector<FieldButterflyBinding>bindings_;std::vector<FieldButterflyAsset>assets_;std::vector<FieldButterflyClip>clips_;
};
struct FieldButterflyState{uint32_t id=0,asset=0,frame=0;bool ready=false,alive=true,visible=false,process=false,fly_playing=false,orbit_playing=false,flip=false,returning=false,timer_running=false;float fly_time=0,orbit_time=0;double timer_left=0;Vec2 position{},start{},velocity{},input{},sprite_position{},sprite_offset{};std::vector<uint32_t>waiters;};
struct FieldButterflyBody{bool party_member_player=false,global_player=false,walking=false;Vec2 world{};};
struct FieldButterflyHost{
 // Admit original Kinematic shape-disabled/layer0/mask0 policy, actual circle
 // Area and notifier/enabler lifecycle, source four signal connections and
 // absence of unsupported AnimationPlayer listeners before activating Ready.
 std::function<bool(const FieldButterflyBinding&,std::string&)>admit_ready;
 std::function<bool(uint32_t,FieldButterflyBody&,std::string&)>body_snapshot;
 // Synchronize true root / Sprite transforms and live Area centre. Texture
 // Sprite offset affects drawing only, never the child's collision centre.
 std::function<bool(const FieldButterflyBinding&,const FieldButterflyState&,std::string&)>publish;
 // Optional paired native Timer bridge. Source start precedes signal-yield;
 // actual timeout restores ordered continuations, without a private clock.
 std::function<bool(uint32_t root,uint32_t phase,std::string&)>timer_start_wait;
 std::function<bool(uint32_t root,std::string&)>timer_cancel;
};
class FieldButterflyRuntime{
public:
 bool initialize(const FieldButterflyData&,FieldButterflyHost,std::string&);bool ready(uint32_t,SourceRandom&,std::string&);bool screen_enter(uint32_t,SourceRandom&,std::string&);bool screen_exit(uint32_t,std::string&);
 bool body_enter(uint32_t,uint32_t body,std::string&);bool body_exit(uint32_t,uint32_t body,std::string&);
 bool actual_timer_timeout(uint32_t root,std::string&);
 bool borrowed_timer()const{return bool(host_.timer_start_wait);}
 // Scene internal Timer/AnimationPlayer idle leaves precede ordinary _process.
 bool idle_leaf(uint32_t,double delta,bool tree_can_process,std::string&);bool idle_process(uint32_t,double delta,bool tree_can_process,std::string&);bool exit_tree(uint32_t,std::string&);
 // Actual native destruction follows child retirement. It cannot publish a
 // new pose to dead Sprite/AP children or impersonate a source _exit_tree.
 bool destroy(uint32_t,std::string&);
 const FieldButterflyState*state(uint32_t)const;const FieldButterflyData*content()const{return data_;}Vec2 world_position(uint32_t)const;Vec2 area_center(uint32_t)const;
private:
 FieldButterflyState*get(uint32_t,std::string&);bool publish(FieldButterflyState&,std::string&);bool animate(FieldButterflyState&,uint32_t role,float delta,bool seek,std::string&);bool apply(FieldButterflyState&,const FieldButterflyTrack&,float time,bool seek,float from,float raw_to,float length,std::string&);
 const FieldButterflyData*data_=nullptr;FieldButterflyHost host_;std::vector<FieldButterflyState>states_;
};
}

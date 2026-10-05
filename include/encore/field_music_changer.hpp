#pragma once
#include "encore/music_regions.hpp"
#include "encore/battle_data.hpp"
#include <map>
namespace encore::upstream {
struct FieldMusicChangerConnection{uint32_t role=0;std::string signal,method;};
struct FieldMusicChangerShape{uint32_t id=0,order=0;bool disabled=false;std::string node;std::vector<std::vector<Vec2>>parts;};
struct FieldMusicChangerBinding{uint32_t id=0,ready_ordinal=0,region_id=0,track_id=0,collision_layer=0,collision_mask=0,flags=0;std::string node;std::vector<FieldMusicChangerShape>shapes;};
class FieldMusicChangerData{
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}const std::array<uint8_t,20>&source_pin()const{return pin_;}
 const std::string&scene()const{return scene_;}const std::string&script()const{return script_;}
 float stop_default()const{return stop_default_;}const MusicRegionData&music()const{return music_;}
 const std::vector<uint8_t>&music_bytes()const{return music_bytes_;}
 const std::vector<FieldMusicChangerConnection>&connections()const{return connections_;}
 const std::vector<FieldMusicChangerBinding>&bindings()const{return bindings_;}
 const FieldMusicChangerBinding*binding(uint32_t)const;
 // Check the same real MusicRegionService preparation input and AudioBank.
 bool matches_service(const MusicRegionData&,const AudioBank&,std::string&)const;
 bool source_hash(std::string_view,std::array<uint8_t,32>&)const;
private:
 std::map<std::string,std::array<uint8_t,32>>sources_;
 bool valid_=false;std::array<uint8_t,20>pin_{};std::string scene_,script_;
 float stop_default_=0;MusicRegionData music_;std::vector<uint8_t>music_bytes_;
 std::vector<FieldMusicChangerConnection>connections_;std::vector<FieldMusicChangerBinding>bindings_;
};
struct FieldMusicChangerHost{
 // Must bind the actual prepared/committed MusicRegionService and NDSP owner,
 // source audio identities, global external-player/Room music history, bounded
 // real voice capacity and actual SceneTree idle signal waiter ordering.
 std::function<bool(const FieldMusicChangerData&,uint64_t epoch,std::string&)>admit_service;
 std::function<bool(const FieldMusicChangerBinding&,const std::vector<FieldMusicChangerConnection>&,std::string&)>admit_ready;
 std::function<bool(uint32_t shape,bool disabled,std::string&)>set_shape_disabled;
 // Actual source global player equality, UI cutscene/battle, body collision
 // availability and globalData flag query. No fabricated trigger point.
 std::function<bool(uint32_t body,MusicRegionContext&,std::string&)>context;
 std::function<bool(uint64_t,std::string_view,const MusicRegionContext&,std::string&)>area_enter,area_exit;
 std::function<bool(uint64_t,std::string_view,std::string&)>play_explicit,tree_exit;
 std::function<bool(uint64_t,std::string_view,double,std::string&)>stop_explicit;
 std::function<bool(uint64_t,std::string&)>idle_frame;
};
struct FieldMusicChangerState{uint32_t id=0;bool ready=false,alive=true,first_disabled=false;};
class FieldMusicChangerRuntime{
public:
 bool initialize(const FieldMusicChangerData&,uint64_t scene_epoch,FieldMusicChangerHost,std::string&);
 bool ready(uint32_t,std::string&);bool set_disabled(uint32_t,bool,std::string&);
 bool body_enter(uint32_t,uint32_t body,std::string&);bool body_exit(uint32_t,uint32_t body,std::string&);
 // Explicit Room source calls ignore overlap/flags/cutscene guards.
 bool play_music(uint32_t,std::string&);bool stop_music(uint32_t,double,std::string&);
 bool stop_music(uint32_t id,std::string&e){return data_?stop_music(id,data_->stop_default(),e):false;}
 bool stop_music_immediately(uint32_t id,std::string&e){return stop_music(id,0,e);}
 // One actual service idle boundary for the scene, not one tick per region.
 // App owner advances fades/NDSP once separately, after actual Room history.
 bool idle_frame(std::string&);bool tree_exiting(uint32_t,std::string&);
 const FieldMusicChangerState*state(uint32_t)const;
 const FieldMusicChangerData*content()const{return data_;}uint64_t epoch()const{return epoch_;}
private:
 FieldMusicChangerState*active(uint32_t,std::string&);
 const FieldMusicChangerData*data_=nullptr;uint64_t epoch_=0;size_t ready_index_=0;
 FieldMusicChangerHost host_;std::vector<FieldMusicChangerState>states_;
};
}

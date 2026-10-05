#pragma once
#include "encore/movement.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>
namespace encore::upstream {
enum class FieldEmoteProperty:uint32_t {Frame=1,Offset=2,Stream=3,Playing=4};
struct FieldEmoteKey {float time=0;Vec2 vector{};uint32_t integer=0;};
struct FieldEmoteTrack {FieldEmoteProperty property{};uint32_t update=0;std::vector<FieldEmoteKey>keys;};
struct FieldEmoteClip {uint32_t id=0;bool direction_sensitive=false;float length=0;std::string name;std::vector<FieldEmoteTrack>tracks;};
struct FieldEmoteSound {uint32_t id=0;float gain_db=0;std::string source,pcm;};
struct FieldEmoteDescriptor {uint32_t id=0,parent_id=0,ready_ordinal=0,object_id=0,direction_id=0,columns=0,rows=0,frame=0,flags=0;int32_t z_index=0;Vec2 position{},offset{},scale{};std::string node,object_path;};
class FieldEmoteData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);bool valid()const{return valid_;}
 uint32_t scene_id()const{return scene_;}const std::array<uint8_t,20>&source_pin()const{return pin_;}float bubble_gap()const{return bubble_gap_;}uint32_t default_sound()const{return default_sound_;}
 uint32_t width()const{return width_;}uint32_t height()const{return height_;}const std::string&texture_source()const{return texture_source_;}const std::string&texture_path()const{return texture_path_;}const std::string&bus()const{return bus_;}
 const std::vector<FieldEmoteDescriptor>&records()const{return records_;}const std::vector<FieldEmoteClip>&clips()const{return clips_;}const std::vector<FieldEmoteSound>&sounds()const{return sounds_;}
 bool source_hash(const std::string&,std::array<uint8_t,32>&)const;
 const FieldEmoteDescriptor*record(uint32_t)const;const FieldEmoteClip*clip(uint32_t)const;const FieldEmoteClip*clip(const std::string&)const;const FieldEmoteSound*sound(uint32_t)const;
private:
 bool valid_=false;uint32_t scene_=0,default_sound_=0,width_=0,height_=0;float bubble_gap_=0;std::array<uint8_t,20>pin_{};std::string texture_source_,texture_path_,bus_;std::vector<FieldEmoteDescriptor>records_;std::vector<FieldEmoteClip>clips_;std::vector<FieldEmoteSound>sounds_;std::map<std::string,std::array<uint8_t,32>>sources_;
};
struct FieldEmoteInstance {uint32_t id=0,object=0,direction_source=0,frame=0,clip_id=0,sound_id=0;Vec2 position{},offset{},scale{},capture_offset{};float elapsed=0;bool ready=false,visible=false,playing=false,started=false,sound_playing=false,capture_valid=false;};
struct FieldEmoteHost {
 // onready source nullable NodePath and get_direction ancestor must resolve
 // only real checked nodes; unavailable adapters reject, never appear absent.
 std::function<bool(const FieldEmoteDescriptor&,bool&,uint32_t&,std::string&)>resolve_object;
 std::function<bool(const FieldEmoteDescriptor&,uint32_t&,std::string&)>resolve_direction;
 std::function<bool(uint32_t,uint32_t&,uint32_t&,std::string&)>texture_geometry;
 std::function<bool(uint32_t,Vec2&,std::string&)>direction;
 std::function<bool(uint32_t,const FieldEmoteInstance&,std::string&)>publish;
 // Existing AudioBank/NDSP Host owns source stream assignment, including stop,
 // and playing=true retriggers from zero even if already playing.
 std::function<bool(uint32_t,const FieldEmoteSound&,std::string&)>sound_stream;
 std::function<bool(uint32_t,const FieldEmoteSound&,bool,std::string&)>sound_playing;
 std::function<bool(uint32_t,const FieldEmoteClip&,std::string&)>animation_finished;
};
class FieldEmoteRuntime {
public:
 bool initialize(const FieldEmoteData&,FieldEmoteHost,std::string&);bool create(uint32_t);bool ready(uint32_t);
 bool set_bubble_offset(uint32_t);bool sprite_changed(uint32_t source_sprite);
 bool play(uint32_t,uint32_t clip);bool play(uint32_t,const std::string&);bool idle_frame(uint32_t,float);bool seek(uint32_t,float,bool update);bool stop(uint32_t,bool reset=true);
 bool visibility(uint32_t,bool);bool audio_finished(uint32_t);bool destroy(uint32_t);
 const FieldEmoteInstance*instance(uint32_t)const;bool is_playing(uint32_t)const;const std::string&error()const{return error_;}
private:
 const FieldEmoteData*data_=nullptr;FieldEmoteHost host_;std::map<uint32_t,FieldEmoteInstance>instances_;std::string error_;uint32_t last_ready_=0;bool had_ready_=false;
 bool fail(const char*);FieldEmoteInstance*get(uint32_t);bool apply(FieldEmoteInstance&,const FieldEmoteTrack&,const FieldEmoteKey&);bool interpolate(FieldEmoteInstance&,const FieldEmoteTrack&,float,bool);bool publish(FieldEmoteInstance&);
};
}

#pragma once
#include "encore/movement.hpp"
#include <array>
#include <string>
#include <vector>
namespace encore::upstream {
struct BasementActorResource {uint32_t id=0,width=0,height=0,columns=0,rows=0;std::string path,source,primary_path;std::array<uint8_t,32>sha256{};Vec2 position{},offset{};};
struct BasementActorKey {float time=0;uint32_t frame=0;};
struct BasementActorAnimation {uint32_t id=0,resource_id=0;std::string name;float length=0;std::vector<BasementActorKey>keys;};
class BasementActorData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}const std::string&reviewed_commit()const{return commit_;}
 const std::vector<BasementActorResource>&resources()const{return resources_;}
 const std::vector<BasementActorAnimation>&animations()const{return animations_;}
 const BasementActorResource*resource(uint32_t)const;const BasementActorAnimation*animation(uint32_t)const;
 uint32_t frame(uint32_t animation_id,double elapsed)const;
 const std::string&present_sound()const{return present_sound_;}float present_sound_stop()const{return present_sound_stop_;}
 uint32_t present_resource_id()const{return present_resource_id_;}uint32_t present_animation_id()const{return present_animation_id_;}
private:std::string present_sound_;float present_sound_stop_=0;uint32_t present_resource_id_=0,present_animation_id_=0;bool valid_=false;std::string commit_;std::vector<BasementActorResource>resources_;std::vector<BasementActorAnimation>animations_;
};
// Non-looping source sprite playback holds its final frame. The existing Room
// scheduler owns dialogue, movement and waits; this owns only a sprite clock.
struct BasementActorPlayback {uint32_t animation_id=0;double elapsed=0;bool visible=false;};
bool begin_basement_actor(const BasementActorData&,uint32_t,BasementActorPlayback&,std::string&);
bool advance_basement_actor(const BasementActorData&,double,BasementActorPlayback&,std::string&);
}

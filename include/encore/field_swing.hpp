#pragma once
#include "encore/movement.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace encore::upstream {
enum class SwingTrack:uint32_t {Frame=1,PivotDegrees,HitboxDisabled,MainVisible,SpecialVisible,Texture,BatBehind,Finished};
struct SwingKey {float time=0;SwingTrack kind=SwingTrack::Frame;float value=0;uint32_t order=0;};
struct SwingClip {uint32_t id=0;Vec2 direction{};float duration=0;uint32_t first=0,count=0;};
struct SwingPose {uint32_t frame=0;float pivot=0;bool main=true,special=false,hitbox=false,behind=false;};
// Independent action resource. Sound, shapes and drawing are caller-owned;
// this class does not emulate arbitrary Player scripts or Godot node methods.
class FieldSwingData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}const SwingPose&initial()const{return initial_;}
 const std::array<float,19>&policy()const{return policy_;}
 const std::array<std::string,4>&strings()const{return strings_;}
 const std::vector<SwingClip>&clips()const{return clips_;}
 const std::vector<SwingKey>&keys()const{return keys_;}
 const SwingClip*clip(uint32_t)const;const SwingClip*direction(Vec2)const;
private:
 bool valid_=false;SwingPose initial_;std::array<float,19>policy_{};
 std::array<std::string,4>strings_;std::vector<SwingClip>clips_;std::vector<SwingKey>keys_;
};
class FieldSwingPlayer {
public:
 bool initialize(const FieldSwingData&,std::string&);
 // Caller must admit source input/skill before play. Changing the assigned
 // animation preserves sparse properties until its next processing pass.
 bool play(uint32_t,bool incapacitated,std::string&);bool advance(double,std::string&);
 // AnimationPlayer's default method mode is deferred. The original finish
 // method disables the hitbox and releases ATTACK only when that queue runs.
 bool flush(std::string&);
 const SwingPose&pose()const{return pose_;}bool playing()const{return playing_;}
 bool attacking()const{return attack_;}float position()const{return time_;}
 uint32_t pending_callbacks()const{return pending_;}
private:
 FieldSwingData data_;SwingPose pose_;float time_=0,rate_=1;uint32_t id_=0,key_=0,pending_=0;
 bool playing_=false,attack_=false;
};
}

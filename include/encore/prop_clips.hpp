#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace encore::upstream {
// Typed local Sprite/collision properties. No node paths or scripts execute.
enum class PropTrack:uint32_t {Frame=1,Visible=2,BodyDisabled=3,HitboxDisabled=4,InteractionDisabled=5};
struct PropPose {uint32_t frame=0;bool visible=false,body=false,hitbox=false,interaction=false;};
struct PropKey {float time=0;PropTrack track=PropTrack::Frame;uint32_t value=0;};
struct PropClip {uint32_t id=0;float duration=0;uint32_t first=0,count=0;};
class PropClipsData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}
 uint32_t frame_count()const{return frames_;}
 const PropPose&initial()const{return initial_;}
 const std::vector<PropClip>&clips()const{return clips_;}
 const std::vector<PropKey>&keys()const{return keys_;}
 const PropClip*clip(uint32_t id)const;
private:
 bool valid_=false;uint32_t frames_=0;PropPose initial_;
 std::vector<PropClip>clips_;std::vector<PropKey>keys_;
};
// AnimationPlayer property subset only. Callers own script callbacks, viewport
// visibility, input, audio, scene ordering and any completion continuation.
class PropClipPlayer {
public:
 bool initialize(const PropClipsData&,std::string&);
 // Same assigned clip continues its position; a finished endpoint restarts.
 bool play(uint32_t,std::string&);bool advance(double,std::string&);
 const PropPose&pose()const{return pose_;}
 bool playing()const{return playing_;}
 uint32_t assigned_clip()const{return id_;}
 double position()const{return time_;}
 std::vector<uint32_t>take_completed(){auto v=std::move(completed_);completed_.clear();return v;}
private:
 PropClipsData data_;PropPose pose_;uint32_t id_=UINT32_MAX,key_=0;
 float time_=0;bool playing_=false;
 std::vector<uint32_t>completed_;
};
}

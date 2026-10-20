#pragma once
#include "encore/prop_clips.hpp"
#include "encore/movement.hpp"
#include <array>

namespace encore::upstream {
class DungeonSwitchesView;class AudioBank;
enum class DungeonWallRole:uint32_t {On=0,Reset=1,StayOff=2,StayOn=3};
enum class DungeonWallCueKind:uint32_t {AudioPlaying=1,CameraShake=2};
struct DungeonWallCue {float time=0;DungeonWallCueKind kind=DungeonWallCueKind::AudioPlaying;double value=0;};
struct DungeonWallCueClip {uint32_t id=0,first=0,count=0;};
struct DungeonWallAudio {
 uint32_t id=0,bus=0;std::string source;std::array<uint8_t,32>sha{};float gain_db=0,pitch=0;bool loop=false;
};
struct DungeonWallCamera {double magnitude=0,interval=0,weight=0;Vec2 direction{};bool diminish=false;};
// ENCDWT01 format1/capability1; no PCM, texture, scene or save admission.
class DungeonWallTimelineData {
public:
 bool load(const uint8_t*,size_t,std::string&);bool load_file(const char*,std::string&);
 bool valid()const{return valid_;}
 uint32_t scene_id()const{return scene_;}uint32_t owner_id()const{return owner_;}
 const std::string&owner_path()const{return path_;}
 const DungeonWallAudio&audio()const{return audio_;}const DungeonWallCamera&camera()const{return camera_;}
 const std::array<uint32_t,4>&roles()const{return roles_;}
 const PropClipsData&properties()const{return properties_;}
 const std::vector<DungeonWallCue>&cues()const{return cues_;}
 const DungeonWallCueClip*clip(uint32_t id)const;
 bool matches(const DungeonSwitchesView&,const AudioBank&,std::string&)const;
private:
 bool valid_=false;uint32_t scene_=0,owner_=0;std::string path_;
 DungeonWallAudio audio_;DungeonWallCamera camera_;std::array<uint32_t,4>roles_{};
 PropClipsData properties_;std::vector<DungeonWallCueClip>clips_;std::vector<DungeonWallCue>cues_;
};
}

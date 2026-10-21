#pragma once
#include "encore/geyser_media.hpp"
namespace encore::upstream {
struct GeyserPcmView {uint32_t id=0;const int16_t*pcm=nullptr;size_t samples=0;};
struct GeyserPlayback {bool present=false,playing=false,paused=false;uint32_t sound=0,cursor=0;uint64_t end_frame=UINT64_MAX;};
// Source logical voices share one output stream. Immutable PCM storage outlives
// this owner;hardware confirms consumed frames independently of rendering.
class GeyserMixer {
public:
 bool prepare(const GeyserMediaData&,const std::vector<GeyserPcmView>&,std::string&);
 bool activate(uint64_t,std::string&);bool bind(uint64_t,uint64_t instance,std::string&);
 bool sync(uint64_t,uint64_t,GeyserTimeline&,Vec2,GeyserListener,std::string&);
 bool user_sfx(float,std::string&);bool global_tree_notification(bool paused,bool can_process,std::string&);
 bool render(int16_t*,uint32_t capacity,uint32_t&frames,std::string&);
 bool reserve_render(int16_t*,uint32_t capacity,uint32_t&frames,std::string&);
 bool commit_render(std::string&);void cancel_render(){pending_=false;}
 bool consumed(uint64_t frames,std::string&);
 bool retire_actor(uint64_t,uint64_t,std::string&);bool retire_scene(uint64_t,std::string&);
 bool inspect(uint64_t,uint64_t,GeyserPlayback&,std::string&)const;
 GeyserNamedVoice global_voice()const;
 GeyserPlayback global_playback()const{return global_.state;}
 bool valid()const{return data_.valid();}uint64_t epoch()const{return epoch_;}
 uint64_t rendered_frames()const{return rendered_;}uint64_t consumed_frames()const{return consumed_;}
 uint64_t play_requests()const{return plays_;}
private:
 struct Cache {AudioAsset asset;GeyserPcmView view;};
 struct Voice {uint64_t instance=0;size_t cache=SIZE_MAX;GeyserPlayback state;GeyserStereoGain gain;};
 bool owner(uint64_t,std::string&)const;size_t find(uint64_t)const;size_t cache(uint32_t)const;
 GeyserMediaData data_;std::vector<Cache>caches_;std::vector<Voice>voices_,next_;Voice global_,next_global_;
 uint64_t epoch_=0,last_epoch_=0,last_instance_=0,rendered_=0,consumed_=0,plays_=0;float user_gain_=1;
 bool pending_=false;uint32_t pending_frames_=0;
};
}

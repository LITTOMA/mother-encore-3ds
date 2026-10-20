#include "encore/dungeon_wall_state.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {namespace {
bool bad(std::string&e,const char*s){e=s;return false;}
bool delta_ok(double v){return std::isfinite(v)&&v>=0&&v<=60;}
}
bool DungeonWallState::initialize(const DungeonWallTimelineData&data,uint64_t generation,std::string&e){
 if(valid_||!data.valid()||!generation)return bad(e,"Wall state requires a fresh owner and checked data");
 DungeonWallState v;if(!v.player_.initialize(data.properties(),e))return false;v.data_=data;v.generation_=generation;v.events_.reserve(128);v.pending_.reserve(64);v.valid_=true;*this=std::move(v);e.clear();return true;
}
bool DungeonWallState::play(uint32_t clip,std::string&e){if(!valid_||!data_.clip(clip))return bad(e,"Wall state clip is not owned");return player_.play(clip,e);}
bool DungeonWallState::play_role(DungeonWallRole role,std::string&e){const auto n=uint32_t(role);if(!valid_||n>=data_.roles().size())return bad(e,"Wall state animation role rejected");return play(data_.roles()[n],e);}
bool DungeonWallState::process(double delta,std::string&e){
 if(!valid_||!delta_ok(delta))return bad(e,"Wall automatic frame rejected");
 if(!player_.playing()){e.clear();return true;}return advance_manual(delta,e);
}
bool DungeonWallState::advance_manual(double delta,std::string&e){
 if(!valid_||!delta_ok(delta))return bad(e,"Wall manual advance rejected");
 const auto*c=data_.clip(player_.assigned_clip());if(!c)return player_.advance(delta,e);
 const auto*prop=data_.properties().clip(c->id);if(!prop)return bad(e,"Wall state property identity lost");
 const float from=float(player_.position());const bool active=player_.playing();const float to=active?std::min(prop->duration,from+float(delta)):from;
 const bool zero=to==from,finish=active&&to>=prop->duration;
 const DungeonWallCue*sample=nullptr,*single=nullptr;unsigned audio_keys=0;
 if(zero)for(uint32_t i=0;i<c->count;++i){const auto&k=data_.cues()[c->first+i];if(k.kind==DungeonWallCueKind::AudioPlaying){++audio_keys;single=&k;if(k.time<=to)sample=&k;}}
 if(audio_keys==1)sample=single;
 std::vector<DungeonWallEvent>immediate,deferred;
 const auto event=[&](DungeonWallEventKind kind){DungeonWallEvent v;v.kind=kind;v.owner=data_.owner_id();v.clip=c->id;v.generation=generation_;return v;};
 if(zero){if(sample){auto v=event(DungeonWallEventKind::AudioPlaying);v.playing=sample->value!=0;immediate.push_back(v);}}
 else for(uint32_t i=0;i<c->count;++i){const auto&k=data_.cues()[c->first+i];if(k.time<from||k.time>to||(!finish&&k.time==to))continue;
  if(k.kind==DungeonWallCueKind::AudioPlaying){auto v=event(DungeonWallEventKind::AudioPlaying);v.playing=k.value!=0;immediate.push_back(v);}
  else if(k.kind==DungeonWallCueKind::CameraShake){auto v=event(DungeonWallEventKind::CameraShake);v.duration=k.value;deferred.push_back(v);}
  else return bad(e,"Wall state unsupported typed cue");
 }
 if(events_.size()+immediate.size()+size_t(finish)>128||pending_.size()+deferred.size()>64)return bad(e,"Wall state command queue capacity");
 // All data and queue admission is complete before the property player mutates.
 // Its own rejected advance preserves its state. No external sink runs here.
 if(!player_.advance(delta,e))return false;
 for(const auto&v:immediate){audio_request_=v.playing;events_.push_back(v);}
 pending_.insert(pending_.end(),deferred.begin(),deferred.end());
 for(auto id:player_.take_completed()){auto v=event(DungeonWallEventKind::Finished);v.clip=id;events_.push_back(v);}
 e.clear();return true;
}
bool DungeonWallState::flush_deferred(std::string&e){
 if(!valid_||events_.size()+pending_.size()>128)return bad(e,"Wall deferred flush owner/capacity rejected");
 events_.insert(events_.end(),pending_.begin(),pending_.end());pending_.clear();e.clear();return true;
}
}

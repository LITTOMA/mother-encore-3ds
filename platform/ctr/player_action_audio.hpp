#pragma once
#include "audio_player.hpp"
#include "encore/field_swing_resources.hpp"
namespace encore::ctr {
// One persistent source AudioStreamPlayer. Preparation admits PCM, buffers
// and an independent pooled channel; sync only applies ordered source events.
// A failed event remains owned here for retry before any newer World events.
class PlayerActionAudioService final {
public:
 bool prepare(const upstream::FieldSwingResources&resources,AudioPlayer&player,std::string&error){
  auto fail=[&](const char*why){error=why;return false;};
  if(resources_||!resources.valid()||resources.entry().capability()!=2||resources.action().capability()!=2)return fail("Player audio needs one complete source resource owner");
  upstream::AudioAsset sound;const auto&binding=resources.action().audio();const std::string path="res://"+resources.swing().strings()[2];
  if(!player.bank().find_source(path,binding.source_sha256,sound)||sound.loops())return fail("Player audio source path/digest/loop differs from the bank");
  if(player.available()&&(!player.prepare(sound.stable_id,error)||!player.prepare_player_voice(error)))return false;
  resources_=&resources;player_=&player;generation_=player.configuration_generation();available_=player.available();sound_=sound.stable_id;error.clear();return true;
 }
 bool reset_player(AudioPlayer&player,std::string&error){
  if(!compatible(player,error))return false;
  if(available_&&!player.stop_lane(AudioLane::Player,error))return false;
  pending_.clear();consumed_=0;owner_=nullptr;paused_=false;starts_=0;error.clear();return true;
 }
 bool sync(upstream::OpeningWorld&world,AudioPlayer&player,std::string&error){
  if(!compatible(player,error))return false;
  if(!world.healthy()||world.player_action_resources()!=resources_||!world.player_action().ready()||(owner_&&owner_!=world.persistent_player())){error="Player audio persistent World/resource owner differs";return false;}
  owner_=world.persistent_player();
  if(!apply(player,error))return false;
  pending_=world.take_player_action_events();consumed_=0;
  return apply(player,error);
 }
 bool prepared()const{return resources_!=nullptr;}bool paused()const{return paused_;}
 uint64_t starts()const{return starts_;}size_t consumed_events()const{return consumed_;}
 const std::vector<upstream::PlayerActionEvent>&pending_events()const{return pending_;}
 uint32_t sound_id()const{return sound_;}
private:
 bool compatible(const AudioPlayer&player,std::string&error)const{
  if(!resources_||player_!=&player||generation_!=player.configuration_generation()||available_!=player.available()){error="Player audio service/player configuration differs";return false;}return true;
 }
 bool apply(AudioPlayer&player,std::string&error){
  using E=upstream::PlayerActionEvent;
  if(consumed_>pending_.size()||pending_.size()>64){error="Player audio event history/bound rejected";return false;}
  bool paused=paused_;uint64_t starts=starts_;
  for(size_t i=consumed_;i<pending_.size();++i){const auto event=pending_[i];
   if(event==E::Start){if(paused||starts==UINT64_MAX){error="Player audio start state/counter rejected";return false;}++starts;}
   else if(event==E::Pause)paused=true;else if(event==E::Resume)paused=false;else{error="Unknown player audio event";return false;}
  }
  for(;consumed_<pending_.size();++consumed_){const auto event=pending_[consumed_];
   if(event==E::Start){const auto&binding=resources_->action().audio();if(available_&&!player.play(sound_,AudioLane::Player,error,binding.gain_db,0,binding.pitch))return false;++starts_;}
   else if(event==E::Pause){if(available_&&!player.stop_lane(AudioLane::Player,error))return false;paused_=true;}
   else{if(available_&&!player.pause_lane(AudioLane::Player,false,error))return false;paused_=false;}
  }
  pending_.clear();consumed_=0;error.clear();return true;
 }
 const upstream::FieldSwingResources*resources_=nullptr;AudioPlayer*player_=nullptr;
 const upstream::PersistentPlayerState*owner_=nullptr;uint64_t generation_=0,starts_=0;
 std::vector<upstream::PlayerActionEvent>pending_;size_t consumed_=0;uint32_t sound_=0;bool available_=false,paused_=false;
};
}

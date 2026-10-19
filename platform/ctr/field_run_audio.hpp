#pragma once
#include "audio_player.hpp"
#include "encore/field_steps.hpp"
namespace encore::ctr {
// Scene-owned prepared streams; sync never admits new media. Successful
// operations remain consumed if a later operation in the batch fails.
class FieldRunAudioService final {
public:
 bool prepare(upstream::FieldStepsView data,AudioPlayer&player,std::string&error){
  auto fail=[&](const char*why){error=why;return false;};
  if(data_.valid()||!data.valid())return fail("Run audio preparation owner rejected");
  if(!data.matches(player.bank(),error))return false;
  if(player.available())for(uint32_t i=0;i<data.sound_count();++i)if(!player.prepare(data.sound(i).id,error))return false;
  data_=data;player_=&player;generation_=player.configuration_generation();available_=player.available();error.clear();return true;
 }
 bool sync(upstream::FieldRunLoop&loop,AudioPlayer&player,std::string&error){
  auto fail=[&](const char*why){error=why;return false;};
  if(!data_.valid()||player_!=&player||generation_!=player.configuration_generation()||available_!=player.available()||!loop.prepared()||loop.content().bytes_identity()!=data_.bytes_identity()||(loop_&&loop_!=&loop))return fail("Run audio scene/player owner differs");
  const auto&events=loop.events();
  if(consumed_>events.size())return fail("Run audio pending events were discarded");
  for(size_t i=consumed_;i<events.size();++i){const auto e=events[i];if(e.sound>=data_.sound_count()||(e.kind!=upstream::FieldRunEventKind::Play&&e.kind!=upstream::FieldRunEventKind::ReplacePlay&&e.kind!=upstream::FieldRunEventKind::Stop))return fail("Run audio event/source rejected");}
  loop_=&loop;
  for(;consumed_<events.size();++consumed_){
   const auto e=events[consumed_];
   if(!available_)continue; // Boot reports unavailable NDSP; metadata still checked.
   if(e.kind==upstream::FieldRunEventKind::Stop){if(!player.stop_lane(AudioLane::Run,error))return false;}
   else if(!player.play(data_.sound(e.sound).id,AudioLane::Run,error))return false;
  }
  loop.clear_events();consumed_=0;error.clear();return true;
 }
 bool prepared()const{return data_.valid();}
 size_t consumed_events()const{return consumed_;}
private:
 upstream::FieldStepsView data_;AudioPlayer*player_=nullptr;const upstream::FieldRunLoop*loop_=nullptr;
 uint64_t generation_=0;size_t consumed_=0;bool available_=false;
};
}
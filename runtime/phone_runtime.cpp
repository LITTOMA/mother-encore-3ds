#include "encore/phone_runtime.hpp"
#include <algorithm>
#include <cmath>

namespace encore::upstream {
bool PhoneRuntime::fail(const char*message){failed_=true;error_=message;return false;}
bool PhoneRuntime::initialize(PhoneView view){
 if(!view.valid()||!view.count(PhoneSection::Objects))return fail("Phone initialization requires checked content");
 std::vector<State>next;
 for(uint32_t i=0;i<view.count(PhoneSection::Objects);++i){auto o=view.object(i);next.push_back({o.idle_clip,o.initial_frame,0,false});}
 view_=view;states_.swap(next);failed_=false;error_="";return true;
}
bool PhoneRuntime::ring(uint32_t object){
 if(!valid()||object>=states_.size())return fail("Phone ring object rejected");
 auto&state=states_[object];auto o=view_.object(object);
 if(state.clip==o.ring_clip)return true;
 // AnimationPlayer.play changes current_animation immediately; keys apply on its next process.
 state.clip=o.ring_clip;state.time=0;state.started=false;return true;
}
PhoneSoundRequest PhoneRuntime::sound(uint32_t object,uint32_t resource,PhoneSoundKind kind)const{
 auto o=view_.object(object);return {kind,object,resource,o.audio_bus,o.audio_center,bool(o.policy&uint32_t(PhonePolicy::Positional))};
}
bool PhoneRuntime::interaction_program(uint32_t object,const PhoneFlagQuery&query,uint32_t&out)const{
 if(!valid()||object>=states_.size())return false;
 const auto o=view_.object(object);uint32_t program=o.default_program;
 for(uint32_t i=0;i<o.dispatch_count;++i){const auto d=view_.dispatch(o.first_dispatch+i);bool value=false;if(!query.get_phone_flag(d.flag_ref,value))return false;if(value)program=d.program;}
 out=program;return true;
}
bool PhoneRuntime::interact(uint32_t object,const PhoneFlagQuery&query,PhoneInteraction&out){
 if(!valid()||object>=states_.size())return fail("Phone interaction object rejected");
 uint32_t program=0;if(!interaction_program(object,query,program))return fail("Phone interaction flag unresolved");const auto o=view_.object(object);
 auto&state=states_[object];state.clip=o.idle_clip;state.time=0;state.started=false;
 out={program,o.phone_location,sound(object,o.hangup_sound,PhoneSoundKind::Hangup)};
 return true;
}
bool PhoneRuntime::advance(double delta,PhoneSoundSink&sink){
 if(!valid()||!std::isfinite(delta)||delta<=0||delta>.25)return fail("Phone idle delta rejected");
 for(uint32_t index=0;index<states_.size();++index){auto&state=states_[index];auto clip=view_.clip(state.clip);
  if(!clip.loop&&state.started&&state.time>=clip.length)continue;
  const double begin=state.time;
  // Godot AnimationPlayer's playback position is real_t: each addition rounds
  // to float32, rather than accumulating an ideal double-duration clock.
  const double total=double(float(float(begin)+float(delta)));
  const double wrap_count=clip.loop?std::floor(total/clip.length):0;
  if(!std::isfinite(wrap_count)||wrap_count>64)return fail("Phone animation event budget rejected");
  const uint32_t wraps=uint32_t(wrap_count);
  const double finish=clip.loop?double(float(std::fmod(total,clip.length))):std::min(total,clip.length);
  // Discrete value tracks are traversed in original track order, not timestamp order across tracks.
  auto crossed=[&](double key,uint32_t cycle){
   const double point=key+double(cycle)*clip.length;
   // Native discrete track interval is [previous, current): a key exactly at
   // the new playback position is applied on the following advance.
   return point>=begin&&point<total;
  };
  for(uint32_t cycle=0;cycle<=wraps;++cycle)
   for(uint32_t j=0;j<clip.frame_count;++j){auto key=view_.frame_key(clip.first_frame+j);if(crossed(key.time,cycle))state.frame=key.frame;}
  for(uint32_t cycle=0;cycle<=wraps;++cycle)
   for(uint32_t j=0;j<clip.sound_count;++j){auto key=view_.sound_key(clip.first_sound+j);if(crossed(key.time,cycle)&&!sink.play_phone_sound(sound(index,key.resource,PhoneSoundKind::Ring)))return fail("Phone sound sink rejected request");}
  state.time=finish;state.started=true;
 }
 return true;
}
PhonePose PhoneRuntime::pose(uint32_t index)const{
 if(!view_.valid()||index>=states_.size())return {};
 auto o=view_.object(index);return {o.resource,states_[index].frame,o.sprite_center,o.position,bool(o.policy&uint32_t(PhonePolicy::Centered))};
}
bool PhoneRuntime::ringing(uint32_t object)const{return valid()&&object<states_.size()&&states_[object].clip==view_.object(object).ring_clip;}
double PhoneRuntime::clip_time(uint32_t object)const{return valid()&&object<states_.size()?states_[object].time:0;}
}

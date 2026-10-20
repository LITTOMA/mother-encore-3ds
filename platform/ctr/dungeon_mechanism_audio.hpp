#pragma once
#include "audio_player.hpp"
#include "encore/dungeon_mechanism_audio.hpp"
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <set>

namespace encore::ctr {
// Persistent prepared owner for one checked mechanism resource. Scene-local
// voices retire separately; the named manager channel survives scene exit.
// All PCM admission precedes activation. Reservations touch no live queues.
class DungeonMechanismAudioService final {
public:
 ~DungeonMechanismAudioService(){shutdown();}
 bool prepare(const upstream::DungeonMechanismPresentationData&data,AudioPlayer&player,const char*root,std::string&error){
  auto fail=[&](const char*why){error=why;return false;};
  if(state_||!root||!player.bank().count()||!data.media_bound()||data.sounds().empty()||data.sounds().size()>64||data.audio_bank().count()>64||data.audio_bank().master_db()!=player.bank().master_db())return fail("Mechanism audio needs fresh checked media and matching master bus");
  auto next=std::make_unique<State>();next->data=data;next->player=&player;next->generation=player.configuration_generation();next->available=player.available();next->pending.reserve(4096);next->reserved.reserve(data.sounds().size());size_t bytes=0;
  for(uint32_t i=0;i<data.audio_bank().count();++i){
   const auto asset=data.audio_bank().asset(i);if(asset.loops()||!asset.pcm_bytes||asset.pcm_bytes>512*1024||bytes>4*1024*1024-asset.pcm_bytes)return fail("Mechanism short-sound cache capacity/loop rejected");bytes+=asset.pcm_bytes;
   auto cache=std::make_unique<Cache>();cache->asset=asset;
   std::string path=root;if(!path.empty()&&path.back()!='/'&&path.back()!='\\')path+='/';path+=asset.pcm_path;
   upstream::AudioPcmStream stream;if(!stream.open(asset,path.c_str(),error))return false;
   if(next->available){cache->pcm=static_cast<int16_t*>(linearAlloc(asset.pcm_bytes));if(!cache->pcm)return fail("Mechanism PCM cache allocation failed");uint32_t frames=0;
    if(!stream.read(cache->pcm,asset.frames,frames,error))return false;if(frames!=asset.frames)return fail("Mechanism PCM cache is incomplete");if(R_FAILED(DSP_FlushDataCache(cache->pcm,asset.pcm_bytes)))return fail("Mechanism PCM cache flush failed");
   }next->caches.push_back(std::move(cache));
  }
  for(const auto&sound:data.sounds()){
   auto voice=std::make_unique<Voice>();for(size_t i=0;i<next->caches.size();++i)if(next->caches[i]->asset.stable_id==sound.stable_id)voice->cache=i;
   if(voice->cache==SIZE_MAX)return fail("Mechanism voice cache source is absent");next->voices.push_back(std::move(voice));
  }
  state_=std::move(next);error.clear();return true;
 }
 bool activate(uint64_t epoch,std::string&error){
  if(!compatible(error))return false;if(!epoch||state_->epoch||state_->reserved_batch||epoch<=state_->last_epoch){error="Mechanism sound scene epoch rejected";return false;}
  state_->epoch=state_->last_epoch=epoch;error.clear();return true;
 }
 bool matches(const upstream::DungeonMechanismPresentationData&data,std::string&error)const{
  if(!compatible(error))return false;if(!data.media_bound()||data.encoded_bytes()!=state_->data.encoded_bytes()||data.encoded_crc()!=state_->data.encoded_crc()){error="Mechanism audio resource differs";return false;}error.clear();return true;
 }
 bool reserve(const std::vector<upstream::DungeonMechanismSoundCommand>&commands,std::string&error){
  if(!compatible(error))return false;auto&s=*state_;
  if(!s.epoch||s.reserved_batch||commands.empty()||commands.size()>4096){error="Mechanism audio reservation state/count rejected";return false;}
  std::set<size_t>start_slots;uint64_t starts=0;const auto&sounds=s.data.sounds();
  for(const auto&command:commands){
   if(command.epoch!=s.epoch||command.slot>=sounds.size()){error="Mechanism sound command scene/slot rejected";return false;}
   const auto&sound=sounds[command.slot];if(command.owner!=sound.owner||command.kind!=sound.kind){error="Mechanism sound command source differs";return false;}
   if(sound.kind==upstream::DungeonMechanismSoundKind::Node){if(command.request_owner!=sound.owner){error="Mechanism node request owner differs";return false;}}
   else {bool found=false;const auto graph=s.data.bindings().graph();for(uint32_t i=0;i<graph.switch_count();++i)if(graph.switch_node(i).id==command.request_owner)found=true;if(!command.playing||!found){error="Mechanism manager channel request rejected";return false;}}
   auto&voice=*s.voices[command.slot];if(voice.channel<region_channel_pool::capacity&&region_channel_pool::owners()[voice.channel]!=&voice){error="Mechanism audio channel tag differs";return false;}
   if(command.playing){start_slots.insert(command.slot);++starts;}
  }
  if(starts>UINT64_MAX-plays_){error="Mechanism audio request counter exhausted";return false;}
  size_t needed=0;for(auto slot:start_slots)if(s.voices[slot]->channel==region_channel_pool::capacity)++needed;auto&pool=region_channel_pool::owners();
  if(s.available&&needed>size_t(std::count(pool.begin(),pool.end(),nullptr))){error="Mechanism audio channels unavailable before mutation";return false;}
  s.pending=commands;s.reserved.clear();
  if(s.available)for(auto slot:start_slots){auto&voice=*s.voices[slot];if(voice.channel<pool.size())continue;const auto free=std::find(pool.begin(),pool.end(),nullptr);voice.channel=uint32_t(free-pool.begin());pool[voice.channel]=&voice;s.reserved.push_back(slot);}
  s.reserved_batch=true;error.clear();return true;
 }
 // Caller commits its game-state transaction between reserve and this call.
 // No IO, allocation or fallible preparation occurs while consuming commands.
 void commit(){
  std::string error;if(!compatible(error)||!state_->reserved_batch)std::abort();auto&s=*state_;
  for(const auto&command:s.pending){auto&voice=*s.voices[command.slot];
   if(command.playing){if(s.available){if(voice.channel>=region_channel_pool::capacity||region_channel_pool::owners()[voice.channel]!=&voice)std::abort();const auto channel=hardware(voice);ndspChnWaveBufClear(channel);ndspChnReset(channel);voice.waves={};voice.cursor=0;voice.active=true;const auto&cache=*s.caches[voice.cache];const auto&sound=s.data.sounds()[command.slot];ndspChnSetInterp(channel,NDSP_INTERP_POLYPHASE);ndspChnSetRate(channel,float(cache.asset.sample_rate)*sound.pitch_scale);ndspChnSetFormat(channel,cache.asset.channels==2?NDSP_FORMAT_STEREO_PCM16:NDSP_FORMAT_MONO_PCM16);mix(command.slot);refill(voice);}++plays_;}
   else clear(voice,false);
  }
  for(auto&voice:s.voices)if(!voice->active)clear(*voice,true);
  s.pending.clear();s.reserved.clear();s.reserved_batch=false;
 }
 void cancel(){
  if(!state_||!state_->reserved_batch)return;for(auto slot:state_->reserved)clear(*state_->voices[slot],true);state_->reserved.clear();state_->pending.clear();state_->reserved_batch=false;
 }
 bool retire_scene(uint64_t epoch,std::string&error){
  if(!compatible(error))return false;auto&s=*state_;if(!epoch||epoch!=s.epoch||s.reserved_batch){error="Mechanism sound retirement epoch/state rejected";return false;}
  for(size_t i=0;i<s.voices.size();++i)if(s.data.sounds()[i].kind==upstream::DungeonMechanismSoundKind::Node)clear(*s.voices[i],true);
  s.epoch=0;error.clear();return true;
 }
 bool pump_streams(std::string&error){
  if(!state_){error.clear();return true;}if(!compatible(error))return false;if(state_->reserved_batch){error="Mechanism audio reservation must commit/cancel before pumping";return false;}
  if(state_->available)for(auto&ptr:state_->voices){auto&voice=*ptr;if(!voice.active)continue;if(voice.channel>=region_channel_pool::capacity||region_channel_pool::owners()[voice.channel]!=&voice){error="Mechanism live audio channel tag differs";return false;}refill(voice);bool queued=false;for(const auto&wave:voice.waves)queued|=wave.status==NDSP_WBUF_QUEUED||wave.status==NDSP_WBUF_PLAYING;if(!queued&&voice.cursor==state_->caches[voice.cache]->asset.frames)clear(voice,true);}
  error.clear();return true;
 }
 bool update(std::string&error){if(!pump_streams(error))return false;if(state_&&state_->available)for(size_t i=0;i<state_->voices.size();++i)if(state_->voices[i]->active)mix(i);return true;}
 void shutdown(){if(!state_)return;for(auto&voice:state_->voices)clear(*voice,true);state_.reset();plays_=0;}
 bool prepared()const{return state_!=nullptr;}uint64_t epoch()const{return state_?state_->epoch:0;}uint64_t play_requests()const{return plays_;}
 size_t active_nodes()const{size_t n=0;if(state_)for(size_t i=0;i<state_->voices.size();++i)if(state_->data.sounds()[i].kind==upstream::DungeonMechanismSoundKind::Node)n+=state_->voices[i]->active;return n;}
 size_t active_manager_channels()const{size_t n=0;if(state_)for(size_t i=0;i<state_->voices.size();++i)if(state_->data.sounds()[i].kind==upstream::DungeonMechanismSoundKind::ManagerChannel)n+=state_->voices[i]->active;return n;}
private:
 static constexpr uint32_t buffer_count=3,buffer_frames=2048;
 struct Cache {~Cache(){if(pcm)linearFree(pcm);}upstream::AudioAsset asset;int16_t*pcm=nullptr;};
 struct Voice {std::array<ndspWaveBuf,buffer_count>waves{};size_t cache=SIZE_MAX;uint32_t channel=region_channel_pool::capacity,cursor=0;bool active=false;};
 struct State {upstream::DungeonMechanismPresentationData data;AudioPlayer*player=nullptr;uint64_t generation=0,epoch=0,last_epoch=0;bool available=false,reserved_batch=false;std::vector<std::unique_ptr<Cache>>caches;std::vector<std::unique_ptr<Voice>>voices;std::vector<upstream::DungeonMechanismSoundCommand>pending;std::vector<size_t>reserved;};
 bool compatible(std::string&error)const{if(!state_||!state_->player||state_->player->configuration_generation()!=state_->generation||state_->player->available()!=state_->available){error="Mechanism audio owner/player configuration differs";return false;}return true;}
 static int hardware(const Voice&voice){return int(region_channel_pool::first_channel+voice.channel);}
 void mix(size_t index){auto&s=*state_;auto&voice=*s.voices[index];const auto&sound=s.data.sounds()[index];const auto&asset=s.caches[voice.cache]->asset;float gain[12]{};gain[0]=gain[1]=upstream::audio_linear_gain(asset.gain_db+sound.gain_db+s.player->user_sfx_db());ndspChnSetMix(hardware(voice),gain);}
 void refill(Voice&voice){const auto&cache=*state_->caches[voice.cache];for(auto&wave:voice.waves){if(wave.status!=NDSP_WBUF_FREE&&wave.status!=NDSP_WBUF_DONE)continue;if(voice.cursor==cache.asset.frames)break;const auto frames=std::min(buffer_frames,cache.asset.frames-voice.cursor);wave={};wave.data_pcm16=cache.pcm+size_t(voice.cursor)*cache.asset.channels;wave.nsamples=frames;wave.looping=false;voice.cursor+=frames;ndspChnWaveBufAdd(hardware(voice),&wave);}}
 void clear(Voice&voice,bool release){if(voice.channel<region_channel_pool::capacity){auto&owner=region_channel_pool::owners()[voice.channel];if(owner==&voice){if(state_->available&&state_->player->available()&&state_->generation==state_->player->configuration_generation())ndspChnWaveBufClear(hardware(voice));if(release)owner=nullptr;}if(release)voice.channel=region_channel_pool::capacity;}voice.active=false;voice.cursor=0;voice.waves={};}
 std::unique_ptr<State>state_;uint64_t plays_=0;
};
}

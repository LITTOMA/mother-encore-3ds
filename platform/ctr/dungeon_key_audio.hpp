#pragma once
#include "audio_player.hpp"
#include "encore/dungeon_key_media.hpp"
#include <algorithm>
#include <cstdlib>
#include <memory>
#include <set>
namespace encore::ctr {
struct KeyAudioCompletion {uint64_t epoch=0,play=0;uint32_t slot=0,owner=0,profile=0;};
// Source-node playback only. Admission caches PCM before activation; no IO or
// allocation occurs at commit. Natural finishes are a separately reserved
// transaction, never a duration guessed by the game loop.
class DungeonKeyAudioService final {
public:
 ~DungeonKeyAudioService(){shutdown();}
 bool prepare(const upstream::DungeonKeyMediaData&data,AudioPlayer&player,const char*root,std::string&e){
  auto fail=[&](const char*why){e=why;return false;};
  if(state_||!root||!data.valid()||!player.available()||data.sounds().empty()||data.sounds().size()>64||data.audio_bank().count()>64||!player.bank().count()||data.audio_bank().master_db()!=player.bank().master_db()||data.audio_bank().silence_db()!=player.bank().silence_db())return fail("Key audio needs fresh checked media and an available matching output");
  auto next=std::make_unique<State>();next->data=data;next->player=&player;next->configuration=player.configuration_generation();next->pending.reserve(4096);next->reserved.reserve(data.sounds().size());next->completed.reserve(data.sounds().size());size_t bytes=0;
  for(uint32_t i=0;i<data.audio_bank().count();++i){const auto a=data.audio_bank().asset(i);if(a.loops()||!a.pcm_bytes||a.pcm_bytes>512*1024||bytes>4*1024*1024-a.pcm_bytes)return fail("Key audio cache capacity/loop rejected");bytes+=a.pcm_bytes;
   auto cache=std::make_unique<Cache>();cache->asset=a;std::string path=root;if(!path.empty()&&path.back()!='/'&&path.back()!='\\')path+='/';path+=a.pcm_path;upstream::AudioPcmStream stream;if(!stream.open(a,path.c_str(),e))return false;cache->pcm=static_cast<int16_t*>(linearAlloc(a.pcm_bytes));if(!cache->pcm)return fail("Key audio cache allocation failed");uint32_t frames=0;if(!stream.read(cache->pcm,a.frames,frames,e))return false;if(frames!=a.frames)return fail("Key audio cache incomplete");if(R_FAILED(DSP_FlushDataCache(cache->pcm,a.pcm_bytes)))return fail("Key audio cache flush failed");next->caches.push_back(std::move(cache));
  }
  for(const auto&s:data.sounds()){auto voice=std::make_unique<Voice>();for(size_t i=0;i<next->caches.size();++i)if(next->caches[i]->asset.stable_id==s.asset)voice->cache=i;if(voice->cache==SIZE_MAX)return fail("Key audio source cache absent");next->voices.push_back(std::move(voice));}
  state_=std::move(next);e.clear();return true;
 }
 bool activate(uint64_t epoch,std::string&e){if(!compatible(e))return false;auto&s=*state_;if(!epoch||s.epoch||s.batch||s.finishing||epoch<=s.last_epoch){e="Key audio scene epoch rejected";return false;}s.epoch=s.last_epoch=epoch;e.clear();return true;}
 bool matches(const upstream::DungeonKeyMediaData&data,std::string&e)const{if(!compatible(e))return false;if(!data.valid()||data.clocks().metadata().bytes_identity()!=state_->data.clocks().metadata().bytes_identity()){e="Key audio media owner differs";return false;}e.clear();return true;}
 bool reserve(const std::vector<upstream::KeySoundCommand>&commands,std::string&e){
  if(!compatible(e))return false;auto&s=*state_;if(!s.epoch||s.batch||s.finishing||commands.empty()||commands.size()>4096){e="Key audio reservation state/count rejected";return false;}std::set<size_t>starts;uint64_t count=0;
  for(const auto&c:commands){if(c.epoch!=s.epoch||c.slot>=s.voices.size()){e="Key audio command scene/slot rejected";return false;}const auto&sound=s.data.sounds()[c.slot];const auto&v=*s.voices[c.slot];if(c.owner!=sound.owner||c.profile!=sound.profile||(v.channel<region_channel_pool::capacity&&region_channel_pool::owners()[v.channel]!=&v)){e="Key audio command node/channel rejected";return false;}if(c.playing){starts.insert(c.slot);++count;}}
  if(count>UINT64_MAX-plays_){e="Key audio play generation exhausted";return false;}size_t needed=0;for(auto slot:starts)needed+=s.voices[slot]->channel==region_channel_pool::capacity;auto&pool=region_channel_pool::owners();if(needed>size_t(std::count(pool.begin(),pool.end(),nullptr))){e="Key audio channels unavailable before mutation";return false;}
  s.pending=commands;s.reserved.clear();for(auto slot:starts){auto&v=*s.voices[slot];if(v.channel<pool.size())continue;const auto free=std::find(pool.begin(),pool.end(),nullptr);v.channel=uint32_t(free-pool.begin());pool[v.channel]=&v;s.reserved.push_back(slot);}s.batch=true;e.clear();return true;
 }
 void commit(){
  std::string e;if(!compatible(e)||!state_->batch)std::abort();auto&s=*state_;
  for(const auto&c:s.pending){auto&v=*s.voices[c.slot];drop_completion(c.slot);if(c.playing){if(v.channel>=region_channel_pool::capacity||region_channel_pool::owners()[v.channel]!=&v)std::abort();const int channel=hardware(v);ndspChnWaveBufClear(channel);ndspChnReset(channel);v.waves={};v.cursor=0;v.active=true;v.natural=false;v.play=++plays_;const auto&cache=*s.caches[v.cache];const auto&sound=s.data.sounds()[c.slot];ndspChnSetInterp(channel,NDSP_INTERP_POLYPHASE);ndspChnSetRate(channel,float(cache.asset.sample_rate)*sound.pitch);ndspChnSetFormat(channel,cache.asset.channels==2?NDSP_FORMAT_STEREO_PCM16:NDSP_FORMAT_MONO_PCM16);mix(c.slot);refill(v);}else clear(v,false);}
  for(auto&v:s.voices)if(!v->active&&!v->natural)clear(*v,true);s.pending.clear();s.reserved.clear();s.batch=false;
 }
 void cancel(){if(!state_||!state_->batch)return;for(auto slot:state_->reserved){auto&v=*state_->voices[slot];const bool natural=v.natural;clear(v,true);v.natural=natural;}state_->reserved.clear();state_->pending.clear();state_->batch=false;}
 bool pump_streams(std::string&e){
  if(!state_){e.clear();return true;}if(!compatible(e))return false;auto&s=*state_;if(s.batch||s.finishing){e="Key audio reservation must commit/cancel before pumping";return false;}
  for(const auto&ptr:s.voices){const auto&v=*ptr;if(v.active&&(v.channel>=region_channel_pool::capacity||region_channel_pool::owners()[v.channel]!=&v)){e="Key live audio channel tag differs";return false;}}
  for(uint32_t i=0;i<s.voices.size();++i){auto&v=*s.voices[i];if(!v.active)continue;refill(v);bool queued=false;for(const auto&w:v.waves)queued|=w.status==NDSP_WBUF_QUEUED||w.status==NDSP_WBUF_PLAYING;
   if(!queued&&v.cursor==s.caches[v.cache]->asset.frames){const auto&sound=s.data.sounds()[i];const KeyAudioCompletion done{s.epoch,v.play,i,sound.owner,sound.profile};clear(v,true);v.natural=true;if(s.completed.size()>=s.voices.size())std::abort();s.completed.push_back(done);}
  }e.clear();return true;
 }
 bool update(std::string&e){if(!pump_streams(e))return false;if(state_)for(size_t i=0;i<state_->voices.size();++i)if(state_->voices[i]->active)mix(i);return true;}
 const std::vector<KeyAudioCompletion>&completed()const{static const std::vector<KeyAudioCompletion>empty;return state_?state_->completed:empty;}
 bool completion_current(const KeyAudioCompletion&c,std::string&e)const{
  if(!compatible(e))return false;const auto&s=*state_;if(!s.epoch||c.epoch!=s.epoch||c.slot>=s.voices.size()){e="Key audio completion epoch/slot rejected";return false;}const auto&v=*s.voices[c.slot];const auto&sound=s.data.sounds()[c.slot];if(!c.play||c.play!=v.play||v.active||!v.natural||c.owner!=sound.owner||c.profile!=sound.profile||std::none_of(s.completed.begin(),s.completed.end(),[&](const auto&pending){return pending.slot==c.slot&&pending.play==c.play&&pending.epoch==c.epoch;})){e="Key audio completion generation/node rejected";return false;}e.clear();return true;
 }
 // Writes a candidate only. The caller prepares other consumers, publishes its
 // world transaction, then acknowledges with commit_completed(). Cancellation
 // leaves natural finishes pending for retry; no source callbacks are lost.
 bool reserve_completed(const upstream::DungeonKeyAudioRouter&router,const upstream::DungeonKeyClocks&world,upstream::DungeonKeyClocks&candidate,std::string&e){
  if(!compatible(e))return false;auto&s=*state_;if(s.batch||s.finishing||s.completed.empty()||world.epoch()!=s.epoch||router.epoch()!=s.epoch||!matches(router.data(),e)||!s.data.matches(world,e)){e="Key audio completion transaction owner/state rejected";return false;}
  auto next=world;for(const auto&c:s.completed)if(!completion_current(c,e)||!router.finished(c.epoch,c.slot,next,e))return false;candidate=std::move(next);s.finishing=true;e.clear();return true;
 }
 void commit_completed(){std::string e;if(!compatible(e)||!state_->finishing)std::abort();for(const auto&c:state_->completed)state_->voices[c.slot]->natural=false;state_->completed.clear();state_->finishing=false;}
 void cancel_completed(){if(state_)state_->finishing=false;}
 bool retire_scene(uint64_t epoch,std::string&e){if(!compatible(e))return false;auto&s=*state_;if(!epoch||epoch!=s.epoch||s.batch||s.finishing){e="Key audio retirement epoch/state rejected";return false;}for(auto&v:s.voices)clear(*v,true);s.completed.clear();s.epoch=0;e.clear();return true;}
 void shutdown(){if(!state_)return;for(auto&v:state_->voices)clear(*v,true);state_.reset();}
 bool prepared()const{return bool(state_);}uint64_t epoch()const{return state_?state_->epoch:0;}uint64_t play_requests()const{return plays_;}size_t active_nodes()const{size_t n=0;if(state_)for(const auto&v:state_->voices)n+=v->active;return n;}
private:
 static constexpr uint32_t buffer_count=3,buffer_frames=2048;
 struct Cache{~Cache(){if(pcm)linearFree(pcm);}upstream::AudioAsset asset;int16_t*pcm=nullptr;};
 struct Voice{std::array<ndspWaveBuf,buffer_count>waves{};size_t cache=SIZE_MAX;uint32_t channel=region_channel_pool::capacity,cursor=0;uint64_t play=0;bool active=false,natural=false;};
 struct State{upstream::DungeonKeyMediaData data;AudioPlayer*player=nullptr;uint64_t configuration=0,epoch=0,last_epoch=0;bool batch=false,finishing=false;std::vector<std::unique_ptr<Cache>>caches;std::vector<std::unique_ptr<Voice>>voices;std::vector<upstream::KeySoundCommand>pending;std::vector<size_t>reserved;std::vector<KeyAudioCompletion>completed;};
 bool compatible(std::string&e)const{if(!state_||!state_->player||!state_->player->available()||state_->configuration!=state_->player->configuration_generation()){e="Key audio output configuration differs";return false;}return true;}
 static int hardware(const Voice&v){return int(region_channel_pool::first_channel+v.channel);}
 void drop_completion(uint32_t slot){auto&s=*state_;s.completed.erase(std::remove_if(s.completed.begin(),s.completed.end(),[&](const auto&c){return c.slot==slot;}),s.completed.end());s.voices[slot]->natural=false;}
 void mix(size_t slot){auto&s=*state_;const auto&v=*s.voices[slot];const auto&sound=s.data.sounds()[slot];const auto&asset=s.caches[v.cache]->asset;float gain[12]{};gain[0]=gain[1]=upstream::audio_linear_gain(asset.gain_db+sound.gain+s.player->user_sfx_db());ndspChnSetMix(hardware(v),gain);}
 void refill(Voice&v){const auto&cache=*state_->caches[v.cache];for(auto&w:v.waves){if(w.status!=NDSP_WBUF_FREE&&w.status!=NDSP_WBUF_DONE)continue;if(v.cursor==cache.asset.frames)break;const uint32_t frames=std::min(buffer_frames,cache.asset.frames-v.cursor);w={};w.data_pcm16=cache.pcm+size_t(v.cursor)*cache.asset.channels;w.nsamples=frames;w.looping=false;v.cursor+=frames;ndspChnWaveBufAdd(hardware(v),&w);}}
 void clear(Voice&v,bool release){if(v.channel<region_channel_pool::capacity){auto&owner=region_channel_pool::owners()[v.channel];if(owner==&v){if(state_->player->available()&&state_->configuration==state_->player->configuration_generation())ndspChnWaveBufClear(hardware(v));if(release)owner=nullptr;}if(release)v.channel=region_channel_pool::capacity;}v.waves={};v.cursor=0;v.active=false;v.natural=false;}
 std::unique_ptr<State>state_;uint64_t plays_=0;
};
}

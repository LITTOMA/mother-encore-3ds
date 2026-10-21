#pragma once
#include "audio_player.hpp"
#include "encore/field_steps.hpp"
#include "encore/load_progress.hpp"
#include <algorithm>
#include <cstdlib>
#include <limits>
#include <memory>
namespace encore::ctr {
// Independent scene bank;the persistent AudioPlayer owns NDSP/master volume.
// Metadata/AudioPlayer outlive this service. Scene handoff retires the legacy
// Run lane first;only one independent named run service can be active.
class FieldRunBankAudioService final {
 struct Cache {upstream::AudioAsset asset;upstream::AudioFrameCursor initial;int16_t*pcm=nullptr;~Cache(){if(pcm)linearFree(pcm);}};
 struct Voice {std::array<ndspWaveBuf,3>waves{};upstream::AudioFrameCursor cursor;uint32_t channel=region_channel_pool::capacity,sound=upstream::kFieldNone;bool active=false;float gain=0;};
 struct State {upstream::AudioBank bank;upstream::FieldStepsView data;AudioPlayer*player=nullptr;const upstream::FieldRunLoop*loop=nullptr;uint64_t configuration=0,epoch=0;std::vector<std::unique_ptr<Cache>>caches;Voice voice;};
 static const void*&named_owner(){static const void*value=nullptr;return value;}
 static bool bad(std::string&e,const char*s){e=s;return false;}
 bool output(std::string&e)const{if(!state_||!state_->player->available()||state_->configuration!=state_->player->configuration_generation())return bad(e,"Independent run output configuration differs");return true;}
 bool scene(uint64_t epoch,std::string&e)const{if(!output(e))return false;const auto&s=*state_;const auto&v=s.voice;if(!epoch||epoch!=s.epoch||v.channel>=region_channel_pool::capacity||region_channel_pool::owners()[v.channel]!=&v||named_owner()!=&v||s.player->playing(AudioLane::Run))return bad(e,"Independent run scene/channel/named owner rejected");return true;}
 static int hardware(const Voice&v){return int(region_channel_pool::first_channel+v.channel);}
 bool wave_states(std::string&e)const{for(const auto&w:state_->voice.waves)if(w.status!=NDSP_WBUF_FREE&&w.status!=NDSP_WBUF_QUEUED&&w.status!=NDSP_WBUF_PLAYING&&w.status!=NDSP_WBUF_DONE)return bad(e,"Independent run unknown NDSP wave state rejected");return true;}
 void stop(bool release){auto&s=*state_;auto&v=s.voice;if(v.channel<region_channel_pool::capacity){auto&owner=region_channel_pool::owners()[v.channel];if(owner==&v){if(s.player->available()&&s.configuration==s.player->configuration_generation())ndspChnWaveBufClear(hardware(v));if(release)owner=nullptr;}else if(v.active&&s.player->available()&&s.configuration==s.player->configuration_generation())std::abort();if(release)v.channel=region_channel_pool::capacity;}v.waves={};v.active=false;if(release){v.sound=upstream::kFieldNone;if(named_owner()==&v)named_owner()=nullptr;}}
 void mix(){auto&s=*state_;auto&v=s.voice;v.gain=s.caches[v.sound]->asset.gain_db+s.player->user_sfx_db();float gain[12]{};gain[0]=gain[1]=upstream::audio_linear_gain(v.gain);ndspChnSetMix(hardware(v),gain);}
 void refill(){auto&s=*state_;auto&v=s.voice;const auto&cache=*s.caches[v.sound];for(auto&w:v.waves){if(w.status==NDSP_WBUF_QUEUED||w.status==NDSP_WBUF_PLAYING)continue;uint32_t first=0;const auto frames=v.cursor.take(2048,first);if(!frames||first>=cache.asset.frames||frames>cache.asset.frames-first)std::abort();w={};w.data_pcm16=cache.pcm+size_t(first)*cache.asset.channels;w.nsamples=frames;w.looping=false;ndspChnWaveBufAdd(hardware(v),&w);}}
public:
 ~FieldRunBankAudioService(){shutdown();}
 FieldRunBankAudioService()=default;FieldRunBankAudioService(const FieldRunBankAudioService&)=delete;FieldRunBankAudioService&operator=(const FieldRunBankAudioService&)=delete;
 bool prepare(upstream::FieldStepsView data,const upstream::AudioBank&bank,AudioPlayer&player,const char*root,std::string&e){
  if(state_||!root||!*root||!data.valid()||!player.available()||bank.count()!=data.sound_count()||bank.master_db()!=player.bank().master_db()||bank.silence_db()!=player.bank().silence_db()||!data.matches(bank,e))return bad(e,"Independent run needs fresh matching checked media/output");
  auto next=std::make_unique<State>();next->bank=bank;next->data=data;next->player=&player;next->configuration=player.configuration_generation();size_t bytes=0;
  for(uint32_t i=0;i<data.sound_count();++i){upstream::AudioAsset asset;if(!next->bank.find(data.sound(i).id,asset)||!asset.loops()||asset.loop_start||!asset.pcm_bytes||asset.pcm_bytes>512*1024||bytes>4*1024*1024-asset.pcm_bytes)return bad(e,"Independent run loop/cache capacity rejected");bytes+=asset.pcm_bytes;auto cache=std::make_unique<Cache>();cache->asset=asset;if(!cache->initial.reset(asset.frames,asset.loop_start,true))return bad(e,"Independent run initial cursor rejected");std::string path(root);if(path.back()!='/'&&path.back()!='\\')path+='/';path+=asset.pcm_path;upstream::AudioPcmStream stream;if(!stream.open(asset,path.c_str(),e))return false;cache->pcm=static_cast<int16_t*>(linearAlloc(asset.pcm_bytes));if(!cache->pcm)return bad(e,"Independent run cache allocation failed");uint32_t completed=0;while(completed<asset.frames){uint32_t frames=0;const auto requested=std::min(uint32_t(2048),asset.frames-completed);if(!stream.read(cache->pcm+size_t(completed)*asset.channels,requested,frames,e))return false;if(frames!=requested)return bad(e,"Independent run short cache read rejected");completed+=frames;report_load_progress(LoadPhase::FileRead,uint64_t(completed)*asset.channels*sizeof(int16_t),asset.pcm_bytes);}if(upstream::audio_crc32(reinterpret_cast<const uint8_t*>(cache->pcm),asset.pcm_bytes)!=asset.pcm_crc)return bad(e,"Independent run cached PCM identity rejected");if(R_FAILED(DSP_FlushDataCache(cache->pcm,asset.pcm_bytes)))return bad(e,"Independent run cache flush failed");next->caches.push_back(std::move(cache));}
  state_=std::move(next);e.clear();return true;
 }
 bool activate(uint64_t epoch,std::string&e){if(!output(e))return false;auto&s=*state_;if(!epoch||s.epoch||epoch<=last_epoch_||named_owner()||s.player->playing(AudioLane::Run))return bad(e,"Independent run fresh scene/named lane rejected");auto&pool=region_channel_pool::owners();const auto free=std::find(pool.begin(),pool.end(),nullptr);if(free==pool.end())return bad(e,"Independent run channel unavailable before activation");s.voice.channel=uint32_t(free-pool.begin());*free=&s.voice;named_owner()=&s.voice;s.epoch=last_epoch_=epoch;e.clear();return true;}
 bool sync(uint64_t epoch,upstream::FieldRunLoop&loop,std::string&e){
  if(!scene(epoch,e))return false;auto&s=*state_;auto&v=s.voice;if(!loop.prepared()||loop.content().bytes_identity()!=s.data.bytes_identity()||(s.loop&&s.loop!=&loop)||loop.registered_sound()>=s.caches.size())return bad(e,"Independent run loop/source owner rejected");const auto&events=loop.events();if(events.size()>4096)return bad(e,"Independent run event batch capacity rejected");bool active=v.active;uint32_t sound=v.sound;uint64_t starts=0;
  for(const auto&event:events){if(event.sound>=s.caches.size())return bad(e,"Independent run event sound rejected");switch(event.kind){case upstream::FieldRunEventKind::Play:if(active)return bad(e,"Independent run duplicate source play rejected");active=true;sound=event.sound;++starts;break;case upstream::FieldRunEventKind::ReplacePlay:if(sound==event.sound)return bad(e,"Independent run unchanged replacement rejected");active=true;sound=event.sound;++starts;break;case upstream::FieldRunEventKind::Stop:if(!active||sound!=event.sound)return bad(e,"Independent run source stop mismatch");active=false;break;default:return bad(e,"Independent run unknown source event rejected");}}
  if(sound==upstream::kFieldNone&&!active&&events.empty())sound=loop.registered_sound();if(starts>UINT64_MAX-plays_||active!=loop.playing()||sound!=loop.registered_sound())return bad(e,"Independent run pending events/state mismatch");
  s.loop=&loop;
  for(const auto&event:events){if(event.kind==upstream::FieldRunEventKind::Stop)stop(false);else{stop(false);v.sound=event.sound;v.cursor=s.caches[v.sound]->initial;v.active=true;ndspChnReset(hardware(v));const auto&a=s.caches[v.sound]->asset;ndspChnSetInterp(hardware(v),NDSP_INTERP_POLYPHASE);ndspChnSetRate(hardware(v),float(a.sample_rate));ndspChnSetFormat(hardware(v),a.channels==2?NDSP_FORMAT_STEREO_PCM16:NDSP_FORMAT_MONO_PCM16);mix();refill();++plays_;}}
  v.sound=sound;loop.clear_events();e.clear();return true;
 }
 bool pump_streams(std::string&e){if(!output(e))return false;if(state_->voice.active){if(!scene(state_->epoch,e)||!wave_states(e))return false;refill();}e.clear();return true;}
 bool update(std::string&e){if(!output(e))return false;if(state_->voice.active){if(!scene(state_->epoch,e)||!wave_states(e))return false;const auto&s=*state_;if(s.voice.gain!=s.caches[s.voice.sound]->asset.gain_db+s.player->user_sfx_db())mix();refill();}e.clear();return true;}
 bool retire(uint64_t epoch,std::string&e){if(!scene(epoch,e))return false;stop(true);state_->loop=nullptr;state_->epoch=0;e.clear();return true;}
 void shutdown(){if(!state_)return;stop(true);state_.reset();}
 bool prepared()const{return bool(state_);}uint64_t epoch()const{return state_?state_->epoch:0;}bool playing()const{return state_&&state_->voice.active;}uint32_t registered_sound()const{return state_?state_->voice.sound:upstream::kFieldNone;}uint64_t play_requests()const{return plays_;}
private:std::unique_ptr<State>state_;uint64_t last_epoch_=0,plays_=0;
};
}

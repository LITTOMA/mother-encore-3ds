#pragma once
#include "audio_player.hpp"
#include "encore/geyser_mixer.hpp"
#include "encore/load_progress.hpp"
#include <algorithm>
#include <memory>
namespace encore::ctr {
// One pooled output for source-owned logical Geyser voices;preparing caches
// cooperates with persistent BGM. Queued samples introduce at most three output
// buffers of control latency;this is not Godot decoder/mixer byte equivalence.
class GeyserAudioService final {
 struct Cache {upstream::AudioAsset asset;int16_t*pcm=nullptr;~Cache(){if(pcm)linearFree(pcm);}};
 struct State {
  upstream::GeyserMediaData data;upstream::GeyserMixer mixer;AudioPlayer*player=nullptr;uint64_t configuration=0;
  std::vector<std::unique_ptr<Cache>>caches;int16_t*output=nullptr;uint32_t channel=region_channel_pool::capacity;
  std::array<ndspWaveBuf,3>waves{};std::array<uint64_t,3>ends{};
  ~State(){if(output)linearFree(output);}
 };
 static constexpr uint32_t frames_per_buffer=512;
 static const void*&named_owner(){static const void*value=nullptr;return value;}
 static bool bad(std::string&e,const char*s){e=s;return false;}
 bool compatible(std::string&e)const{if(!state_||!state_->player->available()||state_->configuration!=state_->player->configuration_generation())return bad(e,"Geyser output configuration differs");return true;}
 static int hardware(const State&s){return int(region_channel_pool::first_channel+s.channel);}
 bool channel(std::string&e)const{if(!compatible(e))return false;const auto&s=*state_;if(s.channel>=region_channel_pool::capacity||region_channel_pool::owners()[s.channel]!=&s||named_owner()!=&s)return bad(e,"Geyser output channel/named owner differs");return true;}
 void release(){auto&s=*state_;if(s.channel<region_channel_pool::capacity){auto&owner=region_channel_pool::owners()[s.channel];if(owner==&s){if(s.player->available()&&s.configuration==s.player->configuration_generation())ndspChnWaveBufClear(hardware(s));owner=nullptr;}s.channel=region_channel_pool::capacity;}if(named_owner()==&s)named_owner()=nullptr;s.waves={};s.ends={};}
 bool wave_states(std::string&e)const{for(const auto&w:state_->waves)if(w.status!=NDSP_WBUF_FREE&&w.status!=NDSP_WBUF_QUEUED&&w.status!=NDSP_WBUF_PLAYING&&w.status!=NDSP_WBUF_DONE)return bad(e,"Geyser unknown NDSP wave state");return true;}
public:
 GeyserAudioService()=default;~GeyserAudioService(){shutdown();}
 GeyserAudioService(const GeyserAudioService&)=delete;GeyserAudioService&operator=(const GeyserAudioService&)=delete;
 bool prepare(const upstream::GeyserMediaData&data,AudioPlayer&player,const char*root,std::string&e){
  if(state_||!data.valid()||!root||!*root||!player.available()||data.bank().master_db()!=player.bank().master_db()||data.bank().silence_db()!=player.bank().silence_db())return bad(e,"Geyser needs fresh checked media/output");auto next=std::make_unique<State>();next->data=data;next->player=&player;next->configuration=player.configuration_generation();std::vector<upstream::GeyserPcmView>views;
  for(uint32_t i=0;i<data.bank().count();++i){auto cache=std::make_unique<Cache>();cache->asset=data.bank().asset(i);const auto&a=cache->asset;std::string path(root);if(path.back()!='/'&&path.back()!='\\')path+='/';path+=a.pcm_path;upstream::AudioPcmStream stream;if(!stream.open(a,path.c_str(),e))return false;cache->pcm=static_cast<int16_t*>(linearAlloc(a.pcm_bytes));if(!cache->pcm)return bad(e,"Geyser cache allocation failed");uint32_t completed=0;
   while(completed<a.frames){uint32_t frames=0;const auto requested=std::min(uint32_t(2048),a.frames-completed);if(!stream.read(cache->pcm+size_t(completed)*a.channels,requested,frames,e))return false;if(frames!=requested)return bad(e,"Geyser short cached PCM");completed+=frames;report_load_progress(LoadPhase::FileRead,uint64_t(completed)*a.channels*2,a.pcm_bytes);}views.push_back({a.stable_id,cache->pcm,size_t(a.frames)*a.channels});next->caches.push_back(std::move(cache));
  }
  if(!next->mixer.prepare(data,views,e))return false;next->output=static_cast<int16_t*>(linearAlloc(size_t(3)*frames_per_buffer*2*sizeof(int16_t)));if(!next->output)return bad(e,"Geyser output buffer allocation failed");state_=std::move(next);e.clear();return true;
 }
 bool activate(uint64_t epoch,std::string&e){
  if(!compatible(e))return false;auto&s=*state_;auto candidate=s.mixer;if(!candidate.activate(epoch,e))return false;
  if(s.channel>=region_channel_pool::capacity){if(named_owner())return bad(e,"Geyser named owner already active");auto&pool=region_channel_pool::owners();const auto free=std::find(pool.begin(),pool.end(),nullptr);if(free==pool.end())return bad(e,"Geyser output pool exhausted");s.channel=uint32_t(free-pool.begin());*free=&s;named_owner()=&s;ndspChnReset(hardware(s));ndspChnSetInterp(hardware(s),NDSP_INTERP_POLYPHASE);ndspChnSetRate(hardware(s),float(s.data.sample_rate()));ndspChnSetFormat(hardware(s),NDSP_FORMAT_STEREO_PCM16);float mix[12]{};mix[0]=mix[1]=1;ndspChnSetMix(hardware(s),mix);}
  else if(!channel(e))return false;s.mixer=std::move(candidate);e.clear();return true;
 }
 bool bind(uint64_t epoch,uint64_t instance,std::string&e){return channel(e)&&state_->mixer.bind(epoch,instance,e);}
 bool sync(uint64_t epoch,uint64_t instance,upstream::GeyserTimeline&timeline,upstream::Vec2 position,upstream::GeyserListener listener,std::string&e){return channel(e)&&state_->mixer.sync(epoch,instance,timeline,position,listener,e);}
 bool sync_lifecycle(uint64_t epoch,upstream::GeyserLifecycle&life,upstream::GeyserListener listener,std::string&e){return channel(e)&&state_->mixer.sync_lifecycle(epoch,life,listener,e);}
 bool global_tree_notification(bool paused,bool can_process,std::string&e){return compatible(e)&&state_->mixer.global_tree_notification(paused,can_process,e);}
 bool update(std::string&e){
  if(!compatible(e))return false;auto&s=*state_;if(!s.mixer.user_sfx(s.player->user_sfx_db(),e))return false;if(s.channel>=region_channel_pool::capacity){e.clear();return true;}if(!channel(e)||!wave_states(e))return false;uint64_t consumed=s.mixer.consumed_frames();
  for(size_t i=0;i<s.waves.size();++i)if(s.waves[i].status==NDSP_WBUF_DONE)consumed=std::max(consumed,s.ends[i]);if(!s.mixer.consumed(consumed,e))return false;
  for(size_t i=0;i<s.waves.size();++i){auto&w=s.waves[i];if(w.status==NDSP_WBUF_QUEUED||w.status==NDSP_WBUF_PLAYING)continue;int16_t*buffer=s.output+i*frames_per_buffer*2;uint32_t frames=0;if(!s.mixer.reserve_render(buffer,frames_per_buffer,frames,e))return false;if(!frames){s.mixer.cancel_render();continue;}if(R_FAILED(DSP_FlushDataCache(buffer,frames*2*sizeof(int16_t)))){s.mixer.cancel_render();return bad(e,"Geyser output flush failed before queue commit");}if(!s.mixer.commit_render(e))return false;w={};w.data_pcm16=buffer;w.nsamples=frames;w.looping=false;s.ends[i]=s.mixer.rendered_frames();ndspChnWaveBufAdd(hardware(s),&w);}
  if(!s.mixer.epoch()&&s.mixer.global_voice()!=upstream::GeyserNamedVoice::Playing){bool queued=false;for(const auto&w:s.waves)queued|=w.status==NDSP_WBUF_QUEUED||w.status==NDSP_WBUF_PLAYING;if(!queued)release();}e.clear();return true;
 }
 bool retire_actor(uint64_t epoch,uint64_t instance,std::string&e){return channel(e)&&state_->mixer.retire_actor(epoch,instance,e);}
 bool retire_scene(uint64_t epoch,std::string&e){return channel(e)&&state_->mixer.retire_scene(epoch,e);}
 void shutdown(){if(!state_)return;release();state_.reset();}
 bool prepared()const{return bool(state_);}const upstream::GeyserMixer*observe()const{return state_?&state_->mixer:nullptr;}
 upstream::GeyserNamedVoice global_voice()const{return state_?state_->mixer.global_voice():upstream::GeyserNamedVoice::Absent;}
private:std::unique_ptr<State>state_;
};
}

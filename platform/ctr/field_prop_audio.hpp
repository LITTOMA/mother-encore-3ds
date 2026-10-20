#pragma once
#include "audio_player.hpp"
#include "encore/field_props.hpp"
#include <algorithm>
#include <memory>
#include <set>
#include <cstdlib>
namespace encore::ctr {
// One immutable, checked PCM cache, with independent source-node cursors and
// hardware queues. A hit neither opens a file nor changes any Music/Effect
// lane. Only this service's ownership tags may release its pooled channels.
class FieldPropAudioService final:public upstream::FieldPropContactEffects {
public:
 ~FieldPropAudioService(){shutdown();}
 bool prepare(const upstream::FieldPropsData&data,AudioPlayer&player,const char*root,std::string&error){
  auto fail=[&](const char*why){error=why;return false;};
  if(state_||!root||!data.contact_bound())return fail("Prop sound needs one checked source owner");
  upstream::AudioAsset asset;if(!data.matches_audio(player.bank(),asset,error))return false;
  // This bounded cache admits short, nonloop node sounds. Longer or looping
  // sources need a separately reviewed consumer, never silent truncation.
  if(!asset.pcm_bytes||asset.pcm_bytes>512*1024||data.props().empty()||data.props().size()>128)return fail("Prop sound cache/node capacity rejected");
  auto next=std::make_unique<State>();next->player=&player;next->generation=player.configuration_generation();next->available=player.available();next->bytes=data.encoded_bytes();next->crc=data.encoded_crc();next->asset=asset;next->policy=data.policy().contact;next->pending.reserve(128);
  for(size_t i=0;i<data.props().size();++i)next->voices.push_back(std::make_unique<Voice>());
  if(next->available){
   next->pcm=static_cast<int16_t*>(linearAlloc(asset.pcm_bytes));if(!next->pcm)return fail("Prop sound cache allocation failed");
   std::string path=root;if(!path.empty()&&path.back()!='/'&&path.back()!='\\')path+='/';path+=asset.pcm_path;
   upstream::AudioPcmStream stream;uint32_t frames=0;
   if(!stream.open(asset,path.c_str(),error)||!stream.read(next->pcm,asset.frames,frames,error))return false;
   if(frames!=asset.frames)return fail("Prop sound cache has incomplete PCM");
   if(R_FAILED(DSP_FlushDataCache(next->pcm,asset.pcm_bytes)))return fail("Prop sound cache flush failed");
  }
  state_=std::move(next);error.clear();return true;
 }
 bool matches(const upstream::FieldPropsData&data,std::string&error)const override{
  if(!compatible(error)||!data.contact_bound()||data.encoded_bytes()!=state_->bytes||data.encoded_crc()!=state_->crc){if(error.empty())error="Prop sound source resource differs";return false;}error.clear();return true;
 }
 bool prepare_hits(const upstream::FieldPropsData&data,const std::vector<uint32_t>&indices,std::string&error)override{
  if(!matches(data,error))return false;auto&s=*state_;
  if(!s.pending.empty()||indices.empty()||indices.size()>s.voices.size()){error="Prop sound reservation state/count rejected";return false;}
  std::set<uint32_t>seen;size_t required=0;
  for(auto i:indices){if(i>=s.voices.size()||!seen.insert(i).second){error="Prop sound reservation identity rejected";return false;}auto&v=*s.voices[i];if(v.channel==region_channel_pool::capacity)++required;else if(region_channel_pool::owners()[v.channel]!=&v){error="Prop sound channel ownership differs";return false;}}
  auto&pool=region_channel_pool::owners();if(s.available&&required>size_t(std::count(pool.begin(),pool.end(),nullptr))){error="Prop sound channels unavailable before contact";return false;}
  // All validation precedes reservation. No PCM cursor or existing queue is
  // changed until the matching commit, including when an old node restarts.
  if(s.available)for(auto i:indices){auto&v=*s.voices[i];if(v.channel!=region_channel_pool::capacity)continue;const auto free=std::find(pool.begin(),pool.end(),nullptr);v.channel=uint32_t(free-pool.begin());pool[v.channel]=&v;}
  s.pending=indices;s.consumed=0;error.clear();return true;
 }
 void commit_hit(uint32_t index)override{
  if(!state_||state_->consumed>=state_->pending.size()||state_->pending[state_->consumed]!=index)std::abort();auto&s=*state_;auto&v=*s.voices[index];
  if(s.available){const auto channel=hardware(v);ndspChnWaveBufClear(channel);ndspChnReset(channel);v.waves={};v.cursor=0;v.active=true;
   ndspChnSetInterp(channel,NDSP_INTERP_POLYPHASE);ndspChnSetRate(channel,float(s.asset.sample_rate)*s.policy.pitch);ndspChnSetFormat(channel,s.asset.channels==2?NDSP_FORMAT_STEREO_PCM16:NDSP_FORMAT_MONO_PCM16);mix(v);refill(v);}
  ++starts_;if(++s.consumed==s.pending.size()){s.pending.clear();s.consumed=0;}
 }
 void stop_deleted(uint32_t index)override{if(!state_||index>=state_->voices.size())std::abort();stop(*state_->voices[index]);}
 bool pump_streams(std::string&error){
  if(!state_){error.clear();return true;}if(!compatible(error))return false;if(!state_->available){error.clear();return true;}
  for(auto&ptr:state_->voices){auto&v=*ptr;if(!v.active)continue;if(v.channel>=region_channel_pool::capacity||region_channel_pool::owners()[v.channel]!=&v){error="Prop sound live channel owner differs";return false;}
   refill(v);bool queued=false;for(const auto&w:v.waves)queued|=w.status==NDSP_WBUF_QUEUED||w.status==NDSP_WBUF_PLAYING;if(!queued&&v.cursor==state_->asset.frames)stop(v);
  }error.clear();return true;
 }
 bool update(std::string&error){if(!pump_streams(error))return false;if(state_&&state_->available)for(auto&v:state_->voices)if(v->active)mix(*v);return true;}
 void shutdown(){if(!state_)return;for(auto&v:state_->voices)stop(*v);state_.reset();starts_=0;}
 bool prepared()const{return state_!=nullptr;}uint64_t starts()const{return starts_;}
 size_t active_nodes()const{size_t n=0;if(state_)for(const auto&v:state_->voices)n+=v->active;return n;}
private:
 static constexpr uint32_t buffer_count=3,buffer_frames=2048;
 struct Voice {std::array<ndspWaveBuf,buffer_count>waves{};uint32_t channel=region_channel_pool::capacity,cursor=0;bool active=false;};
 struct State {
  ~State(){if(pcm)linearFree(pcm);}
  AudioPlayer*player=nullptr;uint64_t generation=0;bool available=false;uint32_t bytes=0,crc=0;upstream::AudioAsset asset;upstream::FieldPropContactPolicy policy;int16_t*pcm=nullptr;
  std::vector<std::unique_ptr<Voice>>voices;std::vector<uint32_t>pending;size_t consumed=0;
 };
 bool compatible(std::string&error)const{if(!state_||!state_->player||state_->player->configuration_generation()!=state_->generation||state_->player->available()!=state_->available){error="Prop sound service/player configuration differs";return false;}return true;}
 static int hardware(const Voice&v){return int(region_channel_pool::first_channel+v.channel);}
 void mix(Voice&v){float gain[12]{};gain[0]=gain[1]=upstream::audio_linear_gain(state_->asset.gain_db+state_->policy.gain_db+state_->player->user_sfx_db());ndspChnSetMix(hardware(v),gain);}
 void refill(Voice&v){auto&s=*state_;for(auto&w:v.waves){if(w.status!=NDSP_WBUF_FREE&&w.status!=NDSP_WBUF_DONE)continue;if(v.cursor==s.asset.frames)break;const auto frames=std::min(buffer_frames,s.asset.frames-v.cursor);w={};w.data_pcm16=s.pcm+size_t(v.cursor)*s.asset.channels;w.nsamples=frames;w.looping=false;v.cursor+=frames;ndspChnWaveBufAdd(hardware(v),&w);}}
 void stop(Voice&v){if(v.channel<region_channel_pool::capacity){auto&owner=region_channel_pool::owners()[v.channel];if(owner==&v){if(state_->available&&state_->player->available()&&state_->generation==state_->player->configuration_generation())ndspChnWaveBufClear(hardware(v));owner=nullptr;}v.channel=region_channel_pool::capacity;}v.active=false;v.waves={};}
 std::unique_ptr<State>state_;uint64_t starts_=0;
};
}

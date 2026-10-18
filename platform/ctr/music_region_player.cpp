#include "music_region_player.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
namespace encore::ctr {namespace {
// Per-voice leases let old source fades outlive their scene while the next
// scene starts. No adapter may reset or reuse another adapter's live channel.
auto&channel_owners=region_channel_pool::owners();
bool fail(std::string&e,const char*m){e=m;return false;}}
bool MusicRegionPlayer::begin_prepare(const char*bank,const char*root,const upstream::MusicRegionData&regions,uint32_t capacity,bool available,float master,std::string&e){
 if(prepared_||preparing_||capacity_)return fail(e,"Music region adapter already prepared; use a detached candidate");
 if(!available)return fail(e,"Music region audio unavailable: existing NDSP owner is not initialized");
 if(!root||!capacity||capacity>maximum_voices)return fail(e,"Invalid music region adapter capacity/root");
 if(!bank_.load_file(bank,e)||!regions.matches(bank_,e))return false;
 if(bank_.master_db()!=master||bank_.count()!=regions.tracks().size()||bank_.count()>tracks_.size())return fail(e,"Music region bank/master differs from existing audio owner");
 prepared_regions_=regions;capacity_=capacity;track_count_=bank_.count();asset_root_=root;
 for(uint32_t i=0;i<track_count_;++i){tracks_[i].asset=bank_.asset(i);total_bytes_+=tracks_[i].asset.pcm_bytes;}
 preparing_=true;e.clear();return true;
}
MusicPreparationStep MusicRegionPlayer::prepare_step(uint32_t budget,std::string&e){
 if(prepared_){e.clear();return MusicPreparationStep::Ready;}
 if(!preparing_||!budget||budget>maximum_prepare_bytes){e="Invalid music region preparation state/byte budget";return MusicPreparationStep::Failed;}
 auto abort=[&](const char*message){shutdown();e=message;return MusicPreparationStep::Failed;};
 if(validation_track_<track_count_){
  auto&t=tracks_[validation_track_];const auto&a=t.asset;
  if(!t.file){std::string path=asset_root_;path+=a.pcm_path;t.file=std::fopen(path.c_str(),"rb");if(!t.file)return abort("Cannot open region PCM asset");
   // Bound the actual validation read request too, rather than allowing stdio
   // read-ahead to defeat a small caller budget. Remains a read-only file.
   if(std::setvbuf(t.file,nullptr,_IONBF,0)!=0)return abort("Cannot configure bounded region PCM reads");
  }
  uint8_t scratch[8192];const auto remaining=uint64_t(a.pcm_bytes)+1-track_bytes_;
  const size_t wanted=size_t(std::min({uint64_t(budget),uint64_t(sizeof(scratch)),remaining}));
  const auto n=std::fread(scratch,1,wanted,t.file);track_bytes_+=n;verified_bytes_+=n;validation_crc_=crc32_update(validation_crc_,scratch,n);
  if(std::ferror(t.file)||track_bytes_>a.pcm_bytes)return abort("Region PCM length/checksum/read validation failed");
  if(n<wanted){
   if(!std::feof(t.file)||track_bytes_!=a.pcm_bytes||(validation_crc_^0xffffffffu)!=a.pcm_crc||std::fseek(t.file,0,SEEK_SET)!=0)return abort("Region PCM length/checksum/read validation failed");
   ++validation_track_;track_bytes_=0;validation_crc_=0xffffffffu;
  }
  e.clear();return MusicPreparationStep::Progress;
 }
 if(allocation_voice_<capacity_){auto&v=voices_[allocation_voice_];v.samples=static_cast<int16_t*>(linearAlloc(buffer_count*buffer_frames*2*sizeof(int16_t)));if(!v.samples)return abort("Region streaming-buffer allocation failed; existing audio preserved");++allocation_voice_;}
 if(allocation_voice_==capacity_){preparing_=false;prepared_=true;asset_root_.clear();e.clear();return MusicPreparationStep::Ready;}
 e.clear();return MusicPreparationStep::Progress;
}
void MusicRegionPlayer::stop(uint32_t slot){auto&v=voices_[slot];
 if(v.active&&v.channel<maximum_voices&&channel_owners[v.channel]==this){
  ndspChnWaveBufClear(int(first_channel+v.channel));if(paused_||v.stream_paused)ndspChnSetPaused(int(first_channel+v.channel),false);channel_owners[v.channel]=nullptr;
 }
 v.active=false;v.stream_paused=false;v.waves={};
}
bool MusicRegionPlayer::set_paused(bool paused,std::string&e){
 if(!prepared_){e="Music pause requires prepared stream ownership";return false;}
 if(paused_!=paused){for(uint32_t i=0;i<capacity_;++i)if(voices_[i].active)ndspChnSetPaused(int(first_channel+voices_[i].channel),paused||voices_[i].stream_paused);paused_=paused;}
 e.clear();return true;
}
void MusicRegionPlayer::shutdown(){for(uint32_t i=0;i<capacity_;++i){stop(i);if(voices_[i].samples)linearFree(voices_[i].samples);voices_[i]={};}for(auto&t:tracks_){if(t.file)std::fclose(t.file);t={};}capacity_=track_count_=submitted_=0;owner_=nullptr;graph_owner_.reset();provider_identity_=0;prepared_regions_={};finished_.clear();prepared_=preparing_=paused_=false;validation_track_=allocation_voice_=0;validation_crc_=0xffffffffu;track_bytes_=verified_bytes_=total_bytes_=0;asset_root_.clear();}
std::vector<MusicPlaybackFinished>MusicRegionPlayer::take_finished(){auto events=std::move(finished_);finished_.clear();return events;}
bool MusicRegionPlayer::refill(uint32_t slot,std::string&e){auto&v=voices_[slot];if(!v.active)return true;auto&t=tracks_[v.track];const auto&a=t.asset;bool queued=false;
 for(uint32_t b=0;b<buffer_count;++b){auto&wave=v.waves[b];if(wave.status==NDSP_WBUF_QUEUED||wave.status==NDSP_WBUF_PLAYING){queued=true;continue;}auto*samples=v.samples+size_t(b)*buffer_frames*2;uint32_t frames=0;
  while(frames<buffer_frames){uint32_t first=0;auto n=v.cursor.take(buffer_frames-frames,first);
   if(!n){if(a.loops()){stop(slot);return fail(e,"Region source loop unexpectedly ended");}break;}
   uint64_t offset=uint64_t(first)*a.channels*2;
   if(offset>uint64_t(std::numeric_limits<long>::max())||std::fseek(t.file,long(offset),SEEK_SET)!=0||std::fread(samples+size_t(frames)*a.channels,a.channels*2,n,t.file)!=n){stop(slot);return fail(e,"Region PCM streaming read failed");}frames+=n;
  }
  if(!frames){wave={};continue;}
  auto*bytes=reinterpret_cast<uint8_t*>(samples);for(size_t n=0;n<size_t(frames)*a.channels;++n){uint16_t value=uint16_t(bytes[n*2])|uint16_t(bytes[n*2+1])<<8;std::memcpy(samples+n,&value,2);}
  wave={};wave.data_pcm16=samples;wave.nsamples=frames;wave.looping=false;
  if(R_FAILED(DSP_FlushDataCache(samples,frames*a.channels*sizeof(int16_t)))){stop(slot);return fail(e,"Region DSP cache flush failed");}
  ndspChnWaveBufAdd(int(first_channel+v.channel),&wave);queued=true;
 }
 if(!a.loops()&&!queued&&v.cursor.position()==a.frames){
  if(!prepared_regions_.library()||!graph_owner_){stop(slot);return fail(e,"One-shot completion has no checked global music owner");}
  finished_.push_back({v.identity,v.generation});v.completed=true;stop(slot);
 }
 return true;
}
bool MusicRegionPlayer::sync(const upstream::MusicRegionController&controller,std::string&e){
 if(!prepared_||prepared_regions_.library()||controller.capacity()!=capacity_||graph_owner_||(owner_&&owner_!=&controller))return fail(e,"Music region adapter/controller capacity unavailable");
 PlaybackFrame frame{};
 for(uint32_t i=0;i<capacity_;++i){const auto&voice=controller.voices()[i];if(voice.allocated&&voice.playing)frame[i]={0,voice.generation,voice.track_id,voice.gain_db,true};}
 return sync_frame(frame,&controller,{},0,e);
}
bool MusicRegionPlayer::route_frame(const upstream::MusicBankRoutes&routes,uint64_t provider,PlaybackFrame&frame,std::string&e)const{
 if(!prepared_||!routes.ready()||!routes.owner()||owner_||!provider||provider==upstream::MusicLoopChoice::no_player||(graph_owner_&&graph_owner_!=routes.owner())||(provider_identity_&&provider_identity_!=provider)||routes.routes().size()>maximum_voices)return fail(e,"Global music route owner/provider/capacity is unavailable");
 const auto*metadata=routes.provider(provider);
 if(!metadata&&!provider_identity_)return fail(e,"Global music provider was never bound to this transport");
 if(metadata){
  if(metadata->bank.count()!=bank_.count()||metadata->bank.master_db()!=bank_.master_db()||metadata->bank.silence_db()!=bank_.silence_db()||!metadata->regions.matches(bank_,e))return fail(e,"Prepared music metadata differs from routed provider");
  for(const auto&track:prepared_regions_.tracks()){
   const upstream::MusicRegionTrack*matched=nullptr;for(const auto&t:metadata->regions.tracks())if(t.id==track.id)matched=&t;
   if(!matched||matched->source_path!=track.source_path||matched->source_sha!=track.source_sha||matched->import_sha!=track.import_sha)return fail(e,"Prepared music stream/import differs from routed provider");
  }
  for(uint32_t i=0;i<bank_.count();++i){const auto a=bank_.asset(i);upstream::AudioAsset b;
   if(!metadata->bank.find(a.stable_id,b)||a.source_path!=b.source_path||a.source_sha256!=b.source_sha256||a.sample_rate!=b.sample_rate||a.channels!=b.channels||a.flags!=b.flags||a.frames!=b.frames||a.loop_start!=b.loop_start||a.pcm_bytes!=b.pcm_bytes||a.pcm_crc!=b.pcm_crc||a.gain_db!=b.gain_db)return fail(e,"Prepared music PCM differs from routed provider");
  }
 }
 std::array<bool,maximum_voices>used{};
 for(const auto&route:routes.routes()){
  if(route.slot>=maximum_voices||used[route.slot])return fail(e,"Global music slots are invalid for this backend");
  used[route.slot]=true;
  if(route.provider!=provider)continue;
  if(!metadata||route.slot>=capacity_)return fail(e,"Global music provider voice capacity is unavailable");
  frame[route.slot]={route.player,route.generation,route.track_id,route.gain_db,true,route.stream_paused};
 }
 e.clear();return true;
}
bool MusicRegionPlayer::check_routes(const upstream::MusicBankRoutes&routes,uint64_t provider,std::string&e)const{PlaybackFrame frame{};return route_frame(routes,provider,frame,e);}
bool MusicRegionPlayer::release_routes(const upstream::MusicBankRoutes&routes,uint64_t provider,std::string&e){
 PlaybackFrame frame{};if(!route_frame(routes,provider,frame,e))return false;
 graph_owner_=routes.owner();provider_identity_=provider;
 for(uint32_t i=0;i<capacity_;++i){const auto&s=frame[i];const auto&v=voices_[i];if(!s.playing||s.identity!=v.identity||s.generation!=v.generation)stop(i);}
 e.clear();return true;
}
bool MusicRegionPlayer::sync_routes(const upstream::MusicBankRoutes&routes,uint64_t provider,std::string&e){PlaybackFrame frame{};if(!route_frame(routes,provider,frame,e))return false;return sync_frame(frame,nullptr,routes.owner(),provider,e);}
bool MusicRegionPlayer::sync_frame(const PlaybackFrame&input,const upstream::MusicRegionController*legacy,std::shared_ptr<const upstream::MusicPlayersOwner>global,uint64_t provider,std::string&e){
 PlaybackFrame frame=input;
 // A completed physical stream stays latched until the core publishes another
 // playback generation. Pump/sync may run while the main process is loading.
 for(uint32_t i=0;i<capacity_;++i){const auto&v=voices_[i];if(v.completed&&frame[i].identity==v.identity&&frame[i].generation==v.generation)frame[i].playing=false;}
 // Validate the complete snapshot before any old voice or other channel moves.
 std::array<uint32_t,maximum_voices>index{};
 for(uint32_t i=0;i<capacity_;++i){const auto&s=frame[i];if(!s.playing)continue;if(!s.generation||!std::isfinite(s.gain_db)||s.gain_db<-120||s.gain_db>24)return fail(e,"Invalid music region voice snapshot");uint32_t j=0;while(j<track_count_&&tracks_[j].asset.stable_id!=s.track_id)++j;if(j==track_count_)return fail(e,"Unbound music region track snapshot");index[i]=j;}
 // Build a complete channel assignment before changing any queue. Channels
 // already owned by this snapshot remain stable; released local channels may
 // be reused. Exhaustion leaves every active/draining owner untouched.
 std::array<bool,maximum_voices>used{};std::array<uint32_t,maximum_voices>channels{};channels.fill(maximum_voices);
 for(uint32_t j=0;j<maximum_voices;++j)used[j]=channel_owners[j]&&channel_owners[j]!=this;
 for(uint32_t i=0;i<capacity_;++i){const auto&s=frame[i];const auto&v=voices_[i];
  if(!s.playing||!v.active)continue;
  if(v.channel>=maximum_voices||channel_owners[v.channel]!=this||used[v.channel])return fail(e,"Invalid regional channel lease");
  channels[i]=v.channel;used[v.channel]=true;
 }
 // A failed stream may retry on its prior channel if still free. The hint
 // is not a lease and cannot displace any other active scene voice.
 for(uint32_t i=0;i<capacity_;++i){const auto&s=frame[i];const auto&v=voices_[i];if(s.playing&&channels[i]==maximum_voices&&v.channel<maximum_voices&&!used[v.channel]){channels[i]=v.channel;used[v.channel]=true;}}
 for(uint32_t i=0;i<capacity_;++i){const auto&s=frame[i];if(!s.playing||channels[i]<maximum_voices)continue;
  uint32_t j=0;while(j<maximum_voices&&used[j])++j;if(j==maximum_voices)return fail(e,"Regional channel capacity exhausted; existing audio preserved");channels[i]=j;used[j]=true;
 }
 for(uint32_t i=0;i<capacity_;++i){const auto&s=frame[i];if(!s.playing||(voices_[i].generation!=s.generation||voices_[i].identity!=s.identity))stop(i);}
 owner_=legacy;graph_owner_=std::move(global);provider_identity_=provider;
 for(uint32_t i=0;i<capacity_;++i){const auto&s=frame[i];auto&v=voices_[i];bool newly_started=false;if(!s.playing){stop(i);continue;}
  if(!v.active||(v.generation!=s.generation||v.identity!=s.identity)){stop(i);v.track=index[i];auto&a=tracks_[v.track].asset;if(!v.cursor.reset(a.frames,a.loop_start,a.loops()))return fail(e,"Invalid region cursor");v.identity=s.identity;v.generation=s.generation;v.completed=false;v.channel=channels[i];channel_owners[v.channel]=this;v.active=true;
   int channel=int(first_channel+v.channel);ndspChnReset(channel);ndspChnSetInterp(channel,NDSP_INTERP_POLYPHASE);ndspChnSetRate(channel,float(a.sample_rate));ndspChnSetFormat(channel,a.channels==2?NDSP_FORMAT_STEREO_PCM16:NDSP_FORMAT_MONO_PCM16);newly_started=true;
  }
  const bool changed_pause=v.stream_paused!=s.stream_paused;v.stream_paused=s.stream_paused;
  if((newly_started&&(paused_||v.stream_paused))||(!newly_started&&changed_pause&&!paused_))ndspChnSetPaused(int(first_channel+v.channel),paused_||v.stream_paused);
  float volumes[12]{};volumes[0]=volumes[1]=upstream::audio_linear_gain(s.gain_db+tracks_[v.track].asset.gain_db+user_music_db_);ndspChnSetMix(int(first_channel+v.channel),volumes);if((newly_started||(!paused_&&!v.stream_paused))&&!refill(i,e))return false;if(newly_started)++submitted_;
 }
 e.clear();return true;
}
bool MusicRegionPlayer::pump_streams(std::string&e){
 if(!prepared_||paused_){e.clear();return true;}
 for(uint32_t i=0;i<capacity_;++i)if(voices_[i].active&&(voices_[i].channel>=maximum_voices||channel_owners[voices_[i].channel]!=this))return fail(e,"Invalid regional streaming channel lease");
 for(uint32_t i=0;i<capacity_;++i)if(!voices_[i].stream_paused&&!refill(i,e))return false;
 e.clear();return true;
}

}

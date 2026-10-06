#include "music_region_player.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
namespace encore::ctr {namespace {
// One application-global reservation of the adapter channel range. A detached
// prepared candidate may coexist but cannot replace a live adapter by sync().
MusicRegionPlayer* channel_owner=nullptr;
bool fail(std::string&e,const char*m){e=m;return false;}}
bool MusicRegionPlayer::begin_prepare(const char*bank,const char*root,const upstream::MusicRegionData&regions,uint32_t capacity,bool available,float master,std::string&e){
 if(prepared_||preparing_||capacity_)return fail(e,"Music region adapter already prepared; use a detached candidate");
 if(!available)return fail(e,"Music region audio unavailable: existing NDSP owner is not initialized");
 if(!root||!capacity||capacity>maximum_voices)return fail(e,"Invalid music region adapter capacity/root");
 std::vector<upstream::AudioAsset> selected;
 if(!bank_.load_file(bank,e)||!regions.select_assets(bank_,master,uint32_t(tracks_.size()),selected,e))return false;
 // The shared bank also contains dialogue/menu effects. Only the checked
 // source region projection belongs to this bounded streaming adapter.
 capacity_=capacity;track_count_=uint32_t(selected.size());asset_root_=root;
 for(uint32_t i=0;i<track_count_;++i){tracks_[i].asset=selected[i];total_bytes_+=tracks_[i].asset.pcm_bytes;}
 preparing_=true;e.clear();return true;
}
MusicPreparationStep MusicRegionPlayer::prepare_step(uint32_t budget,std::string&e){
 if(prepared_){e.clear();return MusicPreparationStep::Ready;}
 if(!preparing_||!budget||budget>maximum_prepare_budget){e="Invalid music region preparation state/byte budget";return MusicPreparationStep::Failed;}
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
void MusicRegionPlayer::stop(uint32_t slot){auto&v=voices_[slot];if(v.active)ndspChnWaveBufClear(int(first_channel+slot));v.active=false;v.waves={};}
void MusicRegionPlayer::shutdown(){for(uint32_t i=0;i<capacity_;++i){stop(i);if(voices_[i].samples)linearFree(voices_[i].samples);voices_[i]={};}for(auto&t:tracks_){if(t.file)std::fclose(t.file);t={};}capacity_=track_count_=submitted_=0;owner_=nullptr;prepared_=preparing_=false;validation_track_=allocation_voice_=0;validation_crc_=0xffffffffu;track_bytes_=verified_bytes_=total_bytes_=0;asset_root_.clear();if(channel_owner==this)channel_owner=nullptr;}
bool MusicRegionPlayer::refill(uint32_t slot,std::string&e){auto&v=voices_[slot];if(!v.active)return true;auto&t=tracks_[v.track];const auto&a=t.asset;
 for(uint32_t b=0;b<buffer_count;++b){auto&wave=v.waves[b];if(wave.status==NDSP_WBUF_QUEUED||wave.status==NDSP_WBUF_PLAYING)continue;auto*samples=v.samples+size_t(b)*buffer_frames*2;uint32_t frames=0;
  while(frames<buffer_frames){uint32_t first=0;auto n=v.cursor.take(buffer_frames-frames,first);if(!n){stop(slot);return fail(e,"Region source loop unexpectedly ended");}uint64_t offset=uint64_t(first)*a.channels*2;
   if(offset>uint64_t(std::numeric_limits<long>::max())||std::fseek(t.file,long(offset),SEEK_SET)!=0||std::fread(samples+size_t(frames)*a.channels,a.channels*2,n,t.file)!=n){stop(slot);return fail(e,"Region PCM streaming read failed");}frames+=n;
  }
  auto*bytes=reinterpret_cast<uint8_t*>(samples);for(size_t n=0;n<size_t(frames)*a.channels;++n){uint16_t value=uint16_t(bytes[n*2])|uint16_t(bytes[n*2+1])<<8;std::memcpy(samples+n,&value,2);}
  wave={};wave.data_pcm16=samples;wave.nsamples=frames;wave.looping=false;
  if(R_FAILED(DSP_FlushDataCache(samples,frames*a.channels*sizeof(int16_t)))){stop(slot);return fail(e,"Region DSP cache flush failed");}
  ndspChnWaveBufAdd(int(first_channel+slot),&wave);
 }return true;
}
bool MusicRegionPlayer::sync(const upstream::MusicRegionController&controller,std::string&e){
 if(!prepared_||controller.capacity()!=capacity_||(owner_&&owner_!=&controller))return fail(e,"Music region adapter/controller capacity unavailable");
 if(channel_owner&&channel_owner!=this)return fail(e,"Music region channels already owned; existing audio preserved");
 // Validate the complete snapshot before any old voice or other channel moves.
 std::array<uint32_t,maximum_voices>index{};
 for(uint32_t i=0;i<capacity_;++i){const auto&s=controller.voices()[i];if(!s.allocated||!s.playing)continue;if(!s.generation||!std::isfinite(s.gain_db)||s.gain_db<-120||s.gain_db>24)return fail(e,"Invalid music region voice snapshot");uint32_t j=0;while(j<track_count_&&tracks_[j].asset.stable_id!=s.track_id)++j;if(j==track_count_)return fail(e,"Unbound music region track snapshot");index[i]=j;}
 channel_owner=this;owner_=&controller;
 for(uint32_t i=0;i<capacity_;++i){const auto&s=controller.voices()[i];auto&v=voices_[i];bool newly_started=false;if(!s.allocated||!s.playing){stop(i);continue;}
  if(!v.active||v.generation!=s.generation){stop(i);v.track=index[i];auto&a=tracks_[v.track].asset;if(!v.cursor.reset(a.frames,a.loop_start,a.loops()))return fail(e,"Invalid region cursor");v.generation=s.generation;v.active=true;
   int channel=int(first_channel+i);ndspChnReset(channel);ndspChnSetInterp(channel,NDSP_INTERP_POLYPHASE);ndspChnSetRate(channel,float(a.sample_rate));ndspChnSetFormat(channel,a.channels==2?NDSP_FORMAT_STEREO_PCM16:NDSP_FORMAT_MONO_PCM16);newly_started=true;
  }
  float volumes[12]{};volumes[0]=volumes[1]=upstream::audio_linear_gain(s.gain_db+tracks_[v.track].asset.gain_db);ndspChnSetMix(int(first_channel+i),volumes);if(!refill(i,e))return false;if(newly_started)++submitted_;
 }
 e.clear();return true;
}
}

#include "music_region_player.hpp"
#include "encore/crc32.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
namespace encore::ctr {namespace {
// The borrowed device owns the hardware pool shared with all live voices.
// Source slots retain their lease until their actual source fade ends.
bool fail(std::string&e,const char*m){e=m;return false;}}
bool MusicRegionPlayer::begin_prepare(const char*bank,const char*root,const upstream::MusicRegionData&regions,uint32_t capacity,AudioDevice&device,float master,std::string&e){
 if(prepared_||preparing_||capacity_)return fail(e,"Music region adapter already prepared; use a detached candidate");
 if(!device.available())return fail(e,"Music region audio unavailable: existing audio device is not initialized");
 if(!root||!capacity||capacity>maximum_voices)return fail(e,"Invalid music region adapter capacity/root");
 std::vector<upstream::AudioAsset> selected;
 if(!bank_.load_file(bank,e)||!regions.select_assets(bank_,master,uint32_t(tracks_.size()),selected,e))return false;
 // The shared bank also contains dialogue/menu effects. Only the checked
 // source region projection belongs to this bounded streaming adapter.
 device_=&device;capacity_=capacity;track_count_=uint32_t(selected.size());asset_root_=root;
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
bool MusicRegionPlayer::stop(uint32_t slot,std::string&e){auto&v=voices_[slot];if(v.channel!=UINT32_MAX){if(!device_)return fail(e,"Region lease lost its actual device");device_->clear(int(v.channel));if(!device_->release(this,uint64_t(slot)+1,int(v.channel),e))return false;v.channel=UINT32_MAX;}v.active=false;v.waves={};e.clear();return true;}
void MusicRegionPlayer::shutdown(){for(uint32_t i=0;i<capacity_;++i){std::string ignored;stop(i,ignored);if(voices_[i].samples)linearFree(voices_[i].samples);voices_[i]={};}for(auto&t:tracks_){if(t.file)std::fclose(t.file);t={};}capacity_=track_count_=submitted_=0;owner_=nullptr;prepared_=preparing_=false;validation_track_=allocation_voice_=0;validation_crc_=0xffffffffu;track_bytes_=verified_bytes_=total_bytes_=0;asset_root_.clear();adopted_banks_.clear();device_=nullptr;}
bool MusicRegionPlayer::refill(uint32_t slot,std::string&e){auto&v=voices_[slot];if(!v.active)return true;auto&t=tracks_[v.track];const auto&a=t.asset;
 if(!device_||!device_->available()){std::string ignored;stop(slot,ignored);return fail(e,"Region streaming lost its borrowed audio device");}
 device_->pump();
 for(uint32_t b=0;b<buffer_count;++b){auto&wave=v.waves[b];if(wave.status==NDSP_WBUF_QUEUED||wave.status==NDSP_WBUF_PLAYING)continue;auto*samples=v.samples+size_t(b)*buffer_frames*2;uint32_t frames=0;
  while(frames<buffer_frames){uint32_t first=0;auto n=v.cursor.take(buffer_frames-frames,first);if(!n){if(!stop(slot,e))return false;return fail(e,"Region source loop unexpectedly ended");}uint64_t offset=uint64_t(first)*a.channels*2;
   if(offset>uint64_t(std::numeric_limits<long>::max())||std::fseek(t.file,long(offset),SEEK_SET)!=0||std::fread(samples+size_t(frames)*a.channels,a.channels*2,n,t.file)!=n){if(!stop(slot,e))return false;return fail(e,"Region PCM streaming read failed");}frames+=n;
  }
  auto*bytes=reinterpret_cast<uint8_t*>(samples);for(size_t n=0;n<size_t(frames)*a.channels;++n){uint16_t value=uint16_t(bytes[n*2])|uint16_t(bytes[n*2+1])<<8;std::memcpy(samples+n,&value,2);}
  wave={};wave.data_pcm16=samples;wave.nsamples=frames;wave.looping=false;
  if(R_FAILED(device_->flush(samples,frames*a.channels*sizeof(int16_t)))){if(!stop(slot,e))return false;return fail(e,"Region DSP cache flush failed");}
  if(R_FAILED(device_->add(int(v.channel),&wave))){if(!stop(slot,e))return false;return fail(e,"Region audio wave submission failed");}
 }return true;
}
bool MusicRegionPlayer::adopt_prepared_tracks(MusicRegionPlayer&candidate,std::string&e){
 if(&candidate==this||!prepared_||!candidate.prepared_||candidate.owner_||capacity_!=candidate.capacity_||!device_||device_!=candidate.device_)return fail(e,"Music track handoff requires detached prepared candidate");
 std::array<int,16> destination{};uint32_t count=track_count_;
 for(uint32_t i=0;i<candidate.track_count_;++i){const auto&a=candidate.tracks_[i].asset;int found=-1;
  for(uint32_t j=0;j<track_count_;++j)if(tracks_[j].asset.stable_id==a.stable_id){const auto&b=tracks_[j].asset;if(b.source_path!=a.source_path||b.source_sha256!=a.source_sha256||b.pcm_path!=a.pcm_path||b.pcm_crc!=a.pcm_crc||b.pcm_bytes!=a.pcm_bytes||b.sample_rate!=a.sample_rate||b.channels!=a.channels||b.frames!=a.frames||b.loop_start!=a.loop_start||b.flags!=a.flags||b.gain_db!=a.gain_db)return fail(e,"Music track stable identity collision across scenes");found=int(j);break;}
  if(found<0){if(count>=tracks_.size())return fail(e,"Music track handoff exceeds bounded source bank capacity");found=int(count++);}destination[i]=found;
 }
 // All source admission precedes mutation. Existing FILE cursors, streaming
 // buffers, voice generations and NDSP leases remain with the played owner.
 for(uint32_t i=0;i<candidate.track_count_;++i)if(uint32_t(destination[i])>=track_count_){tracks_[destination[i]]=std::move(candidate.tracks_[i]);candidate.tracks_[i].file=nullptr;}
 adopted_banks_.push_back(std::make_unique<upstream::AudioBank>(std::move(candidate.bank_)));
 track_count_=count;e.clear();return true;
}
bool MusicRegionPlayer::sync(const upstream::MusicRegionController&controller,std::string&e){
 if(!prepared_||!device_||!device_->available()||controller.capacity()!=capacity_||(owner_&&owner_!=&controller))return fail(e,"Music region adapter/controller capacity unavailable");
 // Validate the complete snapshot before any old voice or other channel moves.
 std::array<uint32_t,maximum_voices>index{};
 for(uint32_t i=0;i<capacity_;++i){const auto&s=controller.voices()[i];if(!s.allocated||!s.playing)continue;if(!s.generation||!std::isfinite(s.gain_db)||s.gain_db<-120||s.gain_db>24)return fail(e,"Invalid music region voice snapshot");uint32_t j=0;while(j<track_count_&&tracks_[j].asset.stable_id!=s.track_id)++j;if(j==track_count_)return fail(e,"Unbound music region track snapshot");index[i]=j;}
 // Reserve every new hardware lease before changing any continuing playback.
 std::array<bool,maximum_voices>reserved{};
 for(uint32_t i=0;i<capacity_;++i){const auto&s=controller.voices()[i];auto&v=voices_[i];if(!s.allocated||!s.playing||v.channel!=UINT32_MAX)continue;int channel=-1;
  if(!device_->lease(this,uint64_t(i)+1,channel,e)){const auto reason=e;for(uint32_t j=0;j<i;++j)if(reserved[j]){std::string release_error;if(!stop(j,release_error)){e=release_error;return false;}}e=reason;return false;}
  v.channel=uint32_t(channel);reserved[i]=true;
 }
 for(uint32_t i=0;i<capacity_;++i){const auto&s=controller.voices()[i];if((!s.allocated||!s.playing)&&!stop(i,e))return false;}
 owner_=&controller;
 for(uint32_t i=0;i<capacity_;++i){const auto&s=controller.voices()[i];auto&v=voices_[i];bool newly_started=false;if(!s.allocated||!s.playing)continue;
  if(!v.active||v.generation!=s.generation){device_->clear(int(v.channel));v.waves={};v.track=index[i];auto&a=tracks_[v.track].asset;if(!v.cursor.reset(a.frames,a.loop_start,a.loops()))return fail(e,"Invalid region cursor");v.generation=s.generation;v.active=true;
   int channel=int(v.channel);device_->reset(channel);device_->interp(channel,NDSP_INTERP_POLYPHASE);device_->rate(channel,float(a.sample_rate));device_->format(channel,a.channels==2?NDSP_FORMAT_STEREO_PCM16:NDSP_FORMAT_MONO_PCM16);newly_started=true;
  }
  float volumes[12]{};volumes[0]=volumes[1]=upstream::audio_linear_gain(s.gain_db+tracks_[v.track].asset.gain_db);device_->mix(int(v.channel),volumes);if(!refill(i,e))return false;if(newly_started)++submitted_;
 }
 e.clear();return true;
}
}

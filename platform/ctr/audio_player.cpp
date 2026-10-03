#include "audio_player.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace encore::ctr {
MusicObservation AudioPlayer::observe_music() const {
    const auto& music=voices_[uint32_t(AudioLane::Music)];
    const auto& dialogue=voices_[uint32_t(AudioLane::DialogueMusic)];
    MusicObservation out;out.generation=music_generation_;out.asset_id=music.asset.stable_id;
    out.available=ready_;out.playing=ready_&&music.active;
    out.tweening=out.playing&&music.fading&&music.fade.active();
    out.dialogue_music_playing=ready_&&dialogue.active;
    out.any_music_tweening=out.tweening||(out.dialogue_music_playing&&dialogue.fading&&dialogue.fade.active());
    out.master_db=bank_.master_db();return out;
}

bool AudioPlayer::initialize(const char* bank_path,const char* root,std::string& error){
    shutdown();consumed_=0;submitted_=completed_=queued_frames_=0;dsp_result_=0;
    if(!root){error="Audio asset root is missing";return false;}
    if(!bank_.load_file(bank_path,error))return false;
    // libctru loads a component already supplied by the launcher or from SD.
    // No firmware is included, downloaded, fabricated, or silently substituted.
    // Check availability before scanning the complete PCM bank: a missing DSP
    // service cannot play any of those checked streams.
    dsp_result_=ndspInit();
    if(R_FAILED(dsp_result_)){char message[112];std::snprintf(message,sizeof(message),"Audio unavailable: NDSP init 0x%08lX (DSP component/service required)",static_cast<unsigned long>(uint32_t(dsp_result_)));error=message;shutdown();return false;}
    ndsp_initialized_=true;
    for(uint32_t i=0;i<bank_.count();++i){const auto a=bank_.asset(i);std::string path=root;path+=a.pcm_path;
        if(!streams_[i].open(a,path.c_str(),error)){shutdown();return false;}}
    ndspSetMasterVol(upstream::audio_linear_gain(bank_.master_db()));
    for(uint32_t i=0;i<lane_count;++i){
        voices_[i].samples=static_cast<int16_t*>(linearAlloc(buffer_count*buffer_frames*2*sizeof(int16_t)));
        if(!voices_[i].samples){error="Audio unavailable: linear streaming-buffer allocation failed";shutdown();return false;}
        ndspChnReset(int(i));
    }
    ready_=true;error.clear();return true;
}
void AudioPlayer::shutdown(){
    if(ndsp_initialized_){for(uint32_t i=0;i<lane_count;++i)ndspChnWaveBufClear(int(i));ndspExit();}
    ndsp_initialized_=ready_=false;
    for(auto& v:voices_){if(v.samples)linearFree(v.samples);v=Voice{};}
    for(auto& stream:streams_)stream.close();
}
void AudioPlayer::reset_scene(){
    // The immutable checked bank and open streams belong to the application,
    // not a gameplay scene. Stop NDSP before clearing its wave-buffer objects.
    if(ndsp_initialized_)for(uint32_t i=0;i<lane_count;++i)ndspChnWaveBufClear(int(i));
    for(auto& voice:voices_){auto* samples=voice.samples;voice=Voice{};voice.samples=samples;}
    consumed_=0;submitted_=completed_=queued_frames_=0;
}
void AudioPlayer::stop(uint32_t lane){ndspChnWaveBufClear(int(lane));voices_[lane].active=false;voices_[lane].fading=false;}
void AudioPlayer::mix(uint32_t lane){float volumes[12]{};const float gain=upstream::audio_linear_gain(voices_[lane].fade.db());volumes[0]=gain;volumes[1]=gain;ndspChnSetMix(int(lane),volumes);}
bool AudioPlayer::refill(uint32_t lane,std::string& error){
    auto& voice=voices_[lane];if(!voice.active)return true;bool queued=false;
    for(uint32_t i=0;i<buffer_count;++i){auto& wave=voice.waves[i];
        if(wave.status==NDSP_WBUF_QUEUED||wave.status==NDSP_WBUF_PLAYING){queued=true;continue;}
        uint32_t frames=0;int16_t* samples=voice.samples+size_t(i)*buffer_frames*2;
        if(!streams_[voice.asset_index].read(samples,buffer_frames,frames,error)){stop(lane);return false;}
        if(!frames)continue;
        wave={};wave.data_pcm16=samples;wave.nsamples=frames;wave.looping=false;
        const Result flushed=DSP_FlushDataCache(samples,frames*voice.asset.channels*sizeof(int16_t));
        if(R_FAILED(flushed)){error="Audio DSP cache flush failed";stop(lane);return false;}
        ndspChnWaveBufAdd(int(lane),&wave);queued=true;queued_frames_+=frames;
    }
    if(!queued){voice.active=false;voice.fading=false;++completed_;}return true;
}
bool AudioPlayer::play(uint32_t id,AudioLane which,std::string& error,float gain_db,double fadein_seconds){
    if(!std::isfinite(gain_db)||gain_db<-120||gain_db>24||!std::isfinite(fadein_seconds)||fadein_seconds<0||fadein_seconds>60){error="Invalid audio gain/fade";return false;}
    if(!ready_){error="Audio unavailable: NDSP is not initialized";return false;}
    const uint32_t lane=uint32_t(which);if(lane>=lane_count){error="Unsupported audio lane";return false;}
    if(which==AudioLane::Music&&music_generation_==UINT64_MAX){error="Music observation identity exhausted";return false;}
    uint32_t index=0;while(index<bank_.count()&&bank_.asset(index).stable_id!=id)++index;
    if(index==bank_.count()){error="Audio resource is absent from bank";return false;}
    for(uint32_t i=0;i<lane_count;++i)if(i!=lane&&voices_[i].active&&voices_[i].asset_index==index){error="Concurrent playback of one PCM asset across lanes is outside audio slice";return false;}
    auto& voice=voices_[lane];stop(lane);voice.waves={};voice.asset=bank_.asset(index);voice.asset_index=index;
    if(!streams_[index].rewind()){error="Cannot rewind audio stream";return false;}
    voice.fade.reset(fadein_seconds>0?bank_.silence_db():voice.asset.gain_db+gain_db);voice.active=true;voice.stop_after_fade=false;
    if(fadein_seconds>0){voice.fade.start(voice.asset.gain_db+gain_db,fadein_seconds,true);voice.fading=true;}
    ndspChnSetInterp(int(lane),NDSP_INTERP_POLYPHASE);
    ndspChnSetRate(int(lane),float(voice.asset.sample_rate));
    ndspChnSetFormat(int(lane),voice.asset.channels==2?NDSP_FORMAT_STEREO_PCM16:NDSP_FORMAT_MONO_PCM16);
    mix(lane);if(!refill(lane,error))return false;++submitted_;if(which==AudioLane::Music)++music_generation_;error.clear();return true;
}
bool AudioPlayer::fade_music(double duration,std::string& error){
    if(!ready_){error="Audio unavailable: NDSP is not initialized";return false;}
    if(!std::isfinite(duration)||duration<0){error="Invalid music fade duration";return false;}
    auto& voice=voices_[uint32_t(AudioLane::Music)];
    if(voice.active){voice.fade.start(bank_.silence_db(),duration);voice.fading=true;voice.stop_after_fade=true;if(duration==0)stop(uint32_t(AudioLane::Music));}
    error.clear();return true;
}
bool AudioPlayer::consume(const upstream::RoomView& room,const std::vector<upstream::OpeningAudioRequest>& requests,std::string& error){
    if(!ready_){error="Audio unavailable: NDSP is not initialized";return false;}
    if(!room.valid()||consumed_>requests.size()){error="Invalid or reset audio request history";return false;}
    while(consumed_<requests.size()){
        const auto& request=requests[consumed_];
        if(request.kind==upstream::AudioRequestKind::FadeMusic){if(request.resource_index!=upstream::kRoomNoIndex){error="Fade request has unsupported resource target";return false;}if(!fade_music(request.duration,error))return false;}
        else {
            if(request.kind!=upstream::AudioRequestKind::PlayMusic&&request.kind!=upstream::AudioRequestKind::PlayEffect&&request.kind!=upstream::AudioRequestKind::PlayDialogueMusic&&request.kind!=upstream::AudioRequestKind::StopMusicResource&&request.kind!=upstream::AudioRequestKind::FadeInMusic){error="Unsupported room audio request";return false;}
            if(request.kind!=upstream::AudioRequestKind::FadeInMusic&&(request.duration!=0||request.gain_db!=0)){error="Timed audio playback request is outside audio slice";return false;}
            if(request.resource_index>=room.resource_count()){error="Room audio resource index out of bounds";return false;}
            const auto resource=room.resource(request.resource_index);upstream::AudioAsset asset;
            if(resource.kind!=2||!bank_.find(resource.stable_id,asset)||asset.source_sha256!=resource.sha256||asset.source_path!=room.string(resource.path_string)){error="Room/audio bank source identity mismatch";return false;}
            if(request.kind==upstream::AudioRequestKind::StopMusicResource){
                for(auto lane:{uint32_t(AudioLane::Music),uint32_t(AudioLane::DialogueMusic)})if(voices_[lane].active&&voices_[lane].asset.stable_id==resource.stable_id)stop(lane);
            }else if(!play(resource.stable_id,(request.kind==upstream::AudioRequestKind::PlayMusic||request.kind==upstream::AudioRequestKind::FadeInMusic)?AudioLane::Music:request.kind==upstream::AudioRequestKind::PlayDialogueMusic?AudioLane::DialogueMusic:AudioLane::Effect,error,request.gain_db,request.kind==upstream::AudioRequestKind::FadeInMusic?request.duration:0))return false;
        }
        ++consumed_;
    }error.clear();return true;
}
bool AudioPlayer::pump_streams(std::string& error){
    if(!ready_){error="Audio unavailable: NDSP is not initialized";return false;}
    for(uint32_t i=0;i<lane_count;++i)if(!refill(i,error))return false;
    error.clear();return true;
}
bool AudioPlayer::update(double delta,std::string& error){
    if(!ready_){error="Audio unavailable: NDSP is not initialized";return false;}
    if(!std::isfinite(delta)||delta<0){error="Invalid audio frame delta";return false;}
    for(uint32_t i=0;i<lane_count;++i){auto& v=voices_[i];if(!v.active)continue;
        if(v.fading){v.fade.advance(delta);mix(i);if(!v.fade.active()){v.fading=false;if(v.stop_after_fade){stop(i);continue;}}}
        if(!refill(i,error))return false;
    }error.clear();return true;
}
}

#include "room_audio_identity.hpp"
#include "audio_player.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace encore::ctr {
namespace {
// Area-music streaming owns channels 4..19. Optional cutscene effects use two
// otherwise unowned hardware channels; ordinary gameplay keeps its four lanes.
int hardware_channel(uint32_t lane){return int(lane<4?lane:lane+18);}
}
MusicObservation AudioPlayer::observe_music() const {
    const auto& music=voices_[uint32_t(AudioLane::Music)];
    const auto& dialogue=voices_[uint32_t(AudioLane::DialogueMusic)];
    MusicObservation out;out.generation=music_generation_;out.asset_id=music.asset.stable_id;
    out.player_identity=music_player_identity_;out.retired_player_identity=retired_music_player_identity_;
    out.present=ready_&&music_player_present_;
    out.available=ready_;out.playing=ready_&&music.active;
    out.tweening=out.playing&&music.fading&&music.fade.active();
    out.dialogue_music_playing=ready_&&dialogue.active;
    out.any_music_tweening=out.tweening||(out.dialogue_music_playing&&dialogue.fading&&dialogue.fade.active());
    out.master_db=bank_.master_db();out.volume_db=music.fade.db();return out;
}

bool AudioPlayer::initialize(const char* bank_path,const char* root,std::string& error){
    shutdown();consumed_=0;submitted_=completed_=queued_frames_=0;dsp_result_=0;
    if(!root){error="Audio asset root is missing";return false;}
    if(!bank_.load_file(bank_path,error))return false;
    // libctru loads a component already supplied by the launcher or from SD.
    // No firmware is included, downloaded, fabricated, or silently substituted.
    // Admit metadata and the DSP service here; prepare() validates each needed
    // PCM payload before its first playback, retaining that checked stream.
    dsp_result_=ndspInit();
    if(R_FAILED(dsp_result_)){char message[112];std::snprintf(message,sizeof(message),"Audio unavailable: NDSP init 0x%08lX (DSP component/service required)",static_cast<unsigned long>(uint32_t(dsp_result_)));error=message;shutdown();return false;}
    ndsp_initialized_=true;
    asset_root_=root;
    ndspSetMasterVol(upstream::audio_linear_gain(bank_.master_db()));
    for(uint32_t i=0;i<4;++i){
        voices_[i].samples=static_cast<int16_t*>(linearAlloc(buffer_count*buffer_frames*2*sizeof(int16_t)));
        if(!voices_[i].samples){error="Audio unavailable: linear streaming-buffer allocation failed";shutdown();return false;}
        ndspChnReset(int(i));
    }
    ready_=true;error.clear();return true;
}
bool AudioPlayer::lease_native_channel(uint64_t object,const void* owner,int& channel,std::string&e){
    if(!ready_||!object||!owner){e="Native audio lease requires live DSP and actual Node owner";return false;}
    for(size_t i=0;i<native_leases_.size();++i)if(native_leases_[i].object==object){if(native_leases_[i].owner!=owner){e="Native audio Node has another channel owner";return false;}channel=int(26+i);e.clear();return true;}
    for(size_t i=0;i<native_leases_.size();++i)if(!native_leases_[i].object){native_leases_[i]={object,owner};channel=int(26+i);e.clear();return true;}
    e="All six shared native NDSP channels occupied";return false;
}
bool AudioPlayer::release_native_channel(uint64_t object,const void* owner,int channel,std::string&e){
    if(channel<26||channel>31||!object||!owner){e="Native audio lease release range/owner rejected";return false;}
    auto& lease=native_leases_[size_t(channel-26)];if(lease.object!=object||lease.owner!=owner){e="Native audio channel lease ownership mismatch";return false;}
    lease={};e.clear();return true;
}
void AudioPlayer::shutdown(){
    retire_music_player();
    if(ndsp_initialized_){for(uint32_t i=0;i<lane_count;++i)if(i<4||voices_[i].samples)ndspChnWaveBufClear(hardware_channel(i));ndspExit();}
    ndsp_initialized_=ready_=false;
    for(auto& v:voices_){if(v.samples)linearFree(v.samples);v=Voice{};}
    for(auto& stream:streams_)stream.close();
    native_leases_={};
    scene_banks_.clear();
    asset_root_.clear();
}
bool AudioPlayer::prepare_index(uint32_t index,std::string& error){
    if(streams_[index].is_open()){error.clear();return true;}
    const auto asset=this->asset(index);const auto path=asset_root_+std::string(asset.pcm_path);
    // AudioPcmStream::open checks every byte before publishing its file handle.
    // Missing/truncated/corrupt payloads cannot reach an NDSP wave queue.
    return streams_[index].open(asset,path.c_str(),error);
}
bool AudioPlayer::prepare(uint32_t id,std::string& error){
    if(!ready_){error="Audio unavailable: NDSP is not initialized";return false;}
    for(uint32_t i=0;i<asset_count();++i)if(asset(i).stable_id==id)return prepare_index(i,error);
    error="Audio resource is absent from bank";return false;
}
bool AudioPlayer::prepared(uint32_t id)const{
    if(ready_)for(uint32_t i=0;i<asset_count();++i)if(asset(i).stable_id==id)return streams_[i].is_open();
    return false;
}
uint32_t AudioPlayer::asset_count()const{
    uint32_t n=bank_.count();
    for(const auto& bank:scene_banks_)n+=bank->count();
    return n;
}
upstream::AudioAsset AudioPlayer::asset(uint32_t i)const{
    if(i<bank_.count())return bank_.asset(i);
    i-=bank_.count();
    for(const auto& bank:scene_banks_){if(i<bank->count())return bank->asset(i);i-=bank->count();}
    return {};
}
bool AudioPlayer::include_bank(std::shared_ptr<const upstream::AudioBank> b,std::string& e){
    if(!ready_||!b||!b->count()){e="Scene audio metadata/DSP owner unavailable";return false;}
    if(std::find(scene_banks_.begin(),scene_banks_.end(),b)!=scene_banks_.end()){e.clear();return true;}
    const auto count=asset_count();
    if(b->count()>streams_.size()-count){e="Scene audio stream capacity exhausted";return false;}
    for(uint32_t j=0;j<b->count();++j){const auto next=b->asset(j);
        for(uint32_t i=0;i<count;++i)if(asset(i).stable_id==next.stable_id){
            e="Scene audio ID already belongs to an existing bank";return false;
        }
    }
    scene_banks_.push_back(std::move(b));e.clear();return true;
}
bool AudioPlayer::source_asset(std::string_view source,upstream::AudioAsset& out,std::string& e)const{
    if(source.substr(0,6)=="res://")source.remove_prefix(6);
    uint32_t matches=0;
    for(uint32_t i=0;i<asset_count();++i){auto a=asset(i);auto name=a.source_path;
        if(name.substr(0,6)=="res://")name.remove_prefix(6);
        if(name==source){out=a;++matches;}
    }
    if(matches!=1){e="Scene audio source absent or ambiguous";return false;}
    e.clear();return true;
}
bool AudioPlayer::checked_asset(uint32_t id,upstream::AudioAsset& out,std::string& e)const{
    if(!ready_){e="Audio checked metadata requires live DSP owner";return false;}
    for(uint32_t i=0;i<asset_count();++i)if(asset(i).stable_id==id){out=asset(i);e.clear();return true;}
    e="Audio explicit asset ID absent";return false;
}
void AudioPlayer::reset_scene(){
    // The immutable checked bank and open streams belong to the application,
    // not a gameplay scene. Stop NDSP before clearing its wave-buffer objects.
    if(ndsp_initialized_)for(uint32_t i=0;i<lane_count;++i)if(i<4||voices_[i].samples)ndspChnWaveBufClear(hardware_channel(i));
    retire_music_player();
    for(auto& voice:voices_){auto* samples=voice.samples;voice=Voice{};voice.samples=samples;}
    consumed_=0;submitted_=completed_=queued_frames_=0;
}
void AudioPlayer::stop(uint32_t lane){ndspChnWaveBufClear(hardware_channel(lane));voices_[lane].active=false;voices_[lane].fading=false;}
void AudioPlayer::retire_music_player(){
    // Retirement belongs to this bounded backend's completion/reset boundary.
    // It does not construct a source _add_at_zero replacement or claim global
    // deferred deletion/tween order across the independent region owners.
    if(music_player_present_)retired_music_player_identity_=music_player_identity_;
    music_player_present_=false;
}
void AudioPlayer::mix(uint32_t lane){float volumes[12]{};const float gain=upstream::audio_linear_gain(voices_[lane].fade.db());volumes[0]=gain;volumes[1]=gain;ndspChnSetMix(hardware_channel(lane),volumes);}
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
        ndspChnWaveBufAdd(hardware_channel(lane),&wave);queued=true;queued_frames_+=frames;
    }
    if(!queued){voice.active=false;voice.fading=false;++completed_;if(lane==uint32_t(AudioLane::Music))retire_music_player();}return true;
}
bool AudioPlayer::play(uint32_t id,AudioLane which,std::string& error,float gain_db,double fadein_seconds,float pitch){
    if(!std::isfinite(gain_db)||gain_db<-120||gain_db>24||!std::isfinite(fadein_seconds)||fadein_seconds<0||fadein_seconds>60){error="Invalid audio gain/fade";return false;}
    if(!std::isfinite(pitch)||pitch<=0||pitch>4){error="Invalid audio pitch";return false;}
    if(!ready_){error="Audio unavailable: NDSP is not initialized";return false;}
    const uint32_t lane=uint32_t(which);if(lane>=lane_count){error="Unsupported audio lane";return false;}
    if(which==AudioLane::Music&&music_generation_==UINT64_MAX){error="Music observation identity exhausted";return false;}
    if(which==AudioLane::Music&&!music_player_present_&&music_player_identity_==UINT64_MAX){error="Bounded Music player identity exhausted";return false;}
    uint32_t index=0;while(index<asset_count()&&asset(index).stable_id!=id)++index;
    if(index==asset_count()){error="Audio resource is absent from bank";return false;}
    for(uint32_t i=0;i<lane_count;++i)if(i!=lane&&voices_[i].active&&voices_[i].asset_index==index){error="Concurrent playback of one PCM asset across lanes is outside audio slice";return false;}
    if(!prepare_index(index,error))return false; // Preserve a live voice when preparation fails.
    auto& voice=voices_[lane];
    if(!voice.samples){
        voice.samples=static_cast<int16_t*>(linearAlloc(buffer_count*buffer_frames*2*sizeof(int16_t)));
        if(!voice.samples){error="Audio auxiliary streaming-buffer allocation failed";return false;}
        ndspChnReset(hardware_channel(lane));
    }
    stop(lane);voice.waves={};voice.asset=asset(index);voice.asset_index=index;
    if(!streams_[index].rewind()){error="Cannot rewind audio stream";return false;}
    voice.fade.reset(fadein_seconds>0?bank_.silence_db():voice.asset.gain_db+gain_db);voice.active=true;voice.stop_after_fade=false;
    if(fadein_seconds>0){voice.fade.start(voice.asset.gain_db+gain_db,fadein_seconds,true);voice.fading=true;}
    ndspChnSetInterp(hardware_channel(lane),NDSP_INTERP_POLYPHASE);
    ndspChnSetRate(hardware_channel(lane),float(voice.asset.sample_rate)*pitch);
    ndspChnSetFormat(hardware_channel(lane),voice.asset.channels==2?NDSP_FORMAT_STEREO_PCM16:NDSP_FORMAT_MONO_PCM16);
    mix(lane);if(!refill(lane,error))return false;++submitted_;if(which==AudioLane::Music){if(!music_player_present_){++music_player_identity_;music_player_present_=true;}++music_generation_;}error.clear();return true;
}
bool AudioPlayer::fade_music(double duration,std::string& error){
    if(!ready_){error="Audio unavailable: NDSP is not initialized";return false;}
    if(!std::isfinite(duration)||duration<0){error="Invalid music fade duration";return false;}
    auto& voice=voices_[uint32_t(AudioLane::Music)];
    if(voice.active){voice.fade.start(bank_.silence_db(),duration);voice.fading=true;voice.stop_after_fade=true;if(duration==0){stop(uint32_t(AudioLane::Music));retire_music_player();}}
    error.clear();return true;
}
bool AudioPlayer::fade_all_music(double duration,std::string& error){
    if(!ready_){error="Audio unavailable: NDSP is not initialized";return false;}
    if(!std::isfinite(duration)||duration<0||duration>60){error="Invalid music fade duration";return false;}
    for(auto lane:{uint32_t(AudioLane::Music),uint32_t(AudioLane::DialogueMusic)}){
        auto& voice=voices_[lane];
        if(voice.active){voice.fade.start(bank_.silence_db(),duration);voice.fading=true;voice.stop_after_fade=true;if(duration==0){stop(lane);if(lane==uint32_t(AudioLane::Music))retire_music_player();}}
    }
    error.clear();return true;
}
bool AudioPlayer::stop_lane(AudioLane which,std::string& error){
    const auto lane=uint32_t(which);if(lane>=lane_count){error="Unsupported audio stop lane";return false;}
    if(!ready_){error="Audio unavailable: NDSP is not initialized";return false;}
    if(voices_[lane].samples)stop(lane);
    error.clear();return true;
}
bool AudioPlayer::consume(const upstream::RoomView& room,const std::vector<upstream::OpeningAudioRequest>& requests,std::string& error,const RoomMusicFadeHandler&fade_handler){
    if(!ready_){error="Audio unavailable: NDSP is not initialized";return false;}
    if(!room.valid()||consumed_>requests.size()){error="Invalid or reset audio request history";return false;}
    while(consumed_<requests.size()){
        const auto& request=requests[consumed_];
        if(request.kind==upstream::AudioRequestKind::FadeMusic){if(request.resource_index!=upstream::kRoomNoIndex){error="Fade request has unsupported resource target";return false;}if(fade_handler){if(!fade_handler(room,request,error))return false;}else if(!fade_music(request.duration,error))return false;}
        else {
            if(request.kind!=upstream::AudioRequestKind::PlayMusic&&request.kind!=upstream::AudioRequestKind::PlayEffect&&request.kind!=upstream::AudioRequestKind::PlayDialogueMusic&&request.kind!=upstream::AudioRequestKind::StopMusicResource&&request.kind!=upstream::AudioRequestKind::FadeInMusic){error="Unsupported room audio request";return false;}
            if(request.kind!=upstream::AudioRequestKind::FadeInMusic&&(request.duration!=0||request.gain_db!=0)){error="Timed audio playback request is outside audio slice";return false;}
            if(request.resource_index>=room.resource_count()){error="Room audio resource index out of bounds";return false;}
            upstream::AudioAsset asset;
            if(!resolve_room_audio_asset(room,bank_,request.resource_index,asset,error))return false;
            if(request.kind==upstream::AudioRequestKind::StopMusicResource){
                for(auto lane:{uint32_t(AudioLane::Music),uint32_t(AudioLane::DialogueMusic)})if(voices_[lane].active&&voices_[lane].asset.stable_id==asset.stable_id)stop(lane);
            }else if(!play(asset.stable_id,(request.kind==upstream::AudioRequestKind::PlayMusic||request.kind==upstream::AudioRequestKind::FadeInMusic)?AudioLane::Music:request.kind==upstream::AudioRequestKind::PlayDialogueMusic?AudioLane::DialogueMusic:AudioLane::Effect,error,request.gain_db,request.kind==upstream::AudioRequestKind::FadeInMusic?request.duration:0))return false;
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
        if(v.fading){v.fade.advance(delta);mix(i);if(!v.fade.active()){v.fading=false;if(v.stop_after_fade){stop(i);if(i==uint32_t(AudioLane::Music))retire_music_player();continue;}}}
        if(!refill(i,error))return false;
    }error.clear();return true;
}
}

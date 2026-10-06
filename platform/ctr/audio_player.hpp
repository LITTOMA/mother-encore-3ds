#pragma once
#include "encore/audio_data.hpp"
#include "encore/world.hpp"
#include <3ds.h>
#include <array>
#include <string>
#include <functional>

namespace encore::ctr {
enum class AudioLane : uint8_t { Music=0, Effect=1, Jingle=2, DialogueMusic=3, AuxiliaryEffect0=4, AuxiliaryEffect1=5 };
// Read-only snapshot of the bounded Music lane, not the source cold-Ready
// AudioPlayers child list. Its owner is published on first successful playback.
// generation counts successful starts; player_identity identifies the retained
// instance independently. Stop keeps that instance; removal records its exact
// identity. Counters never reset across scene reset or backend reinitialization.
struct MusicObservation {
    uint64_t generation=0;
    uint64_t player_identity=0,retired_player_identity=0;
    uint32_t asset_id=0;
    bool available=false,present=false,playing=false,tweening=false;
    bool dialogue_music_playing=false,any_music_tweening=false;
    float master_db=0;
};
// Bounded slice adapter: independent area music, named dialogue SFX, musical
// jingle and dialogue music voices. It is not the full Godot audio graph.
class AudioPlayer {
public:
    AudioPlayer()=default;
    ~AudioPlayer(){shutdown();}
    AudioPlayer(const AudioPlayer&)=delete;
    AudioPlayer& operator=(const AudioPlayer&)=delete;
    bool initialize(const char* bank_path,const char* asset_root,std::string& error);
    // Opening a stream performs its complete size/CRC validation once. Metadata
    // admission and NDSP initialization do not read unused PCM payloads.
    bool prepare(uint32_t stable_audio_id,std::string& error);
    bool prepared(uint32_t stable_audio_id) const;
    void shutdown();
    // Stop all voices/history while preserving checked streams and buffers.
    // Also safe when NDSP was unavailable; does not retry initialization.
    void reset_scene();
    using RoomMusicFadeHandler=std::function<bool(const upstream::RoomView&,const upstream::OpeningAudioRequest&,std::string&)>;
    bool consume(const upstream::RoomView& room,const std::vector<upstream::OpeningAudioRequest>& requests,std::string& error,const RoomMusicFadeHandler&fade_handler={});
    bool play(uint32_t stable_audio_id,AudioLane lane,std::string& error,float gain_db=0,double fadein_seconds=0,float pitch=1);
    bool fade_music(double duration,std::string& error);
    bool fade_all_music(double duration,std::string& error);
    bool stop_lane(AudioLane,std::string& error);
    bool update(double delta,std::string& error);
    bool available() const{return ready_;}
    // Borrow only this live NDSP owner and its checked immutable bank.
    const upstream::AudioBank* checked_bank() const{return ready_?&bank_:nullptr;}
    const std::string& asset_root() const{return asset_root_;}
    MusicObservation observe_music() const;
    // Loading cooperation only: feed existing queues and observe natural ends.
    // No elapsed game/fade time, volume mix writes, new voice or init retry.
    bool pump_streams(std::string& error);
    Result dsp_result() const{return dsp_result_;}
    size_t consumed_requests() const{return consumed_;}
    void reset_scene_requests(){consumed_=0;}
    uint32_t submitted_voices() const{return submitted_;}
    uint32_t completed_voices() const{return completed_;}
    uint32_t dropped_frames() const{return ready_?ndspGetDroppedFrames():0;}
    uint32_t queued_frames() const{return queued_frames_;}
private:
    static constexpr uint32_t lane_count=6,buffer_count=3,buffer_frames=2048;
    struct Voice {
        upstream::AudioAsset asset{};
        upstream::AudioFade fade;
        std::array<ndspWaveBuf,buffer_count> waves{};
        int16_t* samples=nullptr;
        uint32_t asset_index=0;
        bool active=false,fading=false,stop_after_fade=false;
    };
    bool prepare_index(uint32_t index,std::string& error);
    bool refill(uint32_t lane,std::string& error);
    void stop(uint32_t lane);
    void retire_music_player();
    void mix(uint32_t lane);
    upstream::AudioBank bank_;
    std::string asset_root_;
    std::array<upstream::AudioPcmStream,64> streams_;
    std::array<Voice,lane_count> voices_;
    Result dsp_result_=0;
    bool ndsp_initialized_=false,ready_=false;
    uint64_t music_generation_=0;
    uint64_t music_player_identity_=0,retired_music_player_identity_=0;
    bool music_player_present_=false;
    size_t consumed_=0;
    uint32_t submitted_=0,completed_=0,queued_frames_=0;
};
}

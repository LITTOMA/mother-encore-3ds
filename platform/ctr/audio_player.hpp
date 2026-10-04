#pragma once
#include "encore/audio_data.hpp"
#include "encore/world.hpp"
#include <3ds.h>
#include <array>

namespace encore::ctr {
enum class AudioLane : uint8_t { Music=0, Effect=1, Jingle=2, DialogueMusic=3, AuxiliaryEffect0=4, AuxiliaryEffect1=5 };
// Read-only snapshot of the existing bounded Music owner, not a inferred
// source-player graph. generation identifies a successful Music start and never
// resets across scene stop/reset/reinitialize; inactive snapshots retain it.
struct MusicObservation {
    uint64_t generation=0;
    uint32_t asset_id=0;
    bool available=false,playing=false,tweening=false;
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
    void shutdown();
    // Stop all voices/history while preserving checked streams and buffers.
    // Also safe when NDSP was unavailable; does not retry initialization.
    void reset_scene();
    bool consume(const upstream::RoomView& room,const std::vector<upstream::OpeningAudioRequest>& requests,std::string& error);
    bool play(uint32_t stable_audio_id,AudioLane lane,std::string& error,float gain_db=0,double fadein_seconds=0,float pitch=1);
    bool fade_music(double duration,std::string& error);
    bool fade_all_music(double duration,std::string& error);
    bool stop_lane(AudioLane,std::string& error);
    bool update(double delta,std::string& error);
    bool available() const{return ready_;}
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
    bool refill(uint32_t lane,std::string& error);
    void stop(uint32_t lane);
    void mix(uint32_t lane);
    upstream::AudioBank bank_;
    std::array<upstream::AudioPcmStream,64> streams_;
    std::array<Voice,lane_count> voices_;
    Result dsp_result_=0;
    bool ndsp_initialized_=false,ready_=false;
    uint64_t music_generation_=0;
    size_t consumed_=0;
    uint32_t submitted_=0,completed_=0,queued_frames_=0;
};
}

#pragma once
#include "encore/music_regions.hpp"
#include <limits>
#include <string_view>
namespace encore::upstream {
// Source-ordered AudioPlayers children, including stopped nodes and the initial
// empty player. Identity is a node identity, not a playback generation.
struct MusicPlayerSnapshot {
    uint64_t identity=0;
    std::string_view stream;
    bool playing=false;
    float gain_db=0;
};
struct MusicLoopRequest {
    std::string_view stream,first_registered_stream;
    bool requester_is_first=false;
    float gain_db=0,silence_db=0;
    double fadein_seconds=0,fade_to_seconds=0;
};
enum class MusicLoopVolume:uint8_t {None,Set,FadeIn,FadeTo};
struct MusicLoopChoice {
    static constexpr uint64_t no_player=std::numeric_limits<uint64_t>::max();
    uint64_t player=no_player;
    bool append_player=false,restart_stream=false,same_song=false;
    MusicLoopVolume volume=MusicLoopVolume::None;
    float initial_gain_db=0,target_gain_db=0;
    double seconds=0;
    MusicRegionCurve curve=MusicRegionCurve::Linear;
};
// Bounded loop-only, non-diegetic MusicChanger decision. New-player execution
// orders attach -> set silence -> start fade -> play; stopped-player reuse
// orders attach -> play -> set volume and retains the old tween.
// This does not allocate players, own banks or provide global scene lifetime.
bool valid_music_stream_uri(std::string_view);
bool choose_music_loop(const std::vector<MusicPlayerSnapshot>&,const MusicLoopRequest&,MusicLoopChoice&,std::string&);
// The manager's observed linear/quartic volume property, independent of DSP.
bool sample_music_tween(float from,float to,double duration,double elapsed,MusicRegionCurve,float&result,std::string&);
}

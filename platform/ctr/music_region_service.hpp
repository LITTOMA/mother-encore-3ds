#pragma once
#include "audio_player.hpp"
#include "music_region_player.hpp"
#include <memory>
namespace encore::ctr {
struct MusicSourcePlayer {
 uint32_t kind=0,asset_id=0;
 uint64_t player_identity=0,order=0;
 bool playing=false,tweening=false;
 float volume_db=0;
};
enum class MusicRegionServicePhase:uint8_t {Dormant,Preparing,Prepared,Active,Draining};
// App-lifetime service. It stays Dormant until the selected scene explicitly
// prepares its source-owned music; title metadata loading cannot start voices.
// The scene factory owns real source-ordered Area callbacks and commit epochs.
class MusicRegionService final {
public:
 MusicRegionService();~MusicRegionService();
 MusicRegionService(const MusicRegionService&)=delete;
 MusicRegionService&operator=(const MusicRegionService&)=delete;
 bool begin_prepare(const char*regions,const char*bank,const char*root,uint32_t capacity,
              const AudioPlayer&,std::string&);
 bool begin_prepare(const upstream::MusicRegionData&,const char*bank,const char*root,uint32_t capacity,const AudioPlayer&,std::string&);
 const upstream::MusicRegionData*content()const;
 MusicPreparationStep prepare_step(uint32_t byte_budget,std::string&);
 uint64_t prepared_pcm_bytes()const;
 uint64_t total_pcm_bytes()const;
 bool cancel_preparation(std::string&);
 bool commit_scene(uint64_t epoch,const AudioPlayer&,std::string&);
 bool handoff_scene(MusicRegionService&prepared,uint64_t epoch,const AudioPlayer&,std::string&);
 bool area_enter(uint64_t,std::string_view,const upstream::MusicRegionContext&,const AudioPlayer&,std::string&);
 bool area_exit(uint64_t,std::string_view,const upstream::MusicRegionContext&,std::string&);
 bool play_explicit(uint64_t,std::string_view,const AudioPlayer&,std::string&);
 bool stop_explicit(uint64_t,std::string_view,double fadeout_seconds,std::string&);
 bool idle_frame(uint64_t,std::string&);
 bool tree_exit(uint64_t,std::string_view,std::string&);
 bool finish_scene(uint64_t,std::string&);
 bool bind_room_history(upstream::RoomView,std::string&);
 bool consume_room_history(upstream::RoomView,const std::vector<upstream::OpeningAudioRequest>&,std::string&);
 // Invoke from AudioPlayer's per-request fade callback, in actual audio
 // history order. This routes exactly one source child, never both owners.
 bool route_room_fade(upstream::RoomView,const upstream::OpeningAudioRequest&,AudioPlayer&,std::string&);
 // Call after the existing AudioPlayer consume/update boundary. Dormant and
 // Preparing/Prepared are strict no-ops; no implicit prepare, scene entry or track play.
 bool update(double delta,const AudioPlayer&,std::string&);
 // Explicit NewGame/application teardown only. Ordinary scene exits must use
 // tree_exit + finish_scene so their voices drain according to source fades.
 void shutdown();
 MusicRegionServicePhase phase()const{return phase_;}
 uint64_t scene_epoch()const;
 uint32_t live_voice_count()const;
 uint32_t submitted_voices()const;
 uint32_t buffer_bytes()const;
 bool registered_regions(std::vector<uint64_t>&,std::string&)const;
 bool source_players(const AudioPlayer&,std::vector<MusicSourcePlayer>&,std::string&)const;
private:
 struct State;std::unique_ptr<State>state_;
 MusicRegionServicePhase phase_=MusicRegionServicePhase::Dormant;
 static bool observe(upstream::MusicRegionController&,MusicObservation&known_owner,const MusicObservation&,std::string&);
};
// Explicit loading-loop cooperation. Only old streaming queues move; no world
// tick, fade advance, new Music identity or gain write is performed. The pure
// preparation methods above remain zero-NDSP. Call repeatedly while Progress.
MusicPreparationStep pump_region_music_preparation(MusicRegionService&,AudioPlayer&,
                                                  uint32_t byte_budget,std::string&);
}

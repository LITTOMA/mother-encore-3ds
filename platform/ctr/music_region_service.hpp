#pragma once
#include "audio_player.hpp"
#include "music_region_player.hpp"
#include <memory>
namespace encore::ctr {
enum class MusicRegionServicePhase:uint8_t {Dormant,Preparing,Prepared,Active,Draining};
// App-lifetime service. It stays Dormant in the current House-only main: no
// resource reads, buffers or region NDSP calls happen until explicit prepare.
// The scene factory owns real source-ordered Area callbacks and commit epochs.
class MusicRegionService final {
public:
 MusicRegionService();~MusicRegionService();
 MusicRegionService(const MusicRegionService&)=delete;
 MusicRegionService&operator=(const MusicRegionService&)=delete;
 bool begin_prepare(const char*regions,const char*bank,const char*root,uint32_t capacity,
              const AudioPlayer&,std::string&);
 MusicPreparationStep prepare_step(uint32_t byte_budget,std::string&);
 uint64_t prepared_pcm_bytes()const;
 uint64_t total_pcm_bytes()const;
 bool cancel_preparation(std::string&);
 bool commit_scene(uint64_t epoch,const AudioPlayer&,std::string&);
 bool area_enter(uint64_t,std::string_view,const upstream::MusicRegionContext&,const AudioPlayer&,std::string&);
 bool area_exit(uint64_t,std::string_view,const upstream::MusicRegionContext&,std::string&);
 bool idle_frame(uint64_t,std::string&);
 bool tree_exit(uint64_t,std::string_view,std::string&);
 bool finish_scene(uint64_t,std::string&);
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
private:
 struct State;std::unique_ptr<State>state_;
 MusicRegionServicePhase phase_=MusicRegionServicePhase::Dormant;
 static bool observe(upstream::MusicRegionController&,uint64_t&known_generation,const MusicObservation&,std::string&);
};
// Explicit loading-loop cooperation. Only old streaming queues move; no world
// tick, fade advance, new Music identity or gain write is performed. The pure
// preparation methods above remain zero-NDSP. Call repeatedly while Progress.
MusicPreparationStep pump_region_music_preparation(MusicRegionService&,AudioPlayer&,
                                                  uint32_t byte_budget,std::string&);
}

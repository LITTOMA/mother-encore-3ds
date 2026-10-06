#pragma once
#include "encore/audio_data.hpp"
#include <array>
#include <cstdint>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
struct MusicRegionTrack {uint32_t id=0;std::array<uint8_t,32> source_sha{},import_sha{};std::string source_path;};
struct MusicRegionBinding {
 uint32_t id=0,track_id=0;bool disabled=false;
 float volume_db=0,fadein_seconds=0,fadeout_seconds=0;
 std::string source_path,appear_flag,disappear_flag;
 std::vector<std::string> shape_paths;
};
class MusicRegionData {
public:
 bool load(const uint8_t*,size_t,std::string&);
 bool load_file(const char*,std::string&);
 bool matches(const AudioBank&,std::string&)const;
 // Select only this checked region scope from a complete audio bank. Asset
 // string_views borrow bank, whose lifetime must cover their use. Extra bank
 // sounds are expected; they never occupy the region player's track array.
 // Failure preserves out, including capacity/master/source mismatches.
 bool select_assets(const AudioBank&,float expected_master_db,uint32_t max_tracks,
                    std::vector<AudioAsset>&out,std::string&)const;
 bool valid()const{return valid_;}
 const std::vector<MusicRegionBinding>&regions()const{return regions_;}
 const std::vector<MusicRegionTrack>&tracks()const{return tracks_;}
 float silence_db()const{return silence_db_;}
 float fade_to_seconds()const{return fade_to_seconds_;}
private:bool valid_=false;float silence_db_=0,fade_to_seconds_=0;
 std::vector<MusicRegionBinding>regions_;std::vector<MusicRegionTrack>tracks_;
};
struct MusicRegionContext {
 bool is_player=true,in_cutscene=false,in_battle=false,has_collisions=true;
 std::function<bool(std::string_view)> flag;
};
// A coordinator-issued monotonic object identity for the observed non-region
// music player. Replacing its stream or stopping it does not mint an identity.
// Report actual removal before a replacement; playback counters are not IDs.
// Report creation in original global-manager order, not based on gain.
// Notifications do not touch NDSP; staged scene preparation must not emit them.
struct MusicExternalPlayer {uint64_t generation=0;bool present=false,playing=false;};
enum class MusicRegionCurve:uint8_t{None,Linear,QuartIn,QuartOut};
struct MusicRegionVoice {
 uint64_t generation=0,order=0;uint32_t track_id=0;
 bool allocated=false,playing=false,tweening=false;
 float gain_db=0,start_db=0,target_db=0;
 double elapsed=0,duration=0;MusicRegionCurve curve=MusicRegionCurve::None;
};
struct MusicRegionState {bool inside=false,registered=false,pending_exit=false;int voice=-1;};
// Only the pinned MusicChanger callback semantics. No overlap solver, priority
// system, Godot interpreter, save identity, or source-path constants live here.
class MusicRegionController {
public:
 static constexpr uint32_t maximum_voices=16;
 bool initialize(const MusicRegionData&,uint32_t voice_capacity,std::string&);
 // Call only after prior scene's tree_exit callbacks; voices survive the switch.
 bool attach_scene(uint64_t scene_epoch,std::string&);
 bool observe_external_player(MusicExternalPlayer,std::string&);
 bool enter(uint64_t,std::string_view,const MusicRegionContext&,std::string&);
 bool exit(uint64_t,std::string_view,const MusicRegionContext&,std::string&);
 // Direct source play_music/stop_music calls are distinct from Area callbacks:
 // they run during cutscenes and do not apply appear/disappear flag guards.
 bool play_explicit(uint64_t,std::string_view,std::string&);
 bool stop_explicit(uint64_t,std::string_view,double fadeout_seconds,std::string&);
 // Source global get_audio_player(0) uses earliest surviving child order,
 // never latest-song order. Reports whether the bounded external owner wins.
 bool fade_index_zero(double duration,bool&external_target,std::string&);
 bool tree_exit(uint64_t,std::string_view,std::string&);
 bool idle_frame(uint64_t,std::string&);
 // Main audio-manager tweens must be counted for source tween_all_completed.
 bool advance(double delta,bool external_tween_active,std::string&);
 const std::vector<MusicRegionVoice>&voices()const{return voices_;}
 const std::vector<MusicRegionState>&regions()const{return states_;}
 uint64_t scene_epoch()const{return epoch_;}
 uint32_t capacity()const{return uint32_t(voices_.size());}
private:
 int region(std::string_view)const;int latest()const;int song(uint32_t)const;
 bool play(uint32_t,std::string&);
 void stop(uint32_t,double);void tween(int,float,double,MusicRegionCurve);
 int allocate(std::string&);void detach_dead(int);
 const MusicRegionData*data_=nullptr;uint64_t epoch_=0,next_generation_=0,next_order_=0;
 MusicExternalPlayer external_{};uint64_t external_order_=0;
 bool cleanup_waiting_=false;
 bool external_history_ambiguous_=false;
 std::vector<MusicRegionVoice>voices_;std::vector<MusicRegionState>states_;
 std::vector<uint32_t>registered_,pending_exits_;
};
}

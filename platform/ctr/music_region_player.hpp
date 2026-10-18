#pragma once
#include "encore/music_regions.hpp"
#include "encore/music_bank_routes.hpp"
#include "region_channel_pool.hpp"
#include <3ds.h>
#include <array>
#include <cmath>

namespace encore::ctr {
enum class MusicPreparationStep:uint8_t {Progress,Ready,Failed};
struct MusicPlaybackFinished{uint64_t player=0,generation=0;};
// Existing AudioPlayer owns NDSP, channels0..3 and global master volume.
// Regional voices lease distinct channels4..19 across active/draining scenes,
// sharing that pool with the optional named run stream.
// Prepared candidates own no channels. Shut down before the AudioPlayer owner.
class MusicRegionPlayer {
public:
 static constexpr uint32_t first_channel=region_channel_pool::first_channel,maximum_voices=region_channel_pool::capacity;
 static constexpr uint32_t maximum_prepare_bytes=64*1024;
 MusicRegionPlayer()=default;~MusicRegionPlayer(){shutdown();}
 MusicRegionPlayer(const MusicRegionPlayer&)=delete;
 MusicRegionPlayer&operator=(const MusicRegionPlayer&)=delete;
 // Metadata-only start; PCM validation is explicitly incremental. Every step
 // reads at most one8KiB chunk and respects its smaller caller byte budget.
 // After CRC/length validation, each step reserves at most one24KiB buffer.
 // Begin/step/cancel do no NDSP work; a loading coordinator pumps the old owner.
 bool begin_prepare(const char* bank,const char* asset_root,const upstream::MusicRegionData&,
                    uint32_t capacity,bool ndsp_available,float existing_master_db,std::string&);
 MusicPreparationStep prepare_step(uint32_t byte_budget,std::string&);
 bool preparing()const{return preparing_;}
 uint64_t prepared_pcm_bytes()const{return verified_bytes_;}
 uint64_t total_pcm_bytes()const{return total_bytes_;}
 bool sync(const upstream::MusicRegionController&,std::string&);
 // Global loop graph transport. Validate every provider first, release obsolete
 // voices across every provider next, then sync; preparation remains zero-DSP.
 bool check_routes(const upstream::MusicBankRoutes&,uint64_t provider,std::string&)const;
 bool release_routes(const upstream::MusicBankRoutes&,uint64_t provider,std::string&);
 bool sync_routes(const upstream::MusicBankRoutes&,uint64_t provider,std::string&);
 // Refill only; preserve gain, fades, pause, controller identity and all other owners.
 bool pump_streams(std::string&);
 std::vector<MusicPlaybackFinished>take_finished();
 bool set_paused(bool,std::string&);
 bool set_user_music_db(float db){if(!std::isfinite(db)||db<-120||db>0)return false;user_music_db_=db;return true;}
 void shutdown();
 bool prepared()const{return prepared_;}
 const upstream::AudioBank&bank()const{return bank_;}
 uint32_t submitted_voices()const{return submitted_;}
 uint32_t buffer_bytes()const{return capacity_*buffer_count*buffer_frames*2*sizeof(int16_t);}
private:
 static constexpr uint32_t buffer_count=3,buffer_frames=2048;
 struct Track{upstream::AudioAsset asset{};FILE*file=nullptr;};
 struct Voice{uint64_t identity=0,generation=0;uint32_t track=0,channel=maximum_voices;bool active=false,completed=false;
  upstream::AudioFrameCursor cursor;std::array<ndspWaveBuf,buffer_count>waves{};int16_t*samples=nullptr;};
 struct PlaybackVoice{uint64_t identity=0,generation=0;uint32_t track_id=0;float gain_db=0;bool playing=false;};
 using PlaybackFrame=std::array<PlaybackVoice,maximum_voices>;
 bool route_frame(const upstream::MusicBankRoutes&,uint64_t,PlaybackFrame&,std::string&)const;
 bool sync_frame(const PlaybackFrame&,const upstream::MusicRegionController*,std::shared_ptr<const upstream::MusicPlayersOwner>,uint64_t,std::string&);
 bool refill(uint32_t,std::string&);void stop(uint32_t);
 upstream::AudioBank bank_;upstream::MusicRegionData prepared_regions_;std::array<Track,64>tracks_{};
 std::array<Voice,maximum_voices>voices_{};uint32_t capacity_=0,track_count_=0,submitted_=0;
 const upstream::MusicRegionController*owner_=nullptr;
 std::shared_ptr<const upstream::MusicPlayersOwner>graph_owner_;uint64_t provider_identity_=0;
 bool prepared_=false,preparing_=false,paused_=false;
 uint32_t validation_track_=0,allocation_voice_=0,validation_crc_=0xffffffffu;
 uint64_t track_bytes_=0,verified_bytes_=0,total_bytes_=0;
 std::vector<MusicPlaybackFinished>finished_;
 std::string asset_root_;
 float user_music_db_=0;
};
}

#pragma once
#include "encore/music_regions.hpp"
#include <3ds.h>
#include <array>

namespace encore::ctr {
enum class MusicPreparationStep:uint8_t {Progress,Ready,Failed};
// Separate, opt-in adapter. Existing AudioPlayer owns NDSP, channels0..3 and
// global master volume. This object uses only channels4..19 and must shut down
// before that NDSP owner. It never initializes, exits, or reconfigures NDSP.
class MusicRegionPlayer {
public:
 static constexpr uint32_t first_channel=4,maximum_voices=16;
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
 void shutdown();
 bool prepared()const{return prepared_;}
 uint32_t submitted_voices()const{return submitted_;}
 uint32_t buffer_bytes()const{return capacity_*buffer_count*buffer_frames*2*sizeof(int16_t);}
private:
 static constexpr uint32_t buffer_count=3,buffer_frames=2048;
 struct Track{upstream::AudioAsset asset{};FILE*file=nullptr;};
 struct Voice{uint64_t generation=0;uint32_t track=0;bool active=false;
  upstream::AudioFrameCursor cursor;std::array<ndspWaveBuf,buffer_count>waves{};int16_t*samples=nullptr;};
 bool refill(uint32_t,std::string&);void stop(uint32_t);
 upstream::AudioBank bank_;std::array<Track,16>tracks_{};
 std::array<Voice,maximum_voices>voices_{};uint32_t capacity_=0,track_count_=0,submitted_=0;
 const upstream::MusicRegionController*owner_=nullptr;
 bool prepared_=false,preparing_=false;
 uint32_t validation_track_=0,allocation_voice_=0,validation_crc_=0xffffffffu;
 uint64_t track_bytes_=0,verified_bytes_=0,total_bytes_=0;
 std::string asset_root_;
};
}

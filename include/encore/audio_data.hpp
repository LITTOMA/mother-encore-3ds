#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace encore::upstream {
// Independent, little-endian ENCAUD01 metadata. PCM is signed 16-bit LE,
// interleaved and streamed separately; neither source paths nor tuning live here.
struct AudioAsset {
    uint32_t stable_id=0, sample_rate=0, frames=0, loop_start=0, pcm_bytes=0, pcm_crc=0;
    uint16_t channels=0, flags=0;
    std::array<uint8_t,32> source_sha256{};
    std::string_view pcm_path{},source_path{};
    float gain_db=0;
    bool loops() const { return (flags&1)!=0; }
};
class AudioBank {
public:
    bool load(const uint8_t* bytes,size_t size,std::string& error);
    bool load_file(const char* path,std::string& error);
    uint32_t count() const;
    AudioAsset asset(uint32_t index) const;
    bool find(uint32_t stable_id,AudioAsset& result) const;
    float master_db() const;
    float silence_db() const;
private:
    std::vector<uint8_t> bytes_;
};
uint32_t audio_crc32(const uint8_t* bytes,size_t size);
// Finite, testable frame cursor. A looping stream plays the whole intro first,
// then repeats [loop_start,frames), including across partially filled buffers.
class AudioFrameCursor {
public:
    bool reset(uint32_t frames,uint32_t loop_start,bool loops);
    uint32_t take(uint32_t maximum,uint32_t& first);
    uint32_t position() const { return position_; }
private:
    uint32_t frames_=0,loop_start_=0,position_=0; bool loops_=false;
};
class AudioPcmStream {
public:
    AudioPcmStream()=default;
    ~AudioPcmStream(){close();}
    AudioPcmStream(const AudioPcmStream&)=delete;
    AudioPcmStream& operator=(const AudioPcmStream&)=delete;
    bool open(const AudioAsset& asset,const char* path,std::string& error);
    void close();
    // Capacity and returned count are stereo/mono FRAMES, not scalar samples.
    bool read(int16_t* output,uint32_t capacity,uint32_t& frames,std::string& error);
    bool rewind();
    bool is_open() const {return file_!=nullptr;}
private:
    FILE* file_=nullptr;
    AudioFrameCursor cursor_;
    uint32_t frames_=0,loop_start_=0;uint16_t channels_=0;bool loops_=false;
};
// Godot audioManager.music_fadeout: quartic-in interpolation in dB.
// The silence threshold is data; fade completion stops playback, not the world.
class AudioFade {
public:
    void reset(float db){current_=start_=target_=db;elapsed_=duration_=0;active_=false;}
    bool start(float target,double duration,bool ease_out=false);
    bool advance(double delta);
    float db() const{return current_;}
    bool active() const{return active_;}
private:
    float current_=0,start_=0,target_=0;double elapsed_=0,duration_=0;bool active_=false,ease_out_=false;
};
float audio_linear_gain(float db);
}

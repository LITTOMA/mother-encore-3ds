#include "encore/audio_data.hpp"
#include "encore/crc32.hpp"
#include "encore/load_progress.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace encore::upstream {
namespace {
constexpr size_t header_size=64,record_size=96,max_bank_size=65536;
uint16_t u16(const uint8_t* p){return uint16_t(p[0])|(uint16_t(p[1])<<8);}
uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
float f32(const uint8_t* p){uint32_t u=u32(p);float f;std::memcpy(&f,&u,4);return f;}
bool zero(const uint8_t* p,size_t n){for(size_t i=0;i<n;++i)if(p[i])return false;return true;}
bool safe_path(std::string_view path){
    if(path.empty()||path.front()=='/'||path.back()=='/')return false;
    size_t first=0;
    while(first<path.size()){
        size_t end=path.find('/',first);if(end==std::string_view::npos)end=path.size();
        auto part=path.substr(first,end-first);if(part.empty()||part=="."||part=="..")return false;
        for(char c:part)if(static_cast<unsigned char>(c)<32||static_cast<unsigned char>(c)>126||c=='\\'||c==':')return false;
        first=end+1;
    }return true;
}
bool db_ok(float db){return std::isfinite(db)&&db>=-120&&db<=0;}
}
uint32_t audio_crc32(const uint8_t* bytes,size_t size){return crc32(bytes,size);}
uint32_t AudioBank::count()const{return bytes_.empty()?0:u32(bytes_.data()+20);}
float AudioBank::master_db()const{return bytes_.empty()?0:f32(bytes_.data()+36);}
float AudioBank::silence_db()const{return bytes_.empty()?0:f32(bytes_.data()+40);}
AudioAsset AudioBank::asset(uint32_t index)const{
    AudioAsset a;if(index>=count())return a;
    const auto* p=bytes_.data()+header_size+size_t(index)*record_size;
    a.stable_id=u32(p);std::copy_n(p+4,32,a.source_sha256.begin());a.sample_rate=u32(p+36);
    a.channels=u16(p+40);a.flags=u16(p+42);a.frames=u32(p+44);a.loop_start=u32(p+48);
    a.pcm_bytes=u32(p+52);a.pcm_crc=u32(p+56);a.gain_db=f32(p+76);
    a.pcm_path={reinterpret_cast<const char*>(bytes_.data()+u32(p+60)),u32(p+64)};
    a.source_path={reinterpret_cast<const char*>(bytes_.data()+u32(p+68)),u32(p+72)};return a;
}
bool AudioBank::find(uint32_t id,AudioAsset& result)const{for(uint32_t i=0;i<count();++i)if(asset(i).stable_id==id){result=asset(i);return true;}return false;}
bool AudioBank::load(const uint8_t* bytes,size_t size,std::string& error){
    auto fail=[&](const char* message){error=message;return false;};
    if(!bytes||size<header_size||size>max_bank_size)return fail("Audio bank size out of bounds");
    if(std::memcmp(bytes,"ENCAUD01",8)||u32(bytes+8)!=1)return fail("Unsupported audio bank version");
    if(u32(bytes+12)!=size||u32(bytes+24)!=record_size||!zero(bytes+44,20))return fail("Invalid audio bank header");
    const uint32_t n=u32(bytes+20),strings=u32(bytes+28);
    if(!n||n>64||strings!=header_size+size_t(n)*record_size||strings>size||u32(bytes+32)!=size-strings)return fail("Invalid audio bank directory");
    if(!db_ok(f32(bytes+36))||!db_ok(f32(bytes+40))||f32(bytes+40)>-20)return fail("Invalid audio bank gains");
    report_load_progress(LoadPhase::Checksum,0,size);
    uint32_t crc=crc32_update(0xffffffffu,bytes,16);const uint8_t zeros[4]{};
    crc=crc32_update(crc,zeros,4);report_load_progress(LoadPhase::Checksum,20,size);
    for(size_t offset=20;offset<size;){
        const size_t n=std::min(size_t(8192),size-offset);
        crc=crc32_update(crc,bytes+offset,n);offset+=n;
        report_load_progress(LoadPhase::Checksum,offset,size);
    }
    crc^=0xffffffffu;
    if(crc!=u32(bytes+16))return fail("Audio bank checksum mismatch");
    size_t next_string=strings;
    for(uint32_t i=0;i<n;++i){
        const auto* p=bytes+header_size+size_t(i)*record_size;
        if(!u32(p)||zero(p+4,32)||!zero(p+80,16))return fail("Invalid audio asset identity/reserved bytes");
        for(uint32_t j=0;j<i;++j)if(u32(bytes+header_size+size_t(j)*record_size)==u32(p))return fail("Duplicate audio asset identity");
        const uint32_t rate=u32(p+36),frames=u32(p+44),loop=u32(p+48),length=u32(p+52);
        const uint16_t channels=u16(p+40),flags=u16(p+42);
        if(rate<8000||rate>48000||(channels!=1&&channels!=2)||flags>1||!frames||uint64_t(frames)*channels*2!=length||length>64*1024*1024u)return fail("Unsupported audio PCM dimensions");
        if((flags&&loop>=frames)||(!flags&&loop!=0)||!db_ok(f32(p+76)))return fail("Invalid audio loop/gain");
        for(unsigned field=0;field<2;++field){
            const uint32_t offset=u32(p+60+field*8),length_string=u32(p+64+field*8);
            if(offset!=next_string||length_string==0||length_string>1024||offset>=size||length_string>=size-offset||bytes[offset+length_string]!=0)return fail("Invalid audio string bounds/order");
            std::string_view value(reinterpret_cast<const char*>(bytes+offset),length_string);
            if(field==0){if(!safe_path(value))return fail("Unsafe audio PCM path");}
            else if(value.substr(0,6)!="res://"||!safe_path(value.substr(6)))return fail("Unsafe audio source path");
            next_string+=length_string+1;
        }
        for(uint32_t j=0;j<i;++j){const auto* q=bytes+header_size+size_t(j)*record_size;
            for(unsigned field=0;field<2;++field){const uint32_t a=u32(p+60+field*8),b=u32(q+60+field*8),al=u32(p+64+field*8),bl=u32(q+64+field*8);
                if(al==bl&&!std::memcmp(bytes+a,bytes+b,al))return fail("Duplicate audio asset path");}}
    }
    if(next_string!=size)return fail("Trailing audio bank data");
    std::vector<uint8_t> validated(bytes,bytes+size);bytes_.swap(validated);error.clear();return true;
}
bool AudioBank::load_file(const char* path,std::string& error){
    FILE* file=path?std::fopen(path,"rb"):nullptr;if(!file){error="Cannot open audio bank";return false;}
    std::vector<uint8_t> bytes(max_bank_size+1);size_t size=0;
    report_load_progress(LoadPhase::FileRead,0,0);
    while(size<bytes.size()){
        const size_t wanted=std::min(size_t(8192),bytes.size()-size);
        const size_t n=std::fread(bytes.data()+size,1,wanted,file);size+=n;
        if(n)report_load_progress(LoadPhase::FileRead,size,0);
        if(n<wanted)break;
    }
    const bool failed=std::ferror(file)!=0;std::fclose(file);
    if(failed){error="Cannot read audio bank";return false;}
    report_load_progress(LoadPhase::FileRead,size,size);return load(bytes.data(),size,error);
}
bool AudioFrameCursor::reset(uint32_t frames,uint32_t loop_start,bool loops){
    if(!frames||(loops&&loop_start>=frames)||(!loops&&loop_start!=0))return false;
    frames_=frames;loop_start_=loop_start;loops_=loops;position_=0;return true;
}
uint32_t AudioFrameCursor::take(uint32_t maximum,uint32_t& first){
    if(position_==frames_&&loops_)position_=loop_start_;
    first=position_;const uint32_t n=std::min(maximum,frames_-position_);position_+=n;return n;
}
bool AudioPcmStream::open(const AudioAsset& a,const char* path,std::string& error){
    AudioFrameCursor cursor;if((a.channels!=1&&a.channels!=2)||uint64_t(a.frames)*a.channels*2!=a.pcm_bytes||a.pcm_bytes>64*1024*1024u||!cursor.reset(a.frames,a.loop_start,a.loops())){error="Invalid PCM stream dimensions";return false;}
    FILE* file=path?std::fopen(path,"rb"):nullptr;if(!file){error="Cannot open PCM asset";return false;}
    uint8_t block[8192];uint64_t total=0;uint32_t crc=0xffffffffu;size_t n;
    report_load_progress(LoadPhase::Checksum,0,a.pcm_bytes);
    while((n=std::fread(block,1,sizeof(block),file))!=0){
        total+=n;crc=crc32_update(crc,block,n);
        report_load_progress(LoadPhase::Checksum,total,a.pcm_bytes);
        if(total>a.pcm_bytes)break;
    }
    const bool failed=std::ferror(file)!=0||total!=a.pcm_bytes||(crc^0xffffffffu)!=a.pcm_crc||std::fseek(file,0,SEEK_SET)!=0;
    if(failed){std::fclose(file);error="PCM length/checksum/read validation failed";return false;}
    close();file_=file;cursor_=cursor;frames_=a.frames;loop_start_=a.loop_start;channels_=a.channels;loops_=a.loops();error.clear();return true;
}
void AudioPcmStream::close(){if(file_)std::fclose(file_);file_=nullptr;}
bool AudioPcmStream::rewind(){return file_&&cursor_.reset(frames_,loop_start_,loops_);}
bool AudioPcmStream::read(int16_t* output,uint32_t capacity,uint32_t& frames,std::string& error){
    frames=0;if(!file_||!output||!capacity){error="Invalid PCM read";return false;}
    while(frames<capacity){uint32_t first=0;const uint32_t n=cursor_.take(capacity-frames,first);if(!n)break;
        const uint64_t offset=uint64_t(first)*channels_*2;
        if(offset>uint64_t(std::numeric_limits<long>::max())||std::fseek(file_,long(offset),SEEK_SET)!=0||std::fread(output+size_t(frames)*channels_,channels_*2,n,file_)!=n){error="PCM streaming read failed";return false;}
        frames+=n;
    }
    // PCM is little-endian on disk. Decode explicitly for portable host tests.
    auto* bytes=reinterpret_cast<uint8_t*>(output);
    for(size_t i=0;i<size_t(frames)*channels_;++i){const uint16_t value=u16(bytes+i*2);std::memcpy(output+i,&value,2);}
    error.clear();return true;
}
bool AudioFade::start(float target,double duration,bool ease_out){if(!std::isfinite(target)||!std::isfinite(duration)||duration<0)return false;start_=current_;target_=target;duration_=duration;elapsed_=0;ease_out_=ease_out;active_=duration>0;if(!active_)current_=target;return true;}
bool AudioFade::advance(double delta){if(!std::isfinite(delta)||delta<0)return false;if(active_){elapsed_=std::min(duration_,elapsed_+delta);const double t=elapsed_/duration_;const double u=1-t;const double weight=ease_out_?1-u*u*u*u:t*t*t*t;current_=float(start_+(target_-start_)*weight);if(elapsed_>=duration_)active_=false;}return true;}
float audio_linear_gain(float db){return std::isfinite(db)?float(std::pow(10.0,double(db)/20.0)):0;}
}

#include "encore/loading_indicator_data.hpp"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <utility>

namespace encore::upstream { namespace {
constexpr size_t limit=4096;
uint32_t u32(const uint8_t* p) { return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24; }
uint32_t crc(const uint8_t* p,size_t n) {
    uint32_t value=~0u;
    while(n--) { value^=*p++; for(int i=0;i<8;++i)value=(value>>1)^(0xedb88320u&uint32_t(-int32_t(value&1))); }
    return ~value;
}
struct Reader {
    const uint8_t* p; size_t n; bool ok=true;
    uint32_t integer() { if(n<4){ok=false;return 0;}const auto value=u32(p);p+=4;n-=4;return value; }
    float real() { const auto bits=integer();float value;std::memcpy(&value,&bits,4);if(!std::isfinite(value))ok=false;return value; }
    std::string path() {
        const auto count=integer();if(!count||count>256||count>n){ok=false;return {};}
        std::string value(reinterpret_cast<const char*>(p),count);p+=count;n-=count;
        for(unsigned char c:value)if(c<33||c>126||c=='\\'||c==':')ok=false;
        size_t start=0;
        while(start<=value.size()) {
            auto end=value.find('/',start);if(end==value.npos)end=value.size();
            const auto part=value.substr(start,end-start);if(part.empty()||part=="."||part=="..")ok=false;
            if(end==value.size())break;
            start=end+1;
        }
        return value;
    }
};
}

bool LoadingIndicatorData::load(const uint8_t* p,size_t n,std::string& error) {
    auto fail=[&](const char* message){error=message;return false;};
    if(!p||n<24||n>limit)return fail("Loading indicator pack size rejected");
    if(std::memcmp(p,"ENCLD001",8)||u32(p+8)!=1||u32(p+12)!=n||u32(p+20)!=1)
        return fail("Loading indicator schema/size/capability rejected");
    if(crc(p+24,n-24)!=u32(p+16))return fail("Loading indicator checksum rejected");
    LoadingIndicatorData data;Reader r{p+24,n-24};
    data.background_=r.integer();data.texture_width_=r.integer();data.texture_height_=r.integer();
    data.frame_width_=r.integer();data.frame_height_=r.integer();data.frame_count_=r.integer();
    data.right_margin_=r.integer();data.bottom_margin_=r.integer();data.clip_.length=r.real();
    if((data.background_>>24)!=255||!data.frame_width_||!data.frame_height_||data.texture_width_>1024||
       data.texture_height_>1024||!data.frame_count_||data.frame_count_>16||
       data.frame_width_>1024||data.frame_height_>1024||data.texture_width_!=data.frame_width_*data.frame_count_||
       data.texture_height_!=data.frame_height_||data.right_margin_>1024||data.bottom_margin_>1024)
        return fail("Loading indicator texture/layout rejected");
    const auto count=r.integer();
    if(!count||count>16||!std::isfinite(data.clip_.length)||data.clip_.length<=0||data.clip_.length>120)
        return fail("Loading indicator timeline rejected");
    data.clip_.count=static_cast<uint8_t>(count);data.clip_.loop=true;
    for(uint32_t i=0;i<count;++i) {
        const auto time=r.real();const auto frame=r.integer();
        if(!std::isfinite(time)||time<0||time>=data.clip_.length||frame>=data.frame_count_||
           (!i&&time!=0)||(i&&time<=data.clip_.keys[i-1].time))return fail("Loading indicator key rejected");
        data.clip_.keys[i]={time,static_cast<uint16_t>(frame)};
    }
    for(uint32_t i=0;i<data.frame_count_;++i) {
        const auto frame=r.integer();if(frame>65535)return fail("Loading indicator source frame rejected");
        for(auto prior:data.source_frames_)if(prior==frame)return fail("Loading indicator duplicate source frame rejected");
        data.source_frames_.push_back(static_cast<uint16_t>(frame));
    }
    data.texture_path_=r.path();const auto viewports=r.integer();
    if(!viewports||viewports>8)return fail("Loading indicator viewport count rejected");
    for(uint32_t i=0;i<viewports;++i) {
        Viewport viewport{r.integer(),r.integer()};
        if(viewport.width>4096||viewport.height>4096||viewport.width<data.frame_width_+data.right_margin_||
           viewport.height<data.frame_height_+data.bottom_margin_)return fail("Loading indicator viewport rejected");
        for(const auto& previous:data.viewports_)if(previous.width==viewport.width&&previous.height==viewport.height)
            return fail("Loading indicator duplicate viewport rejected");
        data.viewports_.push_back(viewport);
    }
    if(!r.ok||r.n)return fail("Loading indicator truncated/invalid/trailing payload");
    data.valid_=true;*this=std::move(data);error.clear();return true;
}

bool LoadingIndicatorData::load_file(const char* path,std::string& error) {
    if(!path){error="Missing loading indicator path";return false;}
    FILE* file=std::fopen(path,"rb");if(!file){error="Could not open loading indicator pack";return false;}
    uint8_t bytes[limit+1];const auto count=std::fread(bytes,1,sizeof(bytes),file);
    const bool good=!std::ferror(file);const bool closed=std::fclose(file)==0;
    if(!good||!closed){error="Loading indicator read failed";return false;}
    return load(bytes,count,error);
}

bool LoadingIndicatorData::sample_frame_index(double seconds,uint16_t& frame)const {
    if(!valid_||!std::isfinite(seconds)||seconds<0)return false;
    // Reduce wall time in double precision before the source's float animation
    // sampler, so long loading intervals never overflow or advance game state.
    const float phase=static_cast<float>(std::fmod(seconds,double(clip_.length)));
    return sample_frame(clip_,phase,static_cast<uint16_t>(frame_count_),frame);
}

int LoadingIndicatorData::frame_at(double seconds)const {
    uint16_t frame=0;
    return sample_frame_index(seconds,frame)?int(source_frames_[frame]):-1;
}

bool LoadingIndicatorData::sample(int width,int height,double seconds,LoadingIndicatorPlacement& result)const {
    if(width<=0||height<=0)return false;
    bool accepted=false;
    for(const auto& viewport:viewports_)if(viewport.width==uint32_t(width)&&viewport.height==uint32_t(height))accepted=true;
    if(!accepted)return false;
    uint16_t frame=0;if(!sample_frame_index(seconds,frame))return false;
    result={width-int(right_margin_+frame_width_),height-int(bottom_margin_+frame_height_),frame};
    return true;
}

bool LoadingIndicatorData::sample_progress(int width,int height,double seconds,double fraction,
                                          LoadingIndicatorPlacement& result)const {
    if(!std::isfinite(fraction)||fraction<0||fraction>1)return false;
    LoadingIndicatorPlacement pose;
    if(!sample(width,height,seconds,pose))return false;
    const int left=int(right_margin_);
    if(pose.x<left)return false;
    // Only placement changes: animation keeps the original source timeline.
    pose.x=left+int(std::floor(double(pose.x-left)*fraction+.5));
    result=pose;
    return true;
}
}

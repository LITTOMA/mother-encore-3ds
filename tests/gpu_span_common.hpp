#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "encore/battle_data.hpp"
#define BackgroundKernel FrozenBackgroundKernel
#include "tests/gpu_span_cpu_reference.hpp"
#undef BackgroundKernel
#define BackgroundKernel ScalarBackgroundKernel
#include "tests/baby_background_scalar_reference.hpp"
#undef BackgroundKernel
namespace gpu_span {
struct Image {uint32_t width=0,height=0;std::vector<uint8_t> indices;std::vector<uint32_t> palette;};
uint32_t word(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
bool image(const std::string& path,const encore::upstream::BattleResource& resource,Image& result){
    FILE* f=std::fopen(path.c_str(),"rb");if(!f){std::fprintf(stderr,"FAIL image open: %s\n",path.c_str());return false;}
    uint8_t header[36];
    if(std::fread(header,1,sizeof(header),f)!=sizeof(header)||std::memcmp(header,"ENCBPIX\0",8)||word(header+8)!=1){std::fclose(f);std::fprintf(stderr,"FAIL BPX header\n");return false;}
    result.width=word(header+12);result.height=word(header+16);const uint32_t colors=word(header+24),bytes=word(header+28);
    if(resource.kind!=2||result.width!=resource.width||result.height!=resource.height||word(header+20)!=1||
       !colors||colors>256||!result.width||result.width>1024||!result.height||result.height>1024||
       bytes!=colors*4+result.width*result.height){std::fclose(f);std::fprintf(stderr,"FAIL BPX dimensions\n");return false;}
    std::vector<uint8_t> payload(bytes);
    const bool complete=std::fread(payload.data(),1,payload.size(),f)==payload.size()&&std::fgetc(f)==EOF;std::fclose(f);
    if(!complete){std::fprintf(stderr,"FAIL BPX length\n");return false;}
    uint32_t crc=UINT32_MAX;
    for(uint8_t byte:payload){crc^=byte;for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}
    if((crc^UINT32_MAX)!=word(header+32)){std::fprintf(stderr,"FAIL BPX CRC\n");return false;}
    result.palette.resize(colors);for(size_t i=0;i<colors;++i)result.palette[i]=word(payload.data()+4*i);
    result.indices.assign(payload.begin()+colors*4,payload.end());
    for(uint8_t index:result.indices)if(index>=colors){std::fprintf(stderr,"FAIL BPX palette index\n");return false;}
    return true;
}
template<class Kernel>
typename Kernel::Layer layer(const encore::upstream::BattleBackground& b,const Image& source,const Image& palette){
    typename Kernel::Layer l;
    l.source={source.width,source.height,source.indices.data(),source.palette.data(),source.palette.size()};
    l.palette_source={palette.width,palette.height,palette.indices.data(),palette.palette.data(),palette.palette.size()};
    l.width=b.width;l.height=b.height;l.barrel=(b.flags&1)!=0;l.repeat=(b.flags&2)!=0;l.opacity=b.opacity;
    l.effect=b.effect;l.effect_scale=b.effect_scale;l.barrel_x=b.barrel.x;l.barrel_y=b.barrel.y;
    l.amplitude_x=b.oscillation_amplitude.x;l.amplitude_y=b.oscillation_amplitude.y;
    l.frequency_x=b.oscillation_frequency.x;l.frequency_y=b.oscillation_frequency.y;l.speed_x=b.oscillation_speed.x;l.speed_y=b.oscillation_speed.y;
    l.move_x=b.move.x;l.move_y=b.move.y;l.ping_pong_speed_x=b.ping_pong_speed.x;l.ping_pong_speed_y=b.ping_pong_speed.y;
    l.compression_amplitude_x=b.compression_amplitude.x;l.compression_amplitude_y=b.compression_amplitude.y;
    l.compression_frequency_x=b.compression_frequency.x;l.compression_frequency_y=b.compression_frequency.y;
    l.compression_speed_x=b.compression_speed.x;l.compression_speed_y=b.compression_speed.y;
    l.palette_shifting=(b.flags&4)!=0;l.palette_frames=b.palette_frames;l.palette_fixed_row=b.palette_fixed_row;l.palette_speed=b.palette_speed;
    return l;
}

struct Span {uint16_t x=0,y=0,width=0;uint32_t color=0;};
constexpr size_t max_spans=8192;
constexpr uint32_t clear_color=0x81234567u;
inline bool encode(const uint32_t* pixels,uint32_t width,uint32_t height,std::vector<Span>& out){
    out.clear();
    for(uint32_t y=0;y<height;++y){
        uint32_t first=0,color=pixels[size_t(y)*width];
        for(uint32_t x=1;x<=width;++x){
            if(x<width&&pixels[size_t(y)*width+x]==color)continue;
            if(out.size()==max_spans)return false;
            out.push_back({uint16_t(first),uint16_t(y),uint16_t(x-first),color});
            if(x<width){first=x;color=pixels[size_t(y)*width+x];}
        }
    }
    return true;
}
inline void reconstruct(const std::vector<Span>& spans,uint32_t width,std::vector<uint32_t>& pixels){
    for(const auto& s:spans)std::fill_n(pixels.data()+size_t(s.y)*width+s.x,s.width,s.color);
}
struct Content {
    encore::upstream::BattleData data;
    std::vector<Image> source,palette;
    std::vector<encore::FrozenBackgroundKernel::Layer> layers;
    std::vector<encore::ScalarBackgroundKernel::Layer> scalar_layers;
    bool load(const std::string& root,std::string& error){
        if(!data.load_file((root+"/data/doll-entry.encbattle").c_str(),error))return false;
        const auto view=data.view();
        if(view.count(encore::upstream::BattleSection::Backgrounds)!=2){error="Expected checked two-layer fixture";return false;}
        source.resize(2);palette.resize(2);
        for(unsigned n=0;n<2;++n){
            const auto b=view.background(n);const auto r=view.resource(b.resource),p=view.resource(b.palette_resource);
            if(!image(root+"/"+std::string(view.string(r.path)),r,source[n])||!image(root+"/"+std::string(view.string(p.path)),p,palette[n])){error="BPX load failed";return false;}
            layers.push_back(layer<encore::FrozenBackgroundKernel>(b,source[n],palette[n]));
            scalar_layers.push_back(layer<encore::ScalarBackgroundKernel>(b,source[n],palette[n]));
        }
        return true;
    }
};
inline uint32_t calibration_pixel(uint32_t x,uint32_t y){
    // Synthetic raster/packing test, never game content. Exercises every RGBA channel,
    // all alpha values across the pattern, and both axes with <=8192 row spans.
    const uint32_t a=x/16,b=y/8;
    return ((13*a+7*b+3)&255)|(((5*a+29*b+11)&255)<<8)|
           (((23*a+17*b+19)&255)<<16)|(((31*a+37*b+41)&255)<<24);
}
}

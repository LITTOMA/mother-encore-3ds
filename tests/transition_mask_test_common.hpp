#pragma once
#include "encore/transition_mask_plan.hpp"
#include "encore/crc32.hpp"
#include <cstdio>
#include <cstring>
#include <vector>
namespace transition_mask_test {
using Plan=encore::TransitionMaskPlan;
struct Span {uint16_t x=0,y=0,width=0,reserved=0;uint32_t color=0;};
struct Image {
    uint32_t width=0,height=0,frames=0;std::vector<uint32_t> palette;std::vector<uint8_t> indices;
    Plan::Source source()const{return {width,height,frames,indices.data(),indices.size(),palette.data(),palette.size()};}
};
inline uint32_t word(const uint8_t* p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
inline bool load(const char* path,Image& image){
    FILE* file=std::fopen(path,"rb");if(!file)return false;uint8_t header[36];
    bool ok=std::fread(header,1,36,file)==36&&!std::memcmp(header,"ENCBPIX\0",8)&&word(header+8)==1;
    if(!ok){std::fclose(file);return false;}
    image.width=word(header+12);image.height=word(header+16);image.frames=word(header+20);
    const size_t colors=word(header+24),bytes=word(header+28);
    const uint64_t pixels=uint64_t(image.width)*image.height*image.frames;
    if(!image.width||image.width>1024||!image.height||image.height>1024||!image.frames||image.frames>256||!colors||colors>256||pixels>16*1024*1024||bytes!=colors*4+pixels){std::fclose(file);return false;}
    std::vector<uint8_t> data(bytes);ok=std::fread(data.data(),1,bytes,file)==bytes&&std::fgetc(file)==EOF;std::fclose(file);
    if(!ok||(encore::crc32_update(UINT32_MAX,data.data(),data.size())^UINT32_MAX)!=word(header+32))return false;
    image.palette.resize(colors);for(size_t i=0;i<colors;++i)image.palette[i]=word(data.data()+4*i);
    image.indices.assign(data.begin()+colors*4,data.end());for(auto index:image.indices)if(index>=colors)return false;return true;
}
// Frozen direct loop from BattleRenderer::compose_transition, independently
// indexes source pixels instead of sharing the plan's row-run mechanism.
inline void oracle(const Image& mask,Plan::Canvas canvas,uint32_t frame,uint32_t old_color,uint32_t new_color,std::vector<uint32_t>& surface){
    for(uint32_t y=0;y<canvas.height;++y)for(uint32_t x=0;x<canvas.width;++x){
        const auto sx=uint32_t(std::clamp(int64_t(x)-canvas.offset_x,int64_t(0),int64_t(mask.width)-1));
        const auto sy=uint32_t(std::clamp(int64_t(y)-canvas.offset_y,int64_t(0),int64_t(mask.height)-1));
        const auto pixel=mask.palette[mask.indices[(size_t(frame)*mask.height+sy)*mask.width+sx]];
        const size_t i=size_t(y)*canvas.width+x;
        if(pixel==old_color)surface[i]=new_color;
        else if((pixel>>24)!=255)surface[i]=pixel;
    }
}
inline std::vector<Span> encode(const std::vector<uint32_t>& pixels,uint32_t width,uint32_t height){
    std::vector<Span> out;
    for(uint32_t y=0;y<height;++y){uint32_t first=0,color=pixels[size_t(y)*width];
        for(uint32_t x=1;x<=width;++x){if(x<width&&pixels[size_t(y)*width+x]==color)continue;
            out.push_back({uint16_t(first),uint16_t(y),uint16_t(x-first),0,color});first=x;if(x<width)color=pixels[size_t(y)*width+x];}}
    return out;
}
template<class S>inline void reconstruct(const S* spans,size_t count,uint32_t width,std::vector<uint32_t>& pixels){
    for(size_t i=0;i<count;++i){const auto& s=spans[i];std::fill_n(pixels.data()+size_t(s.y)*width+s.x,s.width,s.color);}
}
}

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <string>
#include <vector>
#if defined(ENCORE_BLOCK_PROTOTYPE) || defined(ENCORE_INLINE_PROTOTYPE) || defined(ENCORE_COMBINED_PROTOTYPE) || defined(ENCORE_TEMPORAL_PROTOTYPE)
#define BackgroundKernel CurrentProductionBackgroundKernel
#include "encore/background_kernel.hpp"
#undef BackgroundKernel
#define BackgroundKernel GuardedBackgroundKernel
#include "tests/baby_background_guarded_reference.hpp"
#undef BackgroundKernel
#ifdef ENCORE_TEMPORAL_PROTOTYPE
#include "tests/baby_background_temporal_prototype.hpp"
#elif defined(ENCORE_COMBINED_PROTOTYPE)
#include "tests/baby_background_combined_prototype.hpp"
#elif defined(ENCORE_BLOCK_PROTOTYPE)
#include "tests/baby_background_block_prototype.hpp"
#else
#include "tests/baby_background_inline_prototype.hpp"
#endif
#else
#include "encore/background_kernel.hpp"
#endif
#define private public
#include "platform/ctr/battle_renderer.hpp"
#undef private

// A literal scalar CPU oracle for the explicitly represented subset of the
// pinned default_shader.tres. This is not a Godot GPU equivalence claim. Scene
// values below are TEST FIXTURES; production receives all of them externally.
namespace {
size_t checks=0,pixels=0;
void require(bool ok,const char* text){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",text);std::exit(1);}}
using Layer=encore::BackgroundKernel::Layer;
using Source=encore::BackgroundKernel::Source;
struct Image {
    uint32_t w,h;
    std::vector<uint8_t> indices;
    std::vector<uint32_t> colors;
    Source source()const{return {w,h,indices.data(),colors.data(),colors.size()};}
};
uint32_t texel(float u,float v,const Source& image,bool repeat){
    if(repeat){u=u-std::floor(u);v=v-std::floor(v);}
    const auto x=std::min(uint32_t(std::max(0.0f,std::min(u,1.0f))*image.width),image.width-1);
    const auto y=std::min(uint32_t(std::max(0.0f,std::min(v,1.0f))*image.height),image.height-1);
    return image.palette[image.pixels[size_t(y)*image.width+x]];
}
uint32_t reference_pixel(const std::vector<Layer>& layers,uint32_t x,uint32_t y,float time,uint32_t color){
    for(const auto& l:layers){
        float u=(float(x)+0.5f)/l.width,v=(float(y)+0.5f)/l.height;
        if(l.barrel){
            const float px=2.0f*u-l.barrel_x,py=2.0f*v-l.barrel_y;
            const float d=std::sqrt(px*px+py*py);
            const float z=std::sqrt(1.0f+d*d*l.effect);
            float r=std::atan2(d,z)/3.14159f;r*=l.effect_scale;
            const float phi=std::atan2(py,px);
            u=r*std::cos(phi)+0.5f;v=r*std::sin(phi)+0.5f;
        }
        if(l.frequency_x!=0&&l.amplitude_x!=0)
            u+=l.amplitude_x*std::cos(l.frequency_x*v+time*l.speed_x);
        if(l.frequency_y!=0&&l.amplitude_y!=0)
            v+=l.amplitude_y*std::cos(l.frequency_y*u+time*l.speed_y);
        if(l.compression_frequency_x!=0&&l.compression_amplitude_x!=0)
            u+=l.compression_amplitude_x*std::cos(l.compression_frequency_x*u+time*l.compression_speed_x);
        if(l.compression_frequency_y!=0&&l.compression_amplitude_y!=0)
            v+=l.compression_amplitude_y*std::cos(l.compression_frequency_y*v+time*l.compression_speed_y);
        if(l.ping_pong_speed_x!=0)u+=l.move_x*std::cos(l.ping_pong_speed_x*time);
        else u+=time*l.move_x/0.5f;
        if(l.ping_pong_speed_y!=0)v+=l.move_y*std::cos(l.ping_pong_speed_y*time);
        else v+=time*l.move_y/0.5f;
        uint32_t sampled=texel(u,v,l.source,l.repeat);
        if(l.palette_shifting){
            float palette_v;
            if(!l.palette_frames)palette_v=(float(l.palette_fixed_row)+0.5f)/float(l.palette_source.height);
            else{
                const float row=-time*l.palette_speed*1.0f/float(l.palette_frames);
                palette_v=row-std::floor(row);
            }
            const auto palette=texel(float(sampled&255u)/255.0f,palette_v,l.palette_source,false);
            sampled=(sampled&0xff000000u)|(palette&0x00ffffffu);
        }
        const float alpha=float(sampled>>24)/255.0f*l.opacity;
        uint32_t next=0;
        for(unsigned shift=0;shift<24;shift+=8)
            next|=uint32_t(float((sampled>>shift)&255u)*alpha+float((color>>shift)&255u)*(1.0f-alpha)+0.5f)<<shift;
        next|=uint32_t(255.0f*alpha+float(color>>24)*(1.0f-alpha)+0.5f)<<24;
        color=next;
    }
    return color;
}
Layer baby_layer(const Image& source,const Image& palette,unsigned n){
    Layer l;l.source=source.source();l.width=float(source.w);l.height=float(source.h);
    l.repeat=true;l.barrel=true;l.effect=1;l.effect_scale=3.5f;l.barrel_x=1.8f;l.barrel_y=1;
    l.opacity=n?0.7f:1.0f;l.move_x=n?0.3f:0.2f;l.move_y=n?0.4f:0.1f;l.ping_pong_speed_y=n?0.8f:0.4f;
    l.amplitude_x=n?0:0.9f;l.frequency_x=n?0:1;l.speed_x=n?0:0.4f;
    l.compression_amplitude_y=n?0.5f:0.7f;l.compression_frequency_y=0.6f;l.compression_speed_y=n?0.9f:0.6f;
    l.palette_shifting=true;l.palette_source=palette.source();l.palette_speed=n?-0.1f:0.2f;
    // Nonzero fixture divisor exercises defined palette arithmetic. Upstream
    // baby.bbg omits this uniform; zero needs an explicit observed backend row.
    l.palette_frames=4;return l;
}
void compare(const std::vector<Layer>& layers,uint32_t w,uint32_t h,const std::vector<float>& times){
    std::string error;encore::BackgroundKernel kernel;
    require(kernel.prepare(layers,w,h,error),error.c_str());
    require(!kernel.fused_pair(),"extension must not use static-row Lamp pair");
    std::vector<uint32_t> output(size_t(w)*h);
    std::vector<uint32_t> offsets(output.size());for(size_t i=0;i<offsets.size();++i)offsets[i]=uint32_t(i);
    require(kernel.prepare_mapped_output(offsets.data(),offsets.size(),offsets.size(),error),error.c_str());
    std::fill(output.begin(),output.end(),0xbabecafeu);
    if(kernel.separable_pair()){
        require(kernel.compose_mapped(1,0,output.data(),output.size()),"separable extension supports mapped output");
        for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){
            const auto color=reference_pixel(layers,x,y,1,0);
            const auto mapped=((color&0xffu)<<24)|((color&0xff00u)<<8)|((color>>8)&0xff00u)|(color>>24);
            require(output[size_t(y)*w+x]==mapped,"separable mapped bytes match scalar upload");
        }
    }else{
        require(!kernel.compose_mapped(1,0,output.data(),output.size()),"unsupported extended mapped path rejected");
        require(std::all_of(output.begin(),output.end(),[](uint32_t p){return p==0xbabecafeu;}),"mapped rejection does not write");
    }
    for(float t:times){
        const uint32_t clear=0x74201903u;
        require(kernel.compose(t,clear,output.data()),"finite extended compose");
        for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){
            const auto expected=reference_pixel(layers,x,y,t,clear),actual=output[size_t(y)*w+x];
            if(expected!=actual){std::fprintf(stderr,"t %.9g (%u,%u): expected %08x actual %08x\n",t,x,y,expected,actual);require(false,"scalar source-order parity");}
        }
        ++checks;pixels+=output.size();
    }
}
void write_image(const std::string& path,const Image& image){
    std::vector<uint8_t> bytes(36+image.colors.size()*4+image.indices.size());
    const auto put=[&](size_t off,uint32_t value){for(unsigned n=0;n<4;++n)bytes[off+n]=uint8_t(value>>(8*n));};
    std::memcpy(bytes.data(),"ENCBPIX\0",8);put(8,1);put(12,image.w);put(16,image.h);put(20,1);
    put(24,uint32_t(image.colors.size()));put(28,uint32_t(bytes.size()-36));
    for(size_t i=0;i<image.colors.size();++i)put(36+i*4,image.colors[i]);
    std::copy(image.indices.begin(),image.indices.end(),bytes.begin()+36+image.colors.size()*4);
    uint32_t crc=UINT32_MAX;
    for(size_t i=36;i<bytes.size();++i){crc^=bytes[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}
    put(32,crc^UINT32_MAX);
    FILE* file=std::fopen(path.c_str(),"wb");require(file,"test BPX open");
    require(std::fwrite(bytes.data(),1,bytes.size(),file)==bytes.size(),"test BPX write");require(std::fclose(file)==0,"test BPX close");
}
BattleRenderer::BackgroundLayer platform_layer(const Layer& l){
    BattleRenderer::BackgroundLayer p;p.resource=0;p.palette_resource=1;
    p.barrel=l.barrel;p.repeat=l.repeat;p.width=l.width;p.height=l.height;p.opacity=l.opacity;p.effect=l.effect;p.effect_scale=l.effect_scale;
    p.barrel_x=l.barrel_x;p.barrel_y=l.barrel_y;p.amplitude_x=l.amplitude_x;p.amplitude_y=l.amplitude_y;
    p.frequency_x=l.frequency_x;p.frequency_y=l.frequency_y;p.speed_x=l.speed_x;p.speed_y=l.speed_y;
    p.move_x=l.move_x;p.move_y=l.move_y;p.ping_pong_speed_x=l.ping_pong_speed_x;p.ping_pong_speed_y=l.ping_pong_speed_y;
    p.compression_amplitude_x=l.compression_amplitude_x;p.compression_amplitude_y=l.compression_amplitude_y;
    p.compression_frequency_x=l.compression_frequency_x;p.compression_frequency_y=l.compression_frequency_y;
    p.compression_speed_x=l.compression_speed_x;p.compression_speed_y=l.compression_speed_y;
    p.palette_shifting=l.palette_shifting;p.palette_frames=l.palette_frames;p.palette_speed=l.palette_speed;
    p.palette_fixed_row=l.palette_fixed_row;return p;
}
}
int main(int argc,char** argv){
    require(argc==2,"usage: baby-background-test output-directory");
    Image source{176,172,{},{}},palette{4,4,{},{}};
    source.indices.resize(size_t(source.w)*source.h);
    for(uint32_t y=0;y<source.h;++y)for(uint32_t x=0;x<source.w;++x)source.indices[size_t(y)*source.w+x]=uint8_t(x*13+y*31);
    for(uint32_t i=0;i<256;++i)source.colors.push_back(i|((255-i)<<8)|(i<<16)|((i%4?255:i)<<24));
    for(uint32_t i=0;i<16;++i){palette.indices.push_back(uint8_t(i));palette.colors.push_back((i*17)|((255-i*13)<<8)|((i*11)<<16)|(i<<24));}
    const std::vector<Layer> baby={baby_layer(source,palette,0),baby_layer(source,palette,1)};
    const std::vector<float> times={0,-0.0f,1.0f/60.0f,0.125f,0.999f,1,4.999f,5,5.001f,10,12.5663706f,19.999f,20,20.001f,1000,-1000,3599,3600,1000000};
    compare(baby,400,240,times);compare(baby,320,180,times);
    auto observed=baby;
    for(auto& l:observed){l.palette_frames=0;l.palette_fixed_row=0;}
    compare(observed,400,240,times);compare(observed,320,180,times);
    for(uint32_t row=1;row<palette.h;++row){
        auto layers=observed;for(auto& l:layers)l.palette_fixed_row=row;
        compare(layers,37,29,times);
    }
    auto opaque=source;for(auto& color:opaque.colors)color|=0xff000000u;
    auto separable=observed;for(auto& l:separable)l.source=opaque.source();
    compare(separable,400,240,times);compare(separable,320,180,times);
    // Exercise compression's self-coordinate dependency, oscillation's use of
    // the preceding modified X, both movement branches, repeat/clamp, opacity,
    // palette row direction/boundaries and preserved source alpha.
    for(unsigned variant=0;variant<8;++variant){
        auto layers=baby;
        for(auto& l:layers){
            l.barrel=(variant&1)!=0;l.repeat=(variant&2)!=0;l.opacity=0.5f;
            l.amplitude_y=0.19f;l.frequency_y=2.3f;l.speed_y=-0.7f;
            l.compression_amplitude_x=-0.31f;l.compression_frequency_x=1.4f;l.compression_speed_x=0.27f;
            l.ping_pong_speed_x=(variant&4)?0.5f:0;l.ping_pong_speed_y=(variant&4)?0:0.4f;
        }
        compare(layers,37,29,times);
    }
    std::string error;encore::BackgroundKernel kernel;std::vector<uint32_t> output(64);
    for(unsigned variant=0;variant<24;++variant){
        auto bad=baby[0];
        if(variant==0)bad.palette_frames=0;
        if(variant==1)bad.palette_source.pixels=nullptr;
        if(variant==2)bad.palette_source.palette=nullptr;
        if(variant==3)bad.palette_source.width=0;
        if(variant==4)bad.palette_source.height=1025;
        if(variant==5)bad.palette_source.palette_size=0;
        if(variant==6)bad.palette_source.palette_size=1;
        if(variant==7)bad.palette_shifting=false;
        const float nan=std::numeric_limits<float>::quiet_NaN();
        if(variant==8)bad.move_x=nan;
        if(variant==9)bad.move_y=nan;
        if(variant==10)bad.ping_pong_speed_x=nan;
        if(variant==11)bad.ping_pong_speed_y=nan;
        if(variant==12)bad.compression_amplitude_x=nan;
        if(variant==13)bad.compression_amplitude_y=nan;
        if(variant==14)bad.compression_frequency_x=nan;
        if(variant==15)bad.compression_frequency_y=nan;
        if(variant==16)bad.compression_speed_x=nan;
        if(variant==17)bad.compression_speed_y=nan;
        if(variant==18)bad.palette_speed=nan;
        if(variant==19)bad.palette_source.width=1025;
        if(variant==20)bad.palette_frames=UINT32_MAX;
        if(variant==21){bad.palette_frames=0;bad.palette_fixed_row=palette.h;}
        if(variant==22)bad.palette_fixed_row=0;
        if(variant==23){bad.palette_frames=0;bad.palette_fixed_row=0;bad.palette_shifting=false;}
        require(kernel.prepare(baby,8,8,error),error.c_str());
        require(!kernel.prepare({bad},8,8,error),"invalid extension rejected");
        require(!kernel.compose(0,0,output.data()),"failed extension preparation clears previous state");
    }
    const float max=std::numeric_limits<float>::max();
    for(unsigned variant=0;variant<8;++variant){
        auto bad=baby[0];
        if(variant==0)bad.move_x=max;
        if(variant==1)bad.ping_pong_speed_y=max;
        if(variant==2)bad.compression_speed_y=max;
        if(variant==3)bad.palette_speed=max;
        if(variant==4){bad.barrel=false;bad.compression_frequency_x=max;bad.compression_amplitude_x=1;bad.width=0.01f;}
        require(kernel.prepare({bad},8,8,error),error.c_str());
        const float time=variant<5?10.0f:variant==5?std::numeric_limits<float>::infinity():variant==6?-std::numeric_limits<float>::infinity():std::numeric_limits<float>::quiet_NaN();
        require(!kernel.compose(time,0,output.data()),"nonfinite extension intermediate/TIME rejected");
    }
    const std::string source_path=std::string(argv[1])+"/baby-kernel-source.bpx",palette_path=std::string(argv[1])+"/baby-kernel-palette.bpx";
    write_image(source_path,source);write_image(palette_path,palette);
    BattleRenderer renderer;
    require(renderer.load({{source_path.c_str(),2,source.w,source.h,1,1},{palette_path.c_str(),2,palette.w,palette.h,1,1}},37,29,error),error.c_str());
    auto platform=std::vector<BattleRenderer::BackgroundLayer>{platform_layer(baby[0]),platform_layer(baby[1])};
    require(renderer.set_background(platform,error),error.c_str());
    require(renderer.compose_background(3.7f,0x71234567,true),"renderer extension falls back from direct mapped output");
    require(!renderer.direct_surface_,"extended renderer retains linear surface");
    for(uint32_t y=0;y<29;++y)for(uint32_t x=0;x<37;++x)
        require(renderer.surface_[size_t(y)*37+x]==reference_pixel(baby,x,y,3.7f,0x71234567),"renderer passes every extension field and palette image");
    const auto fixed_platform=std::vector<BattleRenderer::BackgroundLayer>{platform_layer(observed[0]),platform_layer(observed[1])};
    require(renderer.set_background(fixed_platform,error),error.c_str());
    require(renderer.compose_background(3.7f,0x71234567,true),"renderer explicit fixed-row backend adaptation");
    for(uint32_t y=0;y<29;++y)for(uint32_t x=0;x<37;++x)
        require(renderer.surface_[size_t(y)*37+x]==reference_pixel(observed,x,y,3.7f,0x71234567),"renderer passes explicit palette row");
    for(unsigned variant=0;variant<6;++variant){
        auto bad=platform;
        if(variant==0)bad[0].palette_resource=UINT32_MAX;
        if(variant==1)bad[0].palette_frames=0;
        if(variant==2)bad[0].palette_shifting=false;
        if(variant==3){bad[0].palette_resource=0;renderer.assets_[0].frames=2;}
        if(variant==4)bad[0].palette_fixed_row=0;
        if(variant==5){bad[0].palette_frames=0;bad[0].palette_fixed_row=palette.h;}
        require(!renderer.set_background(bad,error),"renderer invalid palette configuration rejected");
        require(!renderer.compose_background(0,0),"renderer does not retain old background after rejection");
        renderer.assets_[0].frames=1;
    }
    renderer.free();
    std::printf("{\"checks\":%zu,\"pixels_compared\":%zu,\"scope\":\"literal scalar CPU reference and actual renderer with GPU stubs; zero palette divisor requires explicit backend row\"}\n",checks,pixels);
    return 0;
}

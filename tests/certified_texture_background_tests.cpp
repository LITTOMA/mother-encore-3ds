#include "encore/certified_texture_background_kernel.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <vector>

// Manual mechanism regression only. These generated samples are not game
// content, and host interpolation does not qualify real PICA raster precision.
namespace {
using Base=encore::BackgroundKernel;
using Candidate=encore::CertifiedTextureBackgroundKernel;
constexpr uint32_t width=400,height=240;
constexpr size_t area=size_t(width)*height;
void require(bool value,const char* message){
    if(!value){std::fprintf(stderr,"FAIL certified texture: %s\n",message);std::exit(1);}
}
struct Sources {
    std::array<uint32_t,4> palette{{0xff173965u,0xff497badu,0xff7dabd7u,0xffb3d1efu}};
    std::array<uint8_t,35> pixels{};
    std::array<uint32_t,4> mapped{{0xff214363u,0xff537797u,0xff89abcdu,0xffc3dfedu}};
    std::array<uint8_t,8> palette_pixels{{0,1,2,3,3,2,1,0}};
    Sources(){for(uint32_t y=0;y<5;++y)for(uint32_t x=0;x<7;++x)pixels[size_t(y)*7+x]=uint8_t((x+2*y)%4);}
    std::vector<Base::Layer> layers()const{
        std::vector<Base::Layer> out(2);
        for(unsigned n=0;n<2;++n){auto& l=out[n];
            l.source={7,5,pixels.data(),palette.data(),palette.size()};
            l.barrel=true;l.repeat=true;l.width=400;l.height=240;l.opacity=n?0.7f:1;
            l.effect=0.4f;l.effect_scale=1.1f;l.barrel_x=0.7f;l.barrel_y=0.9f;
            l.amplitude_x=n?0:0.09f;l.frequency_x=n?0:1.7f;l.speed_x=0.8f;
            l.compression_amplitude_y=n?0.06f:0.08f;l.compression_frequency_y=n?1.3f:2.1f;
            l.compression_speed_y=n?-0.37f:0.57f;l.move_x=n?-0.012f:0.01f;l.move_y=n?0.009f:-0.006f;
        }
        return out;
    }
    void fixed_palette(std::vector<Base::Layer>& layers)const{
        for(auto& l:layers){l.palette_shifting=true;l.palette_fixed_row=1;
            l.palette_source={4,2,palette_pixels.data(),mapped.data(),mapped.size()};}
    }
};
uint32_t color(const Base::Layer& layer,uint32_t x,uint32_t y){
    const auto& source=layer.source;
    uint32_t c=source.palette[source.pixels[size_t(y)*source.width+x]];
    if(layer.palette_shifting){const auto& p=layer.palette_source;
        const float red=float(c&255u)/255.0f;
        const uint32_t at=std::min(uint32_t(red*p.width),p.width-1);
        c=(p.palette[p.pixels[size_t(layer.palette_fixed_row)*p.width+at]]&0xffffffu)|(c&0xff000000u);
    }
    return c;
}
uint32_t blend(uint32_t b,uint32_t a,float opacity){
    const float alpha=float(b>>24)/255*opacity;uint32_t out=0;
    for(unsigned shift=0;shift<24;shift+=8){
        const float v=float((b>>shift)&255)*alpha+float((a>>shift)&255)*(1-alpha);
        out|=uint32_t(v+0.5f)<<shift;
    }
    return out|(uint32_t(255*alpha+float(a>>24)*(1-alpha)+0.5f)<<24);
}
bool zero_delta(const Candidate::Strip& s){
    return s.delta[0]==0&&s.delta[1]==0&&s.delta[2]==0&&s.delta[3]==0;
}
bool empty_stats(const Candidate::Stats& s){
    return s.linear_pixels==0&&s.scalar_pixels==0&&s.exact_pixels==0&&
        s.scalar_trig_calls==0&&s.constant_pixels==0&&s.merged_strips==0;
}
void compare(const std::vector<Base::Layer>& layers,const std::vector<float>& times,bool all_scalar=false,bool all_uniform=false){
    Base base;Candidate candidate;std::string error;
    require(base.prepare(layers,width,height,error),error.c_str());
    require(Candidate::supported_shape(layers,width,height),"supported non-POT shape");
    require(candidate.prepare(base,width,height),"prepare");
    require(candidate.ready()&&candidate.prepared_bytes()==Candidate::preparation_upper_bound(layers,width,height),"residual allocation accounting");
    std::vector<Candidate::Strip> strips(area);
    std::vector<uint32_t> expected(area),actual(area);
    for(float t:times){size_t count=0;Candidate::Stats stats;
        require(candidate.generate(base,t,strips.data(),strips.size(),count,stats),"generate");
        require(count&&count<=area&&stats.linear_pixels+stats.scalar_pixels==area,"complete statistics");
        require(stats.exact_pixels<=stats.scalar_pixels&&stats.scalar_trig_calls<=3*stats.exact_pixels&&
            stats.constant_pixels<=area,"bounded exact and constant statistics");
        require((stats.exact_pixels==0)==(stats.scalar_trig_calls==0),"exact-pixel and trig-call statistics agree");
        if(!all_scalar)require(stats.linear_pixels>0,"ordinary frame uses certified multi-pixel strips");
        require(base.compose(t,0x81234567u,expected.data()),"authoritative compose");
        size_t cursor=0;
        for(size_t i=0;i<count;++i){const auto& s=strips[i];
            require(s.width&&s.y<height&&s.x+s.width<=width,"strip dimensions");
            // Curved UV secants retain the certified eight-pixel limit. Wider
            // strips only join identical, fixed two-layer palette samples.
            require(s.width<=8||zero_delta(s),"wide strip has constant two-layer UVs");
            require(size_t(s.y)*width+s.x==cursor,"ordered exactly-once surface coverage");
            for(uint32_t k=0;k<s.width;++k){uint32_t c[2];
                const float ratio=(float(k)+0.5f)/float(s.width);
                for(unsigned n=0;n<2;++n){
                    const float u=s.uv[n*2]+s.delta[n*2]*ratio;
                    const float v=s.uv[n*2+1]+s.delta[n*2+1]*ratio;
                    const float px=u*256,py=(1-v)*256;
                    require(std::isfinite(px)&&std::isfinite(py)&&px>=0&&py>=0&&
                        px<float(layers[n].source.width)&&py<float(layers[n].source.height),"bounded padded-texture sampling");
                    c[n]=color(layers[n],uint32_t(px),uint32_t(py));
                }
                actual[cursor++]=blend(c[1],c[0],layers[1].opacity);
            }
        }
        require(cursor==area,"all 400x240 pixels covered");
        for(size_t i=0;i<area;++i)if(actual[i]!=expected[i]){
            std::fprintf(stderr,"time=%.9g pixel=(%zu,%zu) got=%08x expected=%08x\n",double(t),i%width,i/width,actual[i],expected[i]);
            require(false,"nearest-texel and blend parity");
        }
        if(all_scalar){require(stats.scalar_pixels==area&&stats.linear_pixels==0,"boundary uses single-pixel certificates");
            // Leaves can be coalesced after their original sampled indices
            // are established; coverage and authoritative parity still apply.
            require(std::all_of(strips.begin(),strips.begin()+count,zero_delta),"merged leaves retain fixed texel centers");}
        if(all_uniform){
            require(count==height&&stats.constant_pixels==area&&stats.merged_strips>0,"uniform source collapses to one strip per row");
            for(size_t i=0;i<count;++i){const auto& s=strips[i];
                require(s.y==i&&s.x==0&&s.width==width&&zero_delta(s),"uniform row spans full width with fixed UVs");
            }
        }
    }
    for(float t:{std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity(),1e20f}){
        size_t count=42;Candidate::Stats stats{42,42,42,42,42,42};
        require(!candidate.generate(base,t,strips.data(),strips.size(),count,stats)&&count==0&&empty_stats(stats),"invalid time clears batch and all statistics");
    }
    size_t count=42;Candidate::Stats stats{42,42,42,42,42,42};
    require(!candidate.generate(base,0,strips.data(),1,count,stats)&&count==0&&empty_stats(stats),"capacity clears partial batch and all statistics");
    count=42;stats={42,42,42,42,42,42};
    require(!candidate.generate(base,0,nullptr,strips.size(),count,stats)&&count==0&&empty_stats(stats),"null storage clears all statistics");
    count=42;stats={42,42,42,42,42,42};
    require(!candidate.generate(base,0,strips.data(),0,count,stats)&&count==0&&empty_stats(stats),"zero capacity clears all statistics");
    candidate.clear();require(!candidate.ready()&&!candidate.prepared_bytes(),"clear releases residuals");
    count=42;stats={42,42,42,42,42,42};
    require(!candidate.generate(base,0,strips.data(),strips.size(),count,stats)&&count==0&&empty_stats(stats),"cleared generation clears all statistics");
}
}
int main(){
    Sources sources;auto layers=sources.layers();
    compare(layers,{0,0.001f,0.5f,1.25f,12.25f,256,-1.25f});
    auto mapped=layers;sources.fixed_palette(mapped);compare(mapped,{0,1.25f,-3.5f});
    Sources uniform_sources;uniform_sources.pixels.fill(2);
    auto uniform_layers=uniform_sources.layers();
    compare(uniform_layers,{0,0.001f,1.25f,256,2048,-1.25f,-2048},false,true);
    auto boundary=layers;
    for(auto& l:boundary){l.effect_scale=0;l.move_x=-0.5f;l.ping_pong_speed_x=1;}
    // Static barrel coordinates are exactly .5; cancel the first oscillation
    // in the same float order so both layers sit on a source repeat boundary.
    boundary[0].move_x=-(0.5f+boundary[0].amplitude_x*std::cos(boundary[0].frequency_x*0.5f));
    compare(boundary,{0},true);

    Base base;Candidate candidate;std::string error;
    require(base.prepare(layers,width,height,error),"negative-case baseline");
    require(!candidate.prepare(base,400,239)&&!candidate.ready(),"unsupported height");
    require(!Candidate::supported_shape(layers,320,180)&&!candidate.prepare(base,320,180),"reference view unsupported");
    require(base.prepare(layers,320,180,error),"reference base valid");
    require(!candidate.prepare(base,width,height)&&!candidate.ready(),"base dimensions must match candidate");
    auto unsupported=layers;unsupported[0].barrel=false;
    require(!Candidate::supported_shape(unsupported,width,height),"non-barrel shape rejected");
    require(base.prepare(unsupported,width,height,error)&&!candidate.prepare(base,width,height),"unsupported prepared base");
    auto animated=mapped;for(auto& l:animated){l.palette_frames=2;l.palette_fixed_row=UINT32_MAX;l.palette_speed=0.5f;}
    require(base.prepare(animated,width,height,error),"valid animated palette baseline");
    require(!Candidate::supported_shape(animated,width,height)&&!candidate.prepare(base,width,height),"animated palette uses baseline");
    require(base.prepare(layers,width,height,error),"cancellation baseline");
    encore::PreparationControl cancelled{[](void*){return true;},nullptr};
    require(!candidate.prepare(base,width,height,&cancelled)&&!candidate.ready(),"cancelled preparation releases state");
    std::puts("PASS certified texture mechanism (host only; real PICA precision unqualified)");
}

#pragma once
#include <algorithm>
#include "encore/preparation_control.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace encore {
// CPU presentation mechanism. No content, paths, clocks or game tuning live here.
// Sources are borrowed immutable, already-decoded palette images: their storage
// must outlive this kernel, and preparation must be repeated after any change.
class BackgroundKernel {
    friend class RegionBackgroundKernel;
public:
    struct Source {
        uint32_t width=0,height=0;
        const uint8_t* pixels=nullptr;
        const uint32_t* palette=nullptr;
        size_t palette_size=0;
    };
    struct Layer {
        Source source;
        bool barrel=false,repeat=false;
        float width=0,height=0,opacity=0,effect=0,effect_scale=0;
        float barrel_x=0,barrel_y=0,amplitude_x=0,amplitude_y=0;
        float frequency_x=0,frequency_y=0,speed_x=0,speed_y=0;
        float move_x=0,move_y=0,ping_pong_speed_x=0,ping_pong_speed_y=0;
        float compression_amplitude_x=0,compression_amplitude_y=0;
        float compression_frequency_x=0,compression_frequency_y=0;
        float compression_speed_x=0,compression_speed_y=0;
        // The shader samples palette RGB at (source red, animated row), keeps
        // source alpha, then applies opacity. This is a separate texture, not
        // a replacement of the BPX encoding palette above. Its sampling is
        // nearest/clamped in the reviewed source import.
        Source palette_source;
        bool palette_shifting=false;
        float palette_speed=0;
        uint32_t palette_frames=0;
        // Explicit external backend adaptation only. The source's zero
        // divisor is undefined; a native Godot/GLES2 reference may establish
        // a fixed sampled row for an audited resource. Never infer this from
        // the image height, or select a row without the supplied override.
        uint32_t palette_fixed_row=UINT32_MAX;
    };
private:
    struct Pixel {
        float x=0,cos_x=0,sin_x=0;
        union {float y;uint32_t row;};
        Pixel():y(0){}
    };
    struct Prepared {
        struct Trig {float cosine=0,sine=0;};
        Layer config;
        std::vector<Pixel> pixels;
        std::vector<Trig> compression_trig;
        bool static_y=false,oscillate_x=false,opaque=false,finite_x=true,bounded_x=true,extended=false;
        bool separable=false,compress_y=false;
        float maximum_x=0,maximum_y=0,maximum_oscillation_phase=0,maximum_compression_phase=0;
    };
    std::vector<Prepared> layers_;
    std::vector<uint32_t> pair_colors_;
    std::vector<uint32_t> mapped_pair_colors_;
    std::vector<uint32_t> extended_pair_colors_,mapped_extended_pair_colors_;
    const uint32_t* output_offsets_=nullptr;
    size_t mapped_output_count_=0;
    size_t size_=0;
    struct Range {float lo=0,hi=0;};
    struct BlockBounds {Range u,v,oc,os,cc,cs;};
    struct Block {uint32_t x=0,y=0,width=0,height=0,first_child=0,children=0;BlockBounds layer[2];};
    struct Homogeneity {uint32_t width=0,height=0,stride=0;const uint8_t* indices=nullptr;std::vector<uint32_t> counts;};
    // Isolated experiment: classify blocks first, then run all exact scalar
    // fallback spans in one loop so frame constants are loaded once per frame.
    struct FallbackSpan { uint32_t first=0,end=0; };
    mutable std::vector<FallbackSpan> fallback_spans_;
    mutable size_t fallback_span_count_=0;
    std::vector<Block> blocks_;
    std::vector<uint32_t> block_roots_;
    std::vector<Homogeneity> homogeneity_;
    uint32_t homogeneity_index_[2]{},output_width_=0;
    bool mapped_unique_=false;
public:
    struct BlockStats {uint32_t tested=0,filled=0,filled_pixels=0,fallback_pixels=0;};
    BlockStats block_stats()const{return block_stats_;}
private:
    mutable BlockStats block_stats_{};

    static float outward(float value,bool up){
        if(!std::isfinite(value))return value;
        if(value==0)return up?std::numeric_limits<float>::denorm_min():-std::numeric_limits<float>::denorm_min();
        uint32_t bits;std::memcpy(&bits,&value,sizeof(bits));
        if((value>0)==up)++bits;else --bits;
        std::memcpy(&value,&bits,sizeof(value));return value;
    }
    static Range scale(Range a,float b){return b>=0?Range{a.lo*b,a.hi*b}:Range{a.hi*b,a.lo*b};}
    static Range add(Range a,Range b){return {a.lo+b.lo,a.hi+b.hi};}
    static Range subtract(Range a,Range b){return {a.lo-b.hi,a.hi-b.lo};}
    static Range expand(Range a,float error){return {outward(a.lo-error,false),outward(a.hi+error,true)};}
    static void include(Range& range,float value){range.lo=std::min(range.lo,value);range.hi=std::max(range.hi,value);}
    Block make_block(uint32_t x,uint32_t y,uint32_t width,uint32_t height)const{
        Block b;b.x=x;b.y=y;b.width=width;b.height=height;
        const float inf=std::numeric_limits<float>::infinity();
        for(auto& l:b.layer)l={{inf,-inf},{inf,-inf},{inf,-inf},{inf,-inf},{inf,-inf},{inf,-inf}};
        for(uint32_t py=y;py<y+height;++py)for(uint32_t px=x;px<x+width;++px){
            const size_t i=size_t(py)*output_width_+px;
            for(size_t n=0;n<2;++n){
                auto& l=b.layer[n];const auto& p=layers_[n];const auto& uv=p.pixels[i];
                include(l.u,uv.x);include(l.v,uv.y);
                if(n==0){include(l.oc,uv.cos_x);include(l.os,uv.sin_x);include(l.cc,p.compression_trig[i].cosine);include(l.cs,p.compression_trig[i].sine);}
                else{include(l.oc,0);include(l.os,0);include(l.cc,uv.cos_x);include(l.cs,uv.sin_x);}
            }
        }
        return b;
    }
    void prepare_blocks(uint32_t width,uint32_t height,const PreparationControl* control=nullptr){
        // Bound prototype memory/work independently of content. All color
        // identities and geometric ranges still come from the loaded images.
        if(extended_pair_colors_.empty()||size_>262144)return;
        for(const auto& p:layers_)if(p.config.source.palette_size>8||uint64_t(p.config.source.width)*p.config.source.height>262144)return;
        output_width_=width;
        const size_t roots=size_t((width+7)/8)*((height+7)/8);blocks_.reserve(roots*5);block_roots_.reserve(roots);homogeneity_.reserve(2);
        // One span per leaf row. Rounded-up columns also cover partial tiles.
        fallback_spans_.resize(size_t((width+3)/4)*height);
        for(size_t n=0;n<2;++n){
            const auto& src=layers_[n].config.source;
            if(n&&src.width==layers_[0].config.source.width&&src.height==layers_[0].config.source.height&&
               src.palette_size==layers_[0].config.source.palette_size&&
               std::memcmp(src.pixels,layers_[0].config.source.pixels,size_t(src.width)*src.height)==0){homogeneity_index_[n]=0;continue;}
            homogeneity_index_[n]=uint32_t(homogeneity_.size());Homogeneity h;
            h.width=src.width;h.height=src.height;h.stride=src.width+1;h.indices=src.pixels;
            const size_t plane=size_t(h.stride)*(src.height+1);
            h.counts.resize(plane*src.palette_size);
            for(size_t color=0;color<src.palette_size;++color){
                auto* sum=h.counts.data()+color*plane;
                for(uint32_t y=0;y<src.height;++y){if(control&&control->stopped())return;uint32_t row=0;
                    for(uint32_t x=0;x<src.width;++x){row+=src.pixels[size_t(y)*src.width+x]==color;sum[size_t(y+1)*h.stride+x+1]=sum[size_t(y)*h.stride+x+1]+row;}
                }
            }
            homogeneity_.push_back(std::move(h));
        }
        for(uint32_t y=0;y<height;y+=8)for(uint32_t x=0;x<width;x+=8){
            if(control&&control->stopped())return;
            const auto w=std::min<uint32_t>(8,width-x),h=std::min<uint32_t>(8,height-y);
            const auto root=uint32_t(blocks_.size());blocks_.push_back(make_block(x,y,w,h));block_roots_.push_back(root);
            if(w>4||h>4){
                blocks_[root].first_child=uint32_t(blocks_.size());
                for(uint32_t cy=0;cy<h;cy+=4)for(uint32_t cx=0;cx<w;cx+=4){blocks_.push_back(make_block(x+cx,y+cy,std::min<uint32_t>(4,w-cx),std::min<uint32_t>(4,h-cy)));++blocks_[root].children;}
            }
        }
    }

    static uint32_t coordinate(float v,uint32_t extent,bool repeat){
        if(repeat&&(v<0||v>=1))v-=std::floor(v);
        return std::min(uint32_t(std::clamp(v,0.0f,1.0f)*extent),extent-1);
    }
    // Identical floor/mod arithmetic for ordinary finite coordinates, using
    // the CPU's integer conversion instead of a per-sample libm floorf call.
    // Outside the proven conversion range retain the original implementation.
    static uint32_t coordinate_separable(float v,uint32_t extent,float float_extent){
        if(v<0||v>=1){
            if(v>=-1048576.0f&&v<=1048576.0f){
                const int32_t truncated=int32_t(v);
                const int32_t floored=truncated-(v<float(truncated));
                v-=float(floored);
            }else v-=std::floor(v);
        }
        return std::min(uint32_t(std::clamp(v,0.0f,1.0f)*float_extent),extent-1);
    }
    static uint32_t blend(uint32_t color,uint32_t base,float opacity){
        if(opacity==1&&(color>>24)==255)return color;
        if(opacity==0.5f&&(color>>24)==255&&(base>>24)==255){
            const uint32_t difference=color^base;
            return (color&base)+((difference&0xfefefefeu)>>1)+(difference&0x01010101u);
        }
        const float alpha=float(color>>24)/255*opacity;
        uint32_t mixed=0;
        for(unsigned shift=0;shift<24;shift+=8){
            const float value=float((color>>shift)&255)*alpha+float((base>>shift)&255)*(1-alpha);
            mixed|=uint32_t(value+0.5f)<<shift;
        }
        return mixed|(uint32_t(255*alpha+float(base>>24)*(1-alpha)+0.5f)<<24);
    }
    template<bool Oscillate,bool Repeat,bool Bounded=false>
    static uint8_t index_static_y(const Prepared& p,const Pixel& uv,float c,float s,float source_width){
        float u=uv.x;
        if constexpr(Oscillate)u+=p.config.amplitude_x*(uv.cos_x*c-uv.sin_x*s);
        uint32_t x;
        if constexpr(Bounded)x=uint32_t(u*source_width);
        else x=coordinate(u,p.config.source.width,Repeat);
        return p.config.source.pixels[uv.row+x];
    }
    template<bool Oscillate0,bool Repeat0,bool Oscillate1,bool Repeat1,bool Bounded=false,bool Mapped=false>
    void compose_pair(float c0,float s0,float c1,float s1,uint32_t* output)const{
        const auto& a=layers_[0];const auto& b=layers_[1];
        const size_t stride=b.config.source.palette_size;
        // Output words can alias uint32_t fields in the compiler's analysis.
        // Cache these invariant conversions before the first output write.
        const float width_a=float(a.config.source.width),width_b=float(b.config.source.width);
        for(size_t i=0;i<size_;++i){
            const auto x=index_static_y<Oscillate0,Repeat0,Bounded>(a,a.pixels[i],c0,s0,width_a);
            const auto y=index_static_y<Oscillate1,Repeat1,Bounded>(b,b.pixels[i],c1,s1,width_b);
            if constexpr(Mapped)output[output_offsets_[i]]=mapped_pair_colors_[size_t(x)*stride+y];
            else output[i]=pair_colors_[size_t(x)*stride+y];
        }
    }
    template<bool X0,bool R0,bool Mapped=false>
    void dispatch_pair_second(float c0,float s0,float c1,float s1,uint32_t* output)const{
        const auto& b=layers_[1];
        if(b.oscillate_x){
            if(b.config.repeat)compose_pair<X0,R0,true,true,false,Mapped>(c0,s0,c1,s1,output);
            else compose_pair<X0,R0,true,false,false,Mapped>(c0,s0,c1,s1,output);
        }else{
            if(b.config.repeat)compose_pair<X0,R0,false,true,false,Mapped>(c0,s0,c1,s1,output);
            else compose_pair<X0,R0,false,false,false,Mapped>(c0,s0,c1,s1,output);
        }
    }
    template<bool Mapped=false>
    void compose_pair_ready(const float* cosine,const float* sine,uint32_t* output)const{
        const auto& a=layers_[0];
        if(a.bounded_x&&layers_[1].bounded_x){
            if(a.oscillate_x){
                if(layers_[1].oscillate_x)compose_pair<true,false,true,false,true,Mapped>(cosine[0],sine[0],cosine[1],sine[1],output);
                else compose_pair<true,false,false,false,true,Mapped>(cosine[0],sine[0],cosine[1],sine[1],output);
            }else{
                if(layers_[1].oscillate_x)compose_pair<false,false,true,false,true,Mapped>(cosine[0],sine[0],cosine[1],sine[1],output);
                else compose_pair<false,false,false,false,true,Mapped>(cosine[0],sine[0],cosine[1],sine[1],output);
            }
            return;
        }
        if(a.oscillate_x){
            if(a.config.repeat)dispatch_pair_second<true,true,Mapped>(cosine[0],sine[0],cosine[1],sine[1],output);
            else dispatch_pair_second<true,false,Mapped>(cosine[0],sine[0],cosine[1],sine[1],output);
        }else{
            if(a.config.repeat)dispatch_pair_second<false,true,Mapped>(cosine[0],sine[0],cosine[1],sine[1],output);
            else dispatch_pair_second<false,false,Mapped>(cosine[0],sine[0],cosine[1],sine[1],output);
        }
    }
    struct SeparableFrame {
        const Pixel* coordinates=nullptr;
        const Prepared::Trig* compression_trig=nullptr;
        const uint8_t* indices=nullptr;
        uint32_t width=0,height=0;
        float float_width=0,float_height=0,amplitude=0,compression_amplitude=0;
        float phase=0,compression_phase=0,move_x=0,move_y=0;
        float frequency=0,compression_frequency=0;
        float cosine=0,sine=0,compression_cosine=0,compression_sine=0;
        float error_x=0,error_y=0;
        bool guarded=false;
    };
    static float round_width(float magnitude){
        return std::nextafter(magnitude,std::numeric_limits<float>::infinity())-magnitude;
    }
    bool prepare_separable_frame(float time,SeparableFrame (&frames)[2])const{
        if(extended_pair_colors_.empty()||!std::isfinite(time))return false;
        for(size_t n=0;n<2;++n){
            const auto& p=layers_[n];const auto& l=p.config;auto& f=frames[n];
            f.coordinates=p.pixels.data();f.indices=l.source.pixels;f.width=l.source.width;f.height=l.source.height;
            f.compression_trig=p.compression_trig.data();
            f.float_width=float(f.width);f.float_height=float(f.height);
            f.amplitude=l.amplitude_x;f.compression_amplitude=l.compression_amplitude_y;
            f.frequency=l.frequency_x;f.compression_frequency=l.compression_frequency_y;
            f.phase=time*l.speed_x;f.compression_phase=time*l.compression_speed_y;
            f.move_x=l.ping_pong_speed_x!=0?l.move_x*std::cos(l.ping_pong_speed_x*time):time*l.move_x/0.5f;
            f.move_y=l.ping_pong_speed_y!=0?l.move_y*std::cos(l.ping_pong_speed_y*time):time*l.move_y/0.5f;
            // Prove all pixel phases, products and additions remain finite
            // BEFORE a mapped frame can write any output. Unusual extreme
            // coefficients fall back to the original scalar execution.
            if(!std::isfinite(f.phase)||!std::isfinite(time*l.speed_y)||!std::isfinite(f.compression_phase)||
               !std::isfinite(f.move_x)||!std::isfinite(f.move_y)||
               !std::isfinite(p.maximum_x+std::abs(f.amplitude)+std::abs(f.move_x))||
               !std::isfinite(p.maximum_y+std::abs(f.compression_amplitude)+std::abs(f.move_y))||
               !std::isfinite(p.maximum_oscillation_phase+std::abs(f.phase))||
               !std::isfinite(p.maximum_compression_phase+std::abs(f.compression_phase)))return false;
            // A guarded identity may skip the scalar libm calls only when
            // every possible rounded result remains in ONE sampled texel.
            // Bounds include argument-addition rounding, a deliberately wide
            // f32 libm/identity envelope, and each later multiply/add rounding.
            // Large phases/coordinates retain the original scalar formulas.
            f.guarded=std::numeric_limits<float>::is_iec559&&
                p.maximum_oscillation_phase<=64&&p.maximum_compression_phase<=64&&
                std::abs(f.phase)<=4096&&std::abs(f.compression_phase)<=4096&&
                p.maximum_x+std::abs(f.amplitude)+std::abs(f.move_x)<1048576&&
                p.maximum_y+std::abs(f.compression_amplitude)+std::abs(f.move_y)<1048576;
            if(f.guarded){
                f.cosine=std::cos(f.phase);f.sine=std::sin(f.phase);
                f.compression_cosine=std::cos(f.compression_phase);f.compression_sine=std::sin(f.compression_phase);
                constexpr float trig_rounding=32*std::numeric_limits<float>::epsilon();
                const float ex=trig_rounding+round_width(p.maximum_oscillation_phase+std::abs(f.phase));
                const float ey=trig_rounding+round_width(p.maximum_compression_phase+std::abs(f.compression_phase));
                f.error_x=std::nextafter(std::abs(f.amplitude)*ex+round_width(std::abs(f.amplitude))+
                    round_width(p.maximum_x+std::abs(f.amplitude))+
                    round_width(p.maximum_x+std::abs(f.amplitude)+std::abs(f.move_x)),std::numeric_limits<float>::infinity());
                f.error_y=std::nextafter(std::abs(f.compression_amplitude)*ey+round_width(std::abs(f.compression_amplitude))+
                    round_width(p.maximum_y+std::abs(f.compression_amplitude))+
                    round_width(p.maximum_y+std::abs(f.compression_amplitude)+std::abs(f.move_y)),std::numeric_limits<float>::infinity());
            }
        }
        return true;
    }
    static bool guarded_coordinate(float v,float error,float extent,uint32_t& result){
        // Frame preparation proves this truncation is in range. Subtracting
        // the same integer period preserves the scalar repeat arithmetic.
        const int32_t integer=int32_t(v);
        const int32_t floored=integer-(v<float(integer));
        const float local=v-float(floored),scaled=local*extent;
        const uint32_t pixel=uint32_t(scaled);
        const float padding=error*extent+8*std::numeric_limits<float>::epsilon()*extent;
        if(scaled-float(pixel)<=padding||float(pixel+1)-scaled<=padding)return false;
        result=pixel;return true;
    }
    template<bool Oscillate,bool Guarded>
    __attribute__((always_inline)) static inline uint8_t index_separable(const SeparableFrame& f,size_t i){
        const auto& uv=f.coordinates[i];float u=uv.x,v=uv.y;
        uint32_t x,y;
        if constexpr(Guarded){
            if constexpr(Oscillate){
                u+=f.amplitude*(uv.cos_x*f.cosine-uv.sin_x*f.sine);u+=f.move_x;
                if(!guarded_coordinate(u,f.error_x,f.float_width,x)){
                    u=uv.x;u+=f.amplitude*std::cos(f.frequency*uv.y+f.phase);u+=f.move_x;
                    x=coordinate_separable(u,f.width,f.float_width);
                }
                const auto& c=f.compression_trig[i];
                v+=f.compression_amplitude*(c.cosine*f.compression_cosine-c.sine*f.compression_sine);
            }else{
                u+=f.move_x;x=coordinate_separable(u,f.width,f.float_width);
                v+=f.compression_amplitude*(uv.cos_x*f.compression_cosine-uv.sin_x*f.compression_sine);
            }
            v+=f.move_y;
            if(!guarded_coordinate(v,f.error_y,f.float_height,y)){
                v=uv.y;v+=f.compression_amplitude*std::cos(f.compression_frequency*uv.y+f.compression_phase);v+=f.move_y;
                y=coordinate_separable(v,f.height,f.float_height);
            }
        }else{
            if constexpr(Oscillate)u+=f.amplitude*std::cos(f.frequency*uv.y+f.phase);
            v+=f.compression_amplitude*std::cos(f.compression_frequency*uv.y+f.compression_phase);
            u+=f.move_x;v+=f.move_y;
            x=coordinate_separable(u,f.width,f.float_width);y=coordinate_separable(v,f.height,f.float_height);
        }
        return f.indices[size_t(y)*f.width+x];
    }
    template<bool Mapped,bool Guarded>
    void compose_separable_pair(const SeparableFrame (&frames)[2],uint32_t* output)const{
        block_stats_={};
        if(Guarded&&!blocks_.empty()&&(!Mapped||mapped_unique_)){
            fallback_span_count_=0;
            for(auto root:block_roots_)compose_block<Mapped>(root,frames,output);
            compose_fallback_spans<Mapped>(frames,output);
            return;
        }
        const SeparableFrame a=frames[0],b=frames[1];
        const size_t stride=layers_[1].config.source.palette_size;
        const auto* colors=Mapped?mapped_extended_pair_colors_.data():extended_pair_colors_.data();
        for(size_t i=0;i<size_;++i){
            const auto ia=index_separable<true,Guarded>(a,i),ib=index_separable<false,Guarded>(b,i);
            const auto color=colors[size_t(ia)*stride+ib];
            if constexpr(Mapped)output[output_offsets_[i]]=color;
            else output[i]=color;
        }
    }
    static bool coordinate_range(Range range,uint32_t extent,float float_extent,uint32_t& lo,uint32_t& hi){
        if(!std::isfinite(range.lo)||!std::isfinite(range.hi)||range.lo>range.hi||
           range.lo<=-2097152||range.hi>=2097152)return false;
        const int32_t l=int32_t(range.lo),h=int32_t(range.hi);
        const int32_t period_l=l-(range.lo<float(l)),period_h=h-(range.hi<float(h));
        if(period_l!=period_h)return false;
        lo=coordinate_separable(range.lo,extent,float_extent);hi=coordinate_separable(range.hi,extent,float_extent);
        return lo<=hi;
    }
    bool constant_index(const BlockBounds& b,const SeparableFrame& f,size_t n,uint8_t& color)const{
        // Endpoint operations follow the identity's FLOAT expression order.
        // IEEE rounded add/subtract/multiply are monotonic on each chosen
        // sign branch, so these endpoint intervals contain every candidate
        // float. Expansion by the existing scalar-error bound is then stepped
        // outward by one representable float before texture quantization.
        Range u=b.u;
        if(n==0)u=add(u,scale(subtract(scale(b.oc,f.cosine),scale(b.os,f.sine)),f.amplitude));
        Range v=add(b.v,scale(subtract(scale(b.cc,f.compression_cosine),scale(b.cs,f.compression_sine)),f.compression_amplitude));
        u=expand(add(u,{f.move_x,f.move_x}),n==0?f.error_x:0);
        v=expand(add(v,{f.move_y,f.move_y}),f.error_y);
        uint32_t x0,x1,y0,y1;
        if(!coordinate_range(u,f.width,f.float_width,x0,x1)||!coordinate_range(v,f.height,f.float_height,y0,y1))return false;
        const auto& h=homogeneity_[homogeneity_index_[n]];
        color=h.indices[size_t(y0)*h.width+x0];
        const size_t plane=size_t(h.stride)*(h.height+1);const auto* sum=h.counts.data()+size_t(color)*plane;
        const uint32_t count=sum[size_t(y1+1)*h.stride+x1+1]+sum[size_t(y0)*h.stride+x0]-
            sum[size_t(y0)*h.stride+x1+1]-sum[size_t(y1+1)*h.stride+x0];
        return count==(x1-x0+1)*(y1-y0+1);
    }
    template<bool Mapped>
    void compose_block(uint32_t index,const SeparableFrame (&frames)[2],uint32_t* output)const{
        const auto& b=blocks_[index];++block_stats_.tested;uint8_t a,c;
        const auto* colors=Mapped?mapped_extended_pair_colors_.data():extended_pair_colors_.data();
        const size_t stride=layers_[1].config.source.palette_size;
        if(constant_index(b.layer[0],frames[0],0,a)&&constant_index(b.layer[1],frames[1],1,c)){
            ++block_stats_.filled;block_stats_.filled_pixels+=b.width*b.height;
            const uint32_t color=colors[size_t(a)*stride+c];
            for(uint32_t y=b.y;y<b.y+b.height;++y)for(uint32_t x=b.x;x<b.x+b.width;++x){
                const size_t i=size_t(y)*output_width_+x;
                if constexpr(Mapped)output[output_offsets_[i]]=color;else output[i]=color;
            }
        }else if(b.children){for(uint32_t child=0;child<b.children;++child)compose_block<Mapped>(b.first_child+child,frames,output);}
        else{
            block_stats_.fallback_pixels+=b.width*b.height;
            for(uint32_t y=b.y;y<b.y+b.height;++y){
                const uint32_t first=y*output_width_+b.x;
                fallback_spans_[fallback_span_count_++]={first,first+b.width};
            }
        }
    }
    template<bool Mapped>
    void compose_fallback_spans(const SeparableFrame (&frames)[2],uint32_t* output)const{
        const SeparableFrame first=frames[0],second=frames[1];
        const auto* colors=Mapped?mapped_extended_pair_colors_.data():extended_pair_colors_.data();
        const size_t stride=layers_[1].config.source.palette_size;
        const auto* offsets=output_offsets_;
        const auto* spans=fallback_spans_.data();
        const size_t count=fallback_span_count_;
        for(size_t span=0;span<count;++span){
            const uint32_t end=spans[span].end;
            for(uint32_t i=spans[span].first;i<end;++i){
                const auto color=colors[size_t(index_separable<true,true>(first,i))*stride+index_separable<false,true>(second,i)];
                if constexpr(Mapped)output[offsets[i]]=color;else output[i]=color;
            }
        }
    }

public:
    void clear(){fallback_spans_.clear();fallback_span_count_=0;layers_.clear();pair_colors_.clear();mapped_pair_colors_.clear();extended_pair_colors_.clear();mapped_extended_pair_colors_.clear();blocks_.clear();block_roots_.clear();homogeneity_.clear();block_stats_={};mapped_unique_=false;output_width_=0;output_offsets_=nullptr;mapped_output_count_=0;size_=0;}
    bool fused_pair()const{return !pair_colors_.empty();}
    bool separable_pair()const{return !extended_pair_colors_.empty();}
    bool bounded_pair()const{return fused_pair()&&layers_[0].bounded_x&&layers_[1].bounded_x;}
    size_t prepared_bytes()const{size_t bytes=layers_.size()*size_*sizeof(Pixel)+(pair_colors_.size()+mapped_pair_colors_.size()+extended_pair_colors_.size()+mapped_extended_pair_colors_.size())*sizeof(uint32_t);for(const auto& p:layers_)bytes+=p.compression_trig.size()*sizeof(Prepared::Trig);for(const auto& h:homogeneity_)bytes+=h.counts.size()*sizeof(uint32_t);return bytes+fallback_spans_.size()*sizeof(FallbackSpan)+blocks_.size()*sizeof(Block)+block_roots_.size()*sizeof(uint32_t);}
    // Peak reservation for a FRESH kernel. No pixels or palettes are read;
    // validated source dimensions/palette cardinalities are sufficient. The
    // optional block vectors reserve their complete capacities once above.
    static bool potential_separable_pair(const std::vector<Layer>& layers){
        if(layers.size()!=2)return false;
        for(const auto& l:layers)if((l.frequency_y!=0&&l.amplitude_y!=0)||(l.compression_frequency_x!=0&&l.compression_amplitude_x!=0)||(l.palette_shifting&&l.palette_frames)||!l.repeat)return false;
        return layers[0].frequency_x!=0&&layers[0].amplitude_x!=0&&(layers[1].frequency_x==0||layers[1].amplitude_x==0)&&layers[0].compression_frequency_y!=0&&layers[0].compression_amplitude_y!=0&&layers[1].compression_frequency_y!=0&&layers[1].compression_amplitude_y!=0&&layers[0].opacity==1;
    }
    static size_t preparation_upper_bound(const std::vector<Layer>& layers,uint32_t width,uint32_t height){
        const size_t area=size_t(width)*height;
        size_t bytes=layers.size()*(sizeof(Prepared)+area*sizeof(Pixel));
        if(layers.size()==2)bytes+=layers[0].source.palette_size*layers[1].source.palette_size*sizeof(uint32_t)*2;
        if(potential_separable_pair(layers)){
            bytes+=area*sizeof(Prepared::Trig);
            bool blocks=area<=262144;for(const auto& l:layers)blocks&=l.source.palette_size<=8&&uint64_t(l.source.width)*l.source.height<=262144;
            if(blocks){
                const size_t roots=size_t((width+7)/8)*((height+7)/8);
                bytes+=size_t((width+3)/4)*height*sizeof(FallbackSpan)+roots*5*sizeof(Block)+roots*sizeof(uint32_t)+2*sizeof(Homogeneity);
                for(const auto& l:layers)bytes+=size_t(l.source.width+1)*(l.source.height+1)*l.source.palette_size*sizeof(uint32_t);
            }
        }
        return bytes;
    }
    size_t allocated_bytes()const{
        size_t bytes=layers_.capacity()*sizeof(Prepared)+(pair_colors_.capacity()+mapped_pair_colors_.capacity()+extended_pair_colors_.capacity()+mapped_extended_pair_colors_.capacity())*sizeof(uint32_t);
        for(const auto& p:layers_)bytes+=p.pixels.capacity()*sizeof(Pixel)+p.compression_trig.capacity()*sizeof(Prepared::Trig);
        bytes+=homogeneity_.capacity()*sizeof(Homogeneity);for(const auto& h:homogeneity_)bytes+=h.counts.capacity()*sizeof(uint32_t);
        return bytes+fallback_spans_.capacity()*sizeof(FallbackSpan)+blocks_.capacity()*sizeof(Block)+block_roots_.capacity()*sizeof(uint32_t);
    }
    bool prepare(const std::vector<Layer>& layers,uint32_t width,uint32_t height,std::string& error,const PreparationControl* control=nullptr){
        clear();
        const auto fail=[&](const char* text){error=text;clear();return false;};
        if(layers.empty()||layers.size()>8||!width||width>1024||!height||height>1024)
            return fail("Invalid background kernel dimensions/layers");
        size_=size_t(width)*height;layers_.reserve(layers.size());
        for(const auto& layer:layers){
            if(control&&control->stopped())return fail("Background preparation cancelled");
            const auto& src=layer.source;
            const float values[]={layer.width,layer.height,layer.opacity,layer.effect,layer.effect_scale,
                layer.barrel_x,layer.barrel_y,layer.amplitude_x,layer.amplitude_y,
                layer.frequency_x,layer.frequency_y,layer.speed_x,layer.speed_y,
                layer.move_x,layer.move_y,layer.ping_pong_speed_x,layer.ping_pong_speed_y,
                layer.compression_amplitude_x,layer.compression_amplitude_y,
                layer.compression_frequency_x,layer.compression_frequency_y,
                layer.compression_speed_x,layer.compression_speed_y,layer.palette_speed};
            for(float value:values)if(!std::isfinite(value))return fail("Non-finite background coefficient");
            if(!src.width||src.width>1024||!src.height||src.height>1024||!src.pixels||!src.palette||
               !src.palette_size||src.palette_size>256||layer.width<=0||layer.height<=0||layer.opacity<0||layer.opacity>1)
                return fail("Invalid background kernel source/coefficient");
            for(size_t i=0;i<size_t(src.width)*src.height;++i)
                if(src.pixels[i]>=src.palette_size)return fail("Background kernel palette index out of bounds");
            const auto& pal=layer.palette_source;
            if(layer.palette_shifting){
                // An omitted uniform defaults to zero. Only an explicit,
                // independently observed backend row can resolve that case.
                if(layer.palette_frames>uint32_t(std::numeric_limits<int32_t>::max())||
                   (!layer.palette_frames&&layer.palette_fixed_row>=pal.height)||
                   (layer.palette_frames&&layer.palette_fixed_row!=UINT32_MAX))
                    return fail("Background palette animation has invalid frame divisor");
                if(!pal.width||pal.width>1024||!pal.height||pal.height>1024||!pal.pixels||!pal.palette||
                   !pal.palette_size||pal.palette_size>256)
                    return fail("Invalid background palette texture");
                for(size_t i=0;i<size_t(pal.width)*pal.height;++i)
                    if(pal.pixels[i]>=pal.palette_size)return fail("Background palette texture index out of bounds");
            }else if(pal.width||pal.height||pal.pixels||pal.palette||pal.palette_size||layer.palette_frames||
                     layer.palette_speed!=0||layer.palette_fixed_row!=UINT32_MAX)
                return fail("Disabled background palette has active parameters");
            Prepared p;p.config=layer;
            p.extended=layer.move_x!=0||layer.move_y!=0||layer.ping_pong_speed_x!=0||layer.ping_pong_speed_y!=0||
                layer.compression_amplitude_x!=0||layer.compression_amplitude_y!=0||
                layer.compression_frequency_x!=0||layer.compression_frequency_y!=0||
                layer.compression_speed_x!=0||layer.compression_speed_y!=0||layer.palette_shifting;
            p.static_y=!p.extended&&(layer.frequency_y==0||layer.amplitude_y==0);
            p.oscillate_x=layer.frequency_x!=0&&layer.amplitude_x!=0;
            p.compress_y=layer.compression_frequency_y!=0&&layer.compression_amplitude_y!=0;
            p.separable=p.extended&&(layer.frequency_y==0||layer.amplitude_y==0)&&
                (layer.compression_frequency_x==0||layer.compression_amplitude_x==0)&&
                (!layer.palette_shifting||!layer.palette_frames);
            p.opaque=true;
            for(size_t i=0;i<src.palette_size;++i)if(src.palette[i]>>24!=255)p.opaque=false;
            p.pixels.resize(size_);
            for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x){
                if(!x&&control&&control->stopped())return fail("Background preparation cancelled");
                auto& uv=p.pixels[size_t(y)*width+x];
                float u=(x+0.5f)/layer.width,v=(y+0.5f)/layer.height;
                if(layer.barrel){
                    const float px=2*u-layer.barrel_x,py=2*v-layer.barrel_y;
                    const float d=std::sqrt(px*px+py*py),z2=1+d*d*layer.effect;
                    if(z2<0||!std::isfinite(z2))return fail("Background distortion has invalid square root");
                    const float radius=std::atan2(d,std::sqrt(z2))/3.14159f*layer.effect_scale;
                    const float phi=std::atan2(py,px);
                    u=radius*std::cos(phi)+0.5f;v=radius*std::sin(phi)+0.5f;
                }
                uv.x=u;uv.cos_x=std::cos(layer.frequency_x*v);uv.sin_x=std::sin(layer.frequency_x*v);
                if(!std::isfinite(u)||!std::isfinite(v)||!std::isfinite(uv.cos_x)||!std::isfinite(uv.sin_x))
                    return fail("Invalid background sampling coordinate");
                if(!std::isfinite(std::abs(u)+2*std::abs(layer.amplitude_x)))p.finite_x=false;
                // cos(TIME) and sin(TIME) are in [-1,1]. Bound each original
                // multiply/subtract without reassociating the runtime formula.
                // Outward rounding deliberately makes the envelope wider.
                const float inf=std::numeric_limits<float>::infinity();
                const float envelope=p.oscillate_x?std::nextafter(std::abs(layer.amplitude_x)*
                    std::nextafter(std::abs(uv.cos_x)+std::abs(uv.sin_x),inf),inf):0;
                const float lower=std::nextafter(u-envelope,-inf),upper=std::nextafter(u+envelope,inf);
                if(lower<0||!std::isfinite(upper)||upper>=1||std::nextafter(upper*src.width,inf)>=src.width)
                    p.bounded_x=false;
                if(p.static_y)uv.row=coordinate(v,src.height,layer.repeat)*src.width;
                else uv.y=v;
                if(p.extended){
                    // These products are the same operations the scalar
                    // shader performs before adding TIME*speed. Caching them
                    // does not use a trig identity or reassociate arithmetic.
                    uv.cos_x=layer.frequency_x*v;
                    uv.sin_x=layer.compression_frequency_y*v;
                    if(!std::isfinite(uv.cos_x)||!std::isfinite(uv.sin_x))p.separable=false;
                    p.maximum_x=std::max(p.maximum_x,std::abs(u));p.maximum_y=std::max(p.maximum_y,std::abs(v));
                    p.maximum_oscillation_phase=std::max(p.maximum_oscillation_phase,std::abs(uv.cos_x));
                    p.maximum_compression_phase=std::max(p.maximum_compression_phase,std::abs(uv.sin_x));
                }
            }
            layers_.push_back(std::move(p));
        }
        // First layer completely replaces clear_color. The next layer has only
        // palette_size squared possible blends, evaluated in the SAME float
        // order as the reference. No quantized time or modified UVs are used.
        if(layers_.size()==2&&layers_[0].static_y&&layers_[1].static_y&&
           layers_[0].opaque&&layers_[0].config.opacity==1){
            const auto& a=layers_[0].config.source;const auto& b=layers_[1].config.source;
            pair_colors_.resize(a.palette_size*b.palette_size);
            mapped_pair_colors_.resize(pair_colors_.size());
            for(size_t i=0;i<a.palette_size;++i)for(size_t j=0;j<b.palette_size;++j){
                const auto color=blend(b.palette[j],a.palette[i],layers_[1].config.opacity);
                pair_colors_[i*b.palette_size+j]=color;
                mapped_pair_colors_[i*b.palette_size+j]=((color&0xffu)<<24)|((color&0xff00u)<<8)|((color>>8)&0xff00u)|(color>>24);
            }
        }
        if(layers_.size()==2&&layers_[0].separable&&layers_[1].separable&&
           layers_[0].oscillate_x&&!layers_[1].oscillate_x&&layers_[0].compress_y&&layers_[1].compress_y&&
           layers_[0].config.repeat&&layers_[1].config.repeat&&
           layers_[0].opaque&&layers_[0].config.opacity==1){
            const auto mapped_color=[](const Layer& l,size_t index){
                uint32_t color=l.source.palette[index];
                if(l.palette_shifting){
                    const auto& pal=l.palette_source;
                    const auto x=coordinate(float(color&255u)/255.0f,pal.width,false);
                    const auto mapped=pal.palette[pal.pixels[size_t(l.palette_fixed_row)*pal.width+x]];
                    color=(mapped&0xffffffu)|(color&0xff000000u);
                }
                return color;
            };
            const auto& a=layers_[0].config;const auto& b=layers_[1].config;
            extended_pair_colors_.resize(a.source.palette_size*b.source.palette_size);
            mapped_extended_pair_colors_.resize(extended_pair_colors_.size());
            for(size_t i=0;i<a.source.palette_size;++i)for(size_t j=0;j<b.source.palette_size;++j){
                const auto color=blend(mapped_color(b,j),mapped_color(a,i),b.opacity);
                extended_pair_colors_[i*b.source.palette_size+j]=color;
                mapped_extended_pair_colors_[i*b.source.palette_size+j]=((color&0xffu)<<24)|((color&0xff00u)<<8)|((color>>8)&0xff00u)|(color>>24);
            }
            for(size_t n=0;n<2;++n){
                auto& p=layers_[n];if(n==0)p.compression_trig.resize(size_);
                for(size_t i=0;i<size_;++i){
                    auto& uv=p.pixels[i];const float oscillation=uv.cos_x,compression=uv.sin_x;
                    if(n==0){p.compression_trig[i]={std::cos(compression),std::sin(compression)};
                        uv.cos_x=std::cos(oscillation);uv.sin_x=std::sin(oscillation);
                    }else{uv.cos_x=std::cos(compression);uv.sin_x=std::sin(compression);}
                }
            }
        }
        prepare_blocks(width,height,control);
        if(control&&control->stopped())return fail("Background preparation cancelled");
        return true;
    }
    // Borrow an immutable layout, validating its length and bounds once. Its
    // storage must outlive use and must not overlap the output. clear/prepare,
    // or any failed layout preparation, invalidate the previous layout.
    bool prepare_mapped_output(const uint32_t* offsets,size_t offset_count,size_t output_count,std::string& error){
        output_offsets_=nullptr;mapped_output_count_=0;mapped_unique_=false;
        if(layers_.empty()||!offsets||offset_count!=size_||!output_count){
            error="Invalid background output layout";return false;
        }
        for(size_t i=0;i<offset_count;++i)if(offsets[i]>=output_count){
            error="Background output offset out of bounds";return false;
        }
        // Blocks change visitation order. Preserve the original last-index
        // wins rule for duplicate mapped offsets by using the old pixel loop.
        // Cap this temporary layout check; larger layouts still work normally.
        if(output_count<=1024*1024){
            std::vector<uint8_t> seen(output_count);mapped_unique_=true;
            for(size_t i=0;i<offset_count;++i){if(seen[offsets[i]]){mapped_unique_=false;break;}seen[offsets[i]]=1;}
        }
        output_offsets_=offsets;mapped_output_count_=output_count;return true;
    }
    // Exact fused-pair output in GPU byte order. Returns false BEFORE writing
    // anything for unsupported layers, absent layout, insufficient capacity or
    // invalid TIME/phases. The caller may then use compose + its normal upload.
    // The caller owns GPU synchronization and flush; this kernel does no GPU IO.
    bool compose_mapped(float time,uint32_t,uint32_t* output,size_t output_count)const{
        if(!std::isfinite(time)||!output_offsets_||!output||output_count<mapped_output_count_)return false;
        if(!extended_pair_colors_.empty()){
            SeparableFrame frames[2];if(!prepare_separable_frame(time,frames))return false;
            if(frames[0].guarded&&frames[1].guarded)compose_separable_pair<true,true>(frames,output);
            else compose_separable_pair<true,false>(frames,output);
            return true;
        }
        if(pair_colors_.empty())return false;
        float cosine[2],sine[2];
        for(size_t i=0;i<2;++i){
            const auto& p=layers_[i];const auto& l=p.config;
            if(p.oscillate_x&&!p.finite_x)return false;
            const float phase=time*l.speed_x,phase_y=time*l.speed_y;
            if(!std::isfinite(phase)||!std::isfinite(phase_y))return false;
            cosine[i]=std::cos(phase);sine[i]=std::sin(phase);
        }
        compose_pair_ready<true>(cosine,sine,output);return true;
    }
    // Output must address width*height words. A failed compose may partially
    // write it, like the reference; callers must not upload failed output.
    bool compose(float time,uint32_t clear_color,uint32_t* output)const{
        if(!std::isfinite(time)||layers_.empty()||!output)return false;
        if(!extended_pair_colors_.empty()){
            SeparableFrame frames[2];
            if(prepare_separable_frame(time,frames)){
                if(frames[0].guarded&&frames[1].guarded)compose_separable_pair<false,true>(frames,output);
                else compose_separable_pair<false,false>(frames,output);
                return true;
            }
        }
        float cosine[8],sine[8],phase_y[8];
        for(size_t i=0;i<layers_.size();++i){
            const auto& l=layers_[i].config;
            const float phase=time*l.speed_x;phase_y[i]=time*l.speed_y;
            if(!std::isfinite(phase)||!std::isfinite(phase_y[i]))return false;
            cosine[i]=std::cos(phase);sine[i]=std::sin(phase);
        }
        if(!pair_colors_.empty()){
            // Preparation proves finite UVs, but coefficients may still be
            // enormous. Avoid unchecked float-to-integer conversion on overflow.
            for(const auto& p:layers_)
                if(p.oscillate_x&&!p.finite_x)return compose_generic(time,clear_color,output,cosine,sine,phase_y);
            compose_pair_ready(cosine,sine,output);
            return true;
        }
        return compose_generic(time,clear_color,output,cosine,sine,phase_y);
    }
private:
    // Literal scalar order of default_shader.tres for the reviewed extension:
    // barrel (prepared), x/y oscillation, x/y compression, x/y movement, source
    // sample, palette RGB sample, opacity. Do not reuse the Lamp trig identity
    // here: reassociation can change nearest-neighbor texel selection. No
    // interlacing, amplitude/time pingpong modulation or alternate blend modes
    // are represented by this schema; the content compiler must reject those.
    bool compose_extended(const Prepared& p,float time,uint32_t* output)const{
        const auto& l=p.config;const auto& src=l.source;const auto& pal=l.palette_source;
        const bool oscillate_y=l.frequency_y!=0&&l.amplitude_y!=0;
        const bool compress_x=l.compression_frequency_x!=0&&l.compression_amplitude_x!=0;
        const bool compress_y=l.compression_frequency_y!=0&&l.compression_amplitude_y!=0;
        const float phase_x=time*l.speed_x,phase_y=time*l.speed_y;
        const float compression_phase_x=compress_x?time*l.compression_speed_x:0;
        const float compression_phase_y=compress_y?time*l.compression_speed_y:0;
        const float move_x=l.ping_pong_speed_x!=0?l.move_x*std::cos(l.ping_pong_speed_x*time):time*l.move_x/0.5f;
        const float move_y=l.ping_pong_speed_y!=0?l.move_y*std::cos(l.ping_pong_speed_y*time):time*l.move_y/0.5f;
        if(!std::isfinite(compression_phase_x)||!std::isfinite(compression_phase_y)||
           !std::isfinite(move_x)||!std::isfinite(move_y))return false;
        uint32_t palette_row=0;
        if(l.palette_shifting){
            if(!l.palette_frames)palette_row=l.palette_fixed_row*pal.width;
            else{
                const float palette_position=-time*l.palette_speed*1.0f/float(l.palette_frames);
                if(!std::isfinite(palette_position))return false;
                const float palette_v=palette_position-std::floor(palette_position);
                palette_row=coordinate(palette_v,pal.height,false)*pal.width;
            }
        }
        for(size_t i=0;i<size_;++i){
            const auto& uv=p.pixels[i];float u=uv.x,v=uv.y;
            if(p.oscillate_x)u+=l.amplitude_x*std::cos(l.frequency_x*v+phase_x);
            if(oscillate_y)v+=l.amplitude_y*std::cos(l.frequency_y*u+phase_y);
            if(compress_x)u+=l.compression_amplitude_x*std::cos(l.compression_frequency_x*u+compression_phase_x);
            if(compress_y)v+=l.compression_amplitude_y*std::cos(l.compression_frequency_y*v+compression_phase_y);
            u+=move_x;v+=move_y;
            if(!std::isfinite(u)||!std::isfinite(v))return false;
            const auto x=coordinate(u,src.width,l.repeat),y=coordinate(v,src.height,l.repeat);
            uint32_t color=src.palette[src.pixels[size_t(y)*src.width+x]];
            if(l.palette_shifting){
                const auto px=coordinate(float(color&255u)/255.0f,pal.width,false);
                const auto mapped=pal.palette[pal.pixels[palette_row+px]];
                color=(mapped&0xffffffu)|(color&0xff000000u);
            }
            output[i]=blend(color,output[i],l.opacity);
        }
        return true;
    }
    bool compose_generic(float time,uint32_t clear_color,uint32_t* output,const float* cosine,const float* sine,const float* phase_y)const{
        std::fill(output,output+size_,clear_color);
        for(size_t n=0;n<layers_.size();++n){
            const auto& p=layers_[n];const auto& l=p.config;const auto& src=l.source;
            if(p.extended){if(!compose_extended(p,time,output))return false;continue;}
            for(size_t i=0;i<size_;++i){
                const auto& uv=p.pixels[i];float u=uv.x;
                if(p.oscillate_x)u+=l.amplitude_x*(uv.cos_x*cosine[n]-uv.sin_x*sine[n]);
                if(!std::isfinite(u))return false;
                uint32_t row;
                if(p.static_y)row=uv.row;
                else{
                    const float v=uv.y+l.amplitude_y*std::cos(l.frequency_y*u+phase_y[n]);
                    if(!std::isfinite(v))return false;
                    row=coordinate(v,src.height,l.repeat)*src.width;
                }
                const auto sx=coordinate(u,src.width,l.repeat);
                output[i]=blend(src.palette[src.pixels[row+sx]],output[i],l.opacity);
            }
        }
        return true;
    }
};
}

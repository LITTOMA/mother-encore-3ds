#pragma once
#include "encore/background_kernel.hpp"
#include <memory>
#include <new>
namespace encore {
// Exact sparse output for two repeated, non-barrel layers with row-constant
// X oscillation and movement. No content, clock or renderer dependencies.
class RowLinearBackgroundKernel {
    using Layer=BackgroundKernel::Layer;
    struct Prepared {Layer layer;std::unique_ptr<float[]> u,v;std::unique_ptr<uint16_t[]> forward;double division_error=0;};
    Prepared layers_[2];
    std::unique_ptr<uint32_t[]> colors_;
    uint32_t width_=0,height_=0;
    size_t bytes_=0;
    mutable uint64_t evaluations_=0;
    static uint32_t blend(uint32_t color,uint32_t base,float opacity){
        if(opacity==1&&(color>>24)==255)return color;
        if(opacity==0.5f&&(color>>24)==255&&(base>>24)==255){
            const uint32_t difference=color^base;
            return (color&base)+((difference&0xfefefefeu)>>1)+(difference&0x01010101u);
        }
        const float alpha=float(color>>24)/255*opacity;uint32_t mixed=0;
        for(unsigned shift=0;shift<24;shift+=8){
            const float value=float((color>>shift)&255)*alpha+float((base>>shift)&255)*(1-alpha);
            mixed|=uint32_t(value+0.5f)<<shift;
        }
        return mixed|(uint32_t(255*alpha+float(base>>24)*(1-alpha)+0.5f)<<24);
    }
    static uint32_t coordinate(float v,uint32_t extent){
        if(v<0||v>=1)v-=std::floor(v);
        return std::min(uint32_t(std::clamp(v,0.0f,1.0f)*extent),extent-1);
    }
    struct Row {const Prepared* prepared=nullptr;float oscillation=0,move=0;uint32_t offset=0;bool oscillate=false;};
    // The ordered key includes repeat periods. Searching just a wrapped index
    // could skip a complete texture period. Every operation matches scalar UV.
    int64_t key(const Row& row,uint32_t x)const{
        ++evaluations_;float u=row.prepared->u[x];
        if(row.oscillate)u+=row.oscillation;
        u+=row.move;
        const float period=std::floor(u);
        return int64_t(period)*row.prepared->layer.source.width+coordinate(u,row.prepared->layer.source.width);
    }
    struct Run {uint32_t end=0;uint8_t color=0;};
    Run run(const Row& row,uint32_t x)const{
        const auto& src=row.prepared->layer.source;const int64_t at=key(row,x);
        const uint32_t tx=uint32_t((at%src.width+src.width)%src.width),i=row.offset+tx;
        const uint32_t length=row.prepared->forward[i];
        if(length==src.width)return {width_,src.pixels[i]};
        const int64_t boundary=at+length;
        // The source interval is homogeneous; positive division and additions
        // are monotone in x even when float rounding produces repeated values.
        // Locate the first scalar sample outside it, including skipped texels.
        uint32_t lo=x+1,hi=width_;
        while(lo<hi){const uint32_t mid=lo+(hi-lo)/2;if(key(row,mid)<boundary)lo=mid+1;else hi=mid;}
        return {lo,src.pixels[i]};
    }
public:
    void clear(){for(auto& p:layers_)p=Prepared{};colors_.reset();width_=height_=0;bytes_=0;evaluations_=0;}
    bool ready()const{return bool(colors_);}
    size_t prepared_bytes()const{return bytes_;}
    uint64_t sample_evaluations()const{return evaluations_;}
    static size_t preparation_upper_bound(const std::vector<Layer>& layers,uint32_t width,uint32_t height){
        if(layers.size()!=2)return 0;size_t bytes=0;
        for(const auto& l:layers){if(l.barrel||!l.repeat||l.palette_shifting||(l.frequency_y!=0&&l.amplitude_y!=0)||(l.compression_frequency_x!=0&&l.compression_amplitude_x!=0)||(l.compression_frequency_y!=0&&l.compression_amplitude_y!=0)||uint64_t(l.source.width)*l.source.height>65536)return 0;bytes+=size_t(width+height)*sizeof(float)+size_t(l.source.width)*l.source.height*sizeof(uint16_t);}
        return bytes+layers[0].source.palette_size*layers[1].source.palette_size*sizeof(uint32_t);
    }
    bool prepare(const std::vector<Layer>& layers,uint32_t width,uint32_t height,const PreparationControl* control=nullptr){
        clear();
        if(layers.size()!=2||!width||width>1024||!height||height>1024)return false;
        for(const auto& l:layers){const auto& s=l.source;
            if(l.barrel||!l.repeat||l.palette_shifting||l.palette_source.pixels||l.palette_frames||
               l.palette_speed!=0||l.palette_fixed_row!=UINT32_MAX||
               (l.frequency_y!=0&&l.amplitude_y!=0)||
               (l.compression_frequency_x!=0&&l.compression_amplitude_x!=0)||
               (l.compression_frequency_y!=0&&l.compression_amplitude_y!=0)||
               !(l.move_x!=0||l.move_y!=0||l.ping_pong_speed_x!=0||l.ping_pong_speed_y!=0)||
               !s.width||s.width>1024||!s.height||s.height>1024||uint64_t(s.width)*s.height>65536||
               !s.pixels||!s.palette||!s.palette_size||s.palette_size>256||l.width<=0||l.height<=0||l.opacity<0||l.opacity>1)return false;
            const float values[]={l.width,l.height,l.opacity,l.amplitude_x,l.frequency_x,l.speed_x,l.speed_y,l.move_x,l.move_y,l.ping_pong_speed_x,l.ping_pong_speed_y};
            for(float value:values)if(!std::isfinite(value))return false;
            for(size_t i=0;i<size_t(s.width)*s.height;++i)if(s.pixels[i]>=s.palette_size)return false;
        }
        if(layers[0].opacity!=1)return false;
        for(size_t i=0;i<layers[0].source.palette_size;++i)if(layers[0].source.palette[i]>>24!=255)return false;
        width_=width;height_=height;
        for(size_t n=0;n<2;++n){auto& p=layers_[n];p.layer=layers[n];const auto& s=p.layer.source;
            p.u.reset(new(std::nothrow) float[width]);p.v.reset(new(std::nothrow) float[height]);p.forward.reset(new(std::nothrow) uint16_t[size_t(s.width)*s.height]);
            if(!p.u||!p.v||!p.forward){clear();return false;}
            for(uint32_t x=0;x<width;++x){p.u[x]=(x+0.5f)/p.layer.width;p.division_error=std::max(p.division_error,std::abs(double(p.u[x])*s.width-(double(x)+0.5)));if(!std::isfinite(p.u[x])||p.u[x]>=1048576){clear();return false;}}
            for(uint32_t y=0;y<height;++y){p.v[y]=(y+0.5f)/p.layer.height;if(!std::isfinite(p.v[y])){clear();return false;}}
            for(uint32_t y=0;y<s.height;++y){if(control&&control->stopped()){clear();return false;}const auto* row=s.pixels+size_t(y)*s.width;uint32_t length=1;
                for(uint32_t i=2*s.width;i-->0;){const uint32_t x=i%s.width;if(i+1<2*s.width&&row[x]==row[(x+1)%s.width])length=std::min(length+1,s.width);else length=1;
                    if(i<s.width)p.forward[size_t(y)*s.width+x]=uint16_t(length);
                }
            }
            bytes_+=size_t(width+height)*sizeof(float)+size_t(s.width)*s.height*sizeof(uint16_t);
        }
        const auto& a=layers[0].source;const auto& b=layers[1].source;
        colors_.reset(new(std::nothrow) uint32_t[a.palette_size*b.palette_size]);if(!colors_){clear();return false;}
        for(size_t i=0;i<a.palette_size;++i)for(size_t j=0;j<b.palette_size;++j)colors_[i*b.palette_size+j]=blend(b.palette[j],a.palette[i],layers[1].opacity);
        bytes_+=a.palette_size*b.palette_size*sizeof(uint32_t);return true;
    }
    struct CertifiedRow {int32_t shift[2]{};uint16_t source_y[2]{};};
    // CPU proof only. A successful row preserves every scalar source index;
    // the caller separately qualifies its GPU raster/sampler backend.
    bool certify_integer_rows(float time,CertifiedRow* rows,uint8_t* certified,uint32_t& accepted)const{
        accepted=0;if(!rows||!certified||!ready())return false;
        std::fill_n(certified,height_,uint8_t(0));
        if(!std::isfinite(time)||!std::numeric_limits<float>::is_iec559)return false;
        const auto up=[](double x){return std::nextafter(x,std::numeric_limits<double>::infinity());};
        const auto down=[](double x){return std::nextafter(x,-std::numeric_limits<double>::infinity());};
        constexpr double eps=std::numeric_limits<float>::epsilon();
        constexpr double tiny=std::numeric_limits<float>::denorm_min();
        float phase[2],mx[2],my[2];
        for(size_t n=0;n<2;++n){const auto& l=layers_[n].layer;
            if(l.width!=float(l.source.width))return false;
            phase[n]=time*l.speed_x;
            mx[n]=l.ping_pong_speed_x!=0?l.move_x*std::cos(l.ping_pong_speed_x*time):time*l.move_x/0.5f;
            my[n]=l.ping_pong_speed_y!=0?l.move_y*std::cos(l.ping_pong_speed_y*time):time*l.move_y/0.5f;
            if(!std::isfinite(phase[n])||!std::isfinite(time*l.speed_y)||!std::isfinite(mx[n])||!std::isfinite(my[n]))return false;
        }
        for(uint32_t y=0;y<height_;++y){bool valid=true;
            for(size_t n=0;n<2;++n){const auto& p=layers_[n];const auto& l=p.layer;const double w=l.source.width;
                const bool oscillate=l.frequency_x!=0&&l.amplitude_x!=0;
                const float o=oscillate?l.amplitude_x*std::cos(l.frequency_x*p.v[y]+phase[n]):0;
                const float v=p.v[y]+my[n];
                if(!std::isfinite(o)||!std::isfinite(v)){valid=false;continue;}
                const double first=up(double(p.u[width_-1])+std::abs(double(o)));
                const double e1=oscillate?up(up(eps*first)+tiny):0;
                const double second=up(up(first+e1)+std::abs(double(mx[n])));
                if(second>=1048576){valid=false;continue;}
                const double e2=up(up(eps*second)+tiny);
                // Exact double products bound the cached division error. Every
                // bound operation rounds outward. Float repeat subtraction
                // and final multiplication each add at most one full epsilon.
                const double error=up(up(p.division_error)+up(w*up(up(e1+e2)+up(2*eps))));
                const double sum_lo=down(double(o)+double(mx[n])),sum_hi=up(double(o)+double(mx[n]));
                const double center_lo=down(down(sum_lo*w)+0.5),center_hi=up(up(sum_hi*w)+0.5);
                const double lo=std::floor(down(center_lo-error)),hi=std::floor(up(center_hi+error));
                if(lo!=hi||lo<double(INT32_MIN)||hi>double(INT32_MAX)){valid=false;continue;}
                rows[y].shift[n]=int32_t(lo);rows[y].source_y[n]=uint16_t(coordinate(v,l.source.height));
            }
            if(valid){certified[y]=1;++accepted;}
        }
        return true;
    }
    template<class Span>bool generate_spans(float time,Span* output,size_t capacity,size_t& count,const uint8_t* skip_rows=nullptr)const{
        count=0;evaluations_=0;if(!ready()||!std::isfinite(time)||!output||!capacity)return false;
        float phase[2],mx[2],my[2];
        for(size_t n=0;n<2;++n){const auto& l=layers_[n].layer;phase[n]=time*l.speed_x;
            mx[n]=l.ping_pong_speed_x!=0?l.move_x*std::cos(l.ping_pong_speed_x*time):time*l.move_x/0.5f;
            my[n]=l.ping_pong_speed_y!=0?l.move_y*std::cos(l.ping_pong_speed_y*time):time*l.move_y/0.5f;
            if(!std::isfinite(phase[n])||!std::isfinite(time*l.speed_y)||!std::isfinite(mx[n])||!std::isfinite(my[n]))return false;
        }
        for(uint32_t y=0;y<height_;++y){if(skip_rows&&skip_rows[y])continue;Row rows[2];Run runs[2];
            for(size_t n=0;n<2;++n){const auto& p=layers_[n];const auto& l=p.layer;auto& row=rows[n];row.prepared=&p;row.move=mx[n];
                row.oscillate=l.frequency_x!=0&&l.amplitude_x!=0;
                if(row.oscillate)row.oscillation=l.amplitude_x*std::cos(l.frequency_x*p.v[y]+phase[n]);
                const float v=p.v[y]+my[n];
                // Before conversions, bound exact endpoints and row coordinates.
                float first=p.u[0],last=p.u[width_-1];if(row.oscillate){first+=row.oscillation;last+=row.oscillation;}first+=mx[n];last+=mx[n];
                if(!std::isfinite(v)||!std::isfinite(first)||!std::isfinite(last)||std::abs(first)>=1048576||std::abs(last)>=1048576){count=0;return false;}
                row.offset=coordinate(v,l.source.height)*l.source.width;runs[n]=run(row,0);
            }
            uint32_t x=0;while(x<width_){const uint32_t end=std::min(runs[0].end,runs[1].end);
                const uint32_t color=colors_[size_t(runs[0].color)*layers_[1].layer.source.palette_size+runs[1].color];
                if(count&&output[count-1].y==y&&output[count-1].color==color)output[count-1].width=uint16_t(end-output[count-1].x);
                else{if(count==capacity){count=0;return false;}auto& span=output[count++];span.x=uint16_t(x);span.y=uint16_t(y);span.width=uint16_t(end-x);span.color=color;}
                x=end;if(x<width_)for(size_t n=0;n<2;++n)if(runs[n].end==x)runs[n]=run(rows[n],x);
            }
        }
        return true;
    }
};
}

#pragma once
#include "encore/background_kernel.hpp"
#include <memory>
#include <new>
#include <utility>
namespace encore {
// Exact optional CPU specialization. Requires BackgroundKernel's narrow friend
// declaration; ownership and fallback remain with the existing public contract.
// No image, palette, shader tuning, frame clock or game content is embedded.
class RegionBackgroundKernel : public BackgroundKernel {
    struct Slope {Range u,v,oc,os,cc,cs;};
    struct Region {int16_t alo=0,ahi=0,blo=0,bhi=0;};
    std::unique_ptr<Region[]> regions_[2];
    size_t region_counts_[2]{};
    uint32_t row_factor_[2]{1,1};
    bool regions_ready_=false;
    std::unique_ptr<Slope[]> slopes_[2];
    mutable std::unique_ptr<uint8_t[]> row_[2];
    struct SpanRun {uint16_t end=0;uint8_t color=0;};
    mutable std::unique_ptr<SpanRun[]> span_row_[2];
    mutable uint32_t span_row_count_[2]{};
    struct Reciprocal {float a0=0,a1=0,b0=0,b1=0;};
    struct TrigBox {Range cosine,sine;};
    std::unique_ptr<Reciprocal[]> certificates_[2];
    size_t certificate_bytes_=0;
    mutable bool certificate_used_=false;
    static constexpr size_t certificate_cap=(size_t(36)*1024*1024)/10;
    static TrigBox trig_box(unsigned i){
        // Mechanism-only octant envelopes. Each runtime uniform must pass an
        // explicit membership check; no libm/angle identity is assumed here.
        const float half=std::sqrt(0.5f),lo=outward(half,false),hi=outward(half,true);
        switch(i){
        case 0:return {{lo,1},{0,hi}};case 1:return {{0,hi},{lo,1}};
        case 2:return {{-hi,0},{lo,1}};case 3:return {{-1,-lo},{0,hi}};
        case 4:return {{-1,-lo},{-hi,0}};case 5:return {{-hi,0},{-1,-lo}};
        case 6:return {{0,hi},{-1,-lo}};default:return {{lo,1},{-hi,0}};
        }
    }
    static int trig_bucket(float c,float s){
        unsigned i;
        if(s>=0)i=c>=0?(c>=s?0:1):(-c<=s?2:3);
        else i=c<0?(-c>=-s?4:5):(c<=-s?6:7);
        const auto box=trig_box(i);
        return c>=box.cosine.lo&&c<=box.cosine.hi&&s>=box.sine.lo&&s<=box.sine.hi?int(i):-1;
    }
    static Range enclose(double lo,double hi){return {outward(float(lo),false),outward(float(hi),true)};}
    static Range product(Range a,Range b){
        const double p[]={double(a.lo)*b.lo,double(a.lo)*b.hi,double(a.hi)*b.lo,double(a.hi)*b.hi};
        return enclose(*std::min_element(p,p+4),*std::max_element(p,p+4));
    }
    static Range plus(Range a,Range b){return enclose(double(a.lo)+b.lo,double(a.hi)+b.hi);}
    static Range minus(Range a,Range b){return enclose(double(a.lo)-b.hi,double(a.hi)-b.lo);}
    static Reciprocal reciprocal(Range a,Range b){return {a.lo<0?outward(-1/a.lo,false):0,a.hi>0?outward(1/a.hi,false):0,
        b.lo<0?outward(-1/b.lo,false):0,b.hi>0?outward(1/b.hi,false):0};}

    uint32_t width_=0,height_=0,chunks_=0;
    // Mechanism/memory limits, independent of source content or game tuning.
    static constexpr uint32_t chunk_size=32;
    static constexpr uint32_t maximum_source_pixels=65536,maximum_components=4096;
public:
    enum class PreparationStatus { Inactive,Ready,Unsupported,ResourceLimit,AllocationFailure };
    struct RegionStats {uint64_t samples=0,skipped=0;};
private:
    PreparationStatus preparation_status_=PreparationStatus::Inactive;
    mutable RegionStats stats_{};
    template<class T>bool allocate(std::unique_ptr<T[]>& target,size_t count){
        target.reset(new(std::nothrow) T[count]);
        if(!target){preparation_status_=PreparationStatus::AllocationFailure;return false;}
        return true;
    }
    void clear_regions(){
        regions_ready_=false;width_=height_=chunks_=0;stats_={};certificate_bytes_=0;certificate_used_=false;for(auto& c:certificates_)c.reset();
        for(size_t n=0;n<2;++n){regions_[n].reset();slopes_[n].reset();row_[n].reset();span_row_[n].reset();region_counts_[n]=0;row_factor_[n]=1;}
    }
    static float up(double value) {return outward(float(value),true);}
    bool make_regions(size_t n,const PreparationControl* control=nullptr) {
        const auto& source=layers_[n].config.source;const auto w=source.width,original_h=source.height;
        // Bound all temporary preparation allocations before building the tile.
        // Inputs outside this budget retain the existing exact baseline.
        if(uint64_t(w)*original_h>maximum_source_pixels){preparation_status_=PreparationStatus::ResourceLimit;return false;}
        uint32_t factor=1;
        for(uint32_t f=2;f<=16;++f)if(original_h%f==0){
            bool equal=true;for(uint32_t y=0;y<original_h&&equal;++y)
                equal=std::memcmp(source.pixels+size_t(y)*w,source.pixels+size_t(y/f*f)*w,w)==0;
            if(equal)factor=f;
        }
        row_factor_[n]=factor;const auto h=original_h/factor,W=3*w,H=3*h;
        const size_t tiled_count=size_t(W)*H;
        std::unique_ptr<uint32_t[]> ids,queue;
        struct Component {int alo,ahi,blo,bhi;bool valid;};std::unique_ptr<Component[]> components;
        if(!allocate(ids,tiled_count)||!allocate(queue,tiled_count)||!allocate(components,maximum_components))return false;
        std::fill_n(ids.get(),tiled_count,UINT32_MAX);uint32_t component_count=0;
        const auto pixel=[&](uint32_t i){return source.pixels[size_t((i/W)%h)*factor*w+i%W%w];};
        for(uint32_t at=0;at<tiled_count;++at){
            if(!(at&255)&&control&&control->stopped())return false;
            if(ids[at]!=UINT32_MAX)continue;
            if(component_count>=maximum_components){preparation_status_=PreparationStatus::ResourceLimit;return false;}
            size_t queue_count=1;queue[0]=at;const uint32_t id=component_count;ids[at]=id;
            Component c{INT32_MAX,INT32_MIN,INT32_MAX,INT32_MIN,false};
            for(size_t q=0;q<queue_count;++q){if(!(q&255)&&control&&control->stopped())return false;const auto i=queue[q];const int x=i%W,y=i/W;
                c.alo=std::min(c.alo,x+y);c.ahi=std::max(c.ahi,x+y);c.blo=std::min(c.blo,x-y);c.bhi=std::max(c.bhi,x-y);
                const uint32_t next[]={x?i-1:UINT32_MAX,x+1<int(W)?i+1:UINT32_MAX,y?i-W:UINT32_MAX,y+1<int(H)?i+W:UINT32_MAX};
                for(auto j:next)if(j!=UINT32_MAX&&ids[j]==UINT32_MAX&&pixel(j)==pixel(i)){ids[j]=id;queue[queue_count++]=j;}
            }
            const uint64_t na=c.ahi-c.alo+1,nb=c.bhi-c.blo+1;
            const int correction=(na%2&&nb%2)?(((c.alo-c.blo)%2==0)?1:-1):0;
            const uint64_t integer_points=(int64_t(na*nb)+correction)/2;
            c.valid=integer_points==queue_count;components[component_count++]=c;
        }
        auto& result=regions_[n];if(!allocate(result,size_t(w)*h))return false;region_counts_[n]=size_t(w)*h;
        for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){
            const auto& c=components[ids[size_t(y+h)*W+x+w]];if(!c.valid)return false;
            const int a=int(x+w)+int(y+h),b=int(x+w)-int(y+h);
            result[size_t(y)*w+x]={int16_t(c.alo-a),int16_t(c.ahi-a),int16_t(c.blo-b),int16_t(c.bhi-b)};
        }
        return true;
    }
    bool make_slopes(size_t n,const PreparationControl* control=nullptr){
        auto& out=slopes_[n];if(!allocate(out,size_t(chunks_)*height_))return false;const auto& p=layers_[n];
        for(uint32_t y=0;y<height_;++y)for(uint32_t chunk=0;chunk<chunks_;++chunk){
            if(!chunk&&control&&control->stopped())return false;
            double lo[6],hi[6];for(unsigned k=0;k<6;++k){lo[k]=1e99;hi[k]=-1e99;}const uint32_t end=std::min(width_,(chunk+1)*chunk_size);
            if(end==chunk*chunk_size+1)for(unsigned k=0;k<6;++k)lo[k]=hi[k]=0;
            for(uint32_t x=chunk*chunk_size+1;x<end;++x){
                const size_t i=size_t(y)*width_+x;const auto& a=p.pixels[i-1];const auto& b=p.pixels[i];
                const double dx=double(b.x)-a.x,dy=double(b.y)-a.y;
                const double wx=p.config.source.width,hy=p.config.source.height/row_factor_[n];
                const double delta[]={dx*wx+dy*hy,dx*wx-dy*hy,double(b.cos_x)-a.cos_x,double(b.sin_x)-a.sin_x,
                    n?double(b.cos_x)-a.cos_x:double(p.compression_trig[i].cosine)-p.compression_trig[i-1].cosine,
                    n?double(b.sin_x)-a.sin_x:double(p.compression_trig[i].sine)-p.compression_trig[i-1].sine};
                for(unsigned k=0;k<6;++k){lo[k]=std::min(lo[k],delta[k]);hi[k]=std::max(hi[k],delta[k]);}
            }
            out[size_t(y)*chunks_+chunk]={{-up(-lo[0]),up(hi[0])},{-up(-lo[1]),up(hi[1])},{-up(-lo[2]),up(hi[2])},{-up(-lo[3]),up(hi[3])},{-up(-lo[4]),up(hi[4])},{-up(-lo[5]),up(hi[5])}};
        }
        return true;
    }
    struct Sample{uint32_t texel,x,y;float fx,fy;};
    static float local_scaled(float v,float extent){const auto k=int32_t(v);v-=float(k-(v<float(k)));return v*extent;}
    static bool guarded_fraction(float v,float error,float extent,uint32_t& result,float& fraction){
        const float scaled=local_scaled(v,extent);const uint32_t pixel=uint32_t(scaled);
        const float padding=error*extent+8*std::numeric_limits<float>::epsilon()*extent;
        fraction=scaled-float(pixel);result=pixel;
        return fraction>padding&&1-fraction>padding;
    }
    template<bool Oscillate>
    __attribute__((always_inline)) static inline Sample sample(const SeparableFrame& f,size_t i){
        const auto& uv=f.coordinates[i];float u=uv.x,v=uv.y,fx,fy;uint32_t x,y;
        if constexpr(Oscillate){
            u+=f.amplitude*(uv.cos_x*f.cosine-uv.sin_x*f.sine);u+=f.move_x;
            if(!guarded_fraction(u,f.error_x,f.float_width,x,fx)){
                u=uv.x;u+=f.amplitude*std::cos(f.frequency*uv.y+f.phase);u+=f.move_x;
                const auto scaled=local_scaled(u,f.float_width);x=std::min(uint32_t(scaled),f.width-1);fx=scaled-float(x);
            }
            const auto& c=f.compression_trig[i];
            v+=f.compression_amplitude*(c.cosine*f.compression_cosine-c.sine*f.compression_sine);
        }else{
            u+=f.move_x;const auto scaled=local_scaled(u,f.float_width);x=std::min(uint32_t(scaled),f.width-1);fx=scaled-float(x);
            v+=f.compression_amplitude*(uv.cos_x*f.compression_cosine-uv.sin_x*f.compression_sine);
        }
        v+=f.move_y;
        if(!guarded_fraction(v,f.error_y,f.float_height,y,fy)){
            v=uv.y;v+=f.compression_amplitude*std::cos(f.compression_frequency*uv.y+f.compression_phase);v+=f.move_y;
            const auto scaled=local_scaled(v,f.float_height);y=std::min(uint32_t(scaled),f.height-1);fy=scaled-float(y);
        }
        return {y*f.width+x,x,y,fx,fy};
    }
    template<bool Oscillate,bool EmitSpans=false>
    void compose_row(const SeparableFrame& input,size_t n,uint32_t y,const Reciprocal* certificate=nullptr)const {
        const SeparableFrame f=input;
        if constexpr(EmitSpans)span_row_count_[n]=0;
        auto* out=row_[n].get();const auto& p=layers_[n];
        const uint32_t factor=row_factor_[n];const float inverse_factor=1.0f/factor;const auto* regions=regions_[n].get();
        // This additional expression envelope bounds rounding of both endpoint
        // identity evaluations. It is independent of skipped-pixel count.
        const float eps=std::numeric_limits<float>::epsilon();
        const float error_u=2*f.error_x+32*eps*(p.maximum_x+std::abs(f.amplitude)+std::abs(f.move_x)+1);
        const float error_v=2*f.error_y+32*eps*(p.maximum_y+std::abs(f.compression_amplitude)+std::abs(f.move_y)+1);
        const float ex=outward(error_u*f.float_width+32*eps*f.float_width,true);
        const float ey=outward(error_v*(f.float_height*inverse_factor)+32*eps*f.float_height,true);
        const float error=outward(ex+ey+0.0001f,true);
        for(uint32_t c=0;c<chunks_;++c){
            Reciprocal rates;
            if(certificate)rates=certificate[size_t(y)*chunks_+c];
            else{
            const auto& s=slopes_[n][size_t(y)*chunks_+c];
            Range oscillation{};if constexpr(Oscillate)oscillation=scale(subtract(scale(s.oc,f.cosine),scale(s.os,f.sine)),f.amplitude*f.float_width);
            const Range compression=scale(subtract(scale(s.cc,f.compression_cosine),scale(s.cs,f.compression_sine)),f.compression_amplitude*(f.float_height*inverse_factor));
            const auto magnitude=[](Range r){return std::max(std::abs(r.lo),std::abs(r.hi));};
            const float derivative_error=outward(32*eps*(magnitude(s.u)+magnitude(s.v)+
                std::abs(f.amplitude*f.float_width)*(magnitude(s.oc)+magnitude(s.os))+
                std::abs(f.compression_amplitude*(f.float_height*inverse_factor))*(magnitude(s.cc)+magnitude(s.cs))+1),true);
            const Range a=expand(add(add(s.u,oscillation),compression),derivative_error);
            const Range b=expand(subtract(add(s.v,oscillation),compression),derivative_error);
            rates=reciprocal(a,b);
            }
            const auto ra0=rates.a0,ra1=rates.a1,rb0=rates.b0,rb1=rates.b1;
            const uint32_t end=std::min(width_,(c+1)*chunk_size);
            for(uint32_t x=c*chunk_size;x<end;){
                const auto point=sample<Oscillate>(f,size_t(y)*width_+x);const auto color=f.indices[point.texel];++stats_.samples;
                const uint32_t tx=point.x,ty=point.y;
                const uint32_t effective_y=factor==2?ty>>1:ty/factor;
                const uint32_t subrow=factor==2?ty&1:ty%factor;
                const auto& r=regions[size_t(effective_y)*f.width+tx];
                const float fy=(float(subrow)+point.fy)*inverse_factor;
                const float va=point.fx+fy,vb=point.fx-fy;
                const float al=va-(float(r.alo)+1)-error,ah=(float(r.ahi)+1)-va-error;
                const float bl=vb-float(r.blo)-error,bh=float(r.bhi)-vb-error;
                uint32_t steps=0;
                if(al>0&&ah>0&&bl>0&&bh>0){
                    float limit=float(end-x);
                    if(ra0)limit=std::min(limit,al*ra0);
                    if(ra1)limit=std::min(limit,ah*ra1);
                    if(rb0)limit=std::min(limit,bl*rb0);
                    if(rb1)limit=std::min(limit,bh*rb1);
                    if(limit>0)steps=std::min(end-x-1,uint32_t(outward(limit,false)));
                }
                if constexpr(EmitSpans){
                    const uint16_t run_end=uint16_t(x+steps+1);auto& count=span_row_count_[n];auto* runs=span_row_[n].get();
                    if(count&&runs[count-1].color==color)runs[count-1].end=run_end;
                    else runs[count++]={run_end,color};
                }else std::fill_n(out+x,steps+1,color);
                stats_.skipped+=steps;x+=steps+1;
            }
        }
    }
    template<bool Mapped> void compose_runs(const SeparableFrame (&f)[2],uint32_t* output)const {
        stats_={};const auto* colors=Mapped?mapped_extended_pair_colors_.data():extended_pair_colors_.data();
        const size_t stride=layers_[1].config.source.palette_size;
        for(uint32_t y=0;y<height_;++y){
            compose_row<true>(f[0],0,y);compose_row<false>(f[1],1,y);
            for(uint32_t x=0;x<width_;++x){const size_t i=size_t(y)*width_+x;const auto color=colors[size_t(row_[0][x])*stride+row_[1][x]];
                if constexpr(Mapped)output[output_offsets_[i]]=color;else output[i]=color;
            }
        }
    }
public:
    RegionBackgroundKernel()=default;
    RegionBackgroundKernel(const RegionBackgroundKernel&)=delete;
    RegionBackgroundKernel& operator=(const RegionBackgroundKernel&)=delete;
    RegionBackgroundKernel(RegionBackgroundKernel&&)=default;
    RegionBackgroundKernel& operator=(RegionBackgroundKernel&&)=default;
    PreparationStatus preparation_status()const{return preparation_status_;}
    bool region_fast_path()const{return regions_ready_;}
    RegionStats region_stats()const{return stats_;}
    size_t prepared_bytes()const{
        size_t extra=0;for(size_t n=0;n<2;++n){extra+=region_counts_[n]*sizeof(Region);if(slopes_[n])extra+=size_t(chunks_)*height_*sizeof(Slope);if(row_[n])extra+=width_;if(span_row_[n])extra+=size_t(width_)*sizeof(SpanRun);}
        return BackgroundKernel::prepared_bytes()+extra+certificate_bytes_;
    }
    static size_t preparation_upper_bound(const std::vector<Layer>& layers,uint32_t width,uint32_t height,bool certificates){
        const size_t base=BackgroundKernel::preparation_upper_bound(layers,width,height);
        if(!potential_separable_pair(layers))return base;
        size_t regions=0,temporary=0;for(const auto& l:layers){const size_t pixels=size_t(l.source.width)*l.source.height;if(pixels>maximum_source_pixels)return base+regions+temporary;regions+=pixels*sizeof(Region);temporary=std::max<size_t>(temporary,pixels*9*sizeof(uint32_t)*2+size_t(maximum_components)*20);}
        const size_t cells=size_t((width+chunk_size-1)/chunk_size)*height;
        const size_t rows=2*width*(sizeof(uint8_t)+sizeof(SpanRun)),slopes=2*cells*sizeof(Slope);
        const size_t table=cells*72*sizeof(Reciprocal);const size_t cert=certificates&&table<=certificate_cap?table:0;
        return base+regions+std::max(temporary,slopes+rows+cert);
    }
    size_t allocated_bytes()const{return BackgroundKernel::allocated_bytes()+prepared_bytes()-BackgroundKernel::prepared_bytes();}
    void clear(){clear_regions();preparation_status_=PreparationStatus::Inactive;BackgroundKernel::clear();}
    // Sources/layout retain BackgroundKernel's borrowed-immutable lifetime.
    // Optional allocation/cap/shape failure returns success with exact baseline.
    // A failed BASE prepare still returns false and leaves no usable state.
    bool prepare(const std::vector<Layer>& layers,uint32_t w,uint32_t h,std::string& error,const PreparationControl* control=nullptr){
        clear_regions();preparation_status_=PreparationStatus::Inactive;
        if(!BackgroundKernel::prepare(layers,w,h,error,control))return false;
        width_=w;height_=h;chunks_=(w+chunk_size-1)/chunk_size;
        preparation_status_=PreparationStatus::Unsupported;
        if(separable_pair()){
            regions_ready_=make_regions(0,control)&&make_regions(1,control);
            if(regions_ready_)for(size_t n=0;n<2;++n)if(!make_slopes(n,control)||!allocate(row_[n],w)){regions_ready_=false;break;}
        }
        if(control&&control->stopped()){clear();error="Region preparation cancelled";return false;}
        if(regions_ready_)preparation_status_=PreparationStatus::Ready;
        else clear_regions();
        return true;
    }
    size_t certificate_bytes()const{return certificate_bytes_;}
    bool certificate_frame_used()const{return certificate_used_;}
    // One bounded optional variant. Check complete peak allocation first.
    // Failure leaves the already-prepared exact dynamic proof/CPU path intact.
    bool prepare_certificates(size_t budget=certificate_cap,const PreparationControl* control=nullptr){
        for(auto& c:certificates_)c.reset();
        certificate_bytes_=0;certificate_used_=false;
        if(!regions_ready_)return false;
        const size_t cells=size_t(chunks_)*height_,counts[]={cells*64,cells*8};
        const size_t bytes=(counts[0]+counts[1])*sizeof(Reciprocal);
        if(bytes>std::min(budget,certificate_cap))return false;
        for(size_t n=0;n<2;++n){
            certificates_[n].reset(new(std::nothrow) Reciprocal[counts[n]]);
            if(!certificates_[n]){for(auto& c:certificates_)c.reset();
                        return false;}
        }
        for(size_t n=0;n<2;++n){
            const auto& p=layers_[n];const auto& l=p.config;
            const float eps=std::numeric_limits<float>::epsilon(),inverse_factor=1.0f/row_factor_[n];
            const float oscillation_scale=l.amplitude_x*float(l.source.width);
            const float compression_scale=l.compression_amplitude_y*(float(l.source.height)*inverse_factor);
            for(unsigned first=0;first<(n?1u:8u);++first)for(unsigned second=0;second<8;++second){
                const auto oscillation_box=trig_box(first),compression_box=trig_box(second);
                for(size_t i=0;i<cells;++i){
                    if(!(i&127)&&control&&control->stopped()){for(auto& c:certificates_)c.reset();return false;}
                    const auto& s=slopes_[n][i];Range oscillation{};
                    if(!n)oscillation=product(minus(product(s.oc,oscillation_box.cosine),product(s.os,oscillation_box.sine)),{oscillation_scale,oscillation_scale});
                    const Range compression=product(minus(product(s.cc,compression_box.cosine),product(s.cs,compression_box.sine)),{compression_scale,compression_scale});
                    const auto magnitude=[](Range v){return std::max(std::abs(v.lo),std::abs(v.hi));};
                    const float error=outward(32*eps*(magnitude(s.u)+magnitude(s.v)+
                        std::abs(oscillation_scale)*(magnitude(s.oc)+magnitude(s.os))+
                        std::abs(compression_scale)*(magnitude(s.cc)+magnitude(s.cs))+1),true);
                    const auto a=expand(plus(plus(s.u,oscillation),compression),error);
                    const auto b=expand(minus(plus(s.v,oscillation),compression),error);
                    if(!std::isfinite(a.lo)||!std::isfinite(a.hi)||!std::isfinite(b.lo)||!std::isfinite(b.hi)){
                        for(auto& c:certificates_)c.reset();
                        return false;
                    }
                    certificates_[n][size_t(first*8+second)*cells+i]=reciprocal(a,b);
                }
            }
        }
        certificate_bytes_=bytes;return true;
    }
    // Optional sparse output for a GPU fill backend. Allocation failure does
    // not change the existing CPU readiness/status; no second coefficient or
    // region preparation is retained. CPU compose paths remain unchanged.
    bool prepare_spans(){
        for(auto& row:span_row_)row.reset();
        if(!regions_ready_)return false;
        for(auto& row:span_row_){row.reset(new(std::nothrow) SpanRun[width_]);if(!row){for(auto& other:span_row_)other.reset();return false;}}
        return true;
    }
    // Caller-owned bounded storage may be a GPU-visible linear buffer. The
    // caller must wait for its previous GPU use. Failure always sets count=0;
    // no partial batch may be submitted. There is no hidden raster fallback.
    template<class Span>bool generate_spans(float time,Span* output,size_t capacity,size_t& count)const{
        stats_={};count=0;certificate_used_=false;
        SeparableFrame f[2];
        if(!std::isfinite(time)||!output||!capacity||!span_row_[0]||!span_row_[1]||!regions_ready_||
           !separable_pair()||!prepare_separable_frame(time,f)||!f[0].guarded||!f[1].guarded)return false;
        const size_t stride=layers_[1].config.source.palette_size;const auto* colors=extended_pair_colors_.data();
        const Reciprocal* certificate[2]{};
        if(certificates_[0]&&certificates_[1]){
            const int a=trig_bucket(f[0].cosine,f[0].sine),b=trig_bucket(f[0].compression_cosine,f[0].compression_sine),c=trig_bucket(f[1].compression_cosine,f[1].compression_sine);
            if(a>=0&&b>=0&&c>=0){const size_t cells=size_t(chunks_)*height_;certificate[0]=certificates_[0].get()+size_t(a*8+b)*cells;certificate[1]=certificates_[1].get()+size_t(c)*cells;certificate_used_=true;}
        }
        for(uint32_t y=0;y<height_;++y){
            compose_row<true,true>(f[0],0,y,certificate[0]);compose_row<false,true>(f[1],1,y,certificate[1]);
            uint32_t a=0,b=0,x=0;
            while(x<width_){
                const auto& ra=span_row_[0][a];const auto& rb=span_row_[1][b];
                const uint32_t end=std::min(ra.end,rb.end),color=colors[size_t(ra.color)*stride+rb.color];
                if(count&&output[count-1].y==y&&output[count-1].color==color)output[count-1].width=uint16_t(end-output[count-1].x);
                else{
                    if(count==capacity){count=0;return false;}
                    auto& span=output[count++];span.x=uint16_t(x);span.y=uint16_t(y);span.width=uint16_t(end-x);span.color=color;
                }
                x=end;if(ra.end==end)++a;if(rb.end==end)++b;
            }
        }
        return true;
    }
    bool compose(float time,uint32_t clear,uint32_t* output)const {
        stats_={};
        SeparableFrame f[2];if(output&&regions_ready_&&separable_pair()&&prepare_separable_frame(time,f)&&f[0].guarded&&f[1].guarded){compose_runs<false>(f,output);return true;}
        return BackgroundKernel::compose(time,clear,output);
    }
    bool compose_mapped(float time,uint32_t clear,uint32_t* output,size_t count)const {
        stats_={};
        if(!std::isfinite(time)||!output_offsets_||!output||count<mapped_output_count_)return false;
        SeparableFrame f[2];if(regions_ready_&&separable_pair()&&prepare_separable_frame(time,f)&&f[0].guarded&&f[1].guarded){compose_runs<true>(f,output);return true;}
        return BackgroundKernel::compose_mapped(time,clear,output,count);
    }
};
}

#pragma once
#include "encore/background_kernel.hpp"
#include <memory>
#include <new>

namespace encore {
// Sparse texture coordinates, never a color raster. The source-owned kernel
// remains authoritative; no source IDs, dimensions, colors or tuning are here.
// CPU certificates enclose the original scalar texel. PICA sampler/raster
// qualification remains a separate experimental platform contract.
// Borrows the unchanged prepared coordinate owner: clear/reprepare invalidates
// this plan. Passing the owner at frame time also survives renderer swaps.
class CertifiedTextureBackgroundKernel {
public:
    struct Strip {uint16_t x,y,width,reserved;float uv[4],delta[4];};
    struct Stats {uint32_t linear_pixels=0,scalar_pixels=0,exact_pixels=0,scalar_trig_calls=0,constant_pixels=0,merged_strips=0;};
private:
    static constexpr uint32_t chunk=8;
    struct Residual {float values[2][6]{};};
    std::unique_ptr<Residual[]> residual_;
    std::unique_ptr<uint32_t[]> representatives_;
    size_t representative_count_=0,representative_offset_[2]{};
    uint32_t width_=0,height_=0,columns_=0;
    const void* coordinates_[2]{};
    static float basis(const BackgroundKernel::Prepared& p,size_t i,unsigned k,unsigned n){
        const auto& v=p.pixels[i];
        switch(k){case 0:return v.x;case 1:return v.y;case 2:return n?0:v.cos_x;
        case 3:return n?0:v.sin_x;case 4:return n?v.cos_x:p.compression_trig[i].cosine;
        default:return n?v.sin_x:p.compression_trig[i].sine;}
    }
    static float up(float v){return BackgroundKernel::outward(v,true);}
    static int32_t period(float v){const int32_t i=int32_t(v);return i-(v<float(i));}
    static void project(const BackgroundKernel::Prepared& p,const BackgroundKernel::SeparableFrame& f,
                        size_t i,unsigned n,float& u,float& v){
        const auto& at=p.pixels[i];u=at.x;v=at.y;
        if(!n)u+=f.amplitude*(at.cos_x*f.cosine-at.sin_x*f.sine);
        const float c=n?at.cos_x:p.compression_trig[i].cosine;
        const float s=n?at.sin_x:p.compression_trig[i].sine;
        v+=f.compression_amplitude*(c*f.compression_cosine-s*f.compression_sine);
        u+=f.move_x;v+=f.move_y;
    }
    static bool uniform(const BackgroundKernel& kernel,unsigned n,uint32_t xl,uint32_t xh,uint32_t yl,uint32_t yh){
        const auto& source=kernel.layers_[n].config.source;
        if(xl>xh||yl>yh||xh>=source.width||yh>=source.height)return false;
        const uint8_t index=source.pixels[size_t(yl)*source.width+xl];
        if(xl==xh&&yl==yh)return true;
        const auto hi=kernel.homogeneity_index_[n];
        if(hi<kernel.homogeneity_.size()){
            const auto& h=kernel.homogeneity_[hi];
            if(h.width==source.width&&h.height==source.height&&index<source.palette_size){
                const size_t plane=size_t(h.stride)*(h.height+1);
                if(h.counts.size()>=plane*source.palette_size){
                    const auto* sum=h.counts.data()+size_t(index)*plane;
                    const uint32_t count=sum[size_t(yh+1)*h.stride+xh+1]+sum[size_t(yl)*h.stride+xl]-
                        sum[size_t(yl)*h.stride+xh+1]-sum[size_t(yh+1)*h.stride+xl];
                    return count==(xh-xl+1)*(yh-yl+1);
                }
            }
        }
        // Preserve the bounded proof for shapes without a summed-area table.
        if((xh-xl+1)*(yh-yl+1)>16)return false;
        for(uint32_t y=yl;y<=yh;++y)for(uint32_t x=xl;x<=xh;++x)
            if(source.pixels[size_t(y)*source.width+x]!=index)return false;
        return true;
    }
    void texel_center(const BackgroundKernel& kernel,unsigned n,uint32_t x,uint32_t y,float* uv)const{
        const auto& source=kernel.layers_[n].config.source;
        const uint8_t index=source.pixels[size_t(y)*source.width+x];
        const uint32_t position=representatives_[representative_offset_[n]+index];
        uv[0]=(float(position&0xffffu)+0.5f)/256;
        uv[1]=1-(float(position>>16)+0.5f)/256;
    }
    static void exact(const BackgroundKernel& kernel,const BackgroundKernel::Prepared& p,const BackgroundKernel::SeparableFrame& f,
                      size_t i,unsigned n,float& u,float& v,Stats& stats,bool& used_exact){
        const auto& at=p.pixels[i];project(p,f,i,n,u,v);
        uint32_t x=0,y=0;
        const bool x_good=n? (x=BackgroundKernel::coordinate_separable(u,f.width,f.float_width),true):
            BackgroundKernel::guarded_coordinate(u,f.error_x,f.float_width,x);
        const bool y_good=BackgroundKernel::guarded_coordinate(v,f.error_y,f.float_height,y);
        if(!x_good||!y_good){
            uint32_t xl,xh,yl,yh;
            const float ex=n?0:f.error_x;
            const bool region=BackgroundKernel::coordinate_range({BackgroundKernel::outward(u-ex,false),up(u+ex)},f.width,f.float_width,xl,xh)&&
                BackgroundKernel::coordinate_range({BackgroundKernel::outward(v-f.error_y,false),up(v+f.error_y)},f.height,f.float_height,yl,yh)&&
                uniform(kernel,n,xl,xh,yl,yh);
            if(region){x=xl;y=yl;}
            else{
                if(!x_good){u=at.x;u+=f.amplitude*std::cos(f.frequency*at.y+f.phase);u+=f.move_x;
                    x=BackgroundKernel::coordinate_separable(u,f.width,f.float_width);++stats.scalar_trig_calls;used_exact=true;}
                if(!y_good){v=at.y;v+=f.compression_amplitude*std::cos(f.compression_frequency*at.y+f.compression_phase);v+=f.move_y;
                    y=BackgroundKernel::coordinate_separable(v,f.height,f.float_height);++stats.scalar_trig_calls;used_exact=true;}
            }
        }
        // Fixed texel centers also remove all repeat seams in one-pixel leaves.
        u=(float(x)+0.5f)/256;v=1-(float(y)+0.5f)/256;
    }
    // Enclose scalar versus secant error and guard every interior sample, not
    // only the endpoints. GPU UVs stay near [0,1] even after hours of scrolling.
    bool certify(float u0,float v0,float u1,float v1,float eu,float ev,uint32_t length,
                        const BackgroundKernel& kernel,unsigned n,float* start,float* delta,bool& constant)const{
        constant=false;
        const auto& source=kernel.layers_[n].config.source;
        const float values[]={u0,v0,u1,v1,eu,ev};
        for(float value:values)if(!std::isfinite(value)||std::abs(value)>=1048576)return false;
        const int32_t pu=period(u0),pv=period(v0);
        if(pu!=period(u1)||pv!=period(v1))return false;
        const float u=u0-float(pu),v=v0-float(pv);
        const float end_u=u1-float(pu),end_v=v1-float(pv);
        const float du=(end_u-u)/float(length-1),dv=(end_v-v)/float(length-1);
        constexpr float pica_epsilon=1.0f/65536;
        const auto padding=[](float e,float a,float b,float slope,float extent){
            return up(e*extent+32*std::numeric_limits<float>::epsilon()*extent+
                4*pica_epsilon*(std::max(std::abs(a),std::abs(b))+std::abs(slope)+1)*extent);
        };
        const float ex=padding(eu,u,end_u,du,float(source.width));
        const float ey=padding(ev,v,end_v,dv,float(source.height));
        // Prove a whole swept source rectangle first. A uniform palette index
        // may use a fixed representative texel; this preserves sampled color
        // while avoiding redundant UV interpolation and interior proofs.
        const float xl=std::min(u,end_u)*source.width-ex,xh=std::max(u,end_u)*source.width+ex;
        const float yl=std::min(v,end_v)*source.height-ey,yh=std::max(v,end_v)*source.height+ey;
        if(xl>=0&&yl>=0&&xh<source.width&&yh<source.height&&
            uniform(kernel,n,uint32_t(xl),uint32_t(xh),uint32_t(yl),uint32_t(yh))){
            texel_center(kernel,n,uint32_t(xl),uint32_t(yl),start);
            delta[0]=delta[1]=0;constant=true;return true;
        }
        for(uint32_t k=0;k<length;++k){
            const float x=(u+du*float(k))*source.width,y=(v+dv*float(k))*source.height;
            if(x-ex<0||y-ey<0||x+ex>=source.width||y+ey>=source.height)return false;
            const auto xl=uint32_t(x-ex),xh=uint32_t(x+ex),yl=uint32_t(y-ey),yh=uint32_t(y+ey);
            if(xl==xh&&yl==yh)continue;
            // A curve can cross a texel boundary without crossing a visible
            // palette region. Admit that rectangle only when every possible
            // source index is identical; no frame color raster is produced.
            if(!uniform(kernel,n,xl,xh,yl,yh))return false;
        }
        start[0]=(u-0.5f*du)*source.width/256;
        start[1]=1-(v-0.5f*dv)*source.height/256;
        delta[0]=du*float(length)*source.width/256;
        delta[1]=-dv*float(length)*source.height/256;
        return true;
    }
    bool emit(const BackgroundKernel& kernel,const BackgroundKernel::SeparableFrame (&frames)[2],
              uint32_t x,uint32_t y,uint32_t length,const Residual& residual,float residual_scale,
              Strip* out,size_t capacity,size_t& count,Stats& stats)const{
        Strip s{};s.x=uint16_t(x);s.y=uint16_t(y);s.width=uint16_t(length);bool good=true,used_exact=false,constant=true;
        const size_t first=size_t(y)*width_+x,last=first+length-1;
        for(unsigned n=0;n<2;++n){const auto& p=kernel.layers_[n];const auto& f=frames[n];
            if(length==1){
                float u,v;exact(kernel,p,f,first,n,u,v,stats,used_exact);
                // exact() returns padded texel-center UVs; derive their source
                // texel and canonicalize equal palette regions for merging.
                const uint32_t sx=uint32_t(u*256),sy=uint32_t((1-v)*256);
                texel_center(kernel,n,sx,sy,s.uv+n*2);continue;
            }
            float u0,v0,u1,v1;project(p,f,first,n,u0,v0);project(p,f,last,n,u1,v1);
            const auto* r=residual.values[n];const float scale=length==2?0:residual_scale;
            const float eu=up(scale*(r[0]+std::abs(f.amplitude)*(r[2]*std::abs(f.cosine)+r[3]*std::abs(f.sine)))+f.error_x);
            const float ev=up(scale*(r[1]+std::abs(f.compression_amplitude)*(r[4]*std::abs(f.compression_cosine)+r[5]*std::abs(f.compression_sine)))+f.error_y);
            bool layer_constant=false;
            if(!certify(u0,v0,u1,v1,eu,ev,length,kernel,n,s.uv+n*2,s.delta+n*2,layer_constant)){good=false;break;}
            constant&=layer_constant;
        }
        if(good){
            bool merged=false;
            if(constant&&count){auto& previous=out[count-1];
                if(previous.y==s.y&&uint32_t(previous.x)+previous.width==s.x&&
                    previous.delta[0]==0&&previous.delta[1]==0&&previous.delta[2]==0&&previous.delta[3]==0&&
                    std::equal(s.uv,s.uv+4,previous.uv)){
                    previous.width=uint16_t(previous.width+s.width);merged=true;++stats.merged_strips;
                }
            }
            if(!merged){if(count==capacity)return false;out[count++]=s;}
            if(constant)stats.constant_pixels+=length;
            if(length==1){++stats.scalar_pixels;if(used_exact)++stats.exact_pixels;}else stats.linear_pixels+=length;
            return true;
        }
        const uint32_t left=length/2;
        // Relative to the root secant, both child endpoints deviate by <=E;
        // consequently any child secant deviates from its curve by <=2E.
        return emit(kernel,frames,x,y,left,residual,2,out,capacity,count,stats)&&
            emit(kernel,frames,x+left,y,length-left,residual,2,out,capacity,count,stats);
    }
public:
    static bool supported_shape(const std::vector<BackgroundKernel::Layer>& layers,uint32_t width,uint32_t height){
        if(width!=400||height!=240||!BackgroundKernel::potential_separable_pair(layers))return false;
        for(const auto& l:layers)if(!l.barrel||!l.source.width||!l.source.height||l.source.width>256||l.source.height>256)return false;
        return true;
    }
    static size_t preparation_upper_bound(const std::vector<BackgroundKernel::Layer>& layers,uint32_t w,uint32_t h){
        return supported_shape(layers,w,h)?size_t((w+chunk-1)/chunk)*h*sizeof(Residual)+
            (layers[0].source.palette_size+layers[1].source.palette_size)*sizeof(uint32_t):0;
    }
    void clear(){residual_.reset();representatives_.reset();representative_count_=0;representative_offset_[0]=representative_offset_[1]=0;
        width_=height_=columns_=0;coordinates_[0]=coordinates_[1]=nullptr;}
    bool ready()const{return bool(residual_);}
    size_t prepared_bytes()const{return ready()?size_t(columns_)*height_*sizeof(Residual)+representative_count_*sizeof(uint32_t):0;}
    bool prepare(const BackgroundKernel& kernel,uint32_t w,uint32_t h,const PreparationControl* control=nullptr){
        clear();if(w!=400||h!=240||kernel.prepared_width_!=w||kernel.prepared_height_!=h||kernel.size_!=size_t(w)*h||kernel.extended_pair_colors_.empty()||kernel.layers_.size()!=2)return false;
        for(const auto& p:kernel.layers_)if(!p.config.barrel||p.config.source.width>256||p.config.source.height>256)return false;
        width_=w;height_=h;columns_=(w+chunk-1)/chunk;
        for(unsigned n=0;n<2;++n)coordinates_[n]=kernel.layers_[n].pixels.data();
        representative_offset_[1]=kernel.layers_[0].config.source.palette_size;
        representative_count_=representative_offset_[1]+kernel.layers_[1].config.source.palette_size;
        representatives_.reset(new(std::nothrow) uint32_t[representative_count_]);if(!representatives_){clear();return false;}
        std::fill_n(representatives_.get(),representative_count_,UINT32_MAX);
        for(unsigned n=0;n<2;++n){const auto& source=kernel.layers_[n].config.source;
            for(size_t i=0;i<size_t(source.width)*source.height;++i){
                if(control&&i%source.width==0&&control->stopped()){clear();return false;}
                auto& position=representatives_[representative_offset_[n]+source.pixels[i]];
                if(position==UINT32_MAX)position=uint32_t(i%source.width)|(uint32_t(i/source.width)<<16);
            }
        }
        residual_.reset(new(std::nothrow) Residual[size_t(columns_)*h]);if(!residual_){clear();return false;}
        for(uint32_t y=0;y<h;++y){if(control&&control->stopped()){clear();return false;}
            for(uint32_t c=0;c<columns_;++c){const uint32_t x=c*chunk,length=std::min(chunk,w-x);
                auto& r=residual_[size_t(y)*columns_+c];const size_t first=size_t(y)*w+x;
                for(unsigned n=0;n<2;++n)for(unsigned k=0;k<6;++k){
                    const auto& p=kernel.layers_[n];const float a=basis(p,first,k,n),b=basis(p,first+length-1,k,n);
                    float e=0;for(uint32_t i=0;i<length;++i){const float t=length>1?float(i)/float(length-1):0;
                        e=std::max(e,std::abs(basis(p,first+i,k,n)-(a+(b-a)*t)));}
                    r.values[n][k]=up(e+32*std::numeric_limits<float>::epsilon()*(std::abs(a)+std::abs(b)+1));
                }
            }
        }
        return true;
    }
    bool generate(const BackgroundKernel& kernel,float time,Strip* out,size_t capacity,size_t& count,Stats& stats)const{
        count=0;stats={};if(!ready()||!out||!capacity||kernel.size_!=size_t(width_)*height_||kernel.layers_.size()!=2||
            coordinates_[0]!=kernel.layers_[0].pixels.data()||coordinates_[1]!=kernel.layers_[1].pixels.data())return false;
        BackgroundKernel::SeparableFrame frames[2];if(!kernel.prepare_separable_frame(time,frames)||!frames[0].guarded||!frames[1].guarded)return false;
        for(uint32_t y=0;y<height_;++y)for(uint32_t c=0;c<columns_;++c){const uint32_t x=c*chunk;
            if(!emit(kernel,frames,x,y,std::min(chunk,width_-x),residual_[size_t(y)*columns_+c],1,out,capacity,count,stats)){count=0;stats={};return false;}
        }
        return true;
    }
};
static_assert(sizeof(CertifiedTextureBackgroundKernel::Strip)==40,"Texture-strip ABI");
}

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
    struct Stats {uint32_t linear_pixels=0,scalar_pixels=0;};
private:
    static constexpr uint32_t chunk=8;
    struct Residual {float values[2][6]{};};
    std::unique_ptr<Residual[]> residual_;
    uint32_t width_=0,height_=0,columns_=0;
    const void* coordinates_[2]{};
    static float basis(const BackgroundKernel::Prepared& p,size_t i,unsigned k,unsigned n){
        const auto& v=p.pixels[i];
        switch(k){case 0:return v.x;case 1:return v.y;case 2:return n?0:v.cos_x;
        case 3:return n?0:v.sin_x;case 4:return n?v.cos_x:p.compression_trig[i].cosine;
        default:return n?v.sin_x:p.compression_trig[i].sine;}
    }
    static float up(float v){return std::nextafter(v,std::numeric_limits<float>::infinity());}
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
    static void exact(const BackgroundKernel::Prepared& p,const BackgroundKernel::SeparableFrame& f,
                      size_t i,unsigned n,float& u,float& v){
        const auto& at=p.pixels[i];u=at.x;v=at.y;
        if(!n)u+=f.amplitude*std::cos(f.frequency*at.y+f.phase);
        v+=f.compression_amplitude*std::cos(f.compression_frequency*at.y+f.compression_phase);
        u+=f.move_x;v+=f.move_y;
        const auto x=BackgroundKernel::coordinate_separable(u,f.width,f.float_width);
        const auto y=BackgroundKernel::coordinate_separable(v,f.height,f.float_height);
        // Fixed texel centers also remove all repeat seams in one-pixel leaves.
        u=(float(x)+0.5f)/256;v=1-(float(y)+0.5f)/256;
    }
    // Enclose scalar versus secant error and guard every interior sample, not
    // only the endpoints. GPU UVs stay near [0,1] even after hours of scrolling.
    static bool certify(float u0,float v0,float u1,float v1,float eu,float ev,uint32_t length,
                        const BackgroundKernel::Source& source,float* start,float* delta){
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
        for(uint32_t k=0;k<length;++k){
            const float x=(u+du*float(k))*source.width,y=(v+dv*float(k))*source.height;
            if(x-ex<0||y-ey<0||x+ex>=source.width||y+ey>=source.height)return false;
            const auto xl=uint32_t(x-ex),xh=uint32_t(x+ex),yl=uint32_t(y-ey),yh=uint32_t(y+ey);
            if(xl==xh&&yl==yh)continue;
            // A curve can cross a texel boundary without crossing a visible
            // palette region. Admit that rectangle only when every possible
            // source index is identical; no frame color raster is produced.
            const auto index=source.pixels[size_t(yl)*source.width+xl];
            for(uint32_t yy=yl;yy<=yh;++yy)for(uint32_t xx=xl;xx<=xh;++xx)
                if(source.pixels[size_t(yy)*source.width+xx]!=index)return false;
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
        Strip s{};s.x=uint16_t(x);s.y=uint16_t(y);s.width=uint16_t(length);bool good=true;
        const size_t first=size_t(y)*width_+x,last=first+length-1;
        for(unsigned n=0;n<2;++n){const auto& p=kernel.layers_[n];const auto& f=frames[n];
            if(length==1){exact(p,f,first,n,s.uv[n*2],s.uv[n*2+1]);continue;}
            float u0,v0,u1,v1;project(p,f,first,n,u0,v0);project(p,f,last,n,u1,v1);
            const auto* r=residual.values[n];const float scale=length==2?0:residual_scale;
            const float eu=up(scale*(r[0]+std::abs(f.amplitude)*(r[2]*std::abs(f.cosine)+r[3]*std::abs(f.sine)))+f.error_x);
            const float ev=up(scale*(r[1]+std::abs(f.compression_amplitude)*(r[4]*std::abs(f.compression_cosine)+r[5]*std::abs(f.compression_sine)))+f.error_y);
            good&=certify(u0,v0,u1,v1,eu,ev,length,p.config.source,s.uv+n*2,s.delta+n*2);
        }
        if(good){if(count==capacity)return false;out[count++]=s;if(length==1)++stats.scalar_pixels;else stats.linear_pixels+=length;return true;}
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
        return supported_shape(layers,w,h)?size_t((w+chunk-1)/chunk)*h*sizeof(Residual):0;
    }
    void clear(){residual_.reset();width_=height_=columns_=0;coordinates_[0]=coordinates_[1]=nullptr;}
    bool ready()const{return bool(residual_);}
    size_t prepared_bytes()const{return ready()?size_t(columns_)*height_*sizeof(Residual):0;}
    bool prepare(const BackgroundKernel& kernel,uint32_t w,uint32_t h,const PreparationControl* control=nullptr){
        clear();if(w!=400||h!=240||kernel.prepared_width_!=w||kernel.prepared_height_!=h||kernel.size_!=size_t(w)*h||kernel.extended_pair_colors_.empty()||kernel.layers_.size()!=2)return false;
        for(const auto& p:kernel.layers_)if(!p.config.barrel||p.config.source.width>256||p.config.source.height>256)return false;
        width_=w;height_=h;columns_=(w+chunk-1)/chunk;
        for(unsigned n=0;n<2;++n)coordinates_[n]=kernel.layers_[n].pixels.data();
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

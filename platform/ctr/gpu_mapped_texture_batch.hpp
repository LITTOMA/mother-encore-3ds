#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstring>
#include "encore/certified_texture_background_kernel.hpp"
#include "gpu_row_texture_batch.hpp"
#include "gpu_mapped_texture_shader.hpp"

namespace encore::ctr {
// Direct nearest-source sampling: a certified single-pass palette factorization
// or weighted palettes plus OR/AND corrections preserve the finite CPU blend.
// No source colors, paths, tuning or IDs are part of this execution mechanism.
class GpuMappedTextureBatch {
public:
    using Strip=CertifiedTextureBackgroundKernel::Strip;
    static constexpr size_t maximum_strips=32768;
    static constexpr size_t linear_upper_bound=maximum_strips*sizeof(Strip)+6*256*256*4;
private:
    Strip* strips_=nullptr;size_t count_=0;
    std::array<C3D_Tex,6> textures_{};
    std::array<bool,6> texture_ready_{};
    DVLB_s* dvlb_=nullptr;shaderProgram_s program_{};bool program_ready_=false;
    C3D_AttrInfo attributes_{};C3D_BufInfo buffers_{};
    CertifiedTextureBackgroundKernel::Stats stats_{};
    // A finite palette can sometimes share one binary correction across RGB.
    // Such a decomposition packs both masks into source alpha and uses one
    // geometry pass; other admitted palettes retain the general three passes.
    unsigned passes_=0,texture_count_=0;uint32_t correction_=0;

    static uint32_t morton(uint32_t x,uint32_t y){return (x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);}
    static bool source_valid(const BackgroundKernel::Source& s){
        if(!s.width||!s.height||s.width>256||s.height>256||!s.pixels||!s.palette||!s.palette_size||s.palette_size>16)return false;
        for(size_t i=0;i<size_t(s.width)*s.height;++i)if(s.pixels[i]>=s.palette_size)return false;
        return true;
    }
    static bool palette(const BackgroundKernel::Layer& layer,std::array<uint32_t,16>& output){
        if(!source_valid(layer.source))return false;
        const auto& p=layer.palette_source;
        if(layer.palette_shifting){
            if(layer.palette_frames||layer.palette_fixed_row>=p.height||!p.width||!p.height||!p.pixels||!p.palette||!p.palette_size||p.palette_size>256)return false;
            for(size_t i=0;i<size_t(p.width)*p.height;++i)if(p.pixels[i]>=p.palette_size)return false;
        }
        for(size_t i=0;i<layer.source.palette_size;++i){
            uint32_t color=layer.source.palette[i];if(color>>24!=255)return false;
            if(layer.palette_shifting){
                // Preserve the original float division/multiplication and
                // nearest/clamped fixed-row lookup, including red=255.
                const auto x=std::min(uint32_t(float(color&255u)/255.0f*p.width),p.width-1);
                color=(p.palette[p.pixels[size_t(layer.palette_fixed_row)*p.width+x]]&0xffffffu)|0xff000000u;
            }
            output[i]=color;
        }
        return true;
    }
    static unsigned mixed(unsigned a,unsigned b,float opacity){
        // The 0.5 path is independently special-cased by BackgroundKernel.
        if(opacity==0.5f)return (a+b+1)/2;
        const float alpha=255.0f/255.0f*opacity;
        const float value=float(b)*alpha+float(a)*(1-alpha);
        return unsigned(value+0.5f);
    }
    static bool factor(const std::array<int,256>& residual,size_t a,size_t b,
                       uint32_t& or_a,uint32_t& or_b,uint32_t& and_a,uint32_t& and_b){
        // A binary OR plane plus a binary outer-product AND plane. Enumerate
        // the bounded palette domain once at load; never during drawing.
        const uint32_t a_mask=(1u<<a)-1;
        for(uint32_t bits=0;bits<(1u<<(a+b));++bits){
            const uint32_t ar=bits&a_mask,br=bits>>a;
            uint32_t rows=0,columns=0;bool valid=true;
            for(size_t i=0;i<a&&valid;++i)for(size_t j=0;j<b;++j){
                const int d=residual[i*b+j]-int(((ar>>i)|(br>>j))&1u);
                if(d<0||d>1){valid=false;break;}
                if(d){rows|=1u<<i;columns|=1u<<j;}
            }
            if(!valid)continue;
            for(size_t i=0;i<a&&valid;++i)for(size_t j=0;j<b;++j){
                const int reconstructed=int(((ar>>i)|(br>>j))&1u)+int(((rows>>i)&(columns>>j))&1u);
                if(reconstructed!=residual[i*b+j]){valid=false;break;}
            }
            if(valid){or_a=ar;or_b=br;and_a=rows;and_b=columns;return true;}
        }
        return false;
    }
    static bool single_pass_palettes(const std::vector<BackgroundKernel::Layer>& layers,
                                    std::array<std::array<uint32_t,16>,6>& output,uint32_t& correction){
        std::array<uint32_t,16> a{},b{};
        if(!palette(layers[0],a)||!palette(layers[1],b))return false;
        const size_t na=layers[0].source.palette_size,nb=layers[1].source.palette_size;
        if(na+nb>16||!std::isfinite(layers[1].opacity)||layers[1].opacity<0||layers[1].opacity>1)return false;
        using Color=std::array<int,3>;
        std::array<Color,256> colors{},cross{};uint32_t rows=0,columns=0;Color nonzero{};bool found=false;
        for(size_t i=0;i<na;++i)for(size_t j=0;j<nb;++j)for(unsigned k=0;k<3;++k)
            colors[i*nb+j][k]=int(mixed((a[i]>>(8*k))&255u,(b[j]>>(8*k))&255u,layers[1].opacity));
        for(size_t i=0;i<na;++i)for(size_t j=0;j<nb;++j){
            auto& d=cross[i*nb+j];bool any=false;
            for(unsigned k=0;k<3;++k){d[k]=colors[i*nb+j][k]-colors[i*nb][k]-colors[j][k]+colors[0][k];any|=d[k]!=0;}
            if(any){rows|=1u<<i;columns|=1u<<j;if(!found){nonzero=d;found=true;}}
        }
        // The cross difference must be one RGB vector on a rectangular binary
        // outer product. This is a source-derived admission, not a battle ID.
        for(size_t i=0;i<na;++i)for(size_t j=0;j<nb;++j){
            const Color expected=((rows>>i)&(columns>>j)&1u)?nonzero:Color{};
            if(cross[i*nb+j]!=expected)return false;
        }
        for(unsigned orientation=0;orientation<4;++orientation){
            const bool r0=orientation&1,c0=orientation&2;
            const uint32_t rm=rows^(r0?((1u<<na)-1):0),cm=columns^(c0?((1u<<nb)-1):0);
            Color subtract{};bool valid=true;
            for(unsigned k=0;k<3;++k){subtract[k]=((r0==c0)?-1:1)*nonzero[k];if(subtract[k]<0||subtract[k]>255)valid=false;}
            if(!valid)continue;
            std::array<Color,256> base=colors;
            for(size_t i=0;i<na;++i)for(size_t j=0;j<nb;++j)for(unsigned k=0;k<3;++k){
                base[i*nb+j][k]+=subtract[k]*int((rm>>i)&(cm>>j)&1u);
                // ADD saturates before SUBTRACT: saturation must be impossible.
                if(base[i*nb+j][k]<0||base[i*nb+j][k]>255)valid=false;
            }
            if(!valid)continue;
            for(size_t i=0;i<na;++i)for(size_t j=0;j<nb;++j)for(unsigned k=0;k<3;++k)
                if(base[i*nb+j][k]!=base[i*nb][k]+base[j][k]-base[0][k])valid=false;
            if(!valid)continue;
            std::array<std::array<uint32_t,16>,6> candidate{};uint32_t constant=0xff000000u;
            for(size_t i=0;i<na;++i)candidate[0][i]=((rm>>i)&1u)?0xff000000u:0;
            for(size_t j=0;j<nb;++j)candidate[1][j]=((cm>>j)&1u)?0xff000000u:0;
            for(unsigned k=0;k<3&&valid;++k){
                int low=0,high=255;
                for(size_t i=0;i<na;++i){low=std::max(low,base[i*nb][k]-255);high=std::min(high,base[i*nb][k]);}
                for(size_t j=0;j<nb;++j){low=std::max(low,base[0][k]-base[j][k]);high=std::min(high,255+base[0][k]-base[j][k]);}
                if(low>high){valid=false;break;}
                for(size_t i=0;i<na;++i)candidate[0][i]|=uint32_t(base[i*nb][k]-low)<<(8*k);
                for(size_t j=0;j<nb;++j)candidate[1][j]|=uint32_t(base[j][k]-base[0][k]+low)<<(8*k);
                constant|=uint32_t(subtract[k])<<(8*k);
            }
            if(!valid)continue;
            // Certify the exact CPU float rounding result for every palette
            // pair, including the intermediate eight-bit TEV ADD range.
            for(size_t i=0;i<na;++i)for(size_t j=0;j<nb;++j)for(unsigned k=0;k<3;++k){
                const int sum=int((candidate[0][i]>>(8*k))&255u)+int((candidate[1][j]>>(8*k))&255u);
                const int value=sum-subtract[k]*int((rm>>i)&(cm>>j)&1u);
                if(sum>255||value!=colors[i*nb+j][k])valid=false;
            }
            if(valid){output=candidate;correction=constant;return true;}
        }
        return false;
    }
    static bool palettes(const std::vector<BackgroundKernel::Layer>& layers,
                         std::array<std::array<uint32_t,16>,6>& output){
        std::array<uint32_t,16> a{},b{};
        if(!palette(layers[0],a)||!palette(layers[1],b))return false;
        const size_t na=layers[0].source.palette_size,nb=layers[1].source.palette_size;
        if(na+nb>16||!std::isfinite(layers[1].opacity)||layers[1].opacity<0||layers[1].opacity>1)return false;
        for(auto& p:output)p.fill(0xff000000u);
        const float alpha=layers[1].opacity;
        for(unsigned shift=0;shift<24;shift+=8){
            std::array<int,256> residual{};
            for(size_t i=0;i<na;++i)output[0][i]|=unsigned(float((a[i]>>shift)&255u)*(1-alpha))<<shift;
            for(size_t j=0;j<nb;++j)output[1][j]|=unsigned(float((b[j]>>shift)&255u)*alpha)<<shift;
            for(size_t i=0;i<na;++i)for(size_t j=0;j<nb;++j){
                const unsigned av=(a[i]>>shift)&255u,bv=(b[j]>>shift)&255u;
                residual[i*nb+j]=int(mixed(av,bv,alpha))-int((output[0][i]>>shift)&255u)-int((output[1][j]>>shift)&255u);
            }
            uint32_t oa,ob,aa,ab;
            if(!factor(residual,na,nb,oa,ob,aa,ab))return false;
            for(size_t i=0;i<na;++i){output[2][i]|=(((oa>>i)&1u)*255u)<<shift;output[4][i]|=(((aa>>i)&1u)*255u)<<shift;}
            for(size_t j=0;j<nb;++j){output[3][j]|=(((ob>>j)&1u)*255u)<<shift;output[5][j]|=(((ab>>j)&1u)*255u)<<shift;}
            // Reject any decomposition that changes even one palette pair.
            for(size_t i=0;i<na;++i)for(size_t j=0;j<nb;++j){
                const unsigned value=((output[0][i]>>shift)&255u)+((output[1][j]>>shift)&255u)+
                    unsigned(((output[2][i]|output[3][j])>>shift)&255u)/255u+
                    unsigned(((output[4][i]&output[5][j])>>shift)&255u)/255u;
                if(value!=mixed((a[i]>>shift)&255u,(b[j]>>shift)&255u,alpha))return false;
            }
        }
        return true;
    }
    static void tev_single(uint32_t correction){
        for(unsigned n=0;n<6;++n)C3D_TexEnvInit(C3D_GetTexEnv(n));
        auto* e=C3D_GetTexEnv(0);
        C3D_TexEnvSrc(e,C3D_RGB,GPU_TEXTURE0,GPU_TEXTURE1,GPU_CONSTANT);
        C3D_TexEnvFunc(e,C3D_RGB,GPU_ADD);
        // Buffer bit 0 captures stage 0. The final stage 3 waits beyond the
        // PICA combiner-buffer delay before reading the saved RGB base.
        C3D_TexEnvBufUpdate(C3D_RGB,1);C3D_TexEnvBufUpdate(C3D_Alpha,0);
        e=C3D_GetTexEnv(1);C3D_TexEnvSrc(e,C3D_RGB,GPU_TEXTURE0,GPU_TEXTURE1,GPU_CONSTANT);
        C3D_TexEnvOpRgb(e,GPU_TEVOP_RGB_SRC_ALPHA,GPU_TEVOP_RGB_SRC_ALPHA);
        C3D_TexEnvFunc(e,C3D_RGB,GPU_MODULATE);
        e=C3D_GetTexEnv(2);C3D_TexEnvSrc(e,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT,GPU_CONSTANT);
        C3D_TexEnvFunc(e,C3D_RGB,GPU_MODULATE);C3D_TexEnvColor(e,correction);
        e=C3D_GetTexEnv(3);C3D_TexEnvSrc(e,C3D_RGB,GPU_PREVIOUS_BUFFER,GPU_PREVIOUS,GPU_CONSTANT);
        C3D_TexEnvFunc(e,C3D_RGB,GPU_SUBTRACT);
        // Source alpha carries masks; output alpha remains fully opaque.
        for(unsigned n=0;n<4;++n){e=C3D_GetTexEnv(n);if(n!=2)C3D_TexEnvColor(e,0xffffffffu);C3D_TexEnvSrc(e,C3D_Alpha,GPU_CONSTANT,GPU_CONSTANT,GPU_CONSTANT);C3D_TexEnvFunc(e,C3D_Alpha,GPU_REPLACE);}
        C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);C3D_AlphaTest(false,GPU_ALWAYS,0);C3D_CullFace(GPU_CULL_NONE);
        C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ZERO,GPU_ONE,GPU_ZERO);
    }
    static void tev(unsigned pass){
        C3D_TexEnvBufUpdate(C3D_Both,0);
        for(unsigned n=0;n<6;++n)C3D_TexEnvInit(C3D_GetTexEnv(n));
        auto* e=C3D_GetTexEnv(0);
        C3D_TexEnvSrc(e,C3D_RGB,GPU_TEXTURE0,GPU_TEXTURE1,GPU_CONSTANT);
        C3D_TexEnvFunc(e,C3D_RGB,pass==2?GPU_MODULATE:GPU_ADD);
        C3D_TexEnvSrc(e,C3D_Alpha,GPU_CONSTANT,GPU_CONSTANT,GPU_CONSTANT);
        C3D_TexEnvFunc(e,C3D_Alpha,GPU_REPLACE);C3D_TexEnvColor(e,0xffffffffu);
        if(pass){
            e=C3D_GetTexEnv(1);C3D_TexEnvSrc(e,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT,GPU_CONSTANT);
            C3D_TexEnvFunc(e,C3D_RGB,GPU_MODULATE);C3D_TexEnvColor(e,0xff010101u);
        }
        C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);C3D_AlphaTest(false,GPU_ALWAYS,0);C3D_CullFace(GPU_CULL_NONE);
        if(pass)C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ONE,GPU_ZERO,GPU_ONE);
        else C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ZERO,GPU_ONE,GPU_ZERO);
    }
public:
    GpuMappedTextureBatch()=default;
    ~GpuMappedTextureBatch(){release();}
    GpuMappedTextureBatch(const GpuMappedTextureBatch&)=delete;
    GpuMappedTextureBatch& operator=(const GpuMappedTextureBatch&)=delete;
    void swap(GpuMappedTextureBatch& o){
        using std::swap;swap(strips_,o.strips_);swap(count_,o.count_);swap(textures_,o.textures_);swap(texture_ready_,o.texture_ready_);
        swap(dvlb_,o.dvlb_);swap(program_,o.program_);swap(program_ready_,o.program_ready_);swap(attributes_,o.attributes_);swap(buffers_,o.buffers_);swap(stats_,o.stats_);swap(passes_,o.passes_);swap(texture_count_,o.texture_count_);swap(correction_,o.correction_);
    }
    bool ready()const{return strips_&&program_ready_&&texture_count_&&std::all_of(texture_ready_.begin(),texture_ready_.begin()+texture_count_,[](bool v){return v;});}
    size_t count()const{return count_;}
    unsigned draw_passes()const{return passes_;}
    const CertifiedTextureBackgroundKernel::Stats& stats()const{return stats_;}
    size_t linear_bytes()const{size_t bytes=strips_?maximum_strips*sizeof(Strip):0;for(size_t i=0;i<6;++i)if(texture_ready_[i])bytes+=textures_[i].size;return bytes;}
    size_t tracked_bytes()const{return linear_bytes();}
    bool prepare_frame(const CertifiedTextureBackgroundKernel& kernel,const BackgroundKernel& background,float time){
        count_=0;stats_={};return ready()&&kernel.generate(background,time,strips_,maximum_strips,count_,stats_);
    }
    bool create(const std::vector<BackgroundKernel::Layer>& layers,uint32_t width,uint32_t height){
        release();if(width!=400||height!=240||layers.size()!=2||layers[0].opacity!=1)return false;
        static_assert(sizeof(Strip)==40&&offsetof(Strip,uv)==8&&offsetof(Strip,delta)==24,"Mapped PICA strip ABI");
        std::array<std::array<uint32_t,16>,6> colors{};
        if(single_pass_palettes(layers,colors,correction_)){passes_=1;texture_count_=2;}
        else if(palettes(layers,colors)){passes_=3;texture_count_=6;}
        else return false;
        if(!row_texture_parking.prepare()){release();return false;}
        strips_=static_cast<Strip*>(linearAlloc(maximum_strips*sizeof(Strip)));
        dvlb_=DVLB_ParseFile(reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(encore_gpu_mapped_texture_shader)),sizeof(encore_gpu_mapped_texture_shader));
        if(!strips_||!dvlb_||dvlb_->numDVLE!=2){release();return false;}
        shaderProgramInit(&program_);program_ready_=true;
        if(R_FAILED(shaderProgramSetVsh(&program_,&dvlb_->DVLE[0]))||R_FAILED(shaderProgramSetGsh(&program_,&dvlb_->DVLE[1],3))){release();return false;}
        for(size_t n=0;n<texture_count_;++n){
            auto& texture=textures_[n];texture_ready_[n]=C3D_TexInit(&texture,256,256,GPU_RGBA8);
            if(!texture_ready_[n]){release();return false;}
            const auto& source=layers[n&1].source;
            for(uint32_t y=0;y<256;++y)for(uint32_t x=0;x<256;++x){
                const size_t pixel=size_t(std::min(y,source.height-1))*source.width+std::min(x,source.width-1);
                const size_t offset=((y/8)*32+x/8)*64+morton(x,y);
                static_cast<uint32_t*>(texture.data)[offset]=__builtin_bswap32(colors[n][source.pixels[pixel]]);
            }
            C3D_TexSetFilter(&texture,GPU_NEAREST,GPU_NEAREST);C3D_TexSetWrap(&texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);C3D_TexFlush(&texture);
        }
        AttrInfo_Init(&attributes_);AttrInfo_AddLoader(&attributes_,0,GPU_SHORT,4);AttrInfo_AddLoader(&attributes_,1,GPU_FLOAT,4);AttrInfo_AddLoader(&attributes_,2,GPU_FLOAT,4);
        BufInfo_Init(&buffers_);if(BufInfo_Add(&buffers_,strips_,sizeof(Strip),3,0x210)<0){release();return false;}return true;
    }
    bool draw(){
        if(!ready())return false;if(!count_)return true;if(count_>maximum_strips)return false;
        C2D_Flush();C3D_SetViewport(0,0,256,512);C3D_SetScissor(GPU_SCISSOR_NORMAL,0,0,240,400);
        C3D_BindProgram(&program_);C3D_SetAttrInfo(&attributes_);C3D_SetBufInfo(&buffers_);
        if(passes_==1){C3D_TexBind(0,&textures_[0]);C3D_TexBind(1,&textures_[1]);tev_single(correction_);C3D_DrawArrays(GPU_GEOMETRY_PRIM,0,int(count_));}
        else for(unsigned pass=0;pass<3;++pass){C3D_TexBind(0,&textures_[2*pass]);C3D_TexBind(1,&textures_[2*pass+1]);tev(pass);C3D_DrawArrays(GPU_GEOMETRY_PRIM,0,int(count_));}
        C3D_TexEnvBufUpdate(C3D_Both,0);
        C3D_TexBind(0,nullptr);C3D_TexBind(1,&row_texture_parking.texture);
        C3D_SetViewport(0,0,240,400);C3D_SetScissor(GPU_SCISSOR_DISABLE,0,0,240,400);
        C2D_Prepare();C3D_AlphaTest(true,GPU_GREATER,0);
        C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA);return true;
    }
    void release(){
        if(strips_)linearFree(strips_);strips_=nullptr;count_=0;stats_={};passes_=texture_count_=0;correction_=0;
        for(size_t i=0;i<6;++i){if(texture_ready_[i])C3D_TexDelete(&textures_[i]);texture_ready_[i]=false;}
        if(program_ready_)shaderProgramFree(&program_);program_ready_=false;
        if(dvlb_)DVLB_Free(dvlb_);dvlb_=nullptr;
    }
};
}

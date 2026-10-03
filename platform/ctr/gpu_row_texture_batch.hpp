#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <cstring>
#include <array>
#include "encore/row_linear_background_kernel.hpp"
#include "gpu_row_texture_shader.hpp"
namespace encore::ctr {
// Citro3D 1.7.1 dereferences a null texture for units >0. Keep a tiny
// process-lifetime binding after our draw, rather than leaving a freed battle
// texture in its context. Main calls C3D_Fini before global destruction.
struct RowTextureParking {
    C3D_Tex texture{};bool ready=false;
    bool prepare(){if(ready)return true;ready=C3D_TexInit(&texture,8,8,GPU_RGBA8);if(ready){std::memset(texture.data,0,texture.size);C3D_TexSetFilter(&texture,GPU_NEAREST,GPU_NEAREST);C3D_TexFlush(&texture);}return ready;}
    ~RowTextureParking(){if(ready)C3D_TexDelete(&texture);}
};
inline RowTextureParking row_texture_parking;
// Explicit experimental backend. Integer CPU sampling certificates do not
// establish the undocumented real-PICA raster/sampler error bound. No auto
// emulator detection, approximate fallback, frame allocations or pixel raster.
class GpuRowTextureBatch {
    struct Strip {uint16_t x,y,width,u0,v0,u1,v1,reserved;};
    static constexpr size_t capacity=4096;
    Strip* strips_=nullptr;size_t count_=0;
    C3D_Tex half_{},parity_{};bool half_ready_=false,parity_ready_=false;
    DVLB_s* dvlb_=nullptr;shaderProgram_s program_{};bool program_ready_=false;
    C3D_AttrInfo attributes_{};C3D_BufInfo buffers_{};
    uint32_t source_width_=0,accepted_=0;
    std::array<RowLinearBackgroundKernel::CertifiedRow,240> rows_{};
    std::array<uint8_t,240> certified_{};
    static uint32_t morton(uint32_t x,uint32_t y){return (x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);}
    static void tev(bool correction){
        for(unsigned n=0;n<6;++n)C3D_TexEnvInit(C3D_GetTexEnv(n));
        auto* e=C3D_GetTexEnv(0);C3D_TexEnvSrc(e,C3D_RGB,GPU_TEXTURE0,GPU_TEXTURE1,GPU_CONSTANT);C3D_TexEnvFunc(e,C3D_RGB,GPU_ADD);
        C3D_TexEnvSrc(e,C3D_Alpha,GPU_CONSTANT,GPU_CONSTANT,GPU_CONSTANT);C3D_TexEnvFunc(e,C3D_Alpha,GPU_REPLACE);C3D_TexEnvColor(e,0xffffffffu);
        if(correction){e=C3D_GetTexEnv(1);C3D_TexEnvSrc(e,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT,GPU_CONSTANT);C3D_TexEnvFunc(e,C3D_RGB,GPU_MODULATE);C3D_TexEnvColor(e,0xff010101u);}
        C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);C3D_AlphaTest(false,GPU_ALWAYS,0);C3D_CullFace(GPU_CULL_NONE);
        if(correction)C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ONE,GPU_ZERO,GPU_ONE);
        else C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ZERO,GPU_ONE,GPU_ZERO);
    }
public:
    GpuRowTextureBatch()=default;GpuRowTextureBatch(const GpuRowTextureBatch&)=delete;GpuRowTextureBatch& operator=(const GpuRowTextureBatch&)=delete;
    void swap(GpuRowTextureBatch& o){using std::swap;swap(strips_,o.strips_);swap(count_,o.count_);swap(half_,o.half_);swap(parity_,o.parity_);swap(half_ready_,o.half_ready_);swap(parity_ready_,o.parity_ready_);swap(dvlb_,o.dvlb_);swap(program_,o.program_);swap(program_ready_,o.program_ready_);swap(attributes_,o.attributes_);swap(buffers_,o.buffers_);swap(source_width_,o.source_width_);swap(accepted_,o.accepted_);swap(rows_,o.rows_);swap(certified_,o.certified_);}
    bool ready()const{return strips_&&half_ready_&&parity_ready_&&program_ready_;}
    size_t count()const{return count_;}
    uint32_t accepted_rows()const{return accepted_;}
    const uint8_t* certified_rows()const{return certified_.data();}
    bool prepare_frame(const RowLinearBackgroundKernel& kernel,float time){return ready()&&kernel.certify_integer_rows(time,rows_.data(),certified_.data(),accepted_)&&generate(rows_.data(),certified_.data());}
    size_t linear_bytes()const{return (strips_?capacity*sizeof(Strip):0)+(half_ready_?half_.size:0)+(parity_ready_?parity_.size:0);}
    size_t tracked_bytes()const{return ready()?capacity*sizeof(Strip)+half_.size+parity_.size+sizeof(rows_)+sizeof(certified_):0;}
    bool create(const std::vector<BackgroundKernel::Layer>& layers,uint32_t width,uint32_t height){
        release();if(width!=400||height!=240||layers.size()!=2||layers[0].opacity!=1||layers[1].opacity!=0.5f)return false;
        const auto& a=layers[0].source;const auto& b=layers[1].source;
        if(!a.width||a.width>256||!a.height||a.height>256||a.width!=b.width||a.height!=b.height||a.palette_size!=b.palette_size||
           std::memcmp(a.pixels,b.pixels,size_t(a.width)*a.height)||std::memcmp(a.palette,b.palette,a.palette_size*4))return false;
        for(size_t i=0;i<a.palette_size;++i)if(a.palette[i]>>24!=255)return false;
        if(!row_texture_parking.prepare())return false;
        source_width_=a.width;
        half_ready_=C3D_TexInit(&half_,256,256,GPU_RGBA8);parity_ready_=C3D_TexInit(&parity_,256,256,GPU_RGBA8);
        strips_=static_cast<Strip*>(linearAlloc(capacity*sizeof(Strip)));
        dvlb_=DVLB_ParseFile(reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(encore_gpu_row_texture_shader)),sizeof(encore_gpu_row_texture_shader));
        if(!half_ready_||!parity_ready_||!strips_||!dvlb_||dvlb_->numDVLE!=2){release();return false;}
        shaderProgramInit(&program_);program_ready_=true;
        if(R_FAILED(shaderProgramSetVsh(&program_,&dvlb_->DVLE[0]))||R_FAILED(shaderProgramSetGsh(&program_,&dvlb_->DVLE[1],2))){release();return false;}
        std::memset(strips_,0,capacity*sizeof(Strip));
        for(uint32_t y=0;y<256;++y)for(uint32_t x=0;x<256;++x){
            const uint32_t c=a.palette[a.pixels[size_t(std::min(y,a.height-1))*a.width+std::min(x,a.width-1)]];uint32_t h=0xff000000u,p=0xff000000u;
            for(unsigned s=0;s<24;s+=8){h|=(((c>>s)&255)/2)<<s;p|=(((c>>s)&1)?255u:0u)<<s;}
            const size_t offset=((y/8)*32+x/8)*64+morton(x,y);static_cast<uint32_t*>(half_.data)[offset]=__builtin_bswap32(h);static_cast<uint32_t*>(parity_.data)[offset]=__builtin_bswap32(p);
        }
        for(auto* texture:{&half_,&parity_}){C3D_TexSetFilter(texture,GPU_NEAREST,GPU_NEAREST);C3D_TexSetWrap(texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);C3D_TexFlush(texture);}
        AttrInfo_Init(&attributes_);AttrInfo_AddLoader(&attributes_,0,GPU_SHORT,4);AttrInfo_AddLoader(&attributes_,1,GPU_SHORT,4);
        BufInfo_Init(&buffers_);if(BufInfo_Add(&buffers_,strips_,sizeof(Strip),2,0x10)<0){release();return false;}return true;
    }
    bool generate(const RowLinearBackgroundKernel::CertifiedRow* rows,const uint8_t* certified){
        count_=0;if(!ready()||!rows||!certified)return false;
        for(uint32_t y=0;y<240;++y)if(certified[y]){
            uint32_t x=0;while(x<400){uint32_t u[2];for(unsigned n=0;n<2;++n){const int64_t at=int64_t(x)+rows[y].shift[n];u[n]=uint32_t((at%source_width_+source_width_)%source_width_);}
                const uint32_t length=std::min<uint32_t>(400-x,std::min(source_width_-u[0],source_width_-u[1]));
                if(count_==capacity){count_=0;return false;}
                strips_[count_++]={uint16_t(x),uint16_t(y),uint16_t(length),uint16_t(u[0]),rows[y].source_y[0],uint16_t(u[1]),rows[y].source_y[1],0};x+=length;
            }
        }
        return true;
    }
    bool draw(){
        if(!ready())return false;if(!count_)return true;
        C2D_Flush();C3D_SetViewport(0,0,256,512);C3D_SetScissor(GPU_SCISSOR_NORMAL,0,0,240,400);
        C3D_BindProgram(&program_);C3D_SetAttrInfo(&attributes_);C3D_SetBufInfo(&buffers_);
        C3D_TexBind(0,&half_);C3D_TexBind(1,&half_);tev(false);

        C3D_DrawArrays(GPU_GEOMETRY_PRIM,0,int(count_));
        C3D_TexBind(0,&parity_);C3D_TexBind(1,&parity_);tev(true);C3D_DrawArrays(GPU_GEOMETRY_PRIM,0,int(count_));
        C3D_TexBind(0,nullptr);C3D_TexBind(1,&row_texture_parking.texture);
        C3D_SetViewport(0,0,240,400);C3D_SetScissor(GPU_SCISSOR_DISABLE,0,0,240,400);C2D_Prepare();C3D_AlphaTest(true,GPU_GREATER,0);
        C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA);return true;
    }
    void release(){if(strips_)linearFree(strips_);strips_=nullptr;count_=0;if(half_ready_)C3D_TexDelete(&half_);if(parity_ready_)C3D_TexDelete(&parity_);half_ready_=parity_ready_=false;if(program_ready_)shaderProgramFree(&program_);program_ready_=false;if(dvlb_)DVLB_Free(dvlb_);dvlb_=nullptr;source_width_=0;}
};
}

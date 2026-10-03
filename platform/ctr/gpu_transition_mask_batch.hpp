#pragma once
#include "gpu_region_batch.hpp"
#include <algorithm>
#include <utility>
namespace encore::ctr {
// Exact source-alpha counterpart of GpuRegionBatch. It deliberately reuses
// the qualified integer-coordinate raster program; no sampling or texture
// shader path is introduced. Lifetime/fences belong to the main-thread owner.
class GpuTransitionMaskBatch {
public:
    using Span=GpuRegionBatch::Span;
    static constexpr size_t maximum_spans=400*240;
private:
    DVLB_s* dvlb_=nullptr;shaderProgram_s program_{};bool initialized_=false;
    Span* buffer_=nullptr;size_t capacity_=0;int projection_=-1;
    uint32_t width_=0,height_=0;
    C3D_AttrInfo attributes_{};C3D_BufInfo buffers_{};
public:
    GpuTransitionMaskBatch()=default;
    GpuTransitionMaskBatch(const GpuTransitionMaskBatch&)=delete;
    GpuTransitionMaskBatch& operator=(const GpuTransitionMaskBatch&)=delete;
    Span* data()const{return buffer_;}
    size_t capacity()const{return capacity_;}
    size_t linear_bytes()const{return capacity_*sizeof(Span);}
    bool ready()const{return buffer_&&dvlb_&&projection_>=0;}
    void swap(GpuTransitionMaskBatch& other){using std::swap;
        swap(dvlb_,other.dvlb_);swap(program_,other.program_);swap(initialized_,other.initialized_);
        swap(buffer_,other.buffer_);swap(capacity_,other.capacity_);swap(projection_,other.projection_);
        swap(width_,other.width_);swap(height_,other.height_);swap(attributes_,other.attributes_);swap(buffers_,other.buffers_);
    }
    bool create(uint32_t width,uint32_t height,size_t capacity){
        release();if(!width||width>400||!height||height>240||!capacity||capacity>size_t(width)*height||capacity>maximum_spans)return false;
        width_=width;height_=height;capacity_=capacity;
        dvlb_=DVLB_ParseFile(reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(encore_gpu_region_shader)),sizeof(encore_gpu_region_shader));
        if(!dvlb_||dvlb_->numDVLE!=2){release();return false;}
        shaderProgramInit(&program_);initialized_=true;
        if(R_FAILED(shaderProgramSetVsh(&program_,&dvlb_->DVLE[0]))||R_FAILED(shaderProgramSetGsh(&program_,&dvlb_->DVLE[1],2))){release();return false;}
        projection_=shaderInstanceGetUniformLocation(program_.geometryShader,"projection");if(projection_<0){release();return false;}
        buffer_=static_cast<Span*>(linearAlloc(linear_bytes()));if(!buffer_){release();return false;}
        std::memset(buffer_,0,linear_bytes());
        AttrInfo_Init(&attributes_);AttrInfo_AddLoader(&attributes_,0,GPU_SHORT,4);AttrInfo_AddLoader(&attributes_,1,GPU_UNSIGNED_BYTE,4);
        BufInfo_Init(&buffers_);if(BufInfo_Add(&buffers_,buffer_,sizeof(Span),2,0x10)<0){release();return false;}return true;
    }
    // No scaling, fractional translation, out-of-screen geometry or overlap.
    // Validate before touching GPU state so callers can take an exact fallback.
    bool draw(size_t count,float x,float y){
        if(!ready()||!count||count>capacity_||!std::isfinite(x)||!std::isfinite(y)||x<0||y<0||
           std::floor(x)!=x||std::floor(y)!=y||x+width_>400||y+height_>240)return false;
        uint32_t next_x=0,next_y=0;
        for(size_t i=0;i<count;++i){const auto& s=buffer_[i];
            if(next_y>=height_||s.x!=next_x||s.y!=next_y||!s.width||s.width>width_-next_x||s.reserved)return false;
            next_x+=s.width;if(next_x==width_){next_x=0;++next_y;}
        }
        if(next_y!=height_||next_x)return false;
        C2D_Flush();C3D_BindProgram(&program_);C3D_SetAttrInfo(&attributes_);C3D_SetBufInfo(&buffers_);
        C3D_Mtx projection;Mtx_OrthoTilt(&projection,0,400,240,0,1,-1,true);Mtx_Translate(&projection,x,y,0,true);
        C3D_FVUnifMtx4x4(GPU_GEOMETRY_SHADER,projection_,&projection);
        for(unsigned i=0;i<6;++i)C3D_TexEnvInit(C3D_GetTexEnv(i));
        auto* env=C3D_GetTexEnv(0);C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,GPU_PRIMARY_COLOR,GPU_PRIMARY_COLOR);C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
        C3D_CullFace(GPU_CULL_NONE);C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);C3D_AlphaTest(false,GPU_ALWAYS,0);
        C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA);
        // Keep individual draw sizes inside the existing qualified batch limit.
        for(size_t first=0;first<count;first+=GpuRegionBatch::maximum_spans)
            C3D_DrawArrays(GPU_GEOMETRY_PRIM,int(first),int(std::min(GpuRegionBatch::maximum_spans,count-first)));
        C2D_Prepare();C3D_AlphaTest(true,GPU_GREATER,0);
        C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA);
        return true;
    }
    void release(){
        if(buffer_)linearFree(buffer_);buffer_=nullptr;capacity_=0;width_=height_=0;
        if(initialized_)shaderProgramFree(&program_);initialized_=false;
        if(dvlb_)DVLB_Free(dvlb_);dvlb_=nullptr;projection_=-1;
    }
};
}

#pragma once
#include <3ds.h>
#include <citro2d.h>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <cstring>
#include "gpu_region_shader.hpp"
namespace encore::ctr {
// Experimental mechanism only. Every span/color comes from checked resources.
// The caller owns frame fences. The batch does no allocation during a frame.
class GpuRegionBatch {
public:
 struct Span {uint16_t x,y,width,reserved;uint32_t color;};
 static constexpr size_t maximum_spans=16384;
private:
 DVLB_s* dvlb_=nullptr;shaderProgram_s program_{};bool initialized_=false;
 Span* buffer_=nullptr;size_t capacity_=0;int projection_=-1;
 C3D_AttrInfo attributes_{};C3D_BufInfo buffers_{};
public:
 GpuRegionBatch()=default;GpuRegionBatch(const GpuRegionBatch&)=delete;GpuRegionBatch& operator=(const GpuRegionBatch&)=delete;
 void swap(GpuRegionBatch& other){using std::swap;swap(dvlb_,other.dvlb_);swap(program_,other.program_);swap(initialized_,other.initialized_);swap(buffer_,other.buffer_);swap(capacity_,other.capacity_);swap(projection_,other.projection_);swap(attributes_,other.attributes_);swap(buffers_,other.buffers_);}
 Span* data()const{return buffer_;}
 bool ready()const{return buffer_&&dvlb_&&projection_>=0;}
 size_t capacity()const{return capacity_;}
 bool create(size_t capacity=8192){
  release();if(!capacity||capacity>maximum_spans)return false;capacity_=capacity;static_assert(sizeof(Span)==12&&offsetof(Span,color)==8,"PICA span ABI");
  dvlb_=DVLB_ParseFile(reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(encore_gpu_region_shader)),sizeof(encore_gpu_region_shader));
  if(!dvlb_||dvlb_->numDVLE!=2){release();return false;}
  shaderProgramInit(&program_);initialized_=true;
  if(R_FAILED(shaderProgramSetVsh(&program_,&dvlb_->DVLE[0]))||R_FAILED(shaderProgramSetGsh(&program_,&dvlb_->DVLE[1],2))){release();return false;}
  projection_=shaderInstanceGetUniformLocation(program_.geometryShader,"projection");if(projection_<0){release();return false;}
  buffer_=static_cast<Span*>(linearAlloc(capacity_*sizeof(Span)));if(!buffer_){release();return false;}
  std::memset(buffer_,0,capacity_*sizeof(Span));
  AttrInfo_Init(&attributes_);AttrInfo_AddLoader(&attributes_,0,GPU_SHORT,4);AttrInfo_AddLoader(&attributes_,1,GPU_UNSIGNED_BYTE,4);
  BufInfo_Init(&buffers_);if(BufInfo_Add(&buffers_,buffer_,sizeof(Span),2,0x10)<0){release();return false;}return true;
 }
 // Top-screen 1:1 surface at the caller's letterbox offset. No scaling is
 // accepted. Return false before touching GPU state for an invalid batch.
 bool draw(size_t count,float x,float y){
  if(!ready()||!count||count>capacity_||!std::isfinite(x)||!std::isfinite(y))return false;
  C2D_Flush();C3D_BindProgram(&program_);C3D_SetAttrInfo(&attributes_);C3D_SetBufInfo(&buffers_);
  C3D_Mtx projection;Mtx_OrthoTilt(&projection,0,400,240,0,1,-1,true);Mtx_Translate(&projection,x,y,0,true);
  C3D_FVUnifMtx4x4(GPU_GEOMETRY_SHADER,projection_,&projection);
  for(unsigned i=0;i<6;++i)C3D_TexEnvInit(C3D_GetTexEnv(i));
  auto* env=C3D_GetTexEnv(0);C3D_TexEnvSrc(env,C3D_Both,GPU_PRIMARY_COLOR,GPU_PRIMARY_COLOR,GPU_PRIMARY_COLOR);C3D_TexEnvFunc(env,C3D_Both,GPU_REPLACE);
  C3D_CullFace(GPU_CULL_NONE);C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);C3D_AlphaTest(false,GPU_ALWAYS,0);
  C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ZERO,GPU_ONE,GPU_ZERO);
  C3D_DrawArrays(GPU_GEOMETRY_PRIM,0,int(count));
  // Restore the ordinary sprite/UI state; C2D_Prepare alone does not restore
  // blending. Existing SceneSize/View matrices are retained by Citro2D.
  C2D_Prepare();C3D_AlphaTest(true,GPU_GREATER,0);
  C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA);
  return true;
 }
 void release(){if(buffer_)linearFree(buffer_);buffer_=nullptr;capacity_=0;if(initialized_)shaderProgramFree(&program_);initialized_=false;if(dvlb_)DVLB_Free(dvlb_);dvlb_=nullptr;projection_=-1;}
};
}

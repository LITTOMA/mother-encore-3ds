#include <3ds.h>
#include <citro2d.h>
#include <cstdio>
#include <vector>
#include "tests/gpu_span_common.hpp"
#include "pillow_texture_shader.hpp"
namespace {
struct Vertex {float x,y,z,w,u0,v0,u1,v1;};
uint32_t morton(uint32_t x,uint32_t y){return (x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);}
void state(bool correction){
 for(unsigned n=0;n<6;++n)C3D_TexEnvInit(C3D_GetTexEnv(n));
 auto* e=C3D_GetTexEnv(0);C3D_TexEnvSrc(e,C3D_RGB,GPU_TEXTURE0,GPU_TEXTURE1,GPU_CONSTANT);C3D_TexEnvFunc(e,C3D_RGB,GPU_ADD);
 C3D_TexEnvSrc(e,C3D_Alpha,GPU_CONSTANT,GPU_CONSTANT,GPU_CONSTANT);C3D_TexEnvFunc(e,C3D_Alpha,GPU_REPLACE);C3D_TexEnvColor(e,0xffffffffu);
 if(correction){e=C3D_GetTexEnv(1);C3D_TexEnvSrc(e,C3D_RGB,GPU_PREVIOUS,GPU_CONSTANT,GPU_CONSTANT);C3D_TexEnvFunc(e,C3D_RGB,GPU_MODULATE);C3D_TexEnvColor(e,0xff010101u);}
 C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);C3D_AlphaTest(false,GPU_ALWAYS,0);C3D_CullFace(GPU_CULL_NONE);
 if(correction)C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ONE,GPU_ZERO,GPU_ONE);
 else C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ZERO,GPU_ONE,GPU_ZERO);
}
bool run(FILE* log){
 encore::upstream::BattleData data;std::string error;if(!data.load_file("romfs:/data/pillow-entry.encbattle",error))return false;
 auto view=data.view();auto resource=view.resource(view.background(0).resource);gpu_span::Image image;if(!gpu_span::image("romfs:/"+std::string(view.string(resource.path)),resource,image)||image.palette.size()!=6)return false;
 auto* target=C3D_RenderTargetCreate(64,64,GPU_RB_RGBA8,-1);auto* readback=static_cast<uint32_t*>(linearAlloc(64*64*4));auto* vertices=static_cast<Vertex*>(linearAlloc(36*6*sizeof(Vertex)));
 C3D_Tex half{},parity{};if(!target||!readback||!vertices||!C3D_TexInit(&half,8,8,GPU_RGBA8)||!C3D_TexInit(&parity,8,8,GPU_RGBA8))return false;
 for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){const auto c=image.palette[x%6];uint32_t h=0xff000000u,p=0xff000000u;for(unsigned s=0;s<24;s+=8){h|=(((c>>s)&255)/2)<<s;p|=(((c>>s)&1)?255u:0u)<<s;}static_cast<uint32_t*>(half.data)[morton(x,y)]=__builtin_bswap32(h);static_cast<uint32_t*>(parity.data)[morton(x,y)]=__builtin_bswap32(p);}
 for(auto* texture:{&half,&parity}){C3D_TexSetFilter(texture,GPU_NEAREST,GPU_NEAREST);C3D_TexSetWrap(texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);C3D_TexFlush(texture);}
 unsigned count=0;for(unsigned a=0;a<6;++a)for(unsigned b=0;b<6;++b){for(unsigned corner:{0u,1u,2u,2u,1u,3u}){float x=float(b*8+(corner&1)*8),y=float(a*8+(corner>>1)*8);vertices[count++]={1-y/32,1-x/32,-0.5f,1,(a+0.5f)/8,0.5f,(b+0.5f)/8,0.5f};}}
 auto* dvlb=DVLB_ParseFile(reinterpret_cast<uint32_t*>(const_cast<uint8_t*>(pillow_texture_shader)),sizeof(pillow_texture_shader));shaderProgram_s program{};shaderProgramInit(&program);if(!dvlb||R_FAILED(shaderProgramSetVsh(&program,&dvlb->DVLE[0])))return false;
 C3D_AttrInfo attr;AttrInfo_Init(&attr);AttrInfo_AddLoader(&attr,0,GPU_FLOAT,4);AttrInfo_AddLoader(&attr,1,GPU_FLOAT,4);C3D_BufInfo buf;BufInfo_Init(&buf);if(BufInfo_Add(&buf,vertices,sizeof(Vertex),2,0x10)<0)return false;
 if(!C3D_FrameBegin(0))return false;C3D_RenderTargetClear(target,C3D_CLEAR_COLOR,0,0);if(!C3D_FrameDrawOn(target))return false;C3D_BindProgram(&program);C3D_SetAttrInfo(&attr);C3D_SetBufInfo(&buf);
 C3D_TexBind(0,&half);C3D_TexBind(1,&half);state(false);C3D_DrawArrays(GPU_TRIANGLES,0,count);
 C3D_TexBind(0,&parity);C3D_TexBind(1,&parity);state(true);C3D_DrawArrays(GPU_TRIANGLES,0,count);
 C3D_FrameEnd(0);if(!C3D_FrameBegin(0))return false;
 const auto dim=GX_BUFFER_DIM(64,64);const uint32_t flags=GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
 C3D_SyncDisplayTransfer(static_cast<uint32_t*>(target->frameBuf.colorBuf),dim,readback,dim,flags);C3D_FrameEnd(0);if(!C3D_FrameBegin(0))return false;GSPGPU_InvalidateDataCache(readback,64*64*4);
 unsigned mismatch=0;for(unsigned a=0;a<6;++a)for(unsigned b=0;b<6;++b){uint32_t expected=0xff000000u;for(unsigned s=0;s<24;s+=8)expected|=(((((image.palette[a]>>s)&255)+((image.palette[b]>>s)&255))+1)/2)<<s;
  unsigned errors=0;for(unsigned y=a*8;y<(a+1)*8;++y)for(unsigned x=b*8;x<(b+1)*8;++x){const auto actual=__builtin_bswap32(readback[x*64+63-y]);errors+=actual!=expected;}
  std::fprintf(log,"TEV_PAIR a=%u b=%u expected=%08lx actual=%08lx mismatch=%u\n",a,b,(unsigned long)expected,(unsigned long)__builtin_bswap32(readback[(b*8)*64+63-a*8]),errors);mismatch+=errors;
 }
 std::fprintf(log,"TEV_RESULT %s ordered_pairs=36 channels=RGBA pixels=2304 mismatch=%u formula=floorhalf+OR_parity passes=2\n",mismatch?"FAIL":"PASS",mismatch);std::fflush(log);
 C3D_FrameEnd(0);shaderProgramFree(&program);DVLB_Free(dvlb);C3D_TexDelete(&half);C3D_TexDelete(&parity);C3D_RenderTargetDelete(target);linearFree(readback);linearFree(vertices);return !mismatch;
}
}
int main(){gfxInitDefault();consoleInit(GFX_BOTTOM,nullptr);const auto romfs=romfsInit();auto* log=std::fopen("sdmc:/encore-pillow-tev.log","w");const bool c3d=C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);bool passed=false;if(c3d&&log&&R_SUCCEEDED(romfs))passed=run(log);if(log){std::fprintf(log,"EXIT %s\n",passed?"PASS":"FAIL");std::fclose(log);}if(c3d)C3D_Fini();if(R_SUCCEEDED(romfs))romfsExit();gfxExit();return passed?0:1;}

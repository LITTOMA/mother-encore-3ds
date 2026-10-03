#include <3ds.h>
#include <citro2d.h>
#include "world_effect_renderer_instrumented.hpp"
#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <vector>
using namespace encore::upstream;
namespace {
FILE*log_file=nullptr;
bool frame_open=false;
uint64_t cpu_pixels=0,gpu_pixels=0;uint32_t cpu_failures=0,gpu_failures=0;
void message(const char*format,...){va_list a;va_start(a,format);std::vprintf(format,a);va_end(a);if(log_file){va_start(a,format);std::vfprintf(log_file,format,a);va_end(a);std::fflush(log_file);}gfxFlushBuffers();}
double elapsed(u64 start){return double(svcGetSystemTick()-start)/CPU_TICKS_PER_MSEC;}
bool begin(){if(frame_open)return true;frame_open=C3D_FrameBegin(0);return frame_open;}
void end(){if(frame_open){C3D_FrameEnd(0);frame_open=false;}}
uint32_t word(const uint8_t*p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
float real(const uint8_t*p){const auto x=word(p);float f;std::memcpy(&f,&x,4);return f;}
struct Case {uint32_t width=0,height=0;float time=0;WorldEffectSample sample{};uint32_t cpu_offset=0,cpu_bytes=0,gpu_offset=0,gpu_bytes=0;};
struct Fixture {
 FILE*file=nullptr;uint32_t size=0;std::vector<Case>cases;
 ~Fixture(){if(file)std::fclose(file);}
 bool load(){
  file=std::fopen("romfs:/cases.bin","rb");uint8_t head[32];if(!file||std::fread(head,1,32,file)!=32)return false;
  if(std::memcmp(head,"ENCWFQA1",8)||word(head+8)!=1||word(head+12)!=20||word(head+16)!=48||word(head+24)||word(head+28))return false;
  size=word(head+20);if(size>16*1024*1024||std::fseek(file,0,SEEK_END)||std::ftell(file)!=long(size)||std::fseek(file,32,SEEK_SET))return false;
  uint32_t previous=32+20*48;
  for(unsigned i=0;i<20;++i){uint8_t b[48];if(std::fread(b,1,48,file)!=48)return false;Case c;c.width=word(b);c.height=word(b+4);c.time=real(b+8);c.sample={true,real(b+28),{real(b+12),real(b+16),real(b+20),real(b+24)},0};c.cpu_offset=word(b+32);c.cpu_bytes=word(b+36);c.gpu_offset=word(b+40);c.gpu_bytes=word(b+44);
   if(!((c.width==320&&c.height==180)||(c.width==400&&c.height==240))||c.cpu_bytes!=c.width*c.height*4||c.gpu_bytes!=c.cpu_bytes||c.cpu_offset!=previous||c.gpu_offset!=c.cpu_offset+c.cpu_bytes||uint64_t(c.gpu_offset)+c.gpu_bytes>size)return false;
   previous=c.gpu_offset+c.gpu_bytes;cases.push_back(c);
  }return previous==size;
 }
 bool pixels(uint32_t offset,uint32_t bytes,std::vector<uint32_t>&out){out.resize(bytes/4);return std::fseek(file,long(offset),SEEK_SET)==0&&std::fread(out.data(),1,bytes,file)==bytes;}
};
struct Timing {double submit=0,finish=0,wait=0,gpu=0,readback=0,invalidate=0;};
uint32_t calibration(uint32_t x,uint32_t y){const auto a=x/16,b=y/8;return ((13*a+7*b+3)&255)|(((5*a+29*b+11)&255)<<8)|(((23*a+17*b+19)&255)<<16)|(((31*a+37*b+41)&255)<<24);}
struct Target {
 C3D_RenderTarget*target=nullptr;uint32_t*readback=nullptr;uint32_t width=0,height=0,padded=0;int transform=-1;
 bool create(uint32_t w,uint32_t h){width=w;height=h;padded=(h+7)&~7u;target=C3D_RenderTargetCreate(padded,width,GPU_RB_RGBA8,-1);readback=static_cast<uint32_t*>(linearAlloc(size_t(width)*padded*4));return target&&readback;}
 void release(){if(target)C3D_RenderTargetDelete(target);if(readback)linearFree(readback);target=nullptr;readback=nullptr;}
 uint32_t pixel(uint32_t x,uint32_t y,int mode)const{const auto rx=(mode&1)?padded-1-y:y,ry=(mode&2)?width-1-x:x;const auto c=readback[size_t(ry)*padded+rx];return(mode&4)?__builtin_bswap32(c):c;}
 bool draw_begin(bool blend){
  C2D_TargetClear(target,0);if(!C3D_FrameDrawOn(target))return false;C2D_SceneSize(padded,width,true);C2D_ViewReset();C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);C3D_AlphaTest(false,GPU_ALWAYS,0);
  C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,blend?GPU_SRC_ALPHA:GPU_ONE,blend?GPU_ONE_MINUS_SRC_ALPHA:GPU_ZERO,GPU_ONE,blend?GPU_ONE_MINUS_SRC_ALPHA:GPU_ZERO);return true;
 }
 bool finish_read(Timing&t){
  C2D_Flush();u64 start=svcGetSystemTick();end();t.finish=elapsed(start);start=svcGetSystemTick();if(!begin())return false;t.wait=elapsed(start);t.gpu=C3D_GetDrawingTime();start=svcGetSystemTick();
  const uint32_t dim=GX_BUFFER_DIM(padded,width),flags=GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
  C3D_SyncDisplayTransfer(static_cast<uint32_t*>(target->frameBuf.colorBuf),dim,readback,dim,flags);end();if(!begin())return false;t.readback=elapsed(start);start=svcGetSystemTick();const auto r=GSPGPU_InvalidateDataCache(readback,size_t(width)*padded*4);t.invalidate=elapsed(start);return R_SUCCEEDED(r);
 }
 bool calibrate(){
  if(!draw_begin(false))return false;
  for(uint32_t y=0;y<padded;y+=8)for(uint32_t x=0;x<width;x+=16)if(!C2D_DrawRectSolid(float(x),float(y),0,float(std::min<uint32_t>(16,width-x)),float(std::min<uint32_t>(8,padded-y)),calibration(x,y)))return false;
  Timing t;if(!finish_read(t))return false;unsigned matches=0;
  for(int mode=0;mode<8;++mode){uint32_t mismatch=0;for(uint32_t y=0;y<padded;++y)for(uint32_t x=0;x<width;++x)mismatch+=pixel(x,y,mode)!=calibration(x,y);message("CAL size=%lux%lu mode=%d mismatch=%lu\n",(unsigned long)width,(unsigned long)padded,mode,(unsigned long)mismatch);if(!mismatch){transform=mode;++matches;}}
  message("CAL_RESULT exact_matches=%u transform=%d\n",matches,transform);return matches==1;
 }
 uint32_t compare(const std::vector<uint32_t>&want,uint32_t&max_channel_delta){
  uint32_t failures=0;max_channel_delta=0;
  for(uint32_t y=0;y<padded;++y)for(uint32_t x=0;x<width;++x){const auto expected=y<height?want[size_t(y)*width+x]:0,got=pixel(x,y,transform);if(got!=expected){if(failures<3)message("GPU_DIFFERENCE x=%lu y=%lu expected=%08lx got=%08lx\n",(unsigned long)x,(unsigned long)y,(unsigned long)expected,(unsigned long)got);++failures;for(unsigned shift=0;shift<32;shift+=8)max_channel_delta=std::max(max_channel_delta,uint32_t(std::abs(int((expected>>shift)&255)-int((got>>shift)&255))));}}
  return failures;
 }
};
bool run(){
 WorldEffectData data;std::string error;Fixture fixture;if(!data.load_file("romfs:/melody.encfx",error)||!fixture.load()){message("FAIL fixture/resource %s\n",error.c_str());return false;}
 const uint32_t widths[]={320,400},heights[]={180,240};
 for(unsigned mode=0;mode<2;++mode){const auto width=widths[mode],height=heights[mode];end();Target target;WorldEffectRenderer renderer;WorldEffectKernel kernel;
  if(!target.create(width,height)||!renderer.load(data.view(),width,height,error)||!kernel.prepare(data.view(),width,height,error)||!begin()||!target.calibrate()){message("FAIL target/renderer/kernel/calibration %s\n",error.c_str());return false;}
  std::vector<uint32_t>cpu_expected,gpu_expected,actual(size_t(width)*height);
  for(unsigned i=0;i<fixture.cases.size();++i){const auto&c=fixture.cases[i];if(c.width!=width)continue;
   if(!fixture.pixels(c.cpu_offset,c.cpu_bytes,cpu_expected)||!fixture.pixels(c.gpu_offset,c.gpu_bytes,gpu_expected)){message("FAIL fixture payload\n");return false;}
   const auto start=svcGetSystemTick();if(!kernel.compose(c.time,c.sample,actual.data(),actual.size()))return false;const double standalone=elapsed(start);
   uint32_t cpu_mismatch=0;for(size_t j=0;j<actual.size();++j)if(actual[j]!=cpu_expected[j]){if(cpu_mismatch<3)message("CPU_DIFFERENCE case=%u index=%lu expected=%08lx got=%08lx\n",i,(unsigned long)j,(unsigned long)cpu_expected[j],(unsigned long)actual[j]);++cpu_mismatch;}
   cpu_pixels+=actual.size();cpu_failures+=cpu_mismatch;
   for(unsigned repeat=0;repeat<3;++repeat){
    if(!renderer.compose(c.time,c.sample,error)){message("FAIL renderer compose %s\n",error.c_str());return false;}
    Timing t;const auto submit=svcGetSystemTick();if(!target.draw_begin(true))return false;renderer.draw();C2D_Flush();t.submit=elapsed(submit);if(!target.finish_read(t))return false;
    uint32_t delta=0;const auto gpu_mismatch=target.compare(gpu_expected,delta);gpu_pixels+=actual.size();gpu_failures+=gpu_mismatch;
    message("CASE index=%u w=%lu h=%lu time=%.9g alpha=%.9g repeat=%u cpu_mismatch=%lu gpu_mismatch=%lu max_channel_delta=%lu standalone_ms=%.3f kernel_ms=%.3f upload_ms=%.3f submit_ms=%.3f finish_ms=%.3f wait_ms=%.3f gpu_queue_ms=%.3f readback_ms=%.3f invalidate_ms=%.3f\n",i,(unsigned long)width,(unsigned long)height,double(c.time),double(c.sample.alpha),repeat,(unsigned long)cpu_mismatch,(unsigned long)gpu_mismatch,(unsigned long)delta,standalone,renderer.arm_kernel_ms,renderer.arm_upload_ms,t.submit,t.finish,t.wait,t.gpu,t.readback,t.invalidate);
   }
  }
  end();renderer.free();target.release();if(!begin())return false;
 }
 return cpu_failures==0&&gpu_failures==0;
}
}
int main(){
 gfxInitDefault();consoleInit(GFX_BOTTOM,nullptr);gfxSetDoubleBuffering(GFX_BOTTOM,false);log_file=std::fopen("sdmc:/encore-world-effect-arm.log","w");
 message("WORLD_EFFECT_ARM_REFERENCE_V1\n20 retained native cases; production renderer with timing-only instrumentation\nNumeric RGBA8 render-target transfer, no screenshot or display capture\n");
 const auto rom=romfsInit();const bool c3d=C3D_Init(C3D_DEFAULT_CMDBUF_SIZE),c2d=c3d&&C2D_Init(2048);bool pass=false;
 if(c2d&&R_SUCCEEDED(rom)&&log_file){C2D_Prepare();if(begin())pass=run();}end();
 message("RESULT %s cpu_pixels=%llu gpu_pixels=%llu cpu_differences=%lu gpu_differences=%lu source_cases=20 timed_draws=60 calibration_targets=2\nEmulator measurements are not physical hardware; sampled equivalence only\nPress START to exit\n",pass?"PASS":"FAIL",(unsigned long long)cpu_pixels,(unsigned long long)gpu_pixels,(unsigned long)cpu_failures,(unsigned long)gpu_failures);
 if(log_file){std::fclose(log_file);log_file=nullptr;}
 while(aptMainLoop()){hidScanInput();if(hidKeysDown()&KEY_START)break;gfxFlushBuffers();gspWaitForVBlank();}
 if(c2d)C2D_Fini();
 if(c3d)C3D_Fini();
 if(R_SUCCEEDED(rom))romfsExit();
 gfxExit();return pass?0:1;
}

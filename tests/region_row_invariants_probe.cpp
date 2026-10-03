// The same probe builds against the frozen before-header or the current header.
// It reads checked RomFS, compares every pixel/texture byte with the frozen
// scalar oracle, and reports paired-case timing. It never accesses game saves.
#ifdef __3DS__
#include <3ds.h>
#else
#include <chrono>
#endif
#include <cstdarg>
#include "tests/gpu_span_common.hpp"
#include "encore/region_background_kernel.hpp"
#ifndef ENCORE_REGION_PROBE_TAG
#define ENCORE_REGION_PROBE_TAG "unlabelled"
#endif
#ifndef ENCORE_REGION_PROBE_LOG
#define ENCORE_REGION_PROBE_LOG "sdmc:/encore-region-row-probe.log"
#endif
namespace {
using Kernel=encore::RegionBackgroundKernel;
FILE* logfile=nullptr;
uint64_t pixels=0,mapped_bytes=0;
unsigned cases=0;
void emit(const char* format,...){
 va_list a;va_start(a,format);std::vprintf(format,a);va_end(a);
 if(logfile){va_start(a,format);std::vfprintf(logfile,format,a);va_end(a);std::fflush(logfile);}
}
uint64_t tick(){
#ifdef __3DS__
 return svcGetSystemTick();
#else
 return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
}
double milliseconds(uint64_t elapsed){
#ifdef __3DS__
 return double(elapsed)/CPU_TICKS_PER_MSEC;
#else
 return double(elapsed)/1000000.0;
#endif
}
uint32_t gpu(uint32_t c){return ((c&255)<<24)|((c&0xff00)<<8)|((c>>8)&0xff00)|(c>>24);}
bool run(const gpu_span::Content& content){
 std::string error;
 for(const auto& size:{std::pair<uint32_t,uint32_t>{400,240},{320,180}}){
  const auto w=size.first,h=size.second;const size_t count=size_t(w)*h,texture_count=512*256;
  Kernel candidate;encore::ScalarBackgroundKernel scalar;
  std::vector<Kernel::Layer> layers;for(unsigned n=0;n<2;++n)layers.push_back(gpu_span::layer<Kernel>(content.data.view().background(n),content.source[n],content.palette[n]));
  const auto start=tick();
  if(!candidate.prepare(layers,w,h,error)||!candidate.region_fast_path()){emit("FAIL prepare %s\n",error.c_str());return false;}
  const double prepare_ms=milliseconds(tick()-start);
  if(!scalar.prepare(content.scalar_layers,w,h,error)){emit("FAIL scalar prepare %s\n",error.c_str());return false;}
  std::vector<uint32_t> offsets(count),expected(count),actual(count),mapped(texture_count+2),mapped_expected(texture_count);
  for(uint32_t y=0;y<h;++y)for(uint32_t x=0;x<w;++x){const uint32_t m=(x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);offsets[size_t(y)*w+x]=((y/8)*64+x/8)*64+m;}
  if(!candidate.prepare_mapped_output(offsets.data(),count,texture_count,error)){emit("FAIL map prepare\n");return false;}
  emit("PREPARE tag=%s width=%u height=%u ms=%.6f retained_bytes=%llu\n",ENCORE_REGION_PROBE_TAG,unsigned(w),unsigned(h),prepare_ms,(unsigned long long)candidate.prepared_bytes());
  for(unsigned repeat=0;repeat<2;++repeat)for(float time:{0.f,1.25f,60.f,1000.f}){
   if(!scalar.compose(time,gpu_span::clear_color,expected.data())||!candidate.compose(time,gpu_span::clear_color,actual.data())||expected!=actual){emit("FAIL linear pixels t=%g\n",double(time));return false;}
   pixels+=count;
   std::fill(mapped_expected.begin(),mapped_expected.end(),0xa5a5a5a5);
   for(size_t i=0;i<count;++i)mapped_expected[offsets[i]]=gpu(expected[i]);
   std::vector<double> timings;uint64_t samples=0,skipped=0;
   for(unsigned iteration=0;iteration<10;++iteration){
    std::fill(mapped.begin(),mapped.end(),0xa5a5a5a5);
    const auto begin=tick();const bool ok=candidate.compose_mapped(time,gpu_span::clear_color,mapped.data()+1,texture_count);const double elapsed=milliseconds(tick()-begin);
    if(!ok||mapped.front()!=0xa5a5a5a5||mapped.back()!=0xa5a5a5a5||!std::equal(mapped_expected.begin(),mapped_expected.end(),mapped.begin()+1)){emit("FAIL mapped bytes t=%g\n",double(time));return false;}
    mapped_bytes+=texture_count*4;
    const auto stats=candidate.region_stats();
    if(iteration&&(stats.samples!=samples||stats.skipped!=skipped)){emit("FAIL unstable statistics\n");return false;}
    samples=stats.samples;skipped=stats.skipped;
    if(iteration>=2)timings.push_back(elapsed);
   }
   if(samples+skipped!=2*count){emit("FAIL sample coverage\n");return false;}
   std::sort(timings.begin(),timings.end());const double median=(timings[3]+timings[4])*0.5;
   ++cases;
   emit("CASE tag=%s width=%u height=%u time=%.2f repeat=%u median_ms=%.6f min_ms=%.6f max_ms=%.6f samples=%llu skipped=%llu PASS\n",ENCORE_REGION_PROBE_TAG,unsigned(w),unsigned(h),double(time),repeat,median,timings.front(),timings.back(),(unsigned long long)samples,(unsigned long long)skipped);
  }
 }
 return true;
}
}
int main(int argc,char**argv){
#ifdef __3DS__
 (void)argc;(void)argv;gfxInitDefault();consoleInit(GFX_BOTTOM,nullptr);gfxSetDoubleBuffering(GFX_BOTTOM,false);
 logfile=std::fopen(ENCORE_REGION_PROBE_LOG,"w");const Result result=romfsInit();const char* root="romfs:";
 const bool mounted=R_SUCCEEDED(result);
#else
 const char* root=argc>1?argv[1]:"romfs";const bool mounted=true;
#endif
 emit("Region row-invariant probe tag=%s; exact CPU oracle, not GPU/full-game/hardware parity\n",ENCORE_REGION_PROBE_TAG);
 gpu_span::Content content;std::string error;const bool ok=mounted&&content.load(root,error)&&run(content);
 if(!ok&&!error.empty())emit("FAIL %s\n",error.c_str());
 emit("%s cases=%u linear_pixels=%llu mapped_bytes=%llu\n",ok?"ALL CHECKS PASS":"SELF CHECK FAILED",cases,(unsigned long long)pixels,(unsigned long long)mapped_bytes);
 if(logfile)std::fclose(logfile);
#ifdef __3DS__
 gfxFlushBuffers();gfxSwapBuffers();
 while(aptMainLoop()){hidScanInput();if(hidKeysDown()&KEY_START)break;gspWaitForVBlank();}
 if(mounted){romfsExit();}gfxExit();
#endif
 return ok?0:1;
}

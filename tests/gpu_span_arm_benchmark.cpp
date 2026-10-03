#include <3ds.h>
#include <citro2d.h>
#include <cstdarg>
#include "tests/gpu_span_common.hpp"

namespace {
FILE* diagnostic=nullptr;
uint32_t draw_calls=0,draw_indices=0;
uint64_t compared_pixels=0;
void message(const char* format,...){
    va_list args;va_start(args,format);std::vprintf(format,args);va_end(args);
    if(diagnostic){va_start(args,format);std::vfprintf(diagnostic,format,args);va_end(args);std::fflush(diagnostic);}
    gfxFlushBuffers();
}
double elapsed(u64 tick){return double(svcGetSystemTick()-tick)/CPU_TICKS_PER_MSEC;}
uint32_t morton(uint32_t x,uint32_t y){return (x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);}
struct Timing {double submit=0,finish=0,wait=0,gpu=0,readback=0,invalidate=0;};
struct Target {
    C3D_RenderTarget* target=nullptr;
    uint32_t* readback=nullptr;
    uint32_t width=0,height=0,padded_height=0;
    int transform=-1;
    bool create(uint32_t w,uint32_t h){
        width=w;height=h;padded_height=(h+7)&~7u;
        // Match the sideways 3DS framebuffer. 180 high is padded to 184 so
        // the tiled target has whole 8x8 tiles; logical content stays 320x180.
        target=C3D_RenderTargetCreate(int(padded_height),int(width),GPU_RB_RGBA8,-1);
        readback=static_cast<uint32_t*>(linearAlloc(size_t(width)*padded_height*4));
        return target&&readback;
    }
    void release(){if(target)C3D_RenderTargetDelete(target);if(readback)linearFree(readback);target=nullptr;readback=nullptr;}
    uint32_t pixel(uint32_t x,uint32_t y,int mode)const{
        const uint32_t rx=(mode&1)?padded_height-1-y:y;
        const uint32_t ry=(mode&2)?width-1-x:x;
        const auto c=readback[size_t(ry)*padded_height+rx];
        return (mode&4)?__builtin_bswap32(c):c;
    }
    bool draw_read(const std::vector<gpu_span::Span>& spans,Timing& timing){
        // A frame is already open. End/begin fences below wait for the GPU
        // queue without VBlank pacing; readback is kept outside render timing.
        draw_calls=draw_indices=0;
        u64 tick=svcGetSystemTick();
        C2D_TargetClear(target,gpu_span::clear_color);
        if(!C3D_FrameDrawOn(target))return false;
        C2D_SceneSize(padded_height,width,true);
        C2D_ViewReset();
        C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);
        C3D_AlphaTest(false,GPU_ALWAYS,0);
        C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ZERO,GPU_ONE,GPU_ZERO);
        for(const auto& s:spans){
            if(!C2D_DrawRectSolid(float(s.x),float(s.y),0,float(s.width),1,s.color))return false;
        }
        C2D_Flush();
        timing.submit=elapsed(tick);
        tick=svcGetSystemTick();C3D_FrameEnd(0);timing.finish=elapsed(tick);
        tick=svcGetSystemTick();if(!C3D_FrameBegin(0))return false;timing.wait=elapsed(tick);
        timing.gpu=C3D_GetDrawingTime();
        if(draw_calls!=1||draw_indices!=spans.size()*6){message("FAIL batching calls=%lu indices=%lu spans=%lu\n",(unsigned long)draw_calls,(unsigned long)draw_indices,(unsigned long)spans.size());return false;}
        // This second queue only transfers RGBA8 to RGBA8 and detiles it.
        // It has no RGB565 quantization, scale, display conversion, or UI capture.
        tick=svcGetSystemTick();
        const uint32_t dim=GX_BUFFER_DIM(padded_height,width);
        const uint32_t flags=GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|
            GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8)|
            GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
        C3D_SyncDisplayTransfer(static_cast<uint32_t*>(target->frameBuf.colorBuf),dim,readback,dim,flags);
        C3D_FrameEnd(0);
        if(!C3D_FrameBegin(0))return false;
        timing.readback=elapsed(tick);
        tick=svcGetSystemTick();
        const Result result=GSPGPU_InvalidateDataCache(readback,size_t(width)*padded_height*4);
        timing.invalidate=elapsed(tick);
        if(R_FAILED(result)){message("FAIL invalidate %08lx\n",(unsigned long)result);return false;}
        return true;
    }
    bool calibrate(std::vector<gpu_span::Span>& spans){
        std::vector<uint32_t> expected(size_t(width)*padded_height);
        for(uint32_t y=0;y<padded_height;++y)for(uint32_t x=0;x<width;++x)expected[size_t(y)*width+x]=gpu_span::calibration_pixel(x,y);
        if(!gpu_span::encode(expected.data(),width,padded_height,spans))return false;
        Timing timing;if(!draw_read(spans,timing))return false;
        unsigned matches=0;
        for(int mode=0;mode<8;++mode){
            uint32_t mismatches=0;
            for(uint32_t y=0;y<padded_height;++y)for(uint32_t x=0;x<width;++x)mismatches+=pixel(x,y,mode)!=expected[size_t(y)*width+x];
            message("CAL mode=%d %lux%lu mismatch=%lu\n",mode,(unsigned long)width,(unsigned long)padded_height,(unsigned long)mismatches);
            if(!mismatches){transform=mode;++matches;}
        }
        if(matches!=1){message("FAIL calibration exact_matches=%u raw0=%08lx\n",matches,(unsigned long)readback[0]);return false;}
        message("CAL PASS transform=%d pixel_count=%lu alpha=all_channels\n",transform,(unsigned long)expected.size());
        return true;
    }
    bool compare(const std::vector<uint32_t>& expected){
        uint32_t mismatches=0;
        for(uint32_t y=0;y<padded_height;++y)for(uint32_t x=0;x<width;++x){
            const auto want=y<height?expected[size_t(y)*width+x]:gpu_span::clear_color;
            const auto got=pixel(x,y,transform);
            if(want!=got){
                if(mismatches<4)message("MISMATCH x=%lu y=%lu wanted=%08lx got=%08lx\n",(unsigned long)x,(unsigned long)y,(unsigned long)want,(unsigned long)got);
                ++mismatches;
            }
        }
        if(mismatches){message("FAIL GPU parity mismatch=%lu\n",(unsigned long)mismatches);return false;}
        compared_pixels+=size_t(width)*height;
        return true;
    }
};
bool run(){
    std::string error;gpu_span::Content content;
    if(!content.load("romfs:",error)){message("FAIL data %s\n",error.c_str());return false;}
    const uint32_t widths[]={400,320},heights[]={240,180};
    const float times[]={0,1.25f,60,1000};
    std::vector<gpu_span::Span> spans;spans.reserve(gpu_span::max_spans);
    for(unsigned mode=0;mode<2;++mode){
        const auto width=widths[mode],height=heights[mode];const size_t count=size_t(width)*height;
        encore::FrozenBackgroundKernel generator;encore::ScalarBackgroundKernel oracle;
        u64 tick=svcGetSystemTick();
        if(!generator.prepare(content.layers,width,height,error)||!oracle.prepare(content.scalar_layers,width,height,error)){message("FAIL prepare %s\n",error.c_str());return false;}
        const double prepare_ms=elapsed(tick);
        uint32_t tw=8,th=8;while(tw<width)tw<<=1;while(th<height)th<<=1;
        std::vector<uint32_t> offsets(count),mapped(size_t(tw)*th),expected(count),actual(count),reconstructed(count);
        for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x)offsets[size_t(y)*width+x]=((y/8)*(tw/8)+x/8)*64+morton(x,y);
        if(!generator.prepare_mapped_output(offsets.data(),offsets.size(),mapped.size(),error)){message("FAIL mapped prepare\n");return false;}
        // All previous queues are complete here, but the transfer fence left an
        // empty frame open. Close it before creating/deleting render targets.
        C3D_FrameEnd(0);
        Target surface;if(!surface.create(width,height)){message("FAIL target allocation\n");return false;}
        if(!C3D_FrameBegin(0)||!surface.calibrate(spans))return false;
        message("SIZE %lux%lu prepare_ms=%.3f physical=%lux%lu\n",(unsigned long)width,(unsigned long)height,prepare_ms,(unsigned long)surface.padded_height,(unsigned long)width);
        for(float time:times){
            tick=svcGetSystemTick();if(!oracle.compose(time,gpu_span::clear_color,expected.data()))return false;
            const double oracle_ms=elapsed(tick);
            for(unsigned repeat=0;repeat<3;++repeat){
                tick=svcGetSystemTick();if(!generator.compose_mapped(time,gpu_span::clear_color,mapped.data(),mapped.size()))return false;
                const double mapped_ms=elapsed(tick);
                tick=svcGetSystemTick();if(!generator.compose(time,gpu_span::clear_color,actual.data()))return false;
                const double generation_ms=elapsed(tick);
                if(actual!=expected){message("FAIL CPU oracle parity t=%.2f\n",double(time));return false;}
                tick=svcGetSystemTick();if(!gpu_span::encode(actual.data(),width,height,spans)){message("FAIL span capacity\n");return false;}
                const double encode_ms=elapsed(tick);
                gpu_span::reconstruct(spans,width,reconstructed);
                if(reconstructed!=expected){message("FAIL reconstruction\n");return false;}
                Timing timing;if(!surface.draw_read(spans,timing)||!surface.compare(expected))return false;
                const double candidate_ms=generation_ms+encode_ms+timing.submit+timing.finish+timing.wait;
                message("FRAME w=%lu h=%lu t=%.2f repeat=%u spans=%lu vertices=%lu draws=%lu scalar_ms=%.3f mapped_ms=%.3f generation_ms=%.3f encode_ms=%.3f submit_ms=%.3f finish_ms=%.3f wait_ms=%.3f gpu_queue_ms=%.3f render_total_ms=%.3f candidate_total_ms=%.3f readback_ms=%.3f invalidate_ms=%.3f PARITY=PASS\n",(unsigned long)width,(unsigned long)height,double(time),repeat,(unsigned long)spans.size(),(unsigned long)(4*spans.size()),(unsigned long)draw_calls,oracle_ms,mapped_ms,generation_ms,encode_ms,timing.submit,timing.finish,timing.wait,timing.gpu,timing.submit+timing.finish+timing.wait,candidate_ms,timing.readback,timing.invalidate);
            }
        }
        C3D_FrameEnd(0);surface.release();if(!C3D_FrameBegin(0))return false;
    }
    return true;
}
}
extern "C" void __real_C3D_DrawElements(GPU_Primitive_t,int,int,const void*);
extern "C" void __wrap_C3D_DrawElements(GPU_Primitive_t primitive,int count,int type,const void* indices){
    ++draw_calls;draw_indices+=uint32_t(count);__real_C3D_DrawElements(primitive,count,type,indices);
}
int main(){
    gfxInitDefault();consoleInit(GFX_BOTTOM,nullptr);gfxSetDoubleBuffering(GFX_BOTTOM,false);
    diagnostic=std::fopen("sdmc:/encore-gpu-span-benchmark.log","w");
    message("GPU span benchmark v1\nFrozen e722c99c / scalar 883d150d\nCPU oracle parity only; no Godot/hardware claim\n");
    const Result romfs=romfsInit();
    const bool c3d=C3D_Init(C3D_DEFAULT_CMDBUF_SIZE);
    const bool c2d=c3d&&C2D_Init(gpu_span::max_spans);
    bool passed=false;
    if(c2d&&R_SUCCEEDED(romfs)){C2D_Prepare();if(C3D_FrameBegin(0))passed=run();C3D_FrameEnd(0);}
    message("RESULT %s game_pixels_compared=%llu calibration_frames=2 game_frames=24\nNo production changes. Emulator timing is not hardware.\nPress START to exit\n",passed?"PASS":"FAIL",(unsigned long long)compared_pixels);
    if(diagnostic){std::fclose(diagnostic);diagnostic=nullptr;}
    while(aptMainLoop()){hidScanInput();if(hidKeysDown()&KEY_START)break;gfxFlushBuffers();gspWaitForVBlank();}
    if(c2d)C2D_Fini();
    if(c3d)C3D_Fini();
    if(R_SUCCEEDED(romfs))romfsExit();
    gfxExit();return passed?0:1;
}

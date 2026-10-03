#pragma once
#include <cstdarg>
#include "tests/gpu_span_common.hpp"
#include "encore/row_linear_background_kernel.hpp"
#include "platform/ctr/gpu_region_batch.hpp"
#include "platform/ctr/gpu_row_texture_batch.hpp"
namespace pillow_gpu_probe {
static FILE* log_file=nullptr;
static uint64_t compared_pixels=0;
static encore::ctr::GpuRegionBatch pica;
static encore::ctr::GpuRowTextureBatch texture_batch;
static double elapsed(u64 tick){return double(svcGetSystemTick()-tick)/CPU_TICKS_PER_MSEC;}
static void message(const char* format,...){if(!log_file)return;va_list args;va_start(args,format);std::vfprintf(log_file,format,args);va_end(args);std::fflush(log_file);}
static uint32_t morton(uint32_t x,uint32_t y){return (x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);}
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
    bool draw_read(size_t span_count,Timing& timing,const C2D_Image* image=nullptr,bool texture=false){
        // A frame is already open. End/begin fences below wait for the GPU
        // queue without VBlank pacing; readback is kept outside render timing.
        
        u64 tick=svcGetSystemTick();
        C2D_TargetClear(target,gpu_span::clear_color);
        if(!C3D_FrameDrawOn(target))return false;
        C2D_Prepare();
        C2D_SceneSize(padded_height,width,true);
        C2D_ViewReset();
        C3D_DepthTest(false,GPU_ALWAYS,GPU_WRITE_COLOR);
        C3D_AlphaTest(false,GPU_ALWAYS,0);
        C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ZERO,GPU_ONE,GPU_ZERO);
        if(image){if(!C2D_DrawImageAt(*image,0,0,0,nullptr,1,1))return false;}
        else {
            if(texture&&!texture_batch.draw())return false;
            if(span_count&&!pica.draw(span_count,0,0))return false;
        }
        C2D_Flush();
        timing.submit=elapsed(tick);
        tick=svcGetSystemTick();C3D_FrameEnd(0);timing.finish=elapsed(tick);
        tick=svcGetSystemTick();if(!C3D_FrameBegin(0))return false;timing.wait=elapsed(tick);
        timing.gpu=C3D_GetDrawingTime();
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
    bool calibrate(){
        std::vector<uint32_t> expected(size_t(width)*padded_height);
        for(uint32_t y=0;y<padded_height;++y)for(uint32_t x=0;x<width;++x)expected[size_t(y)*width+x]=gpu_span::calibration_pixel(x,y);
        size_t count=0;for(uint32_t y=0;y<padded_height;++y)for(uint32_t x=0;x<width;x+=16){auto& span=pica.data()[count++];span.x=x;span.y=y;span.width=std::min<uint32_t>(16,width-x);span.color=expected[size_t(y)*width+x];}
        Timing timing;if(!draw_read(count,timing))return false;
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

static bool run(FILE* log){
 log_file=log;encore::upstream::BattleData data;std::string error;
 if(!data.load_file("romfs:/data/pillow-entry.encbattle",error)){message("READBACK FAIL load %s\n",error.c_str());return false;}
 const auto v=data.view();gpu_span::Image src[2],pal[2];std::vector<encore::BackgroundKernel::Layer> layers;std::vector<encore::ScalarBackgroundKernel::Layer> scalar;
 for(unsigned n=0;n<2;++n){const auto b=v.background(n);const auto r=v.resource(b.resource);if(!gpu_span::image("romfs:/"+std::string(v.string(r.path)),r,src[n]))return false;layers.push_back(gpu_span::layer<encore::BackgroundKernel>(b,src[n],pal[n]));scalar.push_back(gpu_span::layer<encore::ScalarBackgroundKernel>(b,src[n],pal[n]));}
 constexpr uint32_t w=400,h=240;encore::RowLinearBackgroundKernel direct;encore::BackgroundKernel baseline;encore::ScalarBackgroundKernel oracle;
 if(!direct.prepare(layers,w,h)||!baseline.prepare(layers,w,h,error)||!oracle.prepare(scalar,w,h,error)||!pica.create(16384)||!texture_batch.create(layers,w,h))return false;
 Target surface;if(!surface.create(w,h))return false;
 C3D_Tex texture{};if(!C3D_TexInit(&texture,512,256,GPU_RGBA8))return false;C3D_TexSetFilter(&texture,GPU_NEAREST,GPU_NEAREST);C3D_TexSetWrap(&texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);std::memset(texture.data,0,texture.size);
 Tex3DS_SubTexture sub={u16(w),u16(h),0,1,float(w)/512,1-float(h)/256};C2D_Image image={&texture,&sub};
 std::vector<uint32_t> expected(size_t(w)*h),cpu(expected.size());
 if(!C3D_FrameBegin(0)||!surface.calibrate())return false;
 message("ROW_SETUP bytes=%lu linear_bytes=%lu cap=%lu\n",(unsigned long)direct.prepared_bytes(),(unsigned long)(pica.capacity()*sizeof(encore::ctr::GpuRegionBatch::Span)),(unsigned long)pica.capacity());
 for(float time:{0.f,1.25f,60.f,1000.f}){
  if(!oracle.compose(time,gpu_span::clear_color,expected.data()))return false;
  size_t count=0;auto tick=svcGetSystemTick();if(!direct.generate_spans(time,pica.data(),pica.capacity(),count))return false;const double cpu_ms=elapsed(tick);
  Timing before;if(!surface.draw_read(count,before)||!surface.compare(expected))return false;
  tick=svcGetSystemTick();if(!texture_batch.prepare_frame(direct,time)||!direct.generate_spans(time,pica.data(),pica.capacity(),count,texture_batch.certified_rows()))return false;const double generate_ms=elapsed(tick);
  Timing after;if(!surface.draw_read(count,after,nullptr,true))return false;
  if(!surface.compare(expected))return false;
  message("TEXTURE_FRAME t=%.3f rows=%lu fallback_rows=%lu strips=%lu fallback_spans=%lu baseline_cpu=%.3f baseline_total=%.3f generate_ms=%.3f gpu_submit=%.3f gpu_finish=%.3f gpu_wait=%.3f total=%.3f extra_bytes=%lu PARITY=PASS\n",double(time),(unsigned long)texture_batch.accepted_rows(),(unsigned long)(240-texture_batch.accepted_rows()),(unsigned long)texture_batch.count(),(unsigned long)count,cpu_ms,cpu_ms+before.submit+before.finish+before.wait,generate_ms,after.submit,after.finish,after.wait,generate_ms+after.submit+after.finish+after.wait,(unsigned long)texture_batch.tracked_bytes());
 }
 size_t count=123;const bool cap=!direct.generate_spans(0,pica.data(),1,count)&&count==0;
 C3D_FrameEnd(0);surface.release();C3D_TexDelete(&texture);pica.release();texture_batch.release();
 message("READBACK %s pixels=%llu calibration=1 cap_empty=%u\n",cap?"PASS":"FAIL",(unsigned long long)compared_pixels,unsigned(cap));return cap;
}
}

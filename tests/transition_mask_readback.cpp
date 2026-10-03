#include <3ds.h>
#include <citro2d.h>
#include <cstdarg>
#include "tests/transition_mask_test_common.hpp"
#include "tests/gpu_span_common.hpp"
#include "encore/row_linear_background_kernel.hpp"
#include "platform/ctr/gpu_transition_mask_batch.hpp"
namespace {
using namespace transition_mask_test;
FILE* log_file=nullptr;uint64_t pixels=0;uint32_t frames=0;
void message(const char* format,...){va_list args;va_start(args,format);std::vprintf(format,args);va_end(args);if(log_file){va_start(args,format);std::vfprintf(log_file,format,args);va_end(args);std::fflush(log_file);}gfxFlushBuffers();}
uint32_t morton(uint32_t x,uint32_t y){return (x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);}
double elapsed(u64 tick){return double(svcGetSystemTick()-tick)/CPU_TICKS_PER_MSEC;}
struct Harness {
    C3D_RenderTarget* target=nullptr;uint32_t* readback=nullptr;C3D_Tex texture{};Tex3DS_SubTexture sub{};bool texture_ready=false;
    encore::ctr::GpuRegionBatch world;encore::ctr::GpuTransitionMaskBatch mask;size_t world_count=0;
    std::vector<uint32_t> prior;
    bool create(){
        target=C3D_RenderTargetCreate(240,400,GPU_RB_RGBA8,-1);readback=static_cast<uint32_t*>(linearAlloc(400*240*4));
        if(!target||!readback||!world.create(8192)||!C3D_TexInit(&texture,512,256,GPU_RGBA8))return false;
        texture_ready=true;C3D_TexSetFilter(&texture,GPU_NEAREST,GPU_NEAREST);C3D_TexSetWrap(&texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);std::memset(texture.data,0,texture.size);
        for(uint32_t y=0;y<240;++y)for(uint32_t x=0;x<400;x+=16){auto& s=world.data()[world_count++];s={uint16_t(x),uint16_t(y),uint16_t(std::min<uint32_t>(16,400-x)),0,gpu_span::calibration_pixel(x,y)};}
        prior.resize(400*240);return true;
    }
    void release(){mask.release();world.release();if(texture_ready)C3D_TexDelete(&texture);if(target)C3D_RenderTargetDelete(target);if(readback)linearFree(readback);}
    void upload(const std::vector<uint32_t>& rgba,uint32_t width,uint32_t height){
        auto* dst=static_cast<uint32_t*>(texture.data);
        for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x){const uint32_t c=rgba[size_t(y)*width+x];dst[((y/8)*64+x/8)*64+morton(x,y)]=__builtin_bswap32(c);}
        C3D_TexFlush(&texture);sub={u16(width),u16(height),0,1,float(width)/512,1-float(height)/256};
    }
    bool draw(bool candidate,size_t count,uint32_t w,uint32_t h,double& cost){
        const u64 begin=svcGetSystemTick();C2D_TargetClear(target,0);if(!C3D_FrameDrawOn(target))return false;
        C2D_Prepare();C2D_SceneSize(240,400,true);C2D_ViewReset();
        if(!world.draw(world_count,0,0))return false;
        const float x=float(400-w)/2,y=float(240-h)/2;
        if(candidate){if(!mask.draw(count,x,y))return false;}
        else {if(!C2D_DrawImageAt({&texture,&sub},x,y,0,nullptr,1,1))return false;}
        // The same ordinary Citro2D sprite after both paths catches leaked
        // projection, TEV, alpha-test and blend state, including transparency.
        const Tex3DS_SubTexture stamp={8,8,0,1,8.f/512,1-8.f/256};
        if(!C2D_DrawImageAt({&texture,&stamp},7,9,0,nullptr,1,1))return false;
        C2D_Flush();C3D_FrameEnd(0);if(!C3D_FrameBegin(0))return false;cost=elapsed(begin);
        const uint32_t dim=GX_BUFFER_DIM(240,400),flags=GX_TRANSFER_FLIP_VERT(0)|GX_TRANSFER_OUT_TILED(0)|GX_TRANSFER_RAW_COPY(0)|GX_TRANSFER_IN_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_OUT_FORMAT(GX_TRANSFER_FMT_RGBA8)|GX_TRANSFER_SCALING(GX_TRANSFER_SCALE_NO);
        C3D_SyncDisplayTransfer(static_cast<uint32_t*>(target->frameBuf.colorBuf),dim,readback,dim,flags);
        C3D_FrameEnd(0);if(!C3D_FrameBegin(0)||R_FAILED(GSPGPU_InvalidateDataCache(readback,400*240*4)))return false;
        if(!candidate){std::copy_n(readback,400*240,prior.data());return true;}
        size_t mismatches=0;for(size_t i=0;i<prior.size();++i)if(prior[i]!=readback[i]){if(mismatches<4)message("MISMATCH raw=%lu cpu_texture=%08lx gpu_mask=%08lx\n",(unsigned long)i,(unsigned long)prior[i],(unsigned long)readback[i]);++mismatches;}
        if(mismatches){message("FAIL pixel_mismatches=%lu\n",(unsigned long)mismatches);return false;}pixels+=prior.size();++frames;return true;
    }
};
bool run(){
    Image source;if(!load("romfs:/battle-preview/transition.bpx",source))return false;
    encore::upstream::BattleData data;std::string error;if(!data.load_file("romfs:/data/pillow-entry.encbattle",error))return false;const auto v=data.view();
    gpu_span::Image src[2],pal[2];std::vector<encore::BackgroundKernel::Layer> layers;
    for(unsigned n=0;n<2;++n){const auto b=v.background(n);const auto r=v.resource(b.resource);if(!gpu_span::image("romfs:/"+std::string(v.string(r.path)),r,src[n]))return false;layers.push_back(gpu_span::layer<encore::BackgroundKernel>(b,src[n],pal[n]));}
    Harness harness;if(!harness.create())return false;
    for(const auto canvas:{Plan::Canvas{400,240,40,30},Plan::Canvas{320,180,0,0}}){
        Plan plan;encore::RowLinearBackgroundKernel background;
        if(!plan.prepare(source.source(),canvas,2*1024*1024,error)||!background.prepare(layers,canvas.width,canvas.height))return false;
        const auto resources=plan.resources(16384);if(!harness.mask.create(canvas.width,canvas.height,resources.maximum_output_spans))return false;
        std::vector<Span> bg(16384);std::vector<uint32_t> cpu(size_t(canvas.width)*canvas.height),surface(cpu.size());
        message("RESERVE canvas=%lux%lu cpu=%lu LINEAR=%lu cap=%lu max_mask=%lu\n",(unsigned long)canvas.width,(unsigned long)canvas.height,(unsigned long)resources.cpu_bytes,(unsigned long)harness.mask.linear_bytes(),(unsigned long)harness.mask.capacity(),(unsigned long)resources.maximum_frame_runs);
        if(!C3D_FrameBegin(0))return false;
        for(uint32_t frame=0;frame<source.frames;++frame){const float time=float(frame)*0.05137f;size_t bg_count=0,count=0;
            if(!background.generate_spans(time,bg.data(),bg.size(),bg_count))return false;reconstruct(bg.data(),bg_count,canvas.width,cpu);
            surface=cpu;oracle(source,canvas,frame,0xffffffffu,0x82ff0000u,surface);harness.upload(surface,canvas.width,canvas.height);
            const auto tick=svcGetSystemTick();if(!plan.compose(frame,0xffffffffu,0x82ff0000u,bg.data(),bg_count,harness.mask.data(),harness.mask.capacity(),count))return false;const double compose=elapsed(tick);
            double before=0,after=0;if(!harness.draw(false,count,canvas.width,canvas.height,before)||!harness.draw(true,count,canvas.width,canvas.height,after))return false;
            message("FRAME canvas=%lux%lu frame=%lu t=%.6f bg=%lu out=%lu compose_ms=%.3f CPU_texture_render_ms=%.3f GPU_mask_render_ms=%.3f PARITY=PASS\n",(unsigned long)canvas.width,(unsigned long)canvas.height,(unsigned long)frame,double(time),(unsigned long)bg_count,(unsigned long)count,compose,before,after);
        }
        C3D_FrameEnd(0);harness.mask.release();
    }
    // Worst-case split reservation and cross-65536 vertex offset. Every pixel
    // has different RGBA from its neighbor, with all 256 alpha values present.
    source={400,240,1,{},std::vector<uint8_t>(400*240)};
    for(unsigned i=0;i<256;++i)source.palette.push_back((i*29u&255u)|((i*53u&255u)<<8)|((i*97u&255u)<<16)|(i<<24));
    for(size_t i=0;i<source.indices.size();++i)source.indices[i]=uint8_t(i);
    Plan plan;const Plan::Canvas canvas{400,240,0,0};if(!plan.prepare(source.source(),canvas,2*1024*1024,error)||!harness.mask.create(400,240,96000))return false;
    std::vector<uint32_t> cpu(96000,0xe14779abu),surface=cpu;auto bg=encode(cpu,400,240);oracle(source,canvas,0,0xffffffffu,0x12578319u,surface);harness.upload(surface,400,240);size_t count=0;
    if(!plan.compose(0,0xffffffffu,0x12578319u,bg.data(),bg.size(),harness.mask.data(),harness.mask.capacity(),count)||count!=96000)return false;
    if(!C3D_FrameBegin(0))return false;double before=0,after=0;if(!harness.draw(false,count,400,240,before)||!harness.draw(true,count,400,240,after))return false;
    const bool guards=!harness.mask.draw(count,0.5f,0)&&!harness.mask.draw(count,1,0)&&!harness.mask.draw(count+1,0,0);
    harness.mask.data()[0].reserved=1;const bool malformed=!harness.mask.draw(count,0,0);harness.mask.data()[0].reserved=0;
    C3D_FrameEnd(0);message("STRESS spans=%lu alpha_values=256 draw_chunks=6 prior_C2D_sprite=PASS following_C2D_sprite=PASS guards=%u malformed=%u CPU_texture_render_ms=%.3f GPU_mask_render_ms=%.3f PARITY=PASS\n",(unsigned long)count,unsigned(guards),unsigned(malformed),before,after);
    harness.release();return guards&&malformed;
}
}
int main(){
    gfxInitDefault();consoleInit(GFX_BOTTOM,nullptr);gfxSetDoubleBuffering(GFX_BOTTOM,false);
    log_file=std::fopen("sdmc:/encore-transition-mask-readback.log","w");message("Exact transition-mask readback: CPU texture vs integer PICA spans; no save IO, timing/RNG/gameplay changes or physical-hardware claim\n");
    const Result romfs=romfsInit();const bool c3d=C3D_Init(C3D_DEFAULT_CMDBUF_SIZE),c2d=c3d&&C2D_Init(4096);bool passed=false;
    if(c2d&&R_SUCCEEDED(romfs)){C2D_Prepare();passed=run();}
    message("RESULT %s frames=%lu RGBA_pixels=%llu\n",passed?"PASS":"FAIL",(unsigned long)frames,(unsigned long long)pixels);
    if(log_file){std::fclose(log_file);log_file=nullptr;}
    if(c2d)C2D_Fini();if(c3d)C3D_Fini();if(R_SUCCEEDED(romfs))romfsExit();gfxExit();return passed?0:1;
}

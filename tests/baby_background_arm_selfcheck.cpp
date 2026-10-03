#include <3ds.h>
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "encore/battle_data.hpp"
#if defined(ENCORE_BLOCK_PROTOTYPE) || defined(ENCORE_INLINE_PROTOTYPE) || defined(ENCORE_COMBINED_PROTOTYPE)
#define BackgroundKernel GuardedBackgroundKernel
#include "tests/baby_background_guarded_reference.hpp"
#undef BackgroundKernel
#ifdef ENCORE_COMBINED_PROTOTYPE
#include "tests/baby_background_combined_prototype.hpp"
#define SELF_CHECK_LABEL "Combined prototype / Scalar 883d150d"
#elif defined(ENCORE_BLOCK_PROTOTYPE)
#include "tests/baby_background_block_prototype.hpp"
#define SELF_CHECK_LABEL "Block prototype / Scalar 883d150d"
#else
#include "tests/baby_background_inline_prototype.hpp"
#define SELF_CHECK_LABEL "Inline prototype / Scalar 883d150d"
#endif
#elif defined(ENCORE_GUARDED_REFERENCE)
#include "tests/baby_background_guarded_reference.hpp"
#define SELF_CHECK_LABEL "Guard b41cc969 / Scalar 883d150d"
#else
#include "encore/background_kernel.hpp"
#ifndef SELF_CHECK_KERNEL_TAG
#define SELF_CHECK_KERNEL_TAG "unrecorded"
#endif
#define SELF_CHECK_LABEL "Production " SELF_CHECK_KERNEL_TAG " / Scalar883d150d"
#endif
#define BackgroundKernel ScalarBackgroundKernel
#include "tests/baby_background_scalar_reference.hpp"
#undef BackgroundKernel

namespace {
FILE* diagnostic=nullptr;
uint64_t pixels_compared=0,mapped_bytes_compared=0;
void message(const char* format,...){
    va_list args;va_start(args,format);std::vprintf(format,args);va_end(args);
    if(diagnostic){va_start(args,format);std::vfprintf(diagnostic,format,args);va_end(args);std::fflush(diagnostic);}
    gfxFlushBuffers();gfxSwapBuffers();gspWaitForVBlank();
}
struct Image {uint32_t width=0,height=0;std::vector<uint8_t> indices;std::vector<uint32_t> palette;};
uint32_t word(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
bool image(const std::string& path,const encore::upstream::BattleResource& resource,Image& result){
    FILE* f=std::fopen(path.c_str(),"rb");if(!f){message("FAIL image open: %s\n",path.c_str());return false;}
    uint8_t header[36];
    if(std::fread(header,1,sizeof(header),f)!=sizeof(header)||std::memcmp(header,"ENCBPIX\0",8)||word(header+8)!=1){std::fclose(f);message("FAIL BPX header\n");return false;}
    result.width=word(header+12);result.height=word(header+16);const uint32_t colors=word(header+24),bytes=word(header+28);
    if(resource.kind!=2||result.width!=resource.width||result.height!=resource.height||word(header+20)!=1||
       !colors||colors>256||!result.width||result.width>1024||!result.height||result.height>1024||
       bytes!=colors*4+result.width*result.height){std::fclose(f);message("FAIL BPX dimensions\n");return false;}
    std::vector<uint8_t> payload(bytes);
    const bool complete=std::fread(payload.data(),1,payload.size(),f)==payload.size()&&std::fgetc(f)==EOF;std::fclose(f);
    if(!complete){message("FAIL BPX length\n");return false;}
    uint32_t crc=UINT32_MAX;
    for(uint8_t byte:payload){crc^=byte;for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}
    if((crc^UINT32_MAX)!=word(header+32)){message("FAIL BPX CRC\n");return false;}
    result.palette.resize(colors);for(size_t i=0;i<colors;++i)result.palette[i]=word(payload.data()+4*i);
    result.indices.assign(payload.begin()+colors*4,payload.end());
    for(uint8_t index:result.indices)if(index>=colors){message("FAIL BPX palette index\n");return false;}
    return true;
}
template<class Kernel>
typename Kernel::Layer layer(const encore::upstream::BattleBackground& b,const Image& source,const Image& palette){
    typename Kernel::Layer l;
    l.source={source.width,source.height,source.indices.data(),source.palette.data(),source.palette.size()};
    l.palette_source={palette.width,palette.height,palette.indices.data(),palette.palette.data(),palette.palette.size()};
    l.width=b.width;l.height=b.height;l.barrel=(b.flags&1)!=0;l.repeat=(b.flags&2)!=0;l.opacity=b.opacity;
    l.effect=b.effect;l.effect_scale=b.effect_scale;l.barrel_x=b.barrel.x;l.barrel_y=b.barrel.y;
    l.amplitude_x=b.oscillation_amplitude.x;l.amplitude_y=b.oscillation_amplitude.y;
    l.frequency_x=b.oscillation_frequency.x;l.frequency_y=b.oscillation_frequency.y;l.speed_x=b.oscillation_speed.x;l.speed_y=b.oscillation_speed.y;
    l.move_x=b.move.x;l.move_y=b.move.y;l.ping_pong_speed_x=b.ping_pong_speed.x;l.ping_pong_speed_y=b.ping_pong_speed.y;
    l.compression_amplitude_x=b.compression_amplitude.x;l.compression_amplitude_y=b.compression_amplitude.y;
    l.compression_frequency_x=b.compression_frequency.x;l.compression_frequency_y=b.compression_frequency.y;
    l.compression_speed_x=b.compression_speed.x;l.compression_speed_y=b.compression_speed.y;
    l.palette_shifting=(b.flags&4)!=0;l.palette_frames=b.palette_frames;l.palette_fixed_row=b.palette_fixed_row;l.palette_speed=b.palette_speed;
    return l;
}
uint32_t morton(uint32_t x,uint32_t y){return (x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);}
uint32_t gpu_word(uint32_t c){return ((c&0xffu)<<24)|((c&0xff00u)<<8)|((c>>8)&0xff00u)|(c>>24);}
double elapsed(u64 start){return double(svcGetSystemTick()-start)/CPU_TICKS_PER_MSEC;}
bool run(){
    std::string error;encore::upstream::BattleData data;
    if(!data.load_file("romfs:/data/doll-entry.encbattle",error)){message("FAIL data: %s\n",error.c_str());return false;}
    const auto view=data.view();if(view.count(encore::upstream::BattleSection::Backgrounds)!=2){message("FAIL layer count\n");return false;}
    std::vector<Image> source(2),palette(2);
    std::vector<encore::BackgroundKernel::Layer> layers;
    std::vector<encore::ScalarBackgroundKernel::Layer> reference_layers;
    for(unsigned n=0;n<2;++n){
        const auto b=view.background(n);const auto r=view.resource(b.resource),p=view.resource(b.palette_resource);
        if(!image("romfs:/"+std::string(view.string(r.path)),r,source[n])||!image("romfs:/"+std::string(view.string(p.path)),p,palette[n]))return false;
        layers.push_back(layer<encore::BackgroundKernel>(b,source[n],palette[n]));
        reference_layers.push_back(layer<encore::ScalarBackgroundKernel>(b,source[n],palette[n]));
    }
    const uint32_t widths[]={400,320},heights[]={240,180};const float times[]={0,1.25f,60,1000};
    for(unsigned mode=0;mode<2;++mode){
        const uint32_t width=widths[mode],height=heights[mode];const size_t count=size_t(width)*height;
        message("Preparing %lux%lu...\n",(unsigned long)width,(unsigned long)height);
        encore::BackgroundKernel guarded;encore::ScalarBackgroundKernel scalar;
        if(!guarded.prepare(layers,width,height,error)||!scalar.prepare(reference_layers,width,height,error)||!guarded.separable_pair()){message("FAIL prepare: %s\n",error.c_str());return false;}
        uint32_t texture_width=8,texture_height=8;while(texture_width<width)texture_width<<=1;while(texture_height<height)texture_height<<=1;
        const size_t texture_count=size_t(texture_width)*texture_height;
        std::vector<uint32_t> offsets(count),expected(count),actual(count),mapped(texture_count+2),mapped_expected(texture_count);
        for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x)offsets[size_t(y)*width+x]=((y/8)*(texture_width/8)+x/8)*64+morton(x,y);
        if(!guarded.prepare_mapped_output(offsets.data(),offsets.size(),texture_count,error)){message("FAIL layout\n");return false;}
        for(float time:times){
            constexpr uint32_t clear=0x81234567u,sentinel=0xa5a5a5a5u;
            u64 start=svcGetSystemTick();const bool reference_ok=scalar.compose(time,clear,expected.data());const double scalar_ms=elapsed(start);
            start=svcGetSystemTick();const bool guarded_ok=guarded.compose(time,clear,actual.data());const double guarded_ms=elapsed(start);
            if(!reference_ok||!guarded_ok){message("FAIL compose t=%.2f\n",double(time));return false;}
            for(size_t i=0;i<count;++i)if(expected[i]!=actual[i]){message("FAIL linear t=%.2f i=%lu\n%08lx != %08lx\n",double(time),(unsigned long)i,(unsigned long)expected[i],(unsigned long)actual[i]);return false;}
            pixels_compared+=count;std::fill(mapped.begin(),mapped.end(),sentinel);std::fill(mapped_expected.begin(),mapped_expected.end(),sentinel);
            for(size_t i=0;i<count;++i)mapped_expected[offsets[i]]=gpu_word(expected[i]);
            start=svcGetSystemTick();const bool mapped_ok=guarded.compose_mapped(time,clear,mapped.data()+1,texture_count);const double mapped_ms=elapsed(start);
            if(!mapped_ok||mapped.front()!=sentinel||mapped.back()!=sentinel){message("FAIL mapped guard t=%.2f\n",double(time));return false;}
            for(size_t i=0;i<texture_count;++i)if(mapped[i+1]!=mapped_expected[i]){message("FAIL mapped t=%.2f i=%lu\n%08lx != %08lx\n",double(time),(unsigned long)i,(unsigned long)mapped_expected[i],(unsigned long)mapped[i+1]);return false;}
            mapped_bytes_compared+=texture_count*sizeof(uint32_t);
            message("t=%7.2f PASS pixel+mapped\n S %.1f G %.1f M %.1f ms\n",double(time),scalar_ms,guarded_ms,mapped_ms);
        }
    }
    return true;
}
}
int main(){
    gfxInitDefault();consoleInit(GFX_BOTTOM,nullptr);gfxSetDoubleBuffering(GFX_BOTTOM,false);
    diagnostic=std::fopen("sdmc:/encore-baby-background-selfcheck.log","w");
    message("Baby ARM kernel selfcheck\n%s\n%s\n",SELF_CHECK_LABEL,diagnostic?"SD log open":"SD log unavailable; screen only");
    const Result romfs=romfsInit();bool passed=false;
    if(R_SUCCEEDED(romfs))passed=run();else message("FAIL RomFS init %08lx\n",(unsigned long)romfs);
    message("%s\nPixels: %llu\nMapped bytes: %llu\n8 frames / 2 sizes only\nPress START to exit\n",passed?"ALL CHECKS PASS":"SELF CHECK FAILED",(unsigned long long)pixels_compared,(unsigned long long)mapped_bytes_compared);
    if(diagnostic){std::fclose(diagnostic);diagnostic=nullptr;}
    while(aptMainLoop()){hidScanInput();if(hidKeysDown()&KEY_START)break;gfxFlushBuffers();gfxSwapBuffers();gspWaitForVBlank();}
    if(R_SUCCEEDED(romfs))romfsExit();
    gfxExit();return passed?0:1;
}

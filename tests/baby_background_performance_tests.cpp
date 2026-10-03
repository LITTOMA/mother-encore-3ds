#include <chrono>
#define main baby_background_fixture_main
#include "tests/baby_background_kernel_tests.cpp"
#undef main
#include "encore/battle_data.hpp"
#define BackgroundKernel ScalarBackgroundKernel
#include "tests/baby_background_scalar_reference.hpp"
#undef BackgroundKernel

template<class Kernel>
typename Kernel::Layer convert(const encore::upstream::BattleBackground& b,const BattleRenderer::Asset& image,const BattleRenderer::Asset& palette){
    typename Kernel::Layer l;
    l.source={image.width,image.height,image.pixels.data(),image.palette.data(),image.palette.size()};
    l.palette_source={palette.width,palette.height,palette.pixels.data(),palette.palette.data(),palette.palette.size()};
    l.width=b.width;l.height=b.height;l.barrel=(b.flags&1)!=0;l.repeat=(b.flags&2)!=0;
    l.opacity=b.opacity;l.effect=b.effect;l.effect_scale=b.effect_scale;l.barrel_x=b.barrel.x;l.barrel_y=b.barrel.y;
    l.amplitude_x=b.oscillation_amplitude.x;l.amplitude_y=b.oscillation_amplitude.y;
    l.frequency_x=b.oscillation_frequency.x;l.frequency_y=b.oscillation_frequency.y;l.speed_x=b.oscillation_speed.x;l.speed_y=b.oscillation_speed.y;
    l.move_x=b.move.x;l.move_y=b.move.y;l.ping_pong_speed_x=b.ping_pong_speed.x;l.ping_pong_speed_y=b.ping_pong_speed.y;
    l.compression_amplitude_x=b.compression_amplitude.x;l.compression_amplitude_y=b.compression_amplitude.y;
    l.compression_frequency_x=b.compression_frequency.x;l.compression_frequency_y=b.compression_frequency.y;
    l.compression_speed_x=b.compression_speed.x;l.compression_speed_y=b.compression_speed.y;
    l.palette_shifting=(b.flags&4)!=0;l.palette_frames=b.palette_frames;l.palette_speed=b.palette_speed;l.palette_fixed_row=b.palette_fixed_row;
    return l;
}
template<class F> double timed(unsigned iterations,F function){
    const auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<iterations;++i)function(i);
    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/iterations;
}
int main(int argc,char** argv){
    require(argc==7||argc==8,"usage: benchmark battle-data romfs width height samples iterations [step]");
    const float step=argc==8?std::stof(argv[7]):0.071f;
    const uint32_t w=uint32_t(std::stoul(argv[3])),h=uint32_t(std::stoul(argv[4]));
    const unsigned samples=unsigned(std::stoul(argv[5])),iterations=unsigned(std::stoul(argv[6]));
    std::string error;encore::upstream::BattleData data;require(data.load_file(argv[1],error),error.c_str());const auto view=data.view();
    require(view.count(encore::upstream::BattleSection::Backgrounds)==2,"actual two-layer background");
    std::vector<std::string> paths(4);std::vector<BattleRenderer::Resource> resources(4);
    for(unsigned n=0;n<2;++n){
        const auto b=view.background(n);
        for(unsigned p=0;p<2;++p){
            const auto r=view.resource(p?b.palette_resource:b.resource);const size_t i=2*n+p;
            paths[i]=std::string(argv[2])+"/"+std::string(view.string(r.path));
            resources[i]={paths[i].c_str(),r.kind,r.width,r.height,r.columns,r.rows};
        }
    }
    BattleRenderer assets;require(assets.load(resources,w,h,error),error.c_str());
    std::vector<encore::BackgroundKernel::Layer> config;
    std::vector<encore::ScalarBackgroundKernel::Layer> scalar_config;
    for(unsigned n=0;n<2;++n){
        config.push_back(convert<encore::BackgroundKernel>(view.background(n),assets.assets_[2*n],assets.assets_[2*n+1]));
        scalar_config.push_back(convert<encore::ScalarBackgroundKernel>(view.background(n),assets.assets_[2*n],assets.assets_[2*n+1]));
    }
    encore::BackgroundKernel optimized;encore::ScalarBackgroundKernel scalar;
    require(optimized.prepare(config,w,h,error),error.c_str());require(scalar.prepare(scalar_config,w,h,error),error.c_str());
    require(optimized.separable_pair(),"actual Baby takes exact separable pair");
#if defined(ENCORE_BLOCK_PROTOTYPE) || defined(ENCORE_INLINE_PROTOTYPE) || defined(ENCORE_COMBINED_PROTOTYPE) || defined(ENCORE_TEMPORAL_PROTOTYPE)
#ifdef ENCORE_TEMPORAL_PROTOTYPE
    using PerformanceBaseline=encore::CurrentProductionBackgroundKernel;
#else
    using PerformanceBaseline=encore::GuardedBackgroundKernel;
#endif
    PerformanceBaseline guarded_baseline;std::vector<PerformanceBaseline::Layer> guarded_config;
    for(unsigned n=0;n<2;++n)guarded_config.push_back(convert<PerformanceBaseline>(view.background(n),assets.assets_[2*n],assets.assets_[2*n+1]));
    require(guarded_baseline.prepare(guarded_config,w,h,error),error.c_str());
#endif
    const size_t size=size_t(w)*h,texture_size=assets.surface_texture_.size/sizeof(uint32_t);
    require(optimized.prepare_mapped_output(assets.surface_offsets_.data(),size,texture_size,error),error.c_str());
    std::vector<uint32_t> expected(size),actual(size),mapped(texture_size);
    std::vector<float> times={0,-0.0f,1.0f/60,1,10,100,1000,-1000,3599,3600,1000000,-1000000,
        std::numeric_limits<float>::denorm_min(),std::numeric_limits<float>::max(),-std::numeric_limits<float>::max(),
        std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()};
    uint32_t state=0x29395210u;
    for(unsigned i=0;i<samples;++i){state^=state<<13;state^=state>>17;state^=state<<5;
        times.push_back(i<samples/2?float(i)/60.0f:float(int32_t(state))/123456.0f);}
    size_t compared=0,mapped_compared=0;
    for(float time:times){
        const bool a=scalar.compose(time,0x81234567u,expected.data()),b=optimized.compose(time,0x81234567u,actual.data());
        require(a==b,"optimized/scalar success parity");
        if(a){require(expected==actual,"actual Baby scalar pixel parity");compared+=size;}
        std::fill(mapped.begin(),mapped.end(),0xa5a5a5a5u);
        const bool gpu=optimized.compose_mapped(time,0x81234567u,mapped.data(),mapped.size());
        if(gpu){require(a,"mapped success implies scalar success");
            for(size_t i=0;i<size;++i){const auto c=expected[i];require(mapped[assets.surface_offsets_[i]]==(((c&0xffu)<<24)|((c&0xff00u)<<8)|((c>>8)&0xff00u)|(c>>24)),"mapped pixel equals original scalar upload");}
            mapped_compared+=size*4;
        }else require(std::all_of(mapped.begin(),mapped.end(),[](uint32_t p){return p==0xa5a5a5a5u;}),"mapped rejection writes nothing");
    }
#if defined(ENCORE_BLOCK_PROTOTYPE) || defined(ENCORE_COMBINED_PROTOTYPE) || defined(ENCORE_TEMPORAL_PROTOTYPE)
    std::vector<uint32_t> duplicate_offsets(size,0);
    require(optimized.prepare_mapped_output(duplicate_offsets.data(),size,texture_size,error),error.c_str());
    require(scalar.compose(1.25f,0,expected.data()),"duplicate layout scalar reference");
    std::fill(mapped.begin(),mapped.end(),0xa5a5a5a5u);
    require(optimized.compose_mapped(1.25f,0,mapped.data(),mapped.size()),"duplicate mapped layout remains supported");
    const uint32_t last=expected.back();
    require(mapped[0]==(((last&0xffu)<<24)|((last&0xff00u)<<8)|((last>>8)&0xff00u)|(last>>24)),"duplicate mapped layout retains last source index");
    require(std::all_of(mapped.begin()+1,mapped.end(),[](uint32_t p){return p==0xa5a5a5a5u;}),"duplicate layout preserves unused words");
    require(optimized.prepare_mapped_output(assets.surface_offsets_.data(),size,texture_size,error),error.c_str());
#endif
    uint32_t checksum=0;
    const double old_ms=timed(iterations,[&](unsigned i){require(scalar.compose(float(i)*step,0,expected.data()),"scalar benchmark");checksum^=expected[i%size];});
#ifdef ENCORE_TEMPORAL_PROTOTYPE
    optimized.reset_temporal_cache();
#endif
    const double new_ms=timed(iterations,[&](unsigned i){require(optimized.compose(float(i)*step,0,actual.data()),"optimized benchmark");checksum^=actual[i%size];});
#ifdef ENCORE_TEMPORAL_PROTOTYPE
    optimized.reset_temporal_cache();
    uint64_t temporal_cached=0,temporal_proofs=0,temporal_stores=0;
#endif
    const double mapped_ms=timed(iterations,[&](unsigned i){require(optimized.compose_mapped(float(i)*step,0,mapped.data(),mapped.size()),"mapped benchmark");checksum^=mapped[assets.surface_offsets_[i%size]];
#ifdef ENCORE_TEMPORAL_PROTOTYPE
        const auto s=optimized.block_stats();temporal_cached+=s.cached_pixels;temporal_proofs+=s.cache_proofs;temporal_stores+=s.cache_stores;
#endif
    });
#if defined(ENCORE_BLOCK_PROTOTYPE) || defined(ENCORE_INLINE_PROTOTYPE) || defined(ENCORE_COMBINED_PROTOTYPE) || defined(ENCORE_TEMPORAL_PROTOTYPE)
    const double guarded_ms=timed(iterations,[&](unsigned i){require(guarded_baseline.compose(float(i)*step,0,actual.data()),"guarded baseline benchmark");checksum^=actual[i%size];});
#if defined(ENCORE_BLOCK_PROTOTYPE) || defined(ENCORE_COMBINED_PROTOTYPE) || defined(ENCORE_TEMPORAL_PROTOTYPE)
    const auto stats=optimized.block_stats();
#ifdef ENCORE_TEMPORAL_PROTOTYPE
    std::printf("{\"prototype\":\"temporal-cache\",\"width\":%u,\"height\":%u,\"step\":%.8f,\"frames\":%u,\"cached_pixels_total\":%llu,\"proofs_total\":%llu,\"stores_total\":%llu}\n",w,h,double(step),iterations,(unsigned long long)temporal_cached,(unsigned long long)temporal_proofs,(unsigned long long)temporal_stores);
#endif
    std::printf("{\"prototype\":\"constant-color-block\",\"width\":%u,\"height\":%u,\"guarded_baseline_ms\":%.6f,\"block_ms\":%.6f,\"speedup_over_guarded\":%.3f,\"last_frame_classified_pixels\":%u,\"last_frame_fallback_pixels\":%u,\"last_frame_tested_blocks\":%u,\"last_frame_filled_blocks\":%u}\n",w,h,guarded_ms,new_ms,guarded_ms/new_ms,stats.filled_pixels,stats.fallback_pixels,stats.tested,stats.filled);
#else
    std::printf("{\"prototype\":\"inline\",\"width\":%u,\"height\":%u,\"guarded_baseline_ms\":%.6f,\"inline_ms\":%.6f,\"speedup_over_guarded\":%.3f}\n",w,h,guarded_ms,new_ms,guarded_ms/new_ms);
#endif
#endif
    std::printf("{\"width\":%u,\"height\":%u,\"checks\":%zu,\"pixels_compared\":%zu,\"mapped_bytes_compared\":%zu,\"scalar_ms\":%.6f,\"optimized_ms\":%.6f,\"mapped_ms\":%.6f,\"speedup\":%.3f,\"prepared_bytes\":%zu,\"checksum\":%u}\n",w,h,checks,compared,mapped_compared,old_ms,new_ms,mapped_ms,old_ms/new_ms,optimized.prepared_bytes(),checksum);
    assets.free();return 0;
}

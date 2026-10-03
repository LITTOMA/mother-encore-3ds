#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include "encore/battle_data.hpp"
#include "encore/background_kernel.hpp"
// GPU calls are stubs, but loading, preparation and the reference inner loop
// are the actual pre-optimization platform helper, snapshotted with its hash.
#define private public
#ifndef ENCORE_BACKGROUND_REFERENCE_HEADER
#define ENCORE_BACKGROUND_REFERENCE_HEADER "platform/ctr/battle_renderer.hpp"
#endif
#include ENCORE_BACKGROUND_REFERENCE_HEADER
#undef private

namespace {
size_t checks=0,pixels_compared=0,mapped_bytes_compared=0,mapped_frames=0,mapped_rejections=0;
void require(bool ok,const char* message){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",message);std::exit(1);}}
uint32_t random_word(uint32_t& state){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
encore::BackgroundKernel::Layer translate(const BattleRenderer::BackgroundLayer& l,const BattleRenderer::Asset& a){
    encore::BackgroundKernel::Layer r;
    r.source={a.width,a.height,a.pixels.data(),a.palette.data(),a.palette.size()};
    r.barrel=l.barrel;r.repeat=l.repeat;r.width=l.width;r.height=l.height;r.opacity=l.opacity;
    r.effect=l.effect;r.effect_scale=l.effect_scale;r.barrel_x=l.barrel_x;r.barrel_y=l.barrel_y;
    r.amplitude_x=l.amplitude_x;r.amplitude_y=l.amplitude_y;r.frequency_x=l.frequency_x;r.frequency_y=l.frequency_y;
    r.speed_x=l.speed_x;r.speed_y=l.speed_y;return r;
}
bool prepare(encore::BackgroundKernel& kernel,const BattleRenderer& reference,const std::vector<BattleRenderer::BackgroundLayer>& config,std::string& error){
    std::vector<encore::BackgroundKernel::Layer> layers;
    for(const auto& l:config)layers.push_back(translate(l,reference.assets_[l.resource]));
    return kernel.prepare(layers,reference.surface_width_,reference.surface_height_,error)&&
        kernel.prepare_mapped_output(reference.surface_offsets_.data(),reference.surface_offsets_.size(),reference.surface_texture_.size/sizeof(uint32_t),error);
}
void compare(BattleRenderer& reference,encore::BackgroundKernel& kernel,float time,uint32_t clear,std::vector<uint32_t>& out,bool expect_mapped=false){
    const bool a=reference.compose_background(time,clear),b=kernel.compose(time,clear,out.data());
    require(a==b,"compose success/failure parity");
    const size_t count=reference.surface_texture_.size/sizeof(uint32_t);
    const uint32_t sentinel=0xa5a5a5a5u;
    std::vector<uint32_t> mapped(count+2,sentinel);
    const bool c=kernel.compose_mapped(time,clear,mapped.data()+1,count);
    require(mapped.front()==sentinel&&mapped.back()==sentinel,"mapped output keeps outer guards");
    if(expect_mapped)require(c==a,"mapped actual-content success/failure parity");
    if(c){
        require(a,"mapped success requires successful reference");
        std::memset(reference.surface_texture_.data,0xa5,reference.surface_texture_.size);
        reference.upload_surface();
        require(std::memcmp(mapped.data()+1,reference.surface_texture_.data,reference.surface_texture_.size)==0,"mapped bytes equal frozen compose + upload including padding");
        mapped_bytes_compared+=reference.surface_texture_.size;++mapped_frames;
    }else{
        require(std::all_of(mapped.begin(),mapped.end(),[&](uint32_t value){return value==sentinel;}),"rejected mapped compose writes nothing");
        ++mapped_rejections;
    }
    if(!a)return;
    for(size_t i=0;i<out.size();++i)if(out[i]!=reference.surface_[i]){
        std::fprintf(stderr,"time %.9g pixel %zu: optimized %08x reference %08x\n",time,i,out[i],reference.surface_[i]);
        require(false,"pixel parity");
    }
    ++checks;pixels_compared+=out.size();
}
template<class F> double measure(unsigned iterations,F&& fn){
    const auto start=std::chrono::steady_clock::now();
    for(unsigned i=0;i<iterations;++i)fn(i);
    return std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/iterations;
}
}
int main(int argc,char** argv){
    require(argc==7,"usage: test battle-data romfs-root width height time-samples benchmark-iterations");
    const uint32_t width=uint32_t(std::stoul(argv[3])),height=uint32_t(std::stoul(argv[4]));
    const unsigned samples=unsigned(std::stoul(argv[5])),iterations=unsigned(std::stoul(argv[6]));
    require(samples>0&&iterations>0,"nonzero sampling/benchmark counts");
    std::string error;encore::upstream::BattleData data;
    require(data.load_file(argv[1],error),error.c_str());const auto view=data.view();
    std::vector<std::string> paths;
    std::vector<BattleRenderer::Resource> resources;
    std::vector<BattleRenderer::BackgroundLayer> config;
    for(uint32_t i=0;i<view.count(encore::upstream::BattleSection::Backgrounds);++i){
        const auto b=view.background(i);const auto a=view.resource(b.resource);
        paths.push_back(std::string(argv[2])+"/"+std::string(view.string(a.path)));
        resources.push_back({nullptr,a.kind,a.width,a.height,a.columns,a.rows});
        BattleRenderer::BackgroundLayer l;l.resource=i;l.barrel=(b.flags&1)!=0;l.repeat=(b.flags&2)!=0;
        l.width=b.width;l.height=b.height;l.opacity=b.opacity;l.effect=b.effect;l.effect_scale=b.effect_scale;
        l.barrel_x=b.barrel.x;l.barrel_y=b.barrel.y;l.amplitude_x=b.oscillation_amplitude.x;l.amplitude_y=b.oscillation_amplitude.y;
        l.frequency_x=b.oscillation_frequency.x;l.frequency_y=b.oscillation_frequency.y;l.speed_x=b.oscillation_speed.x;l.speed_y=b.oscillation_speed.y;config.push_back(l);
    }
    for(size_t i=0;i<paths.size();++i)resources[i].path=paths[i].c_str();
    BattleRenderer reference;require(reference.load(resources,width,height,error),error.c_str());
    require(reference.set_background(config,error),error.c_str());
    encore::BackgroundKernel kernel;require(prepare(kernel,reference,config,error),error.c_str());
    const bool fused=kernel.fused_pair(),bounded=kernel.bounded_pair();const size_t prepared_bytes=kernel.prepared_bytes();
    std::vector<uint32_t> output(size_t(width)*height);uint32_t rng=0x1262a451;
    const float special_times[]={0.0f,-0.0f,1.0f/60,1,12.5663706f,-1000,3600,1000000,
        std::numeric_limits<float>::denorm_min(),std::numeric_limits<float>::max(),
        std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()};
    for(float time:special_times)compare(reference,kernel,time,random_word(rng),output,true);
    for(unsigned i=0;i<samples;++i){
        const float time=i<samples/2?float(i)/60:float(int32_t(random_word(rng)))/123456.0f;
        compare(reference,kernel,time,random_word(rng),output,true);
    }
    // Benchmark continuous arbitrary times. A checksum keeps results observable.
    uint32_t checksum=0;
    const double reference_ms=measure(iterations,[&](unsigned i){require(reference.compose_background(float(i)*0.071f,0),"reference benchmark");checksum^=reference.surface_[i%output.size()];});
    const double optimized_ms=measure(iterations,[&](unsigned i){require(kernel.compose(float(i)*0.071f,0,output.data()),"optimized benchmark");checksum^=output[i%output.size()];});
    const double upload_ms=measure(iterations,[&](unsigned){reference.upload_surface();});
    const size_t texture_count=reference.surface_texture_.size/sizeof(uint32_t);
    std::vector<uint32_t> tiled(texture_count);
    const double mapped_ms=measure(iterations,[&](unsigned i){require(kernel.compose_mapped(float(i)*0.071f,0,tiled.data(),tiled.size()),"mapped benchmark");checksum^=tiled[reference.surface_offsets_[i%output.size()]];});
    const double linear_upload_ms=measure(iterations,[&](unsigned i){require(kernel.compose(float(i)*0.071f,0,reference.surface_.data()),"linear plus upload benchmark");reference.upload_surface();checksum^=static_cast<uint32_t*>(reference.surface_texture_.data)[reference.surface_offsets_[i%output.size()]];});
    // Layout validation happens only on preparation. Failed replacement clears
    // the old layout; a failed frame must never alter any destination word.
    const auto unchanged=[&](size_t capacity){
        std::fill(tiled.begin(),tiled.end(),0x1234abcd);
        require(!kernel.compose_mapped(0,0,tiled.data(),capacity),"invalid mapped call rejected");
        require(std::all_of(tiled.begin(),tiled.end(),[](uint32_t v){return v==0x1234abcd;}),"invalid mapped call leaves output untouched");
    };
    require(!kernel.compose_mapped(0,0,nullptr,texture_count),"null mapped destination rejected");
    unchanged(texture_count-1);
    for(unsigned variant=0;variant<6;++variant){
        auto offsets=reference.surface_offsets_;
        const uint32_t* map=offsets.data();size_t count=offsets.size(),capacity=texture_count;
        if(variant==0)map=nullptr;
        if(variant==1)--count;
        if(variant==2)++count;
        if(variant==3)capacity=0;
        if(variant==4)offsets.front()=uint32_t(texture_count);
        if(variant==5)offsets.back()=UINT32_MAX;
        require(!kernel.prepare_mapped_output(map,count,capacity,error),"invalid mapped layout rejected");
        unchanged(texture_count);
        require(kernel.prepare_mapped_output(reference.surface_offsets_.data(),reference.surface_offsets_.size(),texture_count,error),error.c_str());
    }
    // Any map preserves increasing source-index write order, including
    // duplicate offsets (the last source pixel wins, as in old upload).
    const auto original_offsets=reference.surface_offsets_;
    for(unsigned variant=0;variant<3;++variant){
        if(variant==0)std::reverse(reference.surface_offsets_.begin(),reference.surface_offsets_.end());
        if(variant==1)std::fill(reference.surface_offsets_.begin(),reference.surface_offsets_.end(),uint32_t(texture_count-1));
        if(variant==2)for(size_t i=0;i<reference.surface_offsets_.size();++i)reference.surface_offsets_[i]=uint32_t(i);
        require(kernel.prepare_mapped_output(reference.surface_offsets_.data(),reference.surface_offsets_.size(),texture_count,error),error.c_str());
        compare(reference,kernel,1.723f,random_word(rng),output,true);
    }
    reference.surface_offsets_=original_offsets;
    std::vector<encore::BackgroundKernel::Layer> unmapped_layers;
    for(const auto& l:config)unmapped_layers.push_back(translate(l,reference.assets_[l.resource]));
    require(kernel.prepare(unmapped_layers,width,height,error),error.c_str());
    unchanged(texture_count);
    kernel.clear();unchanged(texture_count);
    require(prepare(kernel,reference,config,error),error.c_str());
    // Exercise all four bounded oscillation choices and all sixteen checked
    // repeat/oscillation choices independently for the two fused layers.
    for(unsigned variant=0;variant<20;++variant){
        auto changed=config;
        for(size_t j=0;j<changed.size();++j){
            auto& l=changed[j];l.amplitude_x=(variant&(1u<<j))?0.013f:0;
            l.frequency_x=2.3f;l.frequency_y=0;l.opacity=j?0.617f:1;
            if(variant>=4){l.barrel=false;l.width=37;l.repeat=(variant&(4u<<j))!=0;}
        }
        require(reference.set_background(changed,error),error.c_str());require(prepare(kernel,reference,changed,error),error.c_str());
        require(kernel.fused_pair(),"dispatch variants use fused pair");
        for(unsigned i=0;i<4;++i)compare(reference,kernel,float(int32_t(random_word(rng)))/123456.0f,random_word(rng),output,true);
    }
    // Exercise generic fallback and guarded specializations with synthetic test
    // parameters; these are tests, never compiled into production game content.
    for(unsigned variant=0;variant<16;++variant){
        auto changed=config;
        for(size_t j=0;j<changed.size();++j){
            auto& l=changed[j];l.barrel=(variant&1)!=0;l.repeat=(variant&2)!=0;
            l.width=37;l.height=29;l.amplitude_x=(variant&4)?1.7f:0;
            l.frequency_y=(variant&8)?2.3f:0;l.amplitude_y=0.7f;
            l.opacity=j==0?((variant&4)?1.0f:0.37f):0.61f;
        }
        require(reference.set_background(changed,error),error.c_str());require(prepare(kernel,reference,changed,error),error.c_str());
        for(unsigned i=0;i<4;++i)compare(reference,kernel,float(int32_t(random_word(rng)))/123456.0f,random_word(rng),output);
    }
    for(unsigned variant=0;variant<8;++variant){
        auto changed=config;
        for(auto& l:changed){
            if(variant<4)l.amplitude_x=(variant&1?-1.0f:1.0f)*std::numeric_limits<float>::max()/(variant&2?4.0f:1.0f);
            else if(variant==4)l.speed_x=2;
            else if(variant==5)l.speed_y=2;
            else if(variant==6){l.barrel=false;l.width=std::numeric_limits<float>::min();}
            else{l.barrel=false;l.height=std::numeric_limits<float>::min();}
        }
        const bool a=reference.set_background(changed,error),b=prepare(kernel,reference,changed,error);
        require(a==b,"extreme preparation parity");
        if(a)for(float time:special_times)compare(reference,kernel,time,random_word(rng),output);
    }
    if(reference.assets_.size()==2){
        auto& a=reference.assets_[1];
        for(size_t i=0;i<a.palette.size();++i)a.palette[i]=(a.palette[i]&0xffffffu)|(uint32_t(i*37%256)<<24);
        auto changed=config;changed[1].opacity=0.617f;
        require(reference.set_background(changed,error),error.c_str());require(prepare(kernel,reference,changed,error),error.c_str());
        require(kernel.fused_pair(),"mixed-alpha second palette still uses exact table");
        for(float time:special_times)compare(reference,kernel,time,random_word(rng),output);
    }
    // Mixed-alpha palette exercises generic blend and table generation, while
    // the immutable source contract requires a reprepare after mutation.
    for(auto& a:reference.assets_)for(size_t i=0;i<a.palette.size();++i)a.palette[i]=(a.palette[i]&0xffffffu)|(uint32_t(i*37%256)<<24);
    require(reference.set_background(config,error),error.c_str());require(prepare(kernel,reference,config,error),error.c_str());
    for(float time:special_times)compare(reference,kernel,time,0x35291807,output);
    auto bad=translate(config[0],reference.assets_[0]);
    for(unsigned variant=0;variant<6;++variant){
        auto l=bad;
        if(variant==0)l.width=0;
        if(variant==1)l.opacity=2;
        if(variant==2)l.speed_x=std::numeric_limits<float>::infinity();
        if(variant==3)l.source.palette_size=0;
        if(variant==4)l.source.pixels=nullptr;
        if(variant==5)l.effect=-100;
        require(!kernel.prepare({l},width,height,error),"invalid preparation rejected");
        require(!kernel.compose(0,0,output.data()),"failed preparation clears state");
        unchanged(texture_count);
    }
    std::printf("{\"width\":%u,\"height\":%u,\"time_samples\":%u,\"checks\":%zu,\"pixels_compared\":%zu,\"mapped_frames\":%zu,\"mapped_rejections\":%zu,\"mapped_bytes_compared\":%zu,\"fused_pair\":%s,\"bounded_pair\":%s,\"prepared_bytes\":%zu,\"iterations\":%u,\"reference_ms\":%.6f,\"optimized_ms\":%.6f,\"speedup\":%.4f,\"stub_upload_ms\":%.6f,\"mapped_ms\":%.6f,\"linear_plus_stub_upload_ms\":%.6f,\"mapped_speedup\":%.4f,\"checksum\":%u}\n",width,height,samples,checks,pixels_compared,mapped_frames,mapped_rejections,mapped_bytes_compared,fused?"true":"false",bounded?"true":"false",prepared_bytes,iterations,reference_ms,optimized_ms,reference_ms/optimized_ms,upload_ms,mapped_ms,linear_upload_ms,linear_upload_ms/mapped_ms,checksum);
    reference.free();
}

#pragma once
// Header probes and exact allocation-shape bounds used before CPU preparation.
// No game state, source execution, RNG or GPU command is reachable here.
namespace battle_budget {
struct Image {uint32_t width=0,height=0,frames=0,colors=0;size_t bytes=0;};
inline uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline bool size(const std::string& path,size_t limit,size_t& bytes,std::string& error){
    FILE* f=std::fopen(path.c_str(),"rb");if(!f){error="Encounter dependency is missing: "+path;return false;}
    bool ok=std::fseek(f,0,SEEK_END)==0;const long count=ok?std::ftell(f):-1;ok=count>0&&size_t(count)<=limit;if(std::fclose(f))ok=false;
    if(!ok){error="Encounter dependency exceeds bounded input: "+path;return false;}bytes=size_t(count);return true;
}
inline bool read(const std::string& path,size_t count,std::vector<uint8_t>& out,const PreparationControl& control,std::string& error){
    FILE* f=std::fopen(path.c_str(),"rb");if(!f){error="Encounter dependency open failed: "+path;return false;}
    out.resize(count);bool ok=true;
    for(size_t offset=0;offset<count;){if(control.stopped()){ok=false;break;}const size_t amount=std::min<size_t>(8192,count-offset);if(std::fread(out.data()+offset,1,amount,f)!=amount){ok=false;break;}offset+=amount;report_load_progress(LoadPhase::FileRead,offset,count);}
    if(ok)ok=std::fgetc(f)==EOF;if(std::fclose(f))ok=false;
    if(!ok){error=control.stopped()?"Encounter preparation cancelled":"Encounter dependency read failed: "+path;return false;}return true;
}
inline bool image_header(const std::string& path,const upstream::BattleResource& resource,Image& out,RawT3x& raw,std::string& error){
    FILE* f=std::fopen(path.c_str(),"rb");if(!f){error="Encounter image is missing: "+path;return false;}
    uint8_t header[36]{};const size_t header_size=resource.kind==2?36:21;
    bool ok=std::fread(header,1,header_size,f)==header_size&&std::fseek(f,0,SEEK_END)==0;const long length=ok?std::ftell(f):-1;if(std::fclose(f))ok=false;
    if(!ok||length<=0){error="Encounter image header read failed: "+path;return false;}
    if(resource.kind==1){if(!inspect_raw_t3x(header,header_size,size_t(length),raw,error)||raw.image_width!=resource.width||raw.image_height!=resource.height)return false;out.bytes=size_t(length);return true;}
    if(resource.kind!=2||std::memcmp(header,"ENCBPIX\0",8)||u32(header+8)!=1){error="Unknown encounter indexed image profile";return false;}
    out={u32(header+12),u32(header+16),u32(header+20),u32(header+24),size_t(length)};
    const uint64_t pixels=uint64_t(out.width)*out.height*out.frames;
    if(out.width!=resource.width||out.height!=resource.height||out.frames!=uint64_t(resource.columns)*resource.rows||!out.colors||out.colors>256||pixels>2*1024*1024||uint64_t(length)!=36+out.colors*4+pixels||u32(header+28)!=out.colors*4+pixels){error="Encounter indexed header exceeds checked residency profile";return false;}return true;
}
inline BackgroundKernel::Layer layer(const upstream::BattleBackground& b,const Image& image){
    BackgroundKernel::Layer l;l.source.width=image.width;l.source.height=image.height;l.source.palette_size=image.colors;
    l.width=b.width;l.height=b.height;l.barrel=(b.flags&1)!=0;l.repeat=(b.flags&2)!=0;l.opacity=b.opacity;
    l.amplitude_x=b.oscillation_amplitude.x;l.amplitude_y=b.oscillation_amplitude.y;l.frequency_x=b.oscillation_frequency.x;l.frequency_y=b.oscillation_frequency.y;
    l.compression_amplitude_x=b.compression_amplitude.x;l.compression_amplitude_y=b.compression_amplitude.y;l.compression_frequency_x=b.compression_frequency.x;l.compression_frequency_y=b.compression_frequency.y;
    l.palette_shifting=(b.flags&4)!=0;l.palette_frames=b.palette_frames;return l;
}
inline bool estimate(upstream::BattleView view,upstream::RoundView round,unsigned width,unsigned height,const std::vector<std::string>& resident,size_t metadata_bytes,const PreparationControl& control,EncounterResidencyBytes& peak,std::string& error){
    const auto count=view.count(upstream::BattleSection::Resources),round_count=round.count(upstream::RoundSection::Resources);
    if(count+round_count>1024||view.count(upstream::BattleSection::Backgrounds)>8){error="Encounter dependency cardinality exceeds bounded preparation profile";return false;}
    uint64_t cpu=metadata_bytes,linear=0;std::vector<Image> images(count);std::vector<BackgroundKernel::Layer> layers;
    std::vector<std::string> paths;paths.reserve(count+round_count);
    for(uint32_t i=0;i<count+round_count;++i){if(control.stopped()){error="Encounter preparation cancelled";return false;}
        const auto r=i<count?view.resource(i):round.resource(i-count);const std::string relative(i<count?view.string(r.path):round.string(r.path));
        if(relative.empty()||relative.size()>256){error="Encounter resource path exceeds bounded profile";return false;}
        const std::string path="romfs:/"+relative;
        if(i>=count&&std::find(paths.begin(),paths.end(),path)!=paths.end())continue;paths.push_back(path);
        Image image;RawT3x raw;if(!image_header(path,r,image,raw,error))return false;if(i<count)images[i]=image;
        cpu+=512+path.size()*4;
        if(r.kind==2)cpu+=size_t(image.width)*image.height+size_t(image.colors)*8+size_t(image.width)*image.height*image.frames;
        else if(std::find(resident.begin(),resident.end(),ctr::loading_texture_key(path.c_str()))==resident.end()){cpu+=image.bytes;linear+=raw.size;}
    }
    for(uint32_t i=0;i<view.count(upstream::BattleSection::Backgrounds);++i){const auto b=view.background(i);if(b.resource>=images.size()||!images[b.resource].colors){error="Encounter background header missing";return false;}layers.push_back(layer(b,images[b.resource]));}
    bool certificates=false;
#ifdef ENCORE_GPU_CERTIFICATE_TABLES
    certificates=true;
#endif
#if defined(ENCORE_EXPERIMENTAL_GPU_BACKGROUND) && defined(ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS)
    const bool mapped_texture=CertifiedTextureBackgroundKernel::supported_shape(layers,width,height);
    cpu+=CertifiedTextureBackgroundKernel::preparation_upper_bound(layers,width,height);
#endif
    cpu+=RegionBackgroundKernel::preparation_upper_bound(layers,width,height,certificates);
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
    cpu+=RowLinearBackgroundKernel::preparation_upper_bound(layers,width,height);
    cpu+=BattleRenderer::transition_cpu_budget+sizeof(TransitionMaskPlan)+64;
    linear+=BattleRenderer::transition_gpu_budget;
    linear+=16384*sizeof(ctr::GpuRegionBatch::Span);
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
    if(mapped_texture)linear+=ctr::GpuMappedTextureBatch::linear_upper_bound;
    else linear+=4096*16+2*256*256*4;
#endif
#endif
    size_t tw=8,th=8;while(tw<width)tw*=2;while(th<height)th*=2;
    linear+=tw*th*4;cpu+=size_t(width)*height*8+tw*th; // surfaces and temporary mapped-offset uniqueness check
    cpu+=view.count(upstream::BattleSection::Glyphs)*sizeof(BattleRenderer::Glyph)+view.count(upstream::BattleSection::Layouts)*8+64*1024;
    if(cpu>SIZE_MAX||linear>SIZE_MAX){error="Encounter preparation reservation overflow";return false;}
    peak={size_t(cpu),size_t(linear)};return true;
}
}

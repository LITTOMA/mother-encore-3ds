#pragma once
#include "loading_texture.hpp"
#include "source_font_renderer.hpp"
#include "encore/raw_t3x.hpp"
#include <3ds.h>
#include <citro2d.h>
#include "encore/region_background_kernel.hpp"
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
#include "gpu_region_batch.hpp"
#include "gpu_transition_mask_batch.hpp"
#include "encore/transition_mask_plan.hpp"
#include "encore/row_linear_background_kernel.hpp"
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
#include "gpu_row_texture_batch.hpp"
#endif
#endif
#include "encore/crc32.hpp"
#include "encore/utf8.hpp"
#include "encore/load_progress.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

// Presentation mechanisms only. The caller supplies every path, dimension,
// layout, palette/color, shader coefficient, glyph metric and animation sample.
// Resource kind 1 = single-image T3X; kind 2 = checked streaming BPX1 palette.
class BattleRenderer {
public:
    struct Resource {const char* path=nullptr;uint32_t kind=0,width=0,height=0,columns=0,rows=0;bool resident=false;};
    struct Glyph {uint32_t codepoint=0,resource=0,u=0,v=0,width=0,height=0;float advance=0,offset_x=0,offset_y=0;};
    struct IndexedResident {std::string path;uint32_t width=0,height=0,frames=0;std::vector<uint32_t> palette;std::shared_ptr<std::vector<uint8_t>> data;
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        std::shared_ptr<const encore::TransitionMaskPlan> mask_plan;
#endif
    };
    struct BackgroundLayer {
        uint32_t resource=0;bool barrel=false;
        // UV normalization extent, not the expanded control/viewport extent.
        // For a tiled source these are the original texture's dimensions.
        float width=0,height=0,opacity=0,effect=0,effect_scale=0;
        float barrel_x=0,barrel_y=0,amplitude_x=0,amplitude_y=0;
        float frequency_x=0,frequency_y=0,speed_x=0,speed_y=0;
        bool repeat=false;
        float move_x=0,move_y=0,ping_pong_speed_x=0,ping_pong_speed_y=0;
        float compression_amplitude_x=0,compression_amplitude_y=0;
        float compression_frequency_x=0,compression_frequency_y=0;
        float compression_speed_x=0,compression_speed_y=0;
        bool palette_shifting=false;
        uint32_t palette_resource=UINT32_MAX,palette_frames=0;
        float palette_speed=0;
        uint32_t palette_fixed_row=UINT32_MAX;
    };
private:
    struct Asset {
        encore::ctr::LoadingSpriteSheet sheet=nullptr;FILE* stream=nullptr;
        uint32_t width=0,height=0,columns=0,rows=0,frames=0;
        uint32_t data_offset=0,cached_frame=UINT32_MAX;
        std::vector<uint32_t> palette;
        std::vector<uint8_t> pixels,texture_bytes;std::shared_ptr<std::vector<uint8_t>> all_frames;
        std::string path;encore::RawT3x raw;size_t uploaded=0;
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        std::shared_ptr<const encore::TransitionMaskPlan> mask_plan;
#endif
    };
    std::vector<Asset> assets_;
    std::vector<Glyph> glyphs_;
    encore::ctr::SourceFontRenderer* source_font_=nullptr; // Non-owning; outlives attachment.
    encore::RegionBackgroundKernel background_;
    std::vector<uint32_t> background_resources_;
    std::vector<uint32_t> surface_;
    std::vector<uint32_t> surface_offsets_;
    C3D_Tex surface_texture_{};
    Tex3DS_SubTexture surface_subtexture_{};
    uint32_t surface_width_=0,surface_height_=0;
    bool texture_ready_=false,direct_surface_=false;
    size_t admitted_linear_bytes_=0;
    bool deferred_gpu_=false;size_t gpu_asset_cursor_=0;unsigned gpu_finish_stage_=0;
    std::vector<encore::BackgroundKernel::Layer> gpu_layers_;
    const std::vector<IndexedResident>* indexed_resident_=nullptr;
    const encore::PreparationControl* prepare_control_=nullptr;
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
    encore::ctr::GpuRegionBatch gpu_background_;
    encore::ctr::GpuTransitionMaskBatch gpu_mask_;
    uint32_t mask_resource_=UINT32_MAX,mask_frame_=0,mask_old_=0,mask_new_=0;
    int32_t mask_x_=0,mask_y_=0;bool gpu_mask_surface_=false;size_t gpu_mask_count_=0;
    encore::RowLinearBackgroundKernel row_background_;
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
    encore::ctr::GpuRowTextureBatch gpu_row_texture_;
    bool gpu_texture_surface_=false;
#endif
    bool gpu_surface_=false;size_t gpu_span_count_=0;
    float gpu_time_=0;uint32_t gpu_clear_=0;
#endif

    static uint32_t u32(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
    static uint32_t crc_update(uint32_t crc,const uint8_t* data,size_t size){
        return encore::crc32_update(crc,data,size);
    }
    static bool fail(std::string& error,const char* reason,const char* path){error=std::string(reason)+": "+(path?path:"");return false;}
    static bool read_frame(Asset& a,uint32_t frame){
        if(frame>=a.frames)return false;
        if(a.all_frames){if(a.cached_frame!=frame)std::memcpy(a.pixels.data(),a.all_frames->data()+size_t(frame)*a.width*a.height,size_t(a.width)*a.height);a.cached_frame=frame;return true;}
        if(!a.stream)return false;
        if(a.cached_frame==frame)return true;
        const size_t size=size_t(a.width)*a.height;
        encore::report_load_progress(encore::LoadPhase::FileRead,0,size);
        if(std::fseek(a.stream,long(a.data_offset+size*frame),SEEK_SET)!=0||
           std::fread(a.pixels.data(),1,size,a.stream)!=size)return false;
        encore::report_load_progress(encore::LoadPhase::FileRead,size,size);
        a.cached_frame=frame;return true;
    }
    bool read_prepared_bytes(FILE* file,uint8_t* destination,size_t size){
        for(size_t offset=0;offset<size;){if(prepare_control_&&prepare_control_->stopped())return false;const size_t amount=std::min<size_t>(8192,size-offset);if(std::fread(destination+offset,1,amount,file)!=amount)return false;offset+=amount;}return true;
    }
    bool load_indexed(Asset& a,const Resource& r,std::string& error){
        if(indexed_resident_)for(const auto& cached:*indexed_resident_)if(cached.path==r.path&&cached.width==r.width&&cached.height==r.height&&cached.frames==uint64_t(r.columns)*r.rows&&cached.data){
            a.frames=cached.frames;a.palette=cached.palette;a.all_frames=cached.data;
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
            a.mask_plan=cached.mask_plan;
#endif
            a.pixels.resize(size_t(r.width)*r.height);return read_frame(a,0);
        }
        a.stream=std::fopen(r.path,"rb");uint8_t header[36];
        if(!a.stream||std::fread(header,1,sizeof(header),a.stream)!=sizeof(header))return fail(error,"Missing/truncated indexed image",r.path);
        if(std::memcmp(header,"ENCBPIX\0",8)||u32(header+8)!=1)return fail(error,"Unknown indexed image format/version",r.path);
        a.width=u32(header+12);a.height=u32(header+16);a.frames=u32(header+20);
        const auto colors=u32(header+24),bytes=u32(header+28),expected_crc=u32(header+32);
        const uint64_t pixels=uint64_t(a.width)*a.height*a.frames;
        if(!a.width||a.width>1024||!a.height||a.height>1024||!a.frames||a.frames>256||
           !colors||colors>256||pixels>16*1024*1024||bytes!=colors*4+pixels||
           a.width!=r.width||a.height!=r.height||a.frames!=uint64_t(r.columns)*r.rows)
            return fail(error,"Indexed image dimensions differ from validated resource",r.path);
        a.palette.resize(colors);std::vector<uint8_t> palette(colors*4);
        if(std::fread(palette.data(),1,palette.size(),a.stream)!=palette.size())return fail(error,"Truncated indexed palette",r.path);
        uint32_t crc=crc_update(UINT32_MAX,palette.data(),palette.size());
        for(uint32_t i=0;i<colors;++i)a.palette[i]=u32(palette.data()+i*4);
        uint8_t block[4096];size_t remaining=size_t(pixels);
        while(remaining){
            if(prepare_control_&&prepare_control_->stopped()){error="Indexed preparation cancelled";return false;}
            const size_t amount=std::min(remaining,sizeof(block));
            if(std::fread(block,1,amount,a.stream)!=amount)return fail(error,"Truncated indexed pixels",r.path);
            crc=crc_update(crc,block,amount);
            for(size_t i=0;i<amount;++i)if(block[i]>=colors)return fail(error,"Indexed palette reference out of range",r.path);
            remaining-=amount;encore::report_load_progress(encore::LoadPhase::Checksum,pixels-remaining,pixels);
        }
        if((crc^UINT32_MAX)!=expected_crc||std::fgetc(a.stream)!=EOF)return fail(error,"Indexed image CRC/length mismatch",r.path);
        a.data_offset=sizeof(header)+colors*4;a.pixels.resize(size_t(a.width)*a.height);
        if(deferred_gpu_){
            if(pixels>2*1024*1024){error="Prewarm indexed residency budget exceeded";return false;}
            a.all_frames=std::make_shared<std::vector<uint8_t>>(size_t(pixels));if(std::fseek(a.stream,long(a.data_offset),SEEK_SET)||!read_prepared_bytes(a.stream,a.all_frames->data(),a.all_frames->size())){error="Prewarm indexed frame read failed";return false;}
        }
        if(!read_frame(a,0))return fail(error,"Could not read indexed first frame",r.path);
        if(deferred_gpu_&&a.stream){const bool closed=std::fclose(a.stream)==0;a.stream=nullptr;if(!closed){error="Prewarm indexed close failed";return false;}}
        return true;
    }
    static unsigned morton(unsigned x,unsigned y){
        return (x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);
    }
    const Glyph* glyph(uint32_t codepoint)const{
        const auto i=std::lower_bound(glyphs_.begin(),glyphs_.end(),codepoint,[](const Glyph& a,uint32_t b){return a.codepoint<b;});
        return i!=glyphs_.end()&&i->codepoint==codepoint?&*i:nullptr;
    }
public:
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
    bool gpu_background_active()const{return gpu_surface_||gpu_mask_surface_;}
    bool gpu_mask_active()const{return gpu_mask_surface_;}
    bool gpu_background_ready()const{return gpu_background_.ready();}
    size_t gpu_background_spans()const{return gpu_span_count_;}
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
    bool gpu_texture_active()const{return gpu_texture_surface_;}
    uint32_t gpu_texture_rows()const{return gpu_row_texture_.accepted_rows();}
    size_t gpu_texture_strips()const{return gpu_row_texture_.count();}
    size_t gpu_texture_bytes()const{return gpu_row_texture_.tracked_bytes();}
#endif
    bool gpu_row_linear_ready()const{return row_background_.ready();}
    size_t gpu_row_linear_bytes()const{return row_background_.prepared_bytes();}
    uint64_t gpu_row_linear_evaluations()const{return row_background_.sample_evaluations();}
    bool gpu_certificate_used()const{return background_.certificate_frame_used();}
    size_t gpu_certificate_bytes()const{return background_.certificate_bytes();}
#endif
    void attach_source_font(encore::ctr::SourceFontRenderer* font){source_font_=font;}
    BattleRenderer()=default;
    BattleRenderer(const BattleRenderer&)=delete;BattleRenderer& operator=(const BattleRenderer&)=delete;
#ifdef ENCORE_FRAME_PROFILE
    struct BackgroundDiagnostics {
        encore::RegionBackgroundKernel::PreparationStatus preparation;
        bool region_ready,region_used,direct_texture;
        uint64_t samples,skipped;
        size_t prepared_bytes;
    };
    BackgroundDiagnostics background_diagnostics()const{
        const auto stats=background_.region_stats();
        return {background_.preparation_status(),background_.region_fast_path(),stats.samples!=0,direct_surface_,stats.samples,stats.skipped,background_.prepared_bytes()};
    }
#endif
    bool load(const std::vector<Resource>& resources,uint32_t width,uint32_t height,std::string& error,bool defer_gpu=false,const encore::PreparationControl* control=nullptr){
        prepare_control_=control;struct ResetControl {const encore::PreparationControl*& value;~ResetControl(){value=nullptr;}} reset{prepare_control_};
        free();if(resources.empty()||resources.size()>1024||!width||width>1024||!height||height>1024){error="Invalid presentation resource/surface bounds";return false;}
        deferred_gpu_=defer_gpu;assets_.resize(resources.size());
        for(size_t i=0;i<resources.size();++i){
            encore::report_load_progress(encore::LoadPhase::Texture,i,resources.size());
            if(control&&control->stopped()){error="Texture preparation cancelled";return false;}
            auto& a=assets_[i];const auto& r=resources[i];
            if(!r.path||!r.width||!r.height||!r.columns||!r.rows){error="Incomplete presentation resource";free();return false;}
            a.width=r.width;a.height=r.height;a.columns=r.columns;a.rows=r.rows;a.path=r.path;
            if(r.kind==2){if(!load_indexed(a,r,error)){free();return false;}}
            else if(r.kind==1){
                if(r.width>1024||r.height>1024||r.width%r.columns||r.height%r.rows){error="Invalid texture grid";free();return false;}
                if(defer_gpu){
                    if(!r.resident){FILE* f=std::fopen(r.path,"rb");if(!f){error="Prewarm texture open failed: "+a.path;return false;}
                        const bool seek=std::fseek(f,0,SEEK_END)==0;const long n=seek?std::ftell(f):-1;
                        if(n<=0||n>8*1024*1024||std::fseek(f,0,SEEK_SET)){std::fclose(f);error="Prewarm texture exceeds bounded input";return false;}
                        a.texture_bytes.resize(size_t(n));const bool ok=read_prepared_bytes(f,a.texture_bytes.data(),a.texture_bytes.size());std::fclose(f);if(!ok){error="Prewarm texture read failed";return false;}
                        if(!encore::parse_raw_t3x(a.texture_bytes.data(),a.texture_bytes.size(),a.raw,error)||a.raw.image_width!=r.width||a.raw.image_height!=r.height){if(error.empty())error="Prewarm T3X image dimensions differ";return false;}
                    }continue;
                }
                a.sheet=encore::ctr::loading_sprite_sheet_acquire(r.path,&error);
                if(!a.sheet){free();return false;}
                if(encore::ctr::loading_sprite_sheet_count(a.sheet)!=1){fail(error,"Presentation texture must contain one image",r.path);free();return false;}
                const auto img=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);
                if(!img.tex||!img.subtex||img.subtex->width!=r.width||img.subtex->height!=r.height||Tex3DS_SubTextureRotated(img.subtex)){
                    fail(error,"Texture differs from validated dimensions",r.path);free();return false;
                }
                C3D_TexSetFilter(img.tex,GPU_NEAREST,GPU_NEAREST);
            }else{error="Unknown presentation resource kind";free();return false;}
        }
        encore::report_load_progress(encore::LoadPhase::Texture,resources.size(),resources.size());
        surface_width_=width;surface_height_=height;surface_.resize(size_t(width)*height);
        unsigned tw=8,th=8;while(tw<width)tw<<=1;while(th<height)th<<=1;
        if(!defer_gpu&&!C3D_TexInit(&surface_texture_,tw,th,GPU_RGBA8)){error="Presentation surface allocation failed";free();return false;}
        texture_ready_=!defer_gpu;if(texture_ready_)std::memset(surface_texture_.data,0,surface_texture_.size);
        surface_offsets_.resize(surface_.size());
        for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)surface_offsets_[size_t(y)*width+x]=((y/8)*(tw/8)+x/8)*64+morton(x,y);
        if(texture_ready_){C3D_TexSetFilter(&surface_texture_,GPU_NEAREST,GPU_NEAREST);
        C3D_TexSetWrap(&surface_texture_,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);}
        surface_subtexture_={u16(width),u16(height),0,1,float(width)/tw,1-float(height)/th};
        return true;
    }
    void free(){
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
        gpu_row_texture_.release();gpu_texture_surface_=false;
#endif
        gpu_mask_.release();mask_resource_=UINT32_MAX;gpu_mask_surface_=false;gpu_mask_count_=0;
        gpu_background_.release();row_background_.clear();gpu_surface_=false;gpu_span_count_=0;
#endif
        background_.clear();background_resources_.clear();
        for(auto& a:assets_){if(a.sheet)encore::ctr::loading_sprite_sheet_free(a.sheet);if(a.stream)std::fclose(a.stream);}
        assets_.clear();glyphs_.clear();surface_.clear();surface_offsets_.clear();gpu_layers_.clear();admitted_linear_bytes_=0;deferred_gpu_=false;gpu_asset_cursor_=0;gpu_finish_stage_=0;
        if(texture_ready_)C3D_TexDelete(&surface_texture_);
        texture_ready_=false;direct_surface_=false;surface_width_=surface_height_=0;surface_texture_={};surface_subtexture_={};
    }
    void swap(BattleRenderer& other){
        using std::swap;
        swap(assets_,other.assets_);swap(glyphs_,other.glyphs_);swap(source_font_,other.source_font_);
        swap(background_,other.background_);swap(background_resources_,other.background_resources_);
        swap(surface_,other.surface_);swap(surface_offsets_,other.surface_offsets_);swap(surface_texture_,other.surface_texture_);swap(surface_subtexture_,other.surface_subtexture_);
        swap(surface_width_,other.surface_width_);swap(surface_height_,other.surface_height_);swap(texture_ready_,other.texture_ready_);swap(direct_surface_,other.direct_surface_);
        swap(admitted_linear_bytes_,other.admitted_linear_bytes_);swap(deferred_gpu_,other.deferred_gpu_);swap(gpu_asset_cursor_,other.gpu_asset_cursor_);swap(gpu_finish_stage_,other.gpu_finish_stage_);swap(gpu_layers_,other.gpu_layers_);
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        gpu_mask_.swap(other.gpu_mask_);swap(mask_resource_,other.mask_resource_);swap(mask_frame_,other.mask_frame_);swap(mask_old_,other.mask_old_);swap(mask_new_,other.mask_new_);swap(mask_x_,other.mask_x_);swap(mask_y_,other.mask_y_);swap(gpu_mask_surface_,other.gpu_mask_surface_);swap(gpu_mask_count_,other.gpu_mask_count_);
        gpu_background_.swap(other.gpu_background_);swap(row_background_,other.row_background_);swap(gpu_surface_,other.gpu_surface_);swap(gpu_span_count_,other.gpu_span_count_);swap(gpu_time_,other.gpu_time_);swap(gpu_clear_,other.gpu_clear_);
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
        gpu_row_texture_.swap(other.gpu_row_texture_);swap(gpu_texture_surface_,other.gpu_texture_surface_);
#endif
#endif
    }
    // CPU preparation owns no Citro3D objects. Main thread admits at most one
    // previously nonresident texture per pump; all file I/O is already complete.
    bool finish_gpu_step(bool& done,std::string& error){
        done=false;if(!deferred_gpu_){done=true;return true;}
        while(gpu_asset_cursor_<assets_.size()){
            auto& a=assets_[gpu_asset_cursor_];if(a.path.empty()||a.frames){++gpu_asset_cursor_;continue;}
            if(!a.sheet){
                if(encore::ctr::loading_sprite_sheet_resident(a.path.c_str())){
                    a.sheet=encore::ctr::loading_sprite_sheet_acquire(a.path.c_str(),&error);
                    const auto image=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);
                    if(encore::ctr::loading_sprite_sheet_count(a.sheet)!=1||!image.tex||!image.subtex||image.subtex->width!=a.width||image.subtex->height!=a.height||Tex3DS_SubTextureRotated(image.subtex)){error="Resident texture metadata differs from checked dependency";return false;}
                    std::vector<uint8_t>().swap(a.texture_bytes);++gpu_asset_cursor_;continue;
                }
                if(a.texture_bytes.empty()){error="Prewarm texture owner expired";return false;}
                a.sheet=new(std::nothrow) encore::ctr::LoadingSpriteSheetData;
                if(!a.sheet||!C3D_TexInit(&a.sheet->texture,a.raw.texture_width,a.raw.texture_height,GPU_RGBA8)){error="Prewarm texture allocation failed";return false;}
                a.sheet->prepared_texture=true;admitted_linear_bytes_+=a.sheet->texture.size;a.sheet->source_path=encore::ctr::loading_texture_key(a.path.c_str());
                a.sheet->prepared_subtextures.push_back({u16(a.raw.image_width),u16(a.raw.image_height),a.raw.left,a.raw.top,a.raw.right,a.raw.bottom});
                a.sheet->texture.border=0;C3D_TexSetFilter(&a.sheet->texture,GPU_NEAREST,GPU_NEAREST);C3D_TexSetWrap(&a.sheet->texture,GPU_CLAMP_TO_BORDER,GPU_CLAMP_TO_BORDER);
                return true;
            }
            // Already-tiled exact pixels; no texture decoding, filesystem read
            // or whole-atlas memcpy is allowed in a frame's upload slice.
            constexpr size_t upload_quantum=64*1024;const size_t amount=std::min(upload_quantum,a.raw.size-a.uploaded);
            auto* target=static_cast<uint8_t*>(a.sheet->texture.data)+a.uploaded;
            std::memcpy(target,a.texture_bytes.data()+a.raw.offset+a.uploaded,amount);GSPGPU_FlushDataCache(target,amount);a.uploaded+=amount;
            if(a.uploaded==a.raw.size){
                if(!encore::ctr::loading_menu_flavor_detail::prepare(a.sheet)){error="Prewarm texture palette rejected";return false;}
                encore::ctr::loading_menu_flavor_detail::sheets.push_back(a.sheet);encore::ctr::loading_menu_flavor_detail::apply(a.sheet);
                std::vector<uint8_t>().swap(a.texture_bytes);++gpu_asset_cursor_;
            }
            return true;
        }
        if(gpu_finish_stage_++==0){
            unsigned tw=8,th=8;while(tw<surface_width_)tw<<=1;while(th<surface_height_)th<<=1;
            if(!C3D_TexInit(&surface_texture_,tw,th,GPU_RGBA8)){error="Prewarm surface allocation failed";return false;}
            texture_ready_=true;admitted_linear_bytes_+=surface_texture_.size;std::memset(surface_texture_.data,0,surface_texture_.size);
            C3D_TexSetFilter(&surface_texture_,GPU_NEAREST,GPU_NEAREST);C3D_TexSetWrap(&surface_texture_,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);return true;
        }
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        if(background_.region_fast_path())gpu_background_.create();
        else if(row_background_.ready()){
            if(!gpu_background_.create(16384))row_background_.clear();
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
            else gpu_row_texture_.create(gpu_layers_,surface_width_,surface_height_);
#endif
        }
#endif
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        if(mask_resource_<assets_.size()&&assets_[mask_resource_].mask_plan){
            const auto required=assets_[mask_resource_].mask_plan->resources(std::max<size_t>(surface_height_,gpu_background_.capacity())).maximum_output_spans;
            if(required*sizeof(encore::ctr::GpuTransitionMaskBatch::Span)>transition_gpu_budget||!gpu_mask_.create(surface_width_,surface_height_,required)){error="Transition GPU residency grant exceeded or allocation failed";return false;}
            admitted_linear_bytes_+=gpu_mask_.linear_bytes();
        }
        admitted_linear_bytes_+=gpu_background_.capacity()*sizeof(encore::ctr::GpuRegionBatch::Span);
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
        admitted_linear_bytes_+=gpu_row_texture_.linear_bytes();
#endif
#endif
        gpu_layers_.clear();deferred_gpu_=false;done=true;return true;
    }
    bool fully_resident()const{
        if(deferred_gpu_||!texture_ready_||background_resources_.empty())return false;
        for(const auto& a:assets_)if(a.frames?!a.all_frames:!a.sheet)return false;
        return true;
    }
    size_t admitted_linear_bytes()const{return admitted_linear_bytes_;}
    void use_indexed_residents(const std::vector<IndexedResident>* residents){indexed_resident_=residents;}
    size_t indexed_snapshot_bytes()const{
        size_t bytes=assets_.size()*sizeof(IndexedResident);for(const auto& a:assets_)if(a.all_frames)bytes+=a.path.capacity()+a.palette.capacity()*sizeof(uint32_t);return bytes;
    }
    std::vector<IndexedResident> indexed_residents()const{
        std::vector<IndexedResident> out;out.reserve(assets_.size());for(const auto& a:assets_)if(a.all_frames)out.push_back({a.path,a.width,a.height,a.frames,a.palette,a.all_frames
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
            ,a.mask_plan
#endif
        });return out;
    }
    size_t prepared_linear_bytes()const{
        size_t bytes=texture_ready_?surface_texture_.size:0;

#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        bytes+=gpu_mask_.linear_bytes();
        bytes+=gpu_background_.capacity()*sizeof(encore::ctr::GpuRegionBatch::Span);
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
        bytes+=gpu_row_texture_.linear_bytes();
#endif
#endif
        return bytes;
    }
    size_t prepared_cpu_bytes()const{
        size_t bytes=background_.allocated_bytes()+(surface_.capacity()+surface_offsets_.capacity())*sizeof(uint32_t)+assets_.capacity()*sizeof(Asset)+glyphs_.capacity()*sizeof(Glyph)+background_resources_.capacity()*sizeof(uint32_t)+gpu_layers_.capacity()*sizeof(encore::BackgroundKernel::Layer);
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        bytes+=row_background_.prepared_bytes();
#endif
        for(const auto& a:assets_){bytes+=a.palette.capacity()*sizeof(uint32_t)+a.pixels.capacity()+a.texture_bytes.capacity()+a.path.capacity();if(a.all_frames)bytes+=a.all_frames->capacity();
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
            if(a.mask_plan)bytes+=a.mask_plan->cpu_bytes()+sizeof(encore::TransitionMaskPlan)+64;
#endif
        }return bytes;
    }
    bool set_glyphs(std::vector<Glyph> glyphs,std::string& error){
        std::sort(glyphs.begin(),glyphs.end(),[](const Glyph& a,const Glyph& b){return a.codepoint<b.codepoint;});
        for(size_t i=0;i<glyphs.size();++i){
            const auto& g=glyphs[i];
            if(g.resource>=assets_.size()||assets_[g.resource].frames||(!assets_[g.resource].sheet&&!deferred_gpu_)||
               g.u>assets_[g.resource].width||g.width>assets_[g.resource].width-g.u||
               g.v>assets_[g.resource].height||g.height>assets_[g.resource].height-g.v||
               !std::isfinite(g.advance)||!std::isfinite(g.offset_x)||!std::isfinite(g.offset_y)||
               g.advance<0||(i&&glyphs[i-1].codepoint==g.codepoint)){
                error="Invalid external glyph metadata";return false;
            }
        }
        glyphs_=std::move(glyphs);return true;
    }
    bool set_background(const std::vector<BackgroundLayer>& layers,std::string& error,const encore::PreparationControl* control=nullptr){
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
        gpu_row_texture_.release();gpu_texture_surface_=false;
#endif
        gpu_mask_.release();mask_resource_=UINT32_MAX;gpu_mask_surface_=false;gpu_mask_count_=0;
        gpu_background_.release();row_background_.clear();gpu_surface_=false;gpu_span_count_=0;
#endif
        background_.clear();background_resources_.clear();
        std::vector<encore::BackgroundKernel::Layer> prepared;
        for(const auto& layer:layers){
            if(layer.resource>=assets_.size()||!assets_[layer.resource].frames){error="Invalid background layer resource";return false;}
            auto& a=assets_[layer.resource];
            if(!read_frame(a,0)){error="Background source cannot be read";return false;}
            encore::BackgroundKernel::Layer p;
            p.source={a.width,a.height,a.pixels.data(),a.palette.data(),a.palette.size()};
            p.barrel=layer.barrel;p.repeat=layer.repeat;p.width=layer.width;p.height=layer.height;
            p.opacity=layer.opacity;p.effect=layer.effect;p.effect_scale=layer.effect_scale;
            p.barrel_x=layer.barrel_x;p.barrel_y=layer.barrel_y;
            p.amplitude_x=layer.amplitude_x;p.amplitude_y=layer.amplitude_y;
            p.frequency_x=layer.frequency_x;p.frequency_y=layer.frequency_y;
            p.speed_x=layer.speed_x;p.speed_y=layer.speed_y;
            p.move_x=layer.move_x;p.move_y=layer.move_y;
            p.ping_pong_speed_x=layer.ping_pong_speed_x;p.ping_pong_speed_y=layer.ping_pong_speed_y;
            p.compression_amplitude_x=layer.compression_amplitude_x;p.compression_amplitude_y=layer.compression_amplitude_y;
            p.compression_frequency_x=layer.compression_frequency_x;p.compression_frequency_y=layer.compression_frequency_y;
            p.compression_speed_x=layer.compression_speed_x;p.compression_speed_y=layer.compression_speed_y;
            p.palette_shifting=layer.palette_shifting;p.palette_frames=layer.palette_frames;p.palette_speed=layer.palette_speed;
            p.palette_fixed_row=layer.palette_fixed_row;
            if(layer.palette_shifting){
                if(layer.palette_resource>=assets_.size()||!assets_[layer.palette_resource].frames){
                    error="Invalid background palette texture resource";return false;
                }
                auto& palette=assets_[layer.palette_resource];
                if(palette.frames!=1||!read_frame(palette,0)){error="Background palette texture cannot be read";return false;}
                p.palette_source={palette.width,palette.height,palette.pixels.data(),palette.palette.data(),palette.palette.size()};
                background_resources_.push_back(layer.palette_resource);
            }else if(layer.palette_resource!=UINT32_MAX){error="Disabled background palette has a resource";return false;}
            prepared.push_back(p);background_resources_.push_back(layer.resource);
        }
        if(!background_.prepare(prepared,surface_width_,surface_height_,error,control))return false;
        if(!background_.prepare_mapped_output(surface_offsets_.data(),surface_offsets_.size(),surface_offsets_.empty()?0:size_t(*std::max_element(surface_offsets_.begin(),surface_offsets_.end()))+1,error))return false;
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        // Both optional allocations must succeed; otherwise keep CPU dispatch.
        if(background_.region_fast_path()&&background_.prepare_spans()&&(deferred_gpu_||gpu_background_.create())){
#ifdef ENCORE_GPU_CERTIFICATE_TABLES
            background_.prepare_certificates(SIZE_MAX,control); // Optional bounded failure retains dynamic proofs.
#endif
        }else if(row_background_.prepare(prepared,surface_width_,surface_height_,control)){
            // Different supported semantics: both layers can oscillate in X.
            // Pillow's actual rows need >8192 exact spans on a 400px surface.
            if(!deferred_gpu_&&!gpu_background_.create(16384))row_background_.clear();
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
            else if(!deferred_gpu_)gpu_row_texture_.create(prepared,surface_width_,surface_height_); // Optional experimental allocation.
#endif
        }
#endif
        if(control&&control->stopped()){error="Background preparation cancelled";return false;}
        if(deferred_gpu_)gpu_layers_=std::move(prepared);
        return true;
    }
    // Explicit bounded profile grants, reserved by the dependency ticket.
    // A future larger mask is rejected at scene admission, never allocated
    // speculatively or silently introduced at a battle request.
    static constexpr size_t transition_cpu_budget=1024*1024;
    static constexpr size_t transition_gpu_budget=300*1024;
    bool prepare_transition(uint32_t resource,int32_t x,int32_t y,std::string& error,const encore::PreparationControl* control=nullptr){
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        if(resource>=assets_.size()||!assets_[resource].all_frames){error="Transition plan needs fully resident checked indices";return false;}
        auto& a=assets_[resource];mask_resource_=resource;
        if(a.mask_plan){const auto canvas=a.mask_plan->canvas();if(canvas.width==surface_width_&&canvas.height==surface_height_&&canvas.offset_x==x&&canvas.offset_y==y)return true;}
        auto plan=std::make_shared<encore::TransitionMaskPlan>();
        if(!plan->prepare({a.width,a.height,a.frames,a.all_frames->data(),a.all_frames->size(),a.palette.data(),a.palette.size()},{surface_width_,surface_height_,x,y},transition_cpu_budget,error,control))return false;
        if(plan->resources(16384).maximum_output_spans*sizeof(encore::ctr::GpuTransitionMaskBatch::Span)>transition_gpu_budget){error="Transition output exceeds bounded GPU residency profile";return false;}
        a.mask_plan=std::move(plan);
#else
        (void)resource;(void)x;(void)y;(void)error;(void)control;
#endif
        return true;
    }
    bool compose_transition_background(float time,uint32_t clear,uint32_t resource,uint32_t frame,uint32_t old_color,uint32_t new_color,int32_t x,int32_t y){
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        gpu_mask_surface_=gpu_surface_=false;gpu_mask_count_=gpu_span_count_=0;direct_surface_=false;
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
        gpu_texture_surface_=false;
#endif
        if(resource==mask_resource_&&resource<assets_.size()&&assets_[resource].mask_plan&&gpu_mask_.ready()){
            const auto& plan=*assets_[resource].mask_plan;const auto canvas=plan.canvas();
            bool ok=canvas.offset_x==x&&canvas.offset_y==y;
            if(ok&&plan.requires_background(frame,old_color))ok=gpu_background_.ready()&&(row_background_.ready()?row_background_.generate_spans(time,gpu_background_.data(),gpu_background_.capacity(),gpu_span_count_):background_.generate_spans(time,gpu_background_.data(),gpu_background_.capacity(),gpu_span_count_));
            if(ok&&plan.compose(frame,old_color,new_color,gpu_background_.data(),gpu_span_count_,gpu_mask_.data(),gpu_mask_.capacity(),gpu_mask_count_)){
                gpu_time_=time;gpu_clear_=clear;mask_frame_=frame;mask_old_=old_color;mask_new_=new_color;mask_x_=x;mask_y_=y;gpu_mask_surface_=true;return true;
            }
        }
#endif
        return compose_background(time,clear,false)&&compose_transition(resource,frame,old_color,new_color,x,y);
    }
    // TIME and all coefficients remain caller-supplied external content. The
    // kernel borrows stable decoded image buffers and never changes source UVs.
    bool compose_background(float time,uint32_t clear_color,bool direct_texture=false){
        direct_surface_=false;
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        gpu_surface_=false;gpu_span_count_=0;gpu_mask_surface_=false;gpu_mask_count_=0;
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
        gpu_texture_surface_=false;
#endif
#endif
        for(auto resource:background_resources_)if(!read_frame(assets_[resource],0))return false;
        // Caller has waited for the previous GPU queue with FrameBegin. The
        // normal transition path retains a linear CPU image for mask blending.
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
        if(direct_texture&&gpu_background_.ready()&&row_background_.ready()&&gpu_row_texture_.ready()&&
           gpu_row_texture_.prepare_frame(row_background_,time)&&
           row_background_.generate_spans(time,gpu_background_.data(),gpu_background_.capacity(),gpu_span_count_,gpu_row_texture_.certified_rows())){
            gpu_surface_=true;gpu_texture_surface_=true;gpu_time_=time;gpu_clear_=clear_color;return true;
        }
#endif
        if(direct_texture&&gpu_background_.ready()&&
           (row_background_.ready()?row_background_.generate_spans(time,gpu_background_.data(),gpu_background_.capacity(),gpu_span_count_):
            background_.generate_spans(time,gpu_background_.data(),gpu_background_.capacity(),gpu_span_count_))){
            gpu_surface_=true;gpu_time_=time;gpu_clear_=clear_color;return true;
        }
#endif
        if(direct_texture&&background_.compose_mapped(time,clear_color,static_cast<uint32_t*>(surface_texture_.data),surface_texture_.size/sizeof(uint32_t))){
            direct_surface_=true;return true;
        }
        return background_.compose(time,clear_color,surface_.data());
    }
    // The current surface is SCREEN_TEXTURE. This writes shader output only;
    // draw_surface's blend choice must match the audited source GPU backend.
    // In particular, a backend that treats blend_disabled as MIX still alpha-
    // composites these output pixels over the world drawn beneath the surface.
    // Offsets place a source-resolution mask within an expanded canvas. Pixels
    // outside that mask repeat its nearest border pixel, with no resampling.
    // The owner supplies this explicit display adaptation; (0,0) on the source
    // canvas preserves the reference path byte-for-byte.
    bool compose_transition(uint32_t resource,uint32_t frame,uint32_t old_color,uint32_t new_color,
                            int32_t offset_x=0,int32_t offset_y=0){
        if(direct_surface_||resource>=assets_.size())return false;
        auto& a=assets_[resource];
        if(a.width>surface_width_||a.height>surface_height_||!read_frame(a,frame))return false;
        for(uint32_t y=0;y<surface_height_;++y)for(uint32_t x=0;x<surface_width_;++x){
            const auto sx=uint32_t(std::clamp(int64_t(x)-offset_x,int64_t(0),int64_t(a.width)-1));
            const auto sy=uint32_t(std::clamp(int64_t(y)-offset_y,int64_t(0),int64_t(a.height)-1));
            const auto pixel=a.palette[a.pixels[size_t(sy)*a.width+sx]];
            const size_t i=size_t(y)*surface_width_+x;
            if(pixel==old_color)surface_[i]=new_color;
            else if((pixel>>24)!=255)surface_[i]=pixel;
        }
        return true;
    }
    void upload_surface(){
        if(!texture_ready_)return;
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        if(gpu_surface_||gpu_mask_surface_)return; // FrameEnd flushes the linear span buffer.
#endif
        if(direct_surface_){C3D_TexFlush(&surface_texture_);return;}
        auto* words=static_cast<uint32_t*>(surface_texture_.data);
        for(size_t i=0;i<surface_.size();++i){const auto c=surface_[i];
            words[surface_offsets_[i]]=((c&0xffu)<<24)|((c&0xff00u)<<8)|((c>>8)&0xff00u)|(c>>24);
        }
        C3D_TexFlush(&surface_texture_);
    }
    void draw_surface(float x,float y,float width,float height,bool overwrite=true){
        if(!texture_ready_)return;
#ifdef ENCORE_EXPERIMENTAL_GPU_BACKGROUND
        if(gpu_mask_surface_){
            if(!overwrite&&width==surface_width_&&height==surface_height_&&gpu_mask_.draw(gpu_mask_count_,x,y))return;
            // Rebuild both background and mask on a rejected draw contract.
            gpu_mask_surface_=false;gpu_mask_count_=0;
            if(!compose_background(gpu_time_,gpu_clear_,false)||!compose_transition(mask_resource_,mask_frame_,mask_old_,mask_new_,mask_x_,mask_y_))return;
            upload_surface();
        }
        if(gpu_surface_){
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
            if(gpu_texture_surface_){
                if(overwrite&&width==400&&height==240&&x==0&&y==0&&gpu_row_texture_.ready()&&gpu_background_.ready()){
                    gpu_row_texture_.draw();if(gpu_span_count_)gpu_background_.draw(gpu_span_count_,0,0);return;
                }
                gpu_texture_surface_=false;
            }else
#endif
            if(overwrite&&width==surface_width_&&height==surface_height_&&gpu_background_.draw(gpu_span_count_,x,y))return;
            // Unexpected draw contract: fall back before submitting a GPU batch.
            gpu_surface_=false;gpu_span_count_=0;gpu_mask_surface_=false;gpu_mask_count_=0;
#ifdef ENCORE_EXPERIMENTAL_GPU_TEXTURE_STRIPS
        gpu_texture_surface_=false;
#endif
            if(background_.compose_mapped(gpu_time_,gpu_clear_,static_cast<uint32_t*>(surface_texture_.data),surface_texture_.size/sizeof(uint32_t))){
                C3D_TexFlush(&surface_texture_);direct_surface_=true;
            }else{
                if(!background_.compose(gpu_time_,gpu_clear_,surface_.data()))return;
                upload_surface();
            }
        }
#endif
        if(overwrite){C2D_Flush();C3D_AlphaTest(false,GPU_ALWAYS,0);C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_ONE,GPU_ZERO,GPU_ONE,GPU_ZERO);}
        C2D_DrawImageAt({&surface_texture_,&surface_subtexture_},x,y,0,nullptr,width/surface_width_,height/surface_height_);
        if(overwrite){C2D_Flush();
            // C2D_Prepare does not restore global blend state. Restore ordinary
            // source-alpha composition explicitly before sprites and both UIs.
            C3D_AlphaBlend(GPU_BLEND_ADD,GPU_BLEND_ADD,GPU_SRC_ALPHA,GPU_ONE_MINUS_SRC_ALPHA,GPU_ONE,GPU_ONE_MINUS_SRC_ALPHA);
            C3D_AlphaTest(true,GPU_GREATER,0);C2D_Prepare();}
    }
    bool draw_region(uint32_t resource,uint32_t u,uint32_t v,uint32_t w,uint32_t h,
                     float x,float y,float width,float height,uint32_t color,float color_blend=0,float depth=0)const{
        if(resource>=assets_.size()||!w||!h||width<=0||height<=0)return false;
        const auto& a=assets_[resource];if(!a.sheet||u>a.width||w>a.width-u||v>a.height||h>a.height-v)return false;
        const auto img=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);auto sub=*img.subtex;
        const float du=(sub.right-sub.left)/a.width,dv=(sub.bottom-sub.top)/a.height,left=sub.left,top=sub.top;
        sub.left=left+u*du;sub.right=left+(u+w)*du;sub.top=top+v*dv;sub.bottom=top+(v+h)*dv;sub.width=w;sub.height=h;
        C2D_ImageTint tint;C2D_PlainImageTint(&tint,color,color_blend);
        return C2D_DrawImageAt({img.tex,&sub},x,y,depth,&tint,width/w,height/h);
    }
    bool draw_sprite(uint32_t resource,uint32_t frame,float x,float y,float width,float height,
                     uint32_t color,float color_blend=0,float depth=0)const{
        if(resource>=assets_.size())return false;
        const auto& a=assets_[resource];
        if(!a.columns||!a.rows||frame>=uint64_t(a.columns)*a.rows)return false;
        const auto w=a.width/a.columns,h=a.height/a.rows;
        return draw_region(resource,frame%a.columns*w,frame/a.columns*h,w,h,x,y,width,height,color,color_blend,depth);
    }
    bool draw_ninepatch(uint32_t resource,float x,float y,float width,float height,const uint32_t margins[4],
                        uint32_t color,float scale_x=1,float scale_y=1,float depth=0)const{
        if(resource>=assets_.size()||!assets_[resource].sheet)return false;
        const auto& a=assets_[resource];
        const unsigned l=margins[0],t=margins[1],r=margins[2],b=margins[3];
        if(l+r>a.width||t+b>a.height||width<(l+r)*scale_x||height<(t+b)*scale_y)return false;
        const unsigned us[]={0,l,a.width-r,a.width},vs[]={0,t,a.height-b,a.height};
        const float xs[]={x,x+l*scale_x,x+width-r*scale_x,x+width},ys[]={y,y+t*scale_y,y+height-b*scale_y,y+height};
        for(unsigned row=0;row<3;++row)for(unsigned col=0;col<3;++col)
            if(us[col+1]>us[col]&&vs[row+1]>vs[row])draw_region(resource,us[col],vs[row],us[col+1]-us[col],vs[row+1]-vs[row],xs[col],ys[row],xs[col+1]-xs[col],ys[row+1]-ys[row],color,0,depth);
        return true;
    }
    bool draw_tiled(uint32_t resource,float x,float y,float width,float height,uint32_t color,
                    float scale_x=1,float scale_y=1,float depth=0)const{
        if(resource>=assets_.size()||!assets_[resource].sheet||width<=0||height<=0||scale_x<=0||scale_y<=0)return false;
        const auto& a=assets_[resource];
        const unsigned w=a.width/a.columns,h=a.height/a.rows;
        for(float dy=0;dy<height;dy+=h*scale_y)for(float dx=0;dx<width;dx+=w*scale_x){
            const float dw=std::min(w*scale_x,width-dx),dh=std::min(h*scale_y,height-dy);
            const unsigned sw=unsigned(std::ceil(dw/scale_x)),sh=unsigned(std::ceil(dh/scale_y));
            if(!draw_region(resource,0,0,sw,sh,x+dx,y+dy,dw,dh,color,0,depth))return false;
        }
        return true;
    }
    float text_height(float legacy_height)const{
        const auto* face=source_font_?source_font_->catalog().selected():nullptr;
        return face&&!face->legacy_ascii?face->height:legacy_height;
    }
    float text_width(const char* text)const{
        if(!text)return 0;
        float width=0,longest=0;const std::string_view input(text);size_t cursor=0;uint32_t codepoint=0;
        while(cursor<input.size()){
            if(!encore::utf8_next(input,cursor,codepoint))return 0;
            if(codepoint=='\n'){longest=std::max(longest,width);width=0;continue;}
            if(source_font_&&source_font_->handles(codepoint)){
                float advance=0;if(!source_font_->glyph_advance(codepoint,advance))return 0;width+=advance;
            }else{const auto* g=glyph(codepoint);if(!g)return 0;width+=g->advance;}
        }
        return std::max(longest,width);
    }
    // UTF-8 is decoded to the checked glyph table. Unsupported scalars fail;
    // no system-font or replacement-character fallback is permitted.
    bool draw_text(const char* text,float x,float y,float scale_x,float scale_y,uint32_t color,float depth=0)const{
        if(!text)return false;
        const float origin_x=x;
        const auto* face=source_font_?source_font_->catalog().selected():nullptr;
        const std::string_view input(text);size_t cursor=0;uint32_t codepoint=0;
        while(cursor<input.size()){
            if(!encore::utf8_next(input,cursor,codepoint))return false;
            if(codepoint=='\n'){if(!face)return false;x=origin_x;y+=face->height*scale_y;continue;}
            if(source_font_&&source_font_->handles(codepoint)){
                const auto* g=source_font_->glyph(codepoint);if(!g||!source_font_->draw_glyph(*g,x,y,scale_x,scale_y,color,depth))return false;
                x+=g->advance*scale_x;continue;
            }
            const auto* g=glyph(codepoint);if(!g||!g->advance)return false;
            if(g->width&&g->height&&!draw_region(g->resource,g->u,g->v,g->width,g->height,x+g->offset_x*scale_x,y+g->offset_y*scale_y,
               g->width*scale_x,g->height*scale_y,color,1,depth))return false;
            x+=g->advance*scale_x;
        }
        return true;
    }
    // Source world dialogue clips its cumulative RichTextLabel. Crop each
    // glyph in source pixels rather than relying on screen-rotation scissoring.
    bool draw_text_clipped(const char* text,float x,float y,float left,float top,float right,float bottom,uint32_t color)const{
        if(!text||right<left||bottom<top)return false;
        const float origin_x=x;
        const auto* face=source_font_?source_font_->catalog().selected():nullptr;
        const std::string_view input(text);size_t cursor=0;uint32_t codepoint=0;
        while(cursor<input.size()){
            if(!encore::utf8_next(input,cursor,codepoint))return false;
            if(codepoint=='\n'){if(!face)return false;x=origin_x;y+=face->height;continue;}
            if(source_font_&&source_font_->handles(codepoint)){
                const auto* g=source_font_->glyph(codepoint);if(!g||!source_font_->draw_glyph_clipped(*g,x,y,left,top,right,bottom,color))return false;
                x+=g->advance;continue;
            }
            const auto* g=glyph(codepoint);if(!g||!g->advance)return false;
            const float gx=std::floor(x+g->offset_x+.5f),gy=std::floor(y+g->offset_y+.5f);
            const int x0=std::max(0,int(std::ceil(left-gx))),y0=std::max(0,int(std::ceil(top-gy)));
            const int x1=std::min(int(g->width),int(std::floor(right-gx))),y1=std::min(int(g->height),int(std::floor(bottom-gy)));
            if(x1>x0&&y1>y0&&!draw_region(g->resource,g->u+x0,g->v+y0,x1-x0,y1-y0,gx+x0,gy+y0,x1-x0,y1-y0,color,1))return false;
            x+=g->advance;
        }return true;
    }
    static void draw_rect(float x,float y,float width,float height,uint32_t color,float depth=0){C2D_DrawRectSolid(x,y,depth,width,height,color);}
};

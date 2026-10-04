#pragma once
#include "encore/introduction.hpp"
#include "encore/utf8.hpp"
#include "encore/crc32.hpp"
#include "source_font_renderer.hpp"
#include "loading_texture.hpp"
#include <citro2d.h>
#include <cmath>
#include <cstdio>
#include <vector>

namespace encore::ctr {
class IntroductionRenderer {
    const upstream::IntroductionData* data_=nullptr;
    std::vector<LoadingSpriteSheet> sheets_;
    SourceFontRenderer body_,hint_;
    static uint32_t rgba(uint32_t v){return C2D_Color32(v>>24,(v>>16)&255,(v>>8)&255,v&255);}
    static bool verify(const upstream::IntroResource& r,std::string& error){
        const auto path=std::string("romfs:/")+r.path;FILE* f=std::fopen(path.c_str(),"rb");
        if(!f){error="Introduction texture unavailable: "+r.path;return false;}
        uint32_t checksum=UINT32_MAX;size_t bytes=0;uint8_t buffer[8192];bool ok=true;
        while(bytes<r.bytes){const auto count=std::min(sizeof(buffer),size_t(r.bytes)-bytes);if(std::fread(buffer,1,count,f)!=count){ok=false;break;}checksum=crc32_update(checksum,buffer,count);bytes+=count;}
        if(std::fgetc(f)!=EOF||std::ferror(f))ok=false;if(std::fclose(f))ok=false;
        if(!ok||(checksum^UINT32_MAX)!=r.crc32){error="Introduction texture length/CRC mismatch: "+r.path;return false;}return true;
    }
    bool text(const SourceFontRenderer& font,std::string_view value,uint32_t visible,float center,float baseline,const upstream::IntroRect* clip,uint32_t color,float spacing,float character_spacing)const{
        size_t begin=0;uint32_t drawn=0;const auto* face=font.catalog().selected();if(!face)return false;
        while(begin<=value.size()){
            const auto end=value.find('\n',begin);const auto line=value.substr(begin,end==value.npos?value.size()-begin:end-begin);float width=0;std::string error;
            if(!font.catalog().measure(line,width,error))return false;
            size_t measure_cursor=0;uint32_t measure_cp=0;
            while(measure_cursor<line.size()){if(!utf8_next(line,measure_cursor,measure_cp))return false;if(measure_cp!=' '&&measure_cursor<line.size())width+=character_spacing;}
            float x=std::floor(center-width/2+.5f);size_t cursor=0;uint32_t cp=0;
            while(cursor<line.size()){
                if(!utf8_next(line,cursor,cp))return false;const auto* glyph=font.glyph(cp);if(!glyph)return false;
                const bool show=cp==' '||drawn++<visible;
                if(show){if(clip){if(!font.draw_glyph_clipped(*glyph,x,baseline,clip->x,clip->y,clip->x+clip->width,clip->y+clip->height,color))return false;}
                    else if(!font.draw_glyph(*glyph,x,baseline,1,1,color))return false;}
                x+=glyph->advance;
                if(cp!=' '&&cursor<line.size())x+=character_spacing;
            }
            if(end==value.npos)break;begin=end+1;baseline+=face->height+spacing;
        }
        return true;
    }
public:
    bool ready()const{return data_!=nullptr;}
    bool load(const upstream::IntroductionData& data,std::string& error){
        if(!data.valid()){error="Introduction renderer needs checked data";return false;}free();
        for(const auto& r:data.resources){
            if(!verify(r,error)){free();return false;}
            auto sheet=loading_sprite_sheet_load((std::string("romfs:/")+r.path).c_str(),&error);
            if(!sheet){free();return false;}sheets_.push_back(sheet);
            const auto image=loading_sprite_sheet_get_image(sheet,0);
            if(loading_sprite_sheet_count(sheet)!=1||!image.tex||!image.subtex||image.subtex->width!=r.width||image.subtex->height!=r.height||Tex3DS_SubTextureRotated(image.subtex)){error="Introduction atlas dimensions rejected";free();return false;}
            C3D_TexSetFilter(&sheet->texture,GPU_NEAREST,GPU_NEAREST);
        }
        const auto path=std::string("romfs:/")+data.font_catalog;const auto root=path.substr(0,path.rfind('/'));
        if(!body_.load_catalog_at_safe_boundary(path.c_str(),root.c_str(),error)||!hint_.load_catalog_at_safe_boundary(path.c_str(),root.c_str(),error)){free();return false;}
        data_=&data;error.clear();return true;
    }
    // Admission, font switches and release always occur before FrameBegin.
    bool prepare(const upstream::IntroductionPose& pose,std::string& error){
        if(!data_){error="Introduction renderer is not loaded";return false;}
        if(!pose.scene_visible){error.clear();return true;}
        return body_.select_font_at_safe_boundary(pose.font_source,error)&&body_.admit_selected_font(error)&&hint_.select_font_at_safe_boundary(pose.hint_font_source,error)&&hint_.admit_selected_font(error);
    }
    bool begin_frame(){return body_.begin_frame()&&hint_.begin_frame();}
    void end_frame(){body_.end_frame();hint_.end_frame();}
    void free(){for(auto* sheet:sheets_)if(sheet)loading_sprite_sheet_free(sheet);sheets_.clear();std::string error;body_.reset_at_safe_boundary(error);hint_.reset_at_safe_boundary(error);data_=nullptr;}
    bool draw(const upstream::IntroductionPose& pose,float width,float height)const{
        if(!data_||!pose.scene_visible)return false;
        C2D_DrawRectSolid(0,0,0,width,height,rgba(pose.background_color));
        for(const auto& background:pose.backgrounds)C2D_DrawRectSolid(background.rect.x,background.rect.y,0,background.rect.width,background.rect.height,rgba(background.color));
        for(const auto& image:pose.images){
            if(!image.visible||image.alpha<=0)continue;
            size_t id=0;while(id<data_->resources.size()&&data_->resources[id].path!=image.path)++id;
            if(id==sheets_.size()||image.frame>=data_->resources[id].frame_count)return false;
            const auto& resource=data_->resources[id];const auto atlas=loading_sprite_sheet_get_image(sheets_[id],0);
            Tex3DS_SubTexture sub=*atlas.subtex;const auto w=resource.width/resource.columns,h=resource.height/resource.rows;
            const auto u=image.frame%resource.columns*w,v=image.frame/resource.columns*h;
            const float du=(sub.right-sub.left)/resource.width,dv=(sub.bottom-sub.top)/resource.height,left=sub.left,top=sub.top;
            sub.left=left+u*du;sub.right=left+(u+w)*du;sub.top=top+v*dv;sub.bottom=top+(v+h)*dv;sub.width=w;sub.height=h;
            const C2D_Image sprite{atlas.tex,&sub};C2D_ImageTint tint;C2D_PlainImageTint(&tint,C2D_Color32(255,255,255,uint8_t(std::round(image.alpha*255))),1);
            if(!C2D_DrawImageAt(sprite,image.rect.x,image.rect.y,0,&tint,1,1))return false;
        }
        for(const auto& mask:pose.masks)C2D_DrawRectSolid(mask.rect.x,mask.rect.y,0,mask.rect.width,mask.rect.height,rgba(mask.color));
        const auto* face=body_.catalog().selected();
        const auto lines=1+std::count(pose.text.begin(),pose.text.end(),'\n');
        const float origin=pose.center_text_vertically&&face?(pose.text_clip.height-face->height*lines-pose.line_spacing*(lines-1))/2*pose.text_center_weight+pose.text_y:pose.text_y;
        if(!pose.text.empty()&&(!face||!text(body_,pose.text,pose.visible_characters,pose.text_clip.x+pose.text_clip.width/2,pose.text_clip.y+origin,&pose.text_clip,rgba(pose.text_color),pose.line_spacing,pose.text_character_spacing)))return false;
        if(pose.hint_alpha>0){const auto* h=hint_.catalog().selected();const auto color=(pose.hint_color&~255u)|uint32_t(std::round((pose.hint_color&255u)*pose.hint_alpha));if(!h||!text(hint_,pose.skip_text,UINT32_MAX,pose.hint_rect.x+pose.hint_rect.width/2,pose.hint_rect.y,&pose.hint_rect,rgba(color),0,pose.hint_character_spacing))return false;}
        return true;
    }
    bool draw_overlay(const upstream::IntroductionPose& pose)const{
        if(!data_)return false;
        for(const auto& mask:pose.door.masks)C2D_DrawRectSolid(mask.rect.x,mask.rect.y,0,mask.rect.width,mask.rect.height,rgba(mask.color));
        return true;
    }
};
}

#pragma once
#include "loading_texture.hpp"
#include "encore/loading_indicator_data.hpp"
#include <string>
#include <utility>

namespace encore::ctr {
class LoadingIndicatorRenderer {
public:
    LoadingIndicatorRenderer()=default;
    LoadingIndicatorRenderer(const LoadingIndicatorRenderer&)=delete;
    LoadingIndicatorRenderer& operator=(const LoadingIndicatorRenderer&)=delete;
    ~LoadingIndicatorRenderer(){free();}
    bool load(const char* prefix,const char* pack_path,std::string& error) {
        free();if(!prefix||!pack_path||!*pack_path){error="Loading indicator resource prefix missing";return false;}
        // Bootstrap must never call the heavy-load observer while its own
        // resources are incomplete, including when reloaded during an observer.
        encore::ScopedLoadProgress silence(nullptr);
        upstream::LoadingIndicatorData data;
        if(!data.load_file((std::string(prefix)+pack_path).c_str(),error))return false;
        auto sheet=loading_sprite_sheet_load((std::string(prefix)+data.texture_path()).c_str());
        if(!sheet||loading_sprite_sheet_count(sheet)!=1) {
            loading_sprite_sheet_free(sheet);error="Loading indicator texture unavailable";return false;
        }
        const auto image=loading_sprite_sheet_get_image(sheet,0);
        if(!image.tex||!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||
           image.subtex->width!=data.texture_width()||image.subtex->height!=data.texture_height()) {
            loading_sprite_sheet_free(sheet);error="Loading indicator texture dimensions rejected";return false;
        }
        C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
        data_=std::move(data);sheet_=sheet;error.clear();return true;
    }
    void free(){loading_sprite_sheet_free(sheet_);sheet_=nullptr;data_=upstream::LoadingIndicatorData{};}
    bool ready()const{return sheet_&&data_.valid();}
    bool valid()const{return ready();}
    uint32_t background_color()const{return data_.background_color();}
    int frame_at(double elapsed_seconds)const{return ready()?data_.frame_at(elapsed_seconds):-1;}
    bool draw(int width,int height,double elapsed_seconds)const {
        upstream::LoadingIndicatorPlacement pose;
        if(!ready()||!data_.sample(width,height,elapsed_seconds,pose))return false;
        return draw_pose(pose);
    }
    bool draw_progress(int width,int height,double elapsed_seconds,double fraction)const {
        upstream::LoadingIndicatorPlacement pose;
        if(!ready()||!data_.sample_progress(width,height,elapsed_seconds,fraction,pose))return false;
        return draw_pose(pose);
    }
private:
    bool draw_pose(const upstream::LoadingIndicatorPlacement& pose)const {
        const auto image=loading_sprite_sheet_get_image(sheet_,0);
        Tex3DS_SubTexture sub=*image.subtex;
        const float step=(sub.right-sub.left)/data_.texture_width();
        sub.left+=pose.frame*data_.frame_width()*step;
        sub.right=sub.left+data_.frame_width()*step;
        sub.width=static_cast<uint16_t>(data_.frame_width());
        sub.height=static_cast<uint16_t>(data_.frame_height());
        return C2D_DrawImageAt({image.tex,&sub},float(pose.x),float(pose.y),0);
    }
    upstream::LoadingIndicatorData data_;
    LoadingSpriteSheet sheet_=nullptr;
};
}

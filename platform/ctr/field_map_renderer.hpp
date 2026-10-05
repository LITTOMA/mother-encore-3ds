#pragma once
#include "loading_texture.hpp"
#include "encore/field_map.hpp"
#include <cmath>
#include <vector>

// A primitive consumer, deliberately without its own world draw order. The
// scene compositor merges source canvas/YSort/tile-z entries with actor poses.
// Texture pages are generated from source pixels; no CPU frame image is built.
class FieldMapRenderer {
    struct Asset {encore::ctr::LoadingSpriteSheet sheet=nullptr;uint32_t width=0,height=0;};
    std::vector<Asset> assets_;
public:
    FieldMapRenderer()=default;
    FieldMapRenderer(const FieldMapRenderer&)=delete;
    FieldMapRenderer& operator=(const FieldMapRenderer&)=delete;
    bool load(const encore::upstream::FieldMapView& data,const char* prefix,std::string& error){
        free();if(!data.valid()||!prefix){error="Map renderer needs checked field data";return false;}
        assets_.resize(data.texture_count());
        for(uint32_t i=0;i<assets_.size();++i){const auto source=data.texture(i);auto& a=assets_[i];a.width=source.width;a.height=source.height;
            const std::string path=std::string(prefix)+std::string(data.string(source.path));
            a.sheet=encore::ctr::loading_sprite_sheet_load(path.c_str());
            if(!a.sheet||encore::ctr::loading_sprite_sheet_count(a.sheet)!=1){error="Missing map texture page: "+path;free();return false;}
            const auto image=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);
            if(!image.tex||!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||image.subtex->width!=a.width||image.subtex->height!=a.height){error="Map texture page dimensions rejected: "+path;free();return false;}
            C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
        }error.clear();return true;
    }
    void free(){for(auto& a:assets_)if(a.sheet)encore::ctr::loading_sprite_sheet_free(a.sheet);assets_.clear();}
    bool draw(const encore::upstream::FieldMapDraw& pose,uint32_t texture,bool transpose,float camera_x,float camera_y) const {
        if(texture>=assets_.size())return false;const auto& a=assets_[texture];
        if(!a.sheet||pose.uv_position.x<0||pose.uv_position.y<0||pose.uv_size.x<=0||pose.uv_size.y<=0||pose.uv_position.x+pose.uv_size.x>a.width||pose.uv_position.y+pose.uv_size.y>a.height||!pose.signed_size.x||!pose.signed_size.y)return false;
        auto image=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);auto sub=*image.subtex;
        const float du=(sub.right-sub.left)/a.width,dv=(sub.bottom-sub.top)/a.height;
        const float left=sub.left+pose.uv_position.x*du,top=sub.top+pose.uv_position.y*dv;
        sub.left=left;sub.right=left+pose.uv_size.x*du;sub.top=top;sub.bottom=top+pose.uv_size.y*dv;
        sub.width=uint16_t(pose.uv_size.x);sub.height=uint16_t(pose.uv_size.y);
        if(pose.signed_size.x<0){const float u=sub.left;sub.left=sub.right;sub.right=u;}
        if(pose.signed_size.y<0){const float v=sub.top;sub.top=sub.bottom;sub.bottom=v;}
        const float width=std::fabs(pose.signed_size.x),height=std::fabs(pose.signed_size.y);
        C2D_ImageTint tint;C2D_PlainImageTint(&tint,C2D_Color32f(pose.color[0],pose.color[1],pose.color[2],pose.color[3]),0);
        // Source negative size flips UVs while retaining the rectangle origin.
        const float x=std::floor(pose.position.x-camera_x+.5f),y=std::floor(pose.position.y-camera_y+.5f);
        if(transpose){
            const float v=sub.top;sub.top=sub.bottom;sub.bottom=v;image.subtex=&sub;
            return C2D_DrawImageAtRotated(image,x+height/2,y+width/2,0,1.5707963267948966f,&tint,width/pose.uv_size.x,height/pose.uv_size.y);
        }
        image.subtex=&sub;return C2D_DrawImageAt(image,x,y,0,&tint,width/pose.uv_size.x,height/pose.uv_size.y);
    }
};

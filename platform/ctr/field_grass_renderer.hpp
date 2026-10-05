#pragma once
#include "loading_texture.hpp"
#include "encore/field_runtime.hpp"
#include <cmath>
#include <vector>

// Rendering consumes the admitted field pose; visibility and overlap events
// belong to the scene host and never advance timers or random state here.
class FieldGrassRenderer {
    struct Asset {
        encore::ctr::LoadingSpriteSheet sheet=nullptr;
        uint16_t width=0,height=0,columns=0,rows=0;
    };
    std::vector<Asset> assets_;
public:
    FieldGrassRenderer()=default;
    FieldGrassRenderer(const FieldGrassRenderer&)=delete;
    FieldGrassRenderer& operator=(const FieldGrassRenderer&)=delete;
    bool load(const encore::upstream::FieldData& data,const char* prefix,std::string& error) {
        free();
        if(!data.valid()||!prefix){error="Grass renderer requires checked field resources";return false;}
        assets_.resize(data.texture_count());
        for(uint32_t i=0;i<assets_.size();++i){
            const auto source=data.texture(i);auto& a=assets_[i];
            a.width=source.width;a.height=source.height;a.columns=source.columns;a.rows=source.rows;
            const std::string path=std::string(prefix)+std::string(data.string(source.path_string));
            a.sheet=encore::ctr::loading_sprite_sheet_load(path.c_str());
            if(!a.sheet||encore::ctr::loading_sprite_sheet_count(a.sheet)!=1){error="Missing grass texture: "+path;free();return false;}
            const auto image=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);
            if(!image.tex||!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||image.subtex->width!=a.width||image.subtex->height!=a.height){
                error="Grass texture dimensions differ from checked resource: "+path;free();return false;
            }
            C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
        }
        error.clear();return true;
    }
    void free(){for(auto& a:assets_)if(a.sheet)encore::ctr::loading_sprite_sheet_free(a.sheet);assets_.clear();}
    bool draw(const encore::upstream::FieldGrassDraw& pose,float camera_x,float camera_y) const {
        if(pose.texture_index>=assets_.size()||!std::isfinite(pose.scale_y)||pose.scale_y<=0)return false;
        const auto& a=assets_[pose.texture_index];
        if(!a.sheet||!a.columns||!a.rows||pose.frame>=a.columns*a.rows)return false;
        const unsigned width=a.width/a.columns,height=a.height/a.rows;
        const unsigned x=pose.frame%a.columns*width,y=pose.frame/a.columns*height;
        auto image=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);auto sub=*image.subtex;
        const float du=(sub.right-sub.left)/a.width,dv=(sub.bottom-sub.top)/a.height;
        const float left=sub.left+x*du,top=sub.top+y*dv;
        sub.left=left;sub.right=left+width*du;sub.top=top;sub.bottom=top+height*dv;sub.width=width;sub.height=height;
        if(pose.flip_h){const float u=sub.left;sub.left=sub.right;sub.right=u;}
        image.subtex=&sub;
        const float offset_x=pose.flip_h?-pose.sprite_offset.x:pose.sprite_offset.x;
        const float screen_x=pose.position.x+offset_x-camera_x-width*.5f;
        const float screen_y=pose.position.y+(pose.sprite_offset.y-height*.5f)*pose.scale_y-camera_y;
        return C2D_DrawImageAt(image,std::floor(screen_x+.5f),std::floor(screen_y+.5f),0,nullptr,1,pose.scale_y);
    }
};

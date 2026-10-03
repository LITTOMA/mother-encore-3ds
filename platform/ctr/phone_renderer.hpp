#pragma once
#include "loading_texture.hpp"
#include <citro2d.h>
#include "encore/phone_runtime.hpp"
#include <cmath>
#include <string>
#include <vector>
namespace encore::ctr {
class PhoneRenderer {
 struct Asset {encore::ctr::LoadingSpriteSheet sheet=nullptr;upstream::PhoneResource meta{};};
 std::vector<Asset>assets_;
public:
 bool load(upstream::PhoneView view,const char*root,std::string&error){
  free();if(!view||!root){error="Phone renderer needs checked content";return false;}
  assets_.resize(view.count(upstream::PhoneSection::Resources));
  for(uint32_t i=0;i<assets_.size();++i){auto&a=assets_[i];a.meta=view.resource(i);const auto path=std::string(root)+std::string(view.string(a.meta.path));a.sheet=encore::ctr::loading_sprite_sheet_load(path.c_str());
   if(!a.sheet||encore::ctr::loading_sprite_sheet_count(a.sheet)!=1){error="Phone texture unavailable: "+path;free();return false;}
   const auto image=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);
   if(!image.tex||!image.subtex||image.subtex->width!=a.meta.width||image.subtex->height!=a.meta.height||Tex3DS_SubTextureRotated(image.subtex)){error="Phone texture dimensions rejected";free();return false;}
   C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
  }return true;
 }
 void free(){for(auto&a:assets_)if(a.sheet)encore::ctr::loading_sprite_sheet_free(a.sheet);assets_.clear();}
 bool draw(const upstream::PhonePose&pose,float camera_x,float camera_y)const{
  if(pose.resource>=assets_.size())return false;const auto&a=assets_[pose.resource];const auto&m=a.meta;
  if(!m.columns||!m.rows||pose.frame>=m.columns*m.rows)return false;
  const auto image=encore::ctr::loading_sprite_sheet_get_image(a.sheet,0);Tex3DS_SubTexture sub=*image.subtex;
  const uint32_t w=m.width/m.columns,h=m.height/m.rows,u=pose.frame%m.columns*w,v=pose.frame/m.columns*h;
  const float du=(sub.right-sub.left)/m.width,dv=(sub.bottom-sub.top)/m.height,left=sub.left,top=sub.top;
  sub.left=left+u*du;sub.right=left+(u+w)*du;sub.top=top+v*dv;sub.bottom=top+(v+h)*dv;sub.width=w;sub.height=h;
  const float x=pose.center.x-camera_x-(pose.centered?w*.5f:0),y=pose.center.y-camera_y-(pose.centered?h*.5f:0);
  return C2D_DrawImageAt({image.tex,&sub},std::floor(x+.5f),std::floor(y+.5f),0);
 }
};
}

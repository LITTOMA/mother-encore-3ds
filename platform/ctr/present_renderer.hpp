#pragma once
#include "loading_texture.hpp"
#include <citro2d.h>
#include "encore/house_presents.hpp"
#include <cmath>
#include <string>
#include <vector>
namespace encore::ctr {
// House gift boxes: centered Sprite frame, then the centered Sparkles region.
class PresentRenderer {
 struct Asset {LoadingSpriteSheet sheet=nullptr;upstream::PresentTexture meta{};};
 std::vector<Asset>assets_;
 bool blit(uint32_t texture,unsigned u,unsigned v,unsigned w,unsigned h,float cx,float cy)const{
  if(texture>=assets_.size())return false;const auto&a=assets_[texture];
  if(!w||!h||u+w>a.meta.width||v+h>a.meta.height)return false;
  const auto image=loading_sprite_sheet_get_image(a.sheet,0);Tex3DS_SubTexture sub=*image.subtex;
  const float du=(sub.right-sub.left)/a.meta.width,dv=(sub.bottom-sub.top)/a.meta.height,left=sub.left,top=sub.top;
  sub.left=left+u*du;sub.right=left+(u+w)*du;sub.top=top+v*dv;sub.bottom=top+(v+h)*dv;sub.width=w;sub.height=h;
  return C2D_DrawImageAt({image.tex,&sub},std::floor(cx-w*.5f+.5f),std::floor(cy-h*.5f+.5f),0);
 }
public:
 bool load(upstream::PresentView view,const char*root,std::string&error){
  free();if(!view||!root){error="Present renderer needs checked content";return false;}
  assets_.resize(view.count(upstream::PresentSection::Textures));
  for(uint32_t i=0;i<assets_.size();++i){auto&a=assets_[i];a.meta=view.texture(i);const auto path=std::string(root)+std::string(view.string(a.meta.path));a.sheet=loading_sprite_sheet_load(path.c_str());
   if(!a.sheet||loading_sprite_sheet_count(a.sheet)!=1){error="Present texture unavailable: "+path;free();return false;}
   const auto image=loading_sprite_sheet_get_image(a.sheet,0);
   if(!image.tex||!image.subtex||image.subtex->width!=a.meta.width||image.subtex->height!=a.meta.height||Tex3DS_SubTextureRotated(image.subtex)){error="Present texture dimensions rejected";free();return false;}
   C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
  }return true;
 }
 void free(){for(auto&a:assets_)if(a.sheet)loading_sprite_sheet_free(a.sheet);assets_.clear();}
 bool loaded()const{return !assets_.empty();}
 bool draw(const upstream::PresentPose&pose,float camera_x,float camera_y)const{
  if(pose.texture>=assets_.size())return false;const auto&m=assets_[pose.texture].meta;
  const unsigned w=m.width/m.columns,h=m.height/m.rows;if(pose.frame>=unsigned(m.columns)*m.rows)return false;
  if(!blit(pose.texture,pose.frame%m.columns*w,pose.frame/m.columns*h,w,h,pose.position.x-camera_x,pose.position.y-camera_y))return false;
  if(!pose.sparkles)return true;
  const auto&r=pose.sparkle_region;
  return blit(pose.sparkle_texture,r.x,r.y,r.w,r.h,pose.sparkle_position.x-camera_x,pose.sparkle_position.y-camera_y);
 }
};
}

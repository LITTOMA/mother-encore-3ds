#pragma once
#include "loading_texture.hpp"
#include "encore/field_sparkles.hpp"
#include <cmath>
#include <string>
// Child draw primitive for the source scene's Canvas draw queue. The host owns
// depth/YSort, source parent material and lifetime. This never advances clocks.
class FieldSparklesRenderer {
 const encore::upstream::FieldSparklesData*data_=nullptr;encore::ctr::LoadingSpriteSheet sheet_=nullptr;
public:
 FieldSparklesRenderer()=default;FieldSparklesRenderer(const FieldSparklesRenderer&)=delete;FieldSparklesRenderer&operator=(const FieldSparklesRenderer&)=delete;~FieldSparklesRenderer(){free();}
 bool load(const encore::upstream::FieldSparklesData&d,const char*prefix,std::string&e){
  if(!d.valid()||!prefix){e="Sparkles renderer requires checked source resource";return false;}
  auto candidate=encore::ctr::loading_sprite_sheet_acquire((std::string(prefix)+d.texture_path()).c_str(),&e);if(!candidate)return false;
  auto reject=[&](const char*m){encore::ctr::loading_sprite_sheet_free(candidate);e=m;return false;};
  if(encore::ctr::loading_sprite_sheet_count(candidate)!=1){return reject("Sparkles atlas image count rejected");}
  auto image=encore::ctr::loading_sprite_sheet_get_image(candidate,0);
  if(!image.tex||!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||image.subtex->width!=d.width()||image.subtex->height!=d.height()){return reject("Sparkles source atlas extent rejected");}
  C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);free();sheet_=candidate;data_=&d;e.clear();return true;
 }
 void free(){if(sheet_){C3D_FrameSync();encore::ctr::loading_sprite_sheet_free(sheet_);}sheet_=nullptr;data_=nullptr;}
 bool draw(const encore::upstream::FieldSparklesDraw&pose,float camera_x,float camera_y)const{
  if(!sheet_||!data_||!data_->record(pose.id)||!std::isfinite(camera_x)||!std::isfinite(camera_y)){return false;}
  if(!pose.visible){return true;}
  auto f=pose.frame;if(!f.width||!f.height||f.x>data_->width()||f.y>data_->height()||f.width>data_->width()-f.x||f.height>data_->height()-f.y)return false;
  float x=pose.world_origin.x-camera_x,y=pose.world_origin.y-camera_y,x2=x+f.width*pose.world_scale.x,y2=y+f.height*pose.world_scale.y;if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(x2)||!std::isfinite(y2))return false;
  // AnimatedSprite floors local centered offset first. Native canvas shader
  // then snaps EACH transformed vertex; fractional collection scales retain
  // exact snapped extents instead of just snapping the upper-left position.
  if(pose.pixel_snap){x=std::floor(x+.5f);y=std::floor(y+.5f);x2=std::floor(x2+.5f);y2=std::floor(y2+.5f);}if(x==x2||y==y2)return true;
  auto image=encore::ctr::loading_sprite_sheet_get_image(sheet_,0);auto sub=*image.subtex;float du=(sub.right-sub.left)/data_->width(),dv=(sub.bottom-sub.top)/data_->height(),left=sub.left,top=sub.top;sub.left=left+f.x*du;sub.right=left+(f.x+f.width)*du;sub.top=top+f.y*dv;sub.bottom=top+(f.y+f.height)*dv;sub.width=uint16_t(f.width);sub.height=uint16_t(f.height);
  for(float v:pose.color){if(!std::isfinite(v)||v<0||v>1)return false;}
  auto c=[](float v){return uint32_t(std::floor(v*255+.5f));};uint32_t color=c(pose.color[0])|c(pose.color[1])<<8|c(pose.color[2])<<16|c(pose.color[3])<<24;C2D_ImageTint tint;C2D_PlainImageTint(&tint,color,0);return C2D_DrawImageAt({image.tex,&sub},x,y,0,&tint,(x2-x)/f.width,(y2-y)/f.height);
 }
};

#pragma once
#include "loading_texture.hpp"
#include "encore/present_sparkles.hpp"
#include <cmath>
#include <string>
// GPU atlas region draw, source pixels 1:1. Initialize/free outside GPU frames.
class PresentSparklesRenderer {
 const encore::upstream::PresentSparklesData*data_=nullptr;
 encore::ctr::LoadingSpriteSheet sheet_=nullptr;std::string error_;
public:
 PresentSparklesRenderer()=default;PresentSparklesRenderer(const PresentSparklesRenderer&)=delete;PresentSparklesRenderer&operator=(const PresentSparklesRenderer&)=delete;
 ~PresentSparklesRenderer(){free();}
 const std::string&error()const{return error_;}
 bool load(const encore::upstream::PresentSparklesData&data,const char*root){
  if(!data.valid()||!root){error_="Sparkles renderer requires admitted data/root";return false;}
  auto candidate=encore::ctr::loading_sprite_sheet_acquire((std::string(root)+data.texture_path()).c_str(),&error_);if(!candidate)return false;
  auto reject=[&](const char*message){encore::ctr::loading_sprite_sheet_free(candidate);error_=message;return false;};
  if(encore::ctr::loading_sprite_sheet_count(candidate)!=1)return reject("Sparkles texture region count");
  const auto image=encore::ctr::loading_sprite_sheet_get_image(candidate,0);
  if(!image.tex||!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||image.subtex->width!=data.width()||image.subtex->height!=data.height())return reject("Sparkles texture dimensions/rotation");
  C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);free();sheet_=candidate;data_=&data;error_.clear();return true;
 }
 void free(){if(sheet_){C3D_FrameSync();encore::ctr::loading_sprite_sheet_free(sheet_);}sheet_=nullptr;data_=nullptr;}
 // dx/dy are the existing world-to-upper-screen transform. The caller retains
 // original Present draw order: box Sprite first, Sparkles child immediately after.
 bool draw(const encore::upstream::PresentSparklesRuntime&runtime,float dx,float dy,uint32_t color=0xffffffffu)const{
  if(!sheet_||!data_||runtime.data()!=data_||!std::isfinite(dx)||!std::isfinite(dy))return false;if(!runtime.visible())return true;
  const auto*frame=runtime.frame();if(!frame)return false;const auto image=encore::ctr::loading_sprite_sheet_get_image(sheet_,0);auto sub=*image.subtex;
  const float du=(sub.right-sub.left)/data_->width(),dv=(sub.bottom-sub.top)/data_->height(),left=sub.left,top=sub.top;
  sub.left=left+frame->x*du;sub.right=left+(frame->x+frame->width)*du;sub.top=top+frame->y*dv;sub.bottom=top+(frame->y+frame->height)*dv;sub.width=uint16_t(frame->width);sub.height=uint16_t(frame->height);
  const auto parent=data_->parent_position(),position=data_->position(),offset=data_->offset();float x=dx+parent.x+position.x+offset.x,y=dy+parent.y+position.y+offset.y;if(data_->centered()){x-=frame->width/2.f;y-=frame->height/2.f;}if(data_->pixel_snap()){x=std::floor(x+.5f);y=std::floor(y+.5f);}
  C2D_ImageTint paint;C2D_PlainImageTint(&paint,color,0);return C2D_DrawImageAt({image.tex,&sub},x,y,0,&paint,1,1);
 }
};

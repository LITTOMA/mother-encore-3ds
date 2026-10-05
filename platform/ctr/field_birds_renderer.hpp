#pragma once
#include "loading_texture.hpp"
#include "encore/field_birds.hpp"
#include <cmath>
#include <string>
#include <vector>
// Source GPU atlas primitive. The actual SceneHost owns Canvas/YSort order,
// visible-in-tree state, ancestors and lifetime. No clocks or RNG live here.
class FieldBirdRenderer {
 const encore::upstream::FieldBirdData*data_=nullptr;
 std::vector<encore::ctr::LoadingSpriteSheet>sheets_;
public:
 FieldBirdRenderer()=default;FieldBirdRenderer(const FieldBirdRenderer&)=delete;FieldBirdRenderer&operator=(const FieldBirdRenderer&)=delete;~FieldBirdRenderer(){free();}
 bool load(const encore::upstream::FieldBirdData&d,const char*prefix,std::string&e){
  if(!d.valid()||!prefix){e="Bird renderer requires checked data";return false;}
  std::vector<encore::ctr::LoadingSpriteSheet>candidate;
  auto reject=[&](const char*m){for(auto s:candidate)encore::ctr::loading_sprite_sheet_free(s);e=m;return false;};
  for(const auto&s:d.skins()){
   auto sheet=encore::ctr::loading_sprite_sheet_acquire((std::string(prefix)+s.path).c_str(),&e);if(!sheet){for(auto old:candidate)encore::ctr::loading_sprite_sheet_free(old);return false;}candidate.push_back(sheet);
   if(encore::ctr::loading_sprite_sheet_count(sheet)!=1)return reject("Bird atlas image count rejected");
   auto image=encore::ctr::loading_sprite_sheet_get_image(sheet,0);
   if(!image.tex||!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||image.subtex->width!=s.width||image.subtex->height!=s.height)return reject("Bird atlas source extent rejected");
   C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
  }
  free();sheets_=std::move(candidate);data_=&d;e.clear();return true;
 }
 void free(){if(!sheets_.empty())C3D_FrameSync();for(auto s:sheets_)encore::ctr::loading_sprite_sheet_free(s);sheets_.clear();data_=nullptr;}
 bool draw(const encore::upstream::FieldBirdDraw&p,float camera_x,float camera_y)const{
  if(!data_||!data_->record(p.id)||p.skin>=sheets_.size()||p.frame>=data_->columns()*data_->rows()||!std::isfinite(camera_x)||!std::isfinite(camera_y))return false;
  if(!p.visible)return true;
  auto*s=data_->skin(p.skin);if(!s)return false;uint32_t w=s->width/data_->columns(),h=s->height/data_->rows(),fx=(p.frame%data_->columns())*w,fy=(p.frame/data_->columns())*h;
  float x=p.origin.x-camera_x,y=p.origin.y-camera_y,x2=x+w*p.scale.x,y2=y+h*p.scale.y;if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(x2)||!std::isfinite(y2))return false;
  if(p.pixel_snap){x=std::floor(x+.5f);y=std::floor(y+.5f);x2=std::floor(x2+.5f);y2=std::floor(y2+.5f);}if(x==x2||y==y2)return true;
  auto image=encore::ctr::loading_sprite_sheet_get_image(sheets_[p.skin],0);auto sub=*image.subtex;float du=(sub.right-sub.left)/s->width,dv=(sub.bottom-sub.top)/s->height,left=sub.left,top=sub.top;sub.left=left+fx*du;sub.right=left+(fx+w)*du;sub.top=top+fy*dv;sub.bottom=top+(fy+h)*dv;sub.width=uint16_t(w);sub.height=uint16_t(h);if(p.flip)std::swap(sub.left,sub.right);
  for(float c:p.color)if(!std::isfinite(c)||c<0||c>1)return false;
  auto c=[](float v){return uint32_t(std::floor(v*255+.5f));};uint32_t color=c(p.color[0])|c(p.color[1])<<8|c(p.color[2])<<16|c(p.color[3])<<24;C2D_ImageTint tint;C2D_PlainImageTint(&tint,color,0);return C2D_DrawImageAt({image.tex,&sub},x,y,0,&tint,(x2-x)/w,(y2-y)/h);
 }
};

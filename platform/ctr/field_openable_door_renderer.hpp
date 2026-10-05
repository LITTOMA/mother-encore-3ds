#pragma once
#include "loading_texture.hpp"
#include "encore/field_openable_door.hpp"
#include "encore/field_geometry.hpp"
#include <cmath>
// Primitive for the common source Canvas/YSort queue. The caller supplies live
// parent transforms/visibility and source tint. Rendering never drives state.
class FieldOpenableDoorRenderer {
 std::vector<encore::ctr::LoadingSpriteSheet>textures_;
public:
 FieldOpenableDoorRenderer()=default;FieldOpenableDoorRenderer(const FieldOpenableDoorRenderer&)=delete;FieldOpenableDoorRenderer&operator=(const FieldOpenableDoorRenderer&)=delete;
 bool load(const encore::upstream::FieldOpenableDoorData&data,const char*prefix,std::string&error){
  free();if(!data.valid()||!prefix){error="OpenableDoor renderer requires checked source data";return false;}textures_.resize(data.texture_count());
  for(uint32_t i=0;i<textures_.size();++i){const auto*t=data.texture(i);std::string path=std::string(prefix)+t->path;textures_[i]=encore::ctr::loading_sprite_sheet_load(path.c_str());if(!textures_[i]||encore::ctr::loading_sprite_sheet_count(textures_[i])!=1){error="OpenableDoor texture missing: "+path;free();return false;}auto image=encore::ctr::loading_sprite_sheet_get_image(textures_[i],0);if(!image.tex||!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||image.subtex->width!=t->region[2]||image.subtex->height!=t->region[3]){error="OpenableDoor native texture geometry rejected: "+path;free();return false;}C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
  }error.clear();return true;
 }
 void free(){for(auto&sheet:textures_)if(sheet){encore::ctr::loading_sprite_sheet_free(sheet);sheet=nullptr;}textures_.clear();}
 bool draw(const encore::upstream::FieldOpenableDescriptor&d,const encore::upstream::FieldOpenableInstance&s,const encore::upstream::FieldGeometryTransform&root,bool parent_visible,float camera_x,float camera_y,float depth=0,const C2D_ImageTint*tint=nullptr)const{
  if(!s.ready||!parent_visible||!s.sprite_visible||s.id!=d.id||d.texture>=textures_.size()||!textures_[d.texture]||root.x.y!=0||root.y.x!=0)return false;
  auto image=encore::ctr::loading_sprite_sheet_get_image(textures_[d.texture],0);float sx=root.x.x*d.sprite_scale.x,sy=root.y.y*d.sprite_scale.y;
  float x=root.origin.x+root.x.x*(s.sprite_position.x+d.sprite_scale.x*(d.sprite_offset.x-image.subtex->width*.5f))-camera_x;
  float y=root.origin.y+root.y.y*(s.sprite_position.y+d.sprite_scale.y*(d.sprite_offset.y-image.subtex->height*.5f))-camera_y;
  if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(sx)||!std::isfinite(sy)||!std::isfinite(depth)||sx==0||sy==0)return false;
  return C2D_DrawImageAt(image,std::floor(x+.5f),std::floor(y+.5f),depth,tint,sx,sy);
 }
};

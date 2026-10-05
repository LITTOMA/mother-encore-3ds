#pragma once
#include "loading_texture.hpp"
#include "encore/field_dandelion.hpp"
#include <array>
#include <cmath>
// GPU sprite primitive only. The scene host supplies actual source parent
// visibility, world position and YSort ordering; this does not emit particles.
class FieldDandelionRenderer {
 std::array<encore::ctr::LoadingSpriteSheet,2>textures_{};
 encore::upstream::FieldDandelionProfile profile_{};
 std::array<encore::upstream::FieldDandelionTexture,2>descriptors_{};
public:
 FieldDandelionRenderer()=default;
 FieldDandelionRenderer(const FieldDandelionRenderer&)=delete;
 FieldDandelionRenderer&operator=(const FieldDandelionRenderer&)=delete;
 bool load(const encore::upstream::FieldDandelionData&data,const char*prefix,std::string&error){
  free();if(!data.valid()||!prefix){error="Dandelion renderer requires checked resources";return false;}profile_=data.profile();
  for(uint32_t i=0;i<2;++i){descriptors_[i]=data.texture(i);const auto&t=descriptors_[i];auto path=std::string(prefix)+std::string(data.string(t.path));textures_[i]=encore::ctr::loading_sprite_sheet_load(path.c_str());
   if(!textures_[i]||encore::ctr::loading_sprite_sheet_count(textures_[i])!=1){error="Dandelion source texture missing: "+path;free();return false;}auto image=encore::ctr::loading_sprite_sheet_get_image(textures_[i],0);
   if(!image.tex||!image.subtex||Tex3DS_SubTextureRotated(image.subtex)||image.subtex->width!=t.width||image.subtex->height!=t.height){error="Dandelion source texture geometry rejected: "+path;free();return false;}C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
  }error.clear();return true;
 }
 void free(){for(auto&sheet:textures_)if(sheet){encore::ctr::loading_sprite_sheet_free(sheet);sheet=nullptr;}}
 bool draw(const encore::upstream::FieldDandelionInstance&pose,float camera_x,float camera_y)const{
  if(!pose.added||!(profile_.flags&4)||profile_.sprite_texture>=textures_.size()||!std::isfinite(pose.world.x)||!std::isfinite(pose.world.y)||pose.frame>=profile_.columns*profile_.rows)return false;
  auto i=profile_.sprite_texture;if(!textures_[i])return false;auto image=encore::ctr::loading_sprite_sheet_get_image(textures_[i],0);auto sub=*image.subtex;auto descriptor=descriptors_[i];const uint32_t width=descriptor.width/profile_.columns,height=descriptor.height/profile_.rows,x=pose.frame%profile_.columns*width,y=pose.frame/profile_.columns*height;
  const float du=(sub.right-sub.left)/descriptor.width,dv=(sub.bottom-sub.top)/descriptor.height,left=sub.left+x*du,top=sub.top+y*dv;sub.left=left;sub.right=left+width*du;sub.top=top;sub.bottom=top+height*dv;sub.width=width;sub.height=height;image.subtex=&sub;
  const float screen_x=pose.world.x+profile_.sprite_position.x+profile_.sprite_offset.x-width*.5f-camera_x,screen_y=pose.world.y+profile_.sprite_position.y+profile_.sprite_offset.y-height*.5f-camera_y;
  return C2D_DrawImageAt(image,std::floor(screen_x+.5f),std::floor(screen_y+.5f),0,nullptr,1,1);
 }
};

#pragma once
#include "loading_texture.hpp"
#include "encore/content.hpp"
#include "encore/field_swing_resources.hpp"
#include "encore/field_player_tree.hpp"
#include <cmath>

// One immutable pair of compact atlases, shared by every scene's persistent
// player. The current tree is supplied at draw time, never borrowed from a
// temporary scene that may be replaced by ScenePlayerTransfer.
class FieldPlayerRenderer final {
 using Sheet=encore::ctr::LoadingSpriteSheet;
 std::array<Sheet,2>sheets_{};const encore::upstream::FieldSwingResources*resources_=nullptr;
public:
 ~FieldPlayerRenderer(){free();}
 FieldPlayerRenderer()=default;FieldPlayerRenderer(const FieldPlayerRenderer&)=delete;FieldPlayerRenderer&operator=(const FieldPlayerRenderer&)=delete;
 bool ready()const{return resources_!=nullptr;}
 bool load(const encore::upstream::FieldSwingResources&resources,const char*root,std::string&error){
  if(ready()||!resources.valid()||!root){error="Player graphics require one checked resource owner and root";return false;}
  std::array<Sheet,2>candidate{};const auto fail=[&](const std::string&why){for(auto sheet:candidate)if(sheet)encore::ctr::loading_sprite_sheet_free(sheet);error=why;return false;};
  for(size_t i=0;i<candidate.size();++i){const auto&layer=resources.graphics().layers()[i];const auto path=std::string(root)+(std::string(root).empty()||std::string(root).back()=='/'?"":"/")+layer.path;std::vector<uint8_t>bytes;
   if(!encore::read_file(path.c_str(),bytes,8*1024*1024,error)||!resources.graphics().texture_matches(layer.role,bytes.data(),bytes.size(),error))return fail(error);
   candidate[i]=encore::ctr::loading_sprite_sheet_load(path.c_str());if(!candidate[i]||encore::ctr::loading_sprite_sheet_count(candidate[i])!=1)return fail("Player texture missing or invalid: "+path);
   const auto image=encore::ctr::loading_sprite_sheet_get_image(candidate[i],0);if(!image.tex||!image.subtex||image.subtex->width!=layer.width||image.subtex->height!=layer.height||Tex3DS_SubTextureRotated(image.subtex))return fail("Player atlas layout differs from metadata: "+path);
   C3D_TexSetFilter(image.tex,GPU_NEAREST,GPU_NEAREST);
  }
  sheets_=candidate;resources_=&resources;error.clear();return true;
 }
 void free(){for(auto&sheet:sheets_){if(sheet)encore::ctr::loading_sprite_sheet_free(sheet);sheet=nullptr;}resources_=nullptr;}
 bool draw(const encore::upstream::FieldPlayerTree&tree,encore::upstream::Vec2 position,float cx,float cy)const{
  if(!ready()||!tree.ready())return false;const auto&pose=tree.pose();if(!pose.special)return true;
  const auto&data=resources_->graphics();const auto*frame=data.frame(pose.frame);if(!frame)return false;
  const auto layer=[&](size_t i){const auto&record=data.layers()[i];auto origin=position;if(record.parent){origin.x+=data.layers()[0].position.x;origin.y+=data.layers()[0].position.y;}origin.x+=record.position.x+record.offset.x;origin.y+=record.position.y+record.offset.y;
   const auto image=encore::ctr::loading_sprite_sheet_get_image(sheets_[i],0);auto sub=*image.subtex;const auto du=(sub.right-sub.left)/record.width,dv=(sub.bottom-sub.top)/record.height,left=sub.left,top=sub.top;
   sub.left=left+frame->x*du;sub.right=left+(frame->x+frame->width)*du;sub.top=top+frame->y*dv;sub.bottom=top+(frame->y+frame->height)*dv;sub.width=frame->width;sub.height=frame->height;
   if(record.flags&1){origin.x-=frame->width*.5f;origin.y-=frame->height*.5f;}if(record.flags&2)std::swap(sub.left,sub.right);if(record.flags&4)std::swap(sub.top,sub.bottom);
   C2D_DrawImageAt({image.tex,&sub},std::floor(origin.x-cx+.5f),std::floor(origin.y-cy+.5f),0);
  };
  const bool bat=tree.selected_skill()==resources_->action().strings()[3];if(bat&&pose.behind)layer(1);layer(0);if(bat&&!pose.behind)layer(1);return true;
 }
};

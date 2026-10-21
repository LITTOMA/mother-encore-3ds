#pragma once
#include <citro2d.h>
#include "encore/content.hpp"
#include "encore/dungeon_key_hud_graphics.hpp"
#include <cmath>
#include <limits>
#include <memory>
// Own one checked icon. Borrow the caller's already resident BottleRocket
// font only for the current frame, using byte identity and reload generation.
// Prepare/retire/destruction require the caller's GPU-idle UI boundary.
class DungeonKeyHudRenderer final {
 using Data=encore::upstream::DungeonKeyHudGraphicsData;
 using Hud=encore::upstream::DungeonKeyIndicator;
 struct Texture {C3D_Tex tex{};Tex3DS_Texture metadata=nullptr;~Texture(){if(metadata){Tex3DS_TextureFree(metadata);C3D_TexDelete(&tex);}}};
 std::unique_ptr<Texture>icon_;Data data_;encore::upstream::KeyHudDraw command_;
 uint64_t epoch_=0,last_epoch_=0,serial_=0,frame_=0,font_generation_=0;const void*font_owner_=nullptr;
 static bool bad(std::string&e,const char*s){e=s;return false;}
 void invalidate(){frame_=font_generation_=0;font_owner_=nullptr;command_={};}
 void clear(){invalidate();if(icon_)C2D_Flush();icon_.reset();data_={};epoch_=0;}
public:
 DungeonKeyHudRenderer()=default;~DungeonKeyHudRenderer(){clear();}
 DungeonKeyHudRenderer(const DungeonKeyHudRenderer&)=delete;DungeonKeyHudRenderer&operator=(const DungeonKeyHudRenderer&)=delete;
 bool ready()const{return epoch_!=0;}uint64_t frame()const{return frame_;}const auto&command()const{return command_;}
 bool prepare(const Data&data,const Hud&hud,const char*root,std::string&e){
  if(ready()||!root||!*root||!data.matches(hud)||hud.epoch()<=last_epoch_)return bad(e,"Key HUD graphics preparation contract/root/epoch rejected");
  const auto&c=data.config();std::string prefix(root);if(prefix.back()!='/')prefix+='/';std::vector<uint8_t>bytes;
  if(!encore::read_file((prefix+c.texture).c_str(),bytes,8*1024*1024,e)||!data.texture_matches(bytes.data(),bytes.size(),e))return false;
  auto next=std::make_unique<Texture>();next->metadata=Tex3DS_TextureImport(bytes.data(),bytes.size(),&next->tex,nullptr,false);
  if(!next->metadata)return bad(e,"Key HUD icon decode/allocation failed");
  const auto*sub=Tex3DS_GetSubTexture(next->metadata,0);
  if(Tex3DS_GetNumSubTextures(next->metadata)!=1||!sub||sub->width!=c.width||sub->height!=c.height||Tex3DS_SubTextureRotated(sub)||!std::isfinite(sub->left)||!std::isfinite(sub->right)||!std::isfinite(sub->top)||!std::isfinite(sub->bottom)||sub->left<0||sub->left>=sub->right||sub->right>1||sub->bottom<0||sub->bottom>=sub->top||sub->top>1)return bad(e,"Key HUD icon image layout rejected");
  C3D_TexSetFilter(&next->tex,GPU_NEAREST,GPU_NEAREST);next->tex.border=0;C3D_TexSetWrap(&next->tex,GPU_CLAMP_TO_BORDER,GPU_CLAMP_TO_BORDER);
  icon_=std::move(next);data_=data;epoch_=last_epoch_=hud.epoch();invalidate();e.clear();return true;
 }
 template<class Font>bool compose(const Hud&hud,const Font&font,float w,float h,std::string&e){
  invalidate();if(!ready()||hud.epoch()!=epoch_||serial_==std::numeric_limits<uint64_t>::max())return bad(e,"Key HUD graphics frame epoch/serial rejected");const auto&c=data_.config();
  if(!font.bottle_matches(c.font_bytes,c.font_crc)||!font.bottle_generation())return bad(e,"Key HUD prepared font identity/generation rejected");
  encore::upstream::KeyHudDraw next;if(!data_.compose(hud,w,h,next,e))return false;
  command_=std::move(next);font_owner_=&font;font_generation_=font.bottle_generation();frame_=++serial_;e.clear();return true;
 }
 template<class Font>bool draw(uint64_t epoch,uint64_t frame,const Font&font,std::string&e)const{
  if(!ready()||epoch!=epoch_||!frame_||frame!=frame_||&font!=font_owner_||font.bottle_generation()!=font_generation_||!font.bottle_matches(data_.config().font_bytes,data_.config().font_crc))return bad(e,"Key HUD draw frame/font owner/generation rejected");
  if(!command_.visible){e.clear();return true;}
  const auto&c=data_.config();C2D_Flush();C3D_TexBind(0,&icon_->tex);
  if(!C2D_DrawImageAt({&icon_->tex,Tex3DS_GetSubTexture(icon_->metadata,0)},std::floor(command_.icon.x+.5f),std::floor(command_.icon.y+.5f),0)||!font.draw_bottle(command_.label,std::floor(command_.text.x+.5f),std::floor(command_.text.y+.5f),c.color,font_generation_))return bad(e,"Key HUD icon/font draw queue rejected");
  e.clear();return true;
 }
 bool retire(uint64_t epoch,std::string&e){if(!ready()||epoch!=epoch_)return bad(e,"Key HUD graphics retirement epoch rejected");clear();e.clear();return true;}
};

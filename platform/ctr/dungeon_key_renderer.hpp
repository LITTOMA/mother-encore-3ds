#pragma once
#include <citro2d.h>
#include "encore/content.hpp"
#include "encore/dungeon_key_media.hpp"
#include <cmath>
#include <limits>
#include <memory>

// Prepared object images only. A scene merges commands() with its terrain and
// actors using their parent/order/y-sort/z metadata, then submits indices in
// that order. This service does not invent a flat ordering for the whole map.
// Prepare/retire/destruction require the caller's GPU-idle scene boundary.
class DungeonKeyRenderer final {
 using Data=encore::upstream::DungeonKeyMediaData;
 using World=encore::upstream::DungeonKeyClocks;
 struct Texture {
  uint32_t profile=0;C3D_Tex texture{};Tex3DS_Texture metadata=nullptr;
  ~Texture(){if(metadata){Tex3DS_TextureFree(metadata);C3D_TexDelete(&texture);}}
  Texture()=default;Texture(const Texture&)=delete;Texture&operator=(const Texture&)=delete;
 };
 std::vector<std::unique_ptr<Texture>>textures_;
 Data data_;uint64_t epoch_=0,last_epoch_=0,serial_=0,frame_=0;
 std::vector<encore::upstream::KeyDraw>commands_;
 static bool fail(std::string&e,const char*why){e=why;return false;}
 const Texture*texture(uint32_t profile)const{for(const auto&t:textures_)if(t->profile==profile)return t.get();return nullptr;}
 void clear(){if(!textures_.empty())C2D_Flush();textures_.clear();commands_.clear();data_={};epoch_=frame_=0;}
public:
 DungeonKeyRenderer()=default;~DungeonKeyRenderer(){clear();}
 DungeonKeyRenderer(const DungeonKeyRenderer&)=delete;DungeonKeyRenderer&operator=(const DungeonKeyRenderer&)=delete;
 bool ready()const{return epoch_!=0;}uint64_t epoch()const{return epoch_;}uint64_t frame()const{return frame_;}
 const std::vector<encore::upstream::KeyDraw>&commands()const{return commands_;}
 bool prepare(const Data&data,const World&world,const char*root,std::string&e){
  if(ready()||!root||!*root||!data.matches(world,e)||!world.epoch()||world.epoch()<=last_epoch_)
   return fail(e,"Key graphics resource/root/epoch rejected");
  std::vector<std::unique_ptr<Texture>>candidate;candidate.reserve(data.sprites().size());
  std::string prefix(root);if(prefix.back()!='/')prefix+='/';
  for(const auto&s:data.sprites()){
   std::vector<uint8_t>bytes;const auto path=prefix+s.texture;
   if(!encore::read_file(path.c_str(),bytes,8*1024*1024,e)||!data.texture_matches(s.id,bytes.data(),bytes.size(),e))return false;
   auto t=std::make_unique<Texture>();t->profile=s.id;
   // Decode precisely the admitted bytes. Do not reopen the file or borrow a
   // global path cache which may belong to a different resource generation.
   t->metadata=Tex3DS_TextureImport(bytes.data(),bytes.size(),&t->texture,nullptr,false);
   if(!t->metadata)return fail(e,"Key graphics texture decode/allocation failed");
   if(Tex3DS_GetNumSubTextures(t->metadata)!=1)return fail(e,"Key graphics requires one atlas image");
   const auto*sub=Tex3DS_GetSubTexture(t->metadata,0);
   if(!sub||sub->width!=s.width||sub->height!=s.height||Tex3DS_SubTextureRotated(sub)||
      !std::isfinite(sub->left)||!std::isfinite(sub->right)||!std::isfinite(sub->top)||!std::isfinite(sub->bottom)||
      sub->left<0||sub->right>1||sub->left>=sub->right||sub->bottom<0||sub->top>1||sub->bottom>=sub->top)
    return fail(e,"Key graphics atlas layout differs from checked media");
   C3D_TexSetFilter(&t->texture,GPU_NEAREST,GPU_NEAREST);
   t->texture.border=0;C3D_TexSetWrap(&t->texture,GPU_CLAMP_TO_BORDER,GPU_CLAMP_TO_BORDER);
   candidate.push_back(std::move(t));
  }
  textures_.swap(candidate);data_=data;epoch_=last_epoch_=world.epoch();e.clear();return true;
 }
 // An attempted new frame invalidates the preceding snapshot, including when
 // admission fails. No stale frame can be drawn after an owner/epoch failure.
 bool compose(const World&world,std::string&e){
  frame_=0;commands_.clear();
  if(!ready()||world.epoch()!=epoch_||!data_.matches(world,e)||serial_==std::numeric_limits<uint64_t>::max())
   return fail(e,"Key graphics frame epoch/resource exhausted or rejected");
  std::vector<encore::upstream::KeyDraw>next;if(!data_.draw(world,next,e))return false;
  for(const auto&d:next){const auto*s=data_.sprite(d.profile);
   if(!s||!texture(d.profile)||!d.width||!d.height||d.x>s->width||d.width>s->width-d.x||d.y>s->height||d.height>s->height-d.y||
      !std::isfinite(d.top_left.x)||!std::isfinite(d.top_left.y))return fail(e,"Key graphics frame layout rejected");
  }
  commands_=std::move(next);frame_=++serial_;e.clear();return true;
 }
 bool draw(uint64_t epoch,uint64_t frame,size_t index,float camera_x,float camera_y,std::string&e)const{
  if(!ready()||epoch!=epoch_||!frame_||frame!=frame_||index>=commands_.size()||!std::isfinite(camera_x)||!std::isfinite(camera_y))
   return fail(e,"Key graphics draw frame/index/camera rejected");
  const auto&d=commands_[index];const auto*t=texture(d.profile);const auto*s=data_.sprite(d.profile);
  if(!t||!s)return fail(e,"Key graphics draw profile rejected");
  auto sub=*Tex3DS_GetSubTexture(t->metadata,0);const float du=(sub.right-sub.left)/s->width,dv=(sub.bottom-sub.top)/s->height,left=sub.left,top=sub.top;
  sub.left=left+d.x*du;sub.right=left+(d.x+d.width)*du;sub.top=top+d.y*dv;sub.bottom=top+(d.y+d.height)*dv;sub.width=d.width;sub.height=d.height;
  if(d.bits&4)std::swap(sub.left,sub.right);if(d.bits&8)std::swap(sub.top,sub.bottom);
  const float x=d.top_left.x-camera_x,y=d.top_left.y-camera_y;
  if(!std::isfinite(x)||!std::isfinite(y))return fail(e,"Key graphics camera subtraction overflow");
  // Explicit rebinding also handles a recycled texture object's address after
  // scene retirement; Citro2D's cached identity must not retain the old storage.
  C2D_Flush();C3D_TexBind(0,const_cast<C3D_Tex*>(&t->texture));
  if(!C2D_DrawImageAt({const_cast<C3D_Tex*>(&t->texture),&sub},std::floor(x+.5f),std::floor(y+.5f),0))
   return fail(e,"Key graphics draw queue rejected");
  e.clear();return true;
 }
 bool retire(uint64_t epoch,std::string&e){if(!ready()||epoch!=epoch_)return fail(e,"Key graphics retirement epoch rejected");clear();e.clear();return true;}
};

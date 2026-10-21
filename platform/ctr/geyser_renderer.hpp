#pragma once
#include <citro2d.h>
#include "encore/geyser_graphics.hpp"
#include "encore/geyser_lifecycle.hpp"
#include "encore/content.hpp"
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
// Prepare/retire/destruction require a GPU-idle boundary. One prepared source
// texture serves sequential actor draws;compose tokens cannot outlive actors.
class GeyserRenderer final {
 using Data=encore::upstream::GeyserGraphicsData;using Actor=encore::upstream::GeyserTimeline;using Vec2=encore::upstream::Vec2;
 struct Texture {C3D_Tex tex{};Tex3DS_Texture metadata=nullptr;~Texture(){if(metadata){Tex3DS_TextureFree(metadata);C3D_TexDelete(&tex);}}};
 std::unique_ptr<Texture>texture_;Data data_;encore::upstream::GeyserSpriteDraw command_;
 const Actor*actor_=nullptr;encore::upstream::GeyserSnapshot pose_;Vec2 root_{},camera_{};bool visible_=false,submitted_=false;
 const encore::upstream::GeyserLifecycle*lifecycle_=nullptr;
 uint64_t epoch_=0,last_epoch_=0,serial_=0,frame_=0,instance_=0;
 static bool bad(std::string&e,const char*s){e=s;return false;}
 void invalidate(){actor_=nullptr;lifecycle_=nullptr;frame_=0;instance_=0;submitted_=false;command_={};}
 void clear(){invalidate();if(texture_)C2D_Flush();texture_.reset();data_={};epoch_=0;}
 bool submit(std::string&e){
  if(!command_.visible){e.clear();return true;}if(submitted_)return bad(e,"Geyser frame already submitted");const auto&c=data_.config();const auto&f=command_.source;const auto*base=Tex3DS_GetSubTexture(texture_->metadata,0);auto sub=*base;const float du=(base->right-base->left)/c.width,dv=(base->bottom-base->top)/c.height;
  sub.left=base->left+f.x*du;sub.right=base->left+(f.x+f.width)*du;sub.top=base->top+f.y*dv;sub.bottom=base->top+(f.y+f.height)*dv;sub.width=f.width;sub.height=f.height;if(command_.flip_h)std::swap(sub.left,sub.right);if(command_.flip_v)std::swap(sub.top,sub.bottom);
  C2D_Flush();C3D_TexBind(0,&texture_->tex);if(!C2D_DrawImageAt({&texture_->tex,&sub},command_.position.x,command_.position.y,0))return bad(e,"Geyser image draw queue rejected");submitted_=true;e.clear();return true;
 }
public:
 GeyserRenderer()=default;~GeyserRenderer(){clear();}
 GeyserRenderer(const GeyserRenderer&)=delete;GeyserRenderer&operator=(const GeyserRenderer&)=delete;
 bool ready()const{return epoch_!=0;}uint64_t epoch()const{return epoch_;}uint64_t frame()const{return frame_;}const auto&command()const{return command_;}
 bool prepare(const Data&data,uint64_t epoch,const char*root,std::string&e){
  if(ready()||!data.valid()||!epoch||epoch<=last_epoch_||!root||!*root)return bad(e,"Geyser renderer fresh data/root/epoch rejected");std::string prefix(root);if(prefix.back()!='/')prefix+='/';std::vector<uint8_t>bytes;
  if(!encore::read_file((prefix+data.config().texture).c_str(),bytes,8*1024*1024,e)||!data.texture_matches(bytes.data(),bytes.size(),e))return false;auto texture=std::make_unique<Texture>();texture->metadata=Tex3DS_TextureImport(bytes.data(),bytes.size(),&texture->tex,nullptr,false);if(!texture->metadata)return bad(e,"Geyser texture import/allocation failed");
  if(Tex3DS_GetNumSubTextures(texture->metadata)!=1)return bad(e,"Geyser texture image count rejected");const auto*sub=Tex3DS_GetSubTexture(texture->metadata,0);const auto&c=data.config();
  if(!sub||sub->width!=c.width||sub->height!=c.height||Tex3DS_SubTextureRotated(sub)||!std::isfinite(sub->left)||!std::isfinite(sub->right)||!std::isfinite(sub->top)||!std::isfinite(sub->bottom)||sub->left<0||sub->left>=sub->right||sub->right>1||sub->bottom<0||sub->bottom>=sub->top||sub->top>1)return bad(e,"Geyser texture image layout rejected");
  C3D_TexSetFilter(&texture->tex,GPU_NEAREST,GPU_NEAREST);texture->tex.border=0;C3D_TexSetWrap(&texture->tex,GPU_CLAMP_TO_BORDER,GPU_CLAMP_TO_BORDER);texture_=std::move(texture);data_=data;epoch_=last_epoch_=epoch;invalidate();e.clear();return true;
 }
 bool compose(const Actor&actor,uint64_t instance,Vec2 root,Vec2 camera,bool visible,std::string&e){
  invalidate();if(!ready()||serial_==std::numeric_limits<uint64_t>::max())return bad(e,"Geyser renderer frame generation rejected");encore::upstream::GeyserSpriteDraw next;encore::upstream::GeyserSnapshot pose;
  if(!data_.compose(actor,epoch_,instance,root,camera,visible,next,e)||!actor.inspect(epoch_,instance,pose,e))return false;actor_=&actor;instance_=instance;root_=root;camera_=camera;visible_=visible;pose_=pose;command_=next;frame_=++serial_;e.clear();return true;
 }
 bool compose(const encore::upstream::GeyserLifecycle&life,uint64_t instance,Vec2 camera,bool visible,std::string&e){
  invalidate();if(!ready()||serial_==std::numeric_limits<uint64_t>::max())return bad(e,"Geyser renderer frame generation rejected");encore::upstream::GeyserSpriteDraw next;encore::upstream::GeyserInstanceSnapshot snapshot;
  if(!data_.compose_lifecycle(life,epoch_,instance,camera,visible,next,e)||!life.inspect(epoch_,instance,snapshot,e)||!snapshot.pose)return false;lifecycle_=&life;instance_=instance;root_=snapshot.source.position;camera_=camera;visible_=visible;pose_=*snapshot.pose;command_=next;frame_=++serial_;e.clear();return true;
 }
 bool draw(uint64_t epoch,uint64_t frame,uint64_t instance,const Actor&actor,Vec2 root,Vec2 camera,bool visible,std::string&e){
  encore::upstream::GeyserSnapshot pose;
  if(!ready()||!frame_||epoch!=epoch_||frame!=frame_||instance!=instance_||&actor!=actor_||root.x!=root_.x||root.y!=root_.y||camera.x!=camera_.x||camera.y!=camera_.y||visible!=visible_||!data_.matches(actor.content())||!actor.inspect(epoch,instance,pose,e)||pose.clip!=pose_.clip||pose.frame!=pose_.frame||pose.time!=pose_.time)return bad(e,"Geyser draw stale actor/frame/transform rejected");
  return submit(e);
 }
 bool draw(uint64_t epoch,uint64_t frame,uint64_t instance,const encore::upstream::GeyserLifecycle&life,Vec2 camera,bool visible,std::string&e){
  encore::upstream::GeyserInstanceSnapshot snapshot;
  if(!ready()||!frame_||epoch!=epoch_||frame!=frame_||instance!=instance_||&life!=lifecycle_||camera.x!=camera_.x||camera.y!=camera_.y||visible!=visible_||!data_.matches(life.content().geyser())||!life.inspect(epoch,instance,snapshot,e)||!snapshot.pose||snapshot.source.position.x!=root_.x||snapshot.source.position.y!=root_.y||snapshot.pose->clip!=pose_.clip||snapshot.pose->frame!=pose_.frame||snapshot.pose->time!=pose_.time)return bad(e,"Geyser draw stale registry/frame/transform rejected");
  return submit(e);
 }
 bool retire(uint64_t epoch,std::string&e){if(!ready()||epoch!=epoch_)return bad(e,"Geyser renderer stale retirement rejected");clear();e.clear();return true;}
};

#pragma once
#include <citro2d.h>
#include "encore/shadow_graphics.hpp"
#include "encore/content.hpp"
#include <cmath>
#include <limits>
#include <memory>
#include <utility>
// Prepare/retire/destruction require a GPU-idle boundary. The scene keeps the
// stepping metadata alive for every supplied Shadow state. No drawing does IO.
enum class ShadowDrawPhase:uint8_t {Behind=0,Front=1};
class ShadowRenderer final {
 using Data=encore::upstream::ShadowGraphicsData;using State=encore::upstream::FieldStepShadowState;using Vec2=encore::upstream::Vec2;
 struct Texture {C3D_Tex tex{};Tex3DS_Texture metadata=nullptr;~Texture(){if(metadata){Tex3DS_TextureFree(metadata);C3D_TexDelete(&tex);}}};
 std::unique_ptr<Texture>texture_;Data data_;encore::upstream::ShadowDraw command_;
 const State*state_=nullptr;const void*source_=nullptr;uint32_t body_=0,animation_=0,index_=0;bool playing_=false,behind_=false,visible_=false,submitted_=false;
 uint64_t epoch_=0,last_epoch_=0,serial_=0,frame_=0;
 static bool bad(std::string&e,const char*s){e=s;return false;}
 void invalidate(){state_=nullptr;source_=nullptr;body_=0;frame_=0;submitted_=false;command_={};}
 void clear(){invalidate();if(texture_)C2D_Flush();texture_.reset();data_={};epoch_=0;}
public:
 ShadowRenderer()=default;~ShadowRenderer(){clear();}
 ShadowRenderer(const ShadowRenderer&)=delete;ShadowRenderer&operator=(const ShadowRenderer&)=delete;
 bool ready()const{return epoch_!=0;}uint64_t epoch()const{return epoch_;}uint64_t frame()const{return frame_;}const auto&command()const{return command_;}
 bool prepare(const Data&data,uint64_t epoch,const char*root,std::string&e){
  if(ready()||!data.valid()||!epoch||epoch<=last_epoch_||!root||!*root)return bad(e,"Shadow renderer fresh resource/root/epoch rejected");
  std::string prefix(root);if(prefix.back()!='/')prefix+='/';std::vector<uint8_t>bytes;
  if(!encore::read_file((prefix+data.config().texture).c_str(),bytes,8*1024*1024,e)||!data.texture_matches(bytes.data(),bytes.size(),e))return false;
  auto texture=std::make_unique<Texture>();texture->metadata=Tex3DS_TextureImport(bytes.data(),bytes.size(),&texture->tex,nullptr,false);
  if(!texture->metadata)return bad(e,"Shadow texture import/allocation failed");
  if(Tex3DS_GetNumSubTextures(texture->metadata)!=1)return bad(e,"Shadow texture image count rejected");const auto*sub=Tex3DS_GetSubTexture(texture->metadata,0);const auto&c=data.config();
  if(!sub||sub->width!=c.width||sub->height!=c.height||Tex3DS_SubTextureRotated(sub)||!std::isfinite(sub->left)||!std::isfinite(sub->right)||!std::isfinite(sub->top)||!std::isfinite(sub->bottom)||sub->left<0||sub->left>=sub->right||sub->right>1||sub->bottom<0||sub->bottom>=sub->top||sub->top>1)return bad(e,"Shadow texture layout rejected");
  C3D_TexSetFilter(&texture->tex,GPU_NEAREST,GPU_NEAREST);texture->tex.border=0;C3D_TexSetWrap(&texture->tex,GPU_CLAMP_TO_BORDER,GPU_CLAMP_TO_BORDER);texture_=std::move(texture);data_=data;epoch_=last_epoch_=epoch;invalidate();e.clear();return true;
 }
 bool compose(const State&state,uint32_t profile,uint32_t body,Vec2 position,Vec2 camera,bool visible,std::string&e){
  invalidate();if(!ready()||!body||body==UINT32_MAX||serial_==std::numeric_limits<uint64_t>::max())return bad(e,"Shadow renderer frame/body owner rejected");
  encore::upstream::ShadowDraw next;if(!data_.compose(state,profile,position,camera,visible,next,e))return false;
  state_=&state;source_=state.content().bytes_identity();body_=body;animation_=state.animation();index_=state.frame();playing_=state.playing();behind_=state.behind();visible_=visible;command_=next;frame_=++serial_;e.clear();return true;
 }
 bool draw(uint64_t epoch,uint64_t frame,uint32_t body,const State&state,bool visible,ShadowDrawPhase phase,std::string&e){
  if(!ready()||epoch!=epoch_||!frame_||frame!=frame_||body!=body_||&state!=state_||state.content().bytes_identity()!=source_||!data_.matches(state)||state.animation()!=animation_||state.frame()!=index_||state.playing()!=playing_||state.behind()!=behind_||visible!=visible_)return bad(e,"Shadow draw stale frame/body/state rejected");
  if(phase!=ShadowDrawPhase::Behind&&phase!=ShadowDrawPhase::Front)return bad(e,"Unknown Shadow draw phase rejected");
  if(!command_.visible||command_.behind!=(phase==ShadowDrawPhase::Behind)){e.clear();return true;}
  if(submitted_)return bad(e,"Shadow frame was already submitted");const auto&c=data_.config();const auto*base=Tex3DS_GetSubTexture(texture_->metadata,0);auto sub=*base;const auto&f=command_.source;const float du=(base->right-base->left)/c.width,dv=(base->bottom-base->top)/c.height;
  sub.left=base->left+f.x*du;sub.right=base->left+(f.x+f.width)*du;sub.top=base->top+f.y*dv;sub.bottom=base->top+(f.y+f.height)*dv;sub.width=f.width;sub.height=f.height;if(command_.flip_h)std::swap(sub.left,sub.right);if(command_.flip_v)std::swap(sub.top,sub.bottom);
  C2D_Flush();C3D_TexBind(0,&texture_->tex);if(!C2D_DrawImageAt({&texture_->tex,&sub},command_.position.x,command_.position.y,0))return bad(e,"Shadow image draw queue rejected");submitted_=true;e.clear();return true;
 }
 bool retire(uint64_t epoch,std::string&e){if(!ready()||epoch!=epoch_)return bad(e,"Shadow renderer retirement epoch rejected");clear();e.clear();return true;}
};

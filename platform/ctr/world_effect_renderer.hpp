#pragma once
#include <3ds.h>
#include <citro2d.h>
#include "encore/world_effect.hpp"
#include <cstring>
// Presentation only; all image pixels, tint, timings and shader tuning are data.
class WorldEffectRenderer {
public:
 WorldEffectRenderer()=default;
 WorldEffectRenderer(const WorldEffectRenderer&)=delete;WorldEffectRenderer&operator=(const WorldEffectRenderer&)=delete;
 bool load(encore::upstream::WorldEffectView view,uint32_t width,uint32_t height,std::string&error){
  free();if(!kernel_.prepare(view,width,height,error))return false;
  unsigned tw=8,th=8;while(tw<width)tw<<=1;while(th<height)th<<=1;
  if(!C3D_TexInit(&texture_,tw,th,GPU_RGBA8)){error="World effect GPU texture allocation failed";return false;}
  ready_=true;width_=width;height_=height;surface_.resize(size_t(width)*height);offsets_.resize(surface_.size());
  std::memset(texture_.data,0,texture_.size);
  for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x)offsets_[size_t(y)*width+x]=((y/8)*(tw/8)+x/8)*64+morton(x,y);
  C3D_TexSetFilter(&texture_,GPU_NEAREST,GPU_NEAREST);C3D_TexSetWrap(&texture_,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);
  sub_={u16(width),u16(height),0,1,float(width)/tw,1-float(height)/th};return true;
 }
 void free(){if(ready_)C3D_TexDelete(&texture_);texture_={};sub_={};surface_.clear();offsets_.clear();width_=height_=0;ready_=visible_=false;}
 bool compose(float global_shader_time,const encore::upstream::WorldEffectSample&sample,std::string&error){
  visible_=false;if(!ready_){error="World effect renderer is not loaded";return false;}
  if(!kernel_.compose(global_shader_time,sample,surface_.data(),surface_.size())){error="World effect sampling rejected";return false;}
  visible_=sample.active&&sample.alpha>0;if(!visible_)return true;
  auto*out=static_cast<uint32_t*>(texture_.data);
  for(size_t i=0;i<surface_.size();++i){const uint32_t c=surface_[i];out[offsets_[i]]=((c&255)<<24)|((c&0xff00)<<8)|((c>>8)&0xff00)|(c>>24);}
  C3D_TexFlush(&texture_);return true;
 }
 // x/y are captured world-center minus current camera-center in screen pixels.
 // The image stays at 1:1; it never follows subsequent camera movement implicitly.
 void draw(float x=0,float y=0)const{
  if(!ready_||!visible_)return;
  // C2D caches texture object identity. A freed/reloaded surface can occupy
  // that same address with a different GPU allocation. Flush prior batches
  // before explicitly rebinding so the cache cannot retain the old allocation.
  C2D_Flush();C3D_TexBind(0,const_cast<C3D_Tex*>(&texture_));
  C2D_DrawImageAt({const_cast<C3D_Tex*>(&texture_),&sub_},x,y,0,nullptr,1,1);
 }
private:
 static unsigned morton(unsigned x,unsigned y){return(x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);}
 encore::upstream::WorldEffectKernel kernel_;std::vector<uint32_t>surface_,offsets_;
 C3D_Tex texture_{};Tex3DS_SubTexture sub_{};uint32_t width_=0,height_=0;bool ready_=false,visible_=false;
};

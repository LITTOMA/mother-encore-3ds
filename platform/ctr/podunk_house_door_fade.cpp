#include "podunk_house_door_fade.hpp"
#include <citro2d.h>
#include <algorithm>
#include <cmath>
namespace encore::ctr {
using namespace upstream;
namespace { bool fail(std::string &e,const char*s){e=s;return false;} }
bool PodunkHouseDoorFade::initialize(HouseView source,const FieldDoorData &doors,const IntroductionData&intro,std::string&e){
  if(initialized_ || !source.valid() || !doors.valid() || !intro.valid() || intro.fades.size()!=6 || intro.fade_mostly.size()!=6 || !source.count(HouseSection::Doors))
    return fail(e,"House door Fade requires the existing checked UI animation profile");
  const auto identity = doors.identity();
  if(source.byte_size()<52 || !std::equal(identity.upstream_commit.begin(),
      identity.upstream_commit.end(),source.bytes()+32))
    return fail(e,"House door Fade profile belongs to a different upstream pin");
  const auto p=source.door(0);
  if(p.fade_in_length<=0 || p.fade_in_opaque<=0 || p.fade_out_length<=0 || p.fade_out_mostly<=0)
    return fail(e,"House UI Fade profile is incomplete");
  for(uint32_t i=1;i<source.count(HouseSection::Doors);++i){
    const auto q=source.door(i);
    if(q.fade_in_length!=p.fade_in_length || q.fade_in_opaque!=p.fade_in_opaque ||
       q.fade_out_length!=p.fade_out_length || q.fade_out_mostly!=p.fade_out_mostly)
      return fail(e,"House doors do not share the admitted UI Fade profile");
  }
  const auto shader=source.parameter(HouseParameter::FadeShader);
  if(shader.y<=0)return fail(e,"House UI Fade smoothstep extent rejected");
  source_=source;doors_=&doors;intro_=&intro;profile_=p;initialized_=true;
  cut_=source.parameter(HouseParameter::FadeCuts).x;e.clear();return true;
}
bool PodunkHouseDoorFade::start(uint32_t id,bool in,std::string_view animation,
    const std::array<float,4>&color,float speed,std::string&e){
  FieldDoorDescriptor door;
  if(!initialized_ || !doors_->find(id,door) || !std::isfinite(speed) || speed<=0)
    return fail(e,"House door Fade lacks actual source request");
  auto expected=doors_->string(in?door.in_anim:door.out_anim);
  if(!in && expected.empty())expected=doors_->string(door.in_anim);
  if(animation!=expected || (animation!="Fade" && animation!="Circle Focus") || color!=(in?door.in_color:door.out_color) ||
      speed!=(in?door.in_speed:door.out_speed))
    return fail(e,"House door Fade animation/color/speed differs or is unsupported");
  if(!in && color_!=color)
    return fail(e,"House Fade color tween requires its actual source tween owner");
  color_=color;speed_=speed;in_=in;position_=0;active_=true;emitted_=false;drawn_=true;kind_=animation=="Fade"?0u:1u;
  cut_=sample_intro_track(intro_->fades[kind_*2+(in?0:1)].tracks.front(),0)[0];
  e.clear();return true;
}
bool PodunkHouseDoorFade::frame(uint64_t epoch,float dt,bool&in_done,bool&out_mostly,std::string&e){
  in_done=out_mostly=false;
  if(!initialized_ || !epoch || epoch<=epoch_ || !std::isfinite(dt) || dt<0 || dt>1)
    return fail(e,"House door Fade actual frame cursor rejected");
  epoch_=epoch;if(!active_){e.clear();return true;}
  const auto old=position_;position_=double(float(position_+float(dt*speed_)));
  const uint32_t clip_index=kind_*2+(in_?0:1);
  const auto&clip=intro_->fades[clip_index];
  const double length=clip.length,key=intro_->fade_mostly[clip_index];
  position_=std::min(position_,length);
  cut_=sample_intro_track(clip.tracks.front(),position_)[0];
  if(in_ && position_>=length && !emitted_){in_done=true;emitted_=true;}
  if(!in_ && old<=key && position_>key && !emitted_){out_mostly=true;emitted_=true;}
  if(position_>=length)active_=false;
  e.clear();return true;
}
bool PodunkHouseDoorFade::cut(std::string&e){
  if(!initialized_)return fail(e,"House door Fade cut before initialization");
  cut_=source_.parameter(HouseParameter::FadeCuts).y;active_=false;e.clear();return true;
}
bool PodunkHouseDoorFade::draw(float x,float y,float width,float height,std::string&e)const{
  if(!initialized_ || !std::isfinite(width) || !std::isfinite(height) || width<=0 || height<=0)
    return fail(e,"House door Fade draw viewport rejected");
  if(!drawn_){e.clear();return true;}
  const auto byte=[](float v){return u8(std::lround(std::clamp(v,0.f,1.f)*255));};
  if(kind_==1){
    const auto&v=intro_->fade_shader;const double ratio=v[1]/v[2];
    const float cx=focus_.x+v[5],cy=focus_.y+v[6];
    const auto color=C2D_Color32(byte(color_[0]),byte(color_[1]),byte(color_[2]),255);
    auto rect=[&](float l,float row,float w){return w<=0||C2D_DrawRectSolid(x+l,y+row,0,w,1,color);};
    // Exact source step mask at native pixel centres, as used by Introduction.
    for(int row=0;row<int(height);++row){const double dy=(row+.5-cy)/v[4],remaining=double(cut_)*cut_-dy*dy;
      if(remaining<=0){if(!rect(0,float(row),width))return fail(e,"House Circle mask draw failed");continue;}
      const double radius=std::sqrt(remaining)*v[3]/ratio;
      const int first=std::clamp(int(std::floor(cx-radius-.5))+1,0,int(width)),last=std::clamp(int(std::ceil(cx+radius-.5))-1,-1,int(width)-1);
      if(first>last){if(!rect(0,float(row),width))return fail(e,"House Circle mask draw failed");}
      else if(!rect(0,float(row),float(first))||!rect(float(last+1),float(row),width-float(last+1)))return fail(e,"House Circle mask draw failed");
    }
    e.clear();return true;
  }
  const auto shader=source_.parameter(HouseParameter::FadeShader);
  const float a=std::clamp((shader.x-cut_)/shader.y,0.f,1.f);
  const float alpha=a*a*(3-2*a); // Source Fade shader replaces COLOR.a.
  if(alpha>0 && !C2D_DrawRectSolid(x,y,0,width,height,C2D_Color32(byte(color_[0]),byte(color_[1]),byte(color_[2]),byte(alpha))))
    return fail(e,"House door Fade GPU rectangle submission failed");
  e.clear();return true;
}
}

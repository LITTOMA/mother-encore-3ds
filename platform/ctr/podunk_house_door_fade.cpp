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
  cut_=source.parameter(HouseParameter::FadeCuts).x;
  path_position_={intro.fade_shader[5],intro.fade_shader[6]};e.clear();return true;
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
bool PodunkHouseDoorFade::bind_dialogue_restore(const HouseUiContinuationData &data,std::string &e){
  if(!initialized_ || restore_source_ || !data.valid() || !data.dialogue_continuation() ||
     data.identity().upstream_commit!=doors_->identity().upstream_commit)
    return fail(e,"Dialogue Fade requires the same actual imported material owner");
  const auto &p=data.business_policy();
  std::array<uint8_t,32> hash{},original{};
  if(!data.source_hash(p.fade_script,hash) || !doors_->source_hash(p.fade_script,original) || hash!=original ||
     !data.source_hash(p.fade_scene,hash) || !doors_->source_hash(p.fade_scene,original) || hash!=original ||
     p.fade_type!=1 || p.transition!=1 || p.ease!=1 ||
     !std::isfinite(p.restore_target) || !std::isfinite(p.restore_duration) ||
     p.restore_target<0 || p.restore_target>1 || p.restore_duration<=0 || p.cut_signal.empty())
    return fail(e,"Dialogue Fade restore source policy rejected");
  // This continuation borrows the imported House material and the checked
  // Curve2D baked data for the source cut and spinning PathFollow methods.
  restore_source_=&data;e.clear();return true;
}
bool PodunkHouseDoorFade::restore_cut(std::function<bool(std::string &)> done,std::string &e){
  if(!initialized_ || !restore_source_ || !done)
    return fail(e,"Dialogue cut tween requires its actual idle House Fade owner");
  // Source kills a running _cut_tween before replacing it. Captured completion
  // belongs to that old tween and must not be resumed by the replacement.
  restore_done_=std::move(done);restore_elapsed_=0;restore_started_=false;
  restoring_=true;positive_cut_=false;kind_=restore_source_->business_policy().fade_type;drawn_=true;
  e.clear();return true;
}
bool PodunkHouseDoorFade::restore_idle(uint64_t epoch,float dt,std::string &e){
  if(!initialized_ || !restore_source_ || !epoch || epoch<=restore_epoch_ ||
     !std::isfinite(dt) || dt<0 || dt>1)
    return fail(e,"Dialogue Fade requires the unique actual idle cursor");
  restore_epoch_=epoch;
  const auto &p=restore_source_->business_policy();
  if(spinning_){
    spin_position_=double(float(spin_position_+float(dt*float(p.spin_speed))));
    spin_position_=std::fmod(spin_position_,p.spin_length);
    const float value=float(p.spin_from+(p.spin_to-p.spin_from)*(spin_position_/p.spin_length));
    path_position_=curve_position(value);
  }
  if(!restoring_){e.clear();return true;}
  // PropertyTweener::start observes the property at the first SceneTree tween
  // traversal, after the native AnimationPlayer phase, not at set_cut call.
  if(!restore_started_){restore_from_=cut_;restore_started_=true;}
  restore_elapsed_=float(restore_elapsed_+dt);
  const float duration=float(positive_cut_ ? p.effect_duration : p.restore_duration);
  const float weight=std::min(restore_elapsed_/duration,1.0f);
  // Checked semantic curve 1/ease 1 is Godot TRANS_QUAD/EASE_IN, not the
  // engine enum's numerical representation. Game duration/target are data.
  const float eased=positive_cut_ ? -(weight*(weight-2)) : weight*weight;
  const double target=positive_cut_ ? p.effect_target : p.restore_target;
  cut_=restore_from_+float(target-double(restore_from_))*eased;
  if(restore_elapsed_>=duration){
    cut_=float(target);restoring_=false;
    auto done=std::move(restore_done_);restore_done_={};
    if(done && !done(e))return false;
  }
  e.clear();return true;
}
bool PodunkHouseDoorFade::stop_dialogue_spin(std::string &e){
  if(!initialized_ || !restore_source_ || restoring_)
    return fail(e,"Dialogue Fade false-spin continuation is not at cut completion");
  // Conditional PathAnim.stop followed by the actual PathFollow unit_offset
  // assignment. The same baked Curve2D owner supplies its new position.
  spinning_=false;
  restored_path_unit_offset_=restore_source_->business_policy().spin_stop_unit_offset;
  path_position_=curve_position(float(restored_path_unit_offset_)*restore_source_->business_policy().curve_length);
  restored_path_=true;e.clear();return true;
}
Vec2 PodunkHouseDoorFade::curve_position(float source_offset)const{
  const auto &p=restore_source_->business_policy();
  float offset=std::fmod(source_offset,p.curve_length);
  if(offset<0)offset+=p.curve_length;
  // PathFollow2D::set_offset preserves the end for a nonzero exact wrap.
  if(std::abs(source_offset)>=1e-5f && std::abs(offset)<1e-5f)offset=p.curve_length;
  const auto &points=p.curve_points;
  if(offset<=0)return points.front();
  if(offset>=p.curve_length)return points.back();
  const auto index=size_t(std::floor(double(offset)/double(p.curve_interval)));
  if(index>=points.size()-1)return points.back();
  float t=std::fmod(offset,p.curve_interval);
  if(index==points.size()-2){if(t>0)t/=std::fmod(p.curve_length,p.curve_interval);}
  else t/=p.curve_interval;
  const auto &a=points[index],&b=points[index+1];
  const auto &pre=points[index ? index-1 : index],&post=points[index<points.size()-2 ? index+2 : index+1];
  const float t2=t*t,t3=t2*t;
  const auto axis=[&](float p0,float p1,float p2,float p3){
    return .5f*((p1*2)+(-p0+p2)*t+(2*p0-5*p1+4*p2-p3)*t2+(-p0+3*p1-3*p2+p3)*t3);
  };
  return {axis(pre.x,a.x,b.x,post.x),axis(pre.y,a.y,b.y,post.y)};
}
bool PodunkHouseDoorFade::telepathy_cut(Vec2 focused,std::string &e){
  if(!initialized_ || !restore_source_ || !std::isfinite(focused.x) || !std::isfinite(focused.y))
    return fail(e,"Telepathy focus requires actual source object/camera screen coordinates");
  const auto &p=restore_source_->business_policy();
  focus_=focused;focus_source_=true;color_=p.effect_color;
  kind_=p.effect_type;drawn_=true;restore_done_={};restore_elapsed_=0;
  positive_cut_=true;restoring_=true;restore_started_=false;
  if(!spinning_)spin_position_=0;
  spinning_=true;e.clear();return true;
}
bool PodunkHouseDoorFade::draw(float x,float y,float width,float height,std::string&e)const{
  if(!initialized_ || !std::isfinite(width) || !std::isfinite(height) || width<=0 || height<=0)
    return fail(e,"House door Fade draw viewport rejected");
  if(!drawn_){e.clear();return true;}
  const auto byte=[](float v){return u8(std::lround(std::clamp(v,0.f,1.f)*255));};
  if(kind_==1){
    const auto&v=intro_->fade_shader;const double ratio=v[1]/v[2];
    const auto *p=restore_source_ ? &restore_source_->business_policy() : nullptr;
    const float cx=focus_.x+path_position_.x+(focus_source_ ? (width-p->screen_size.x)/2 : 0);
    const float cy=focus_.y+path_position_.y+(focus_source_ ? (height-p->screen_size.y)/2 : 0);
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

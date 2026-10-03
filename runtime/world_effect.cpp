#include "encore/world_effect.hpp"
#include <algorithm>
#include <cmath>
namespace encore::upstream {
namespace {
float mix(float a,float b,double f){return a+(b-a)*float(f);}
WorldEffectColor mix(WorldEffectColor a,WorldEffectColor b,double f){return {mix(a.r,b.r,f),mix(a.g,b.g,f),mix(a.b,b.b,f),mix(a.a,b.a,f)};}
uint32_t coordinate(float u,uint32_t extent,float float_extent){
 // Float-to-int conversion is defined inside this conservative range. Its
 // correction is exactly floorf, including negative nonintegral coordinates.
 // Preserve the original libm path for larger finite source positions.
 if(u>=-1048576.0f&&u<=1048576.0f){const int32_t truncated=int32_t(u);u-=float(truncated-(u<float(truncated)));}
 else u-=std::floor(u);
 return std::min(uint32_t(u*float_extent),extent-1);
}
bool color_ok(WorldEffectColor c){return std::isfinite(c.r)&&std::isfinite(c.g)&&std::isfinite(c.b)&&std::isfinite(c.a)&&c.r>=0&&c.r<=1&&c.g>=0&&c.g<=1&&c.b>=0&&c.b<=1&&c.a>=0&&c.a<=1;}
}
bool WorldEffect::initialize(WorldEffectView view){if(!view.valid())return false;view_=view;reset();return true;}
void WorldEffect::reset(){clip_time_=fade_time_=0;alpha_=fade_start_=0;active_=fading_=fade_out_=false;}
bool WorldEffect::appear(){if(!view_.valid())return false;clip_time_=fade_time_=0;alpha_=fade_start_=0;active_=fading_=true;fade_out_=false;return true;}
bool WorldEffect::disappear(){if(!view_.valid()||!active_)return false;fade_time_=0;fade_start_=alpha_;fading_=fade_out_=true;return true;}
bool WorldEffect::advance(double delta){
 if(!view_.valid()||!std::isfinite(delta)||delta<0||delta>120)return false;
 if(!active_)return true;
 const auto s=view_.settings();const float elapsed=float(delta);
 if(fading_){const float duration=fade_out_?s.disappear_duration:s.appear_duration;
  fade_time_+=elapsed;const float f=std::min(1.0f,fade_time_/duration);
  // EASE_OUT with the native default TRANS_LINEAR remains linear.
  alpha_=mix(fade_start_,fade_out_?0.0f:1.0f,f);
  if(fade_time_>=duration){fading_=false;if(fade_out_)active_=false;}
 }
 const float position=clip_time_+elapsed,phase=std::fmod(position,s.cycle_duration);
 clip_time_=phase==0&&position!=0?s.cycle_duration:phase;return true;
}
WorldEffectSample WorldEffect::sample()const{
 WorldEffectSample out;out.active=active_;out.alpha=alpha_;out.animation_time=clip_time_;
 if(!view_.valid())return out;
 out.modulate=view_.settings().initial_modulate;if(!active_)return out;
 // Native Animation key search uses Math::is_equal_approx. Near a key it
 // selects that key before interpolation, including a tiny negative weight.
 // This is an engine comparison rule, verified by the 140-frame native probe.
 uint32_t i=0;while(i+1<view_.key_count()){const float next=view_.key(i+1).time;
  if(next>clip_time_&&std::abs(next-clip_time_)>=.00001f*std::max(1.0f,std::abs(clip_time_)))break;
  ++i;
 }
 const auto a=view_.key(i),b=view_.key((i+1)%view_.key_count());
 const float end=i+1==view_.key_count()?view_.settings().cycle_duration:b.time;
 out.modulate=mix(a.color,b.color,(clip_time_-a.time)/(end-a.time));return out;
}
bool WorldEffectKernel::prepare(WorldEffectView view,uint32_t width,uint32_t height,std::string&error){
 width_=height_=0;texels_.clear();palette_.clear();u_.clear();v_.clear();column_displacement_.clear();column_index_.clear();frame_palette_.clear();
 if(!view.valid()||!width||!height||width>1024||height>1024||uint64_t(width)*height>1024*1024){error="World effect surface bounds rejected";return false;}
 settings_=view.settings();width_=width;height_=height;
 texels_.assign(view.texels(),view.texels()+size_t(settings_.texture_width)*settings_.texture_height);
 for(uint32_t i=0;i<view.palette_count();++i)palette_.push_back(view.palette(i));
 frame_palette_.resize(palette_.size());u_.resize(width);v_.resize(height);column_displacement_.resize(width);column_index_.resize(width);
 const float origin_x=(settings_.source_width-float(width))/2,origin_y=(settings_.source_height-float(height))/2;
 for(uint32_t x=0;x<width;++x)u_[x]=(float(x)+origin_x+.5f)/settings_.texture_width+settings_.pixel_snap_uv_epsilon;
 for(uint32_t y=0;y<height;++y)v_[y]=(float(y)+origin_y+.5f)/settings_.texture_height+settings_.pixel_snap_uv_epsilon;
 error.clear();return true;
}
bool WorldEffectKernel::compose(float time,const WorldEffectSample&sample,uint32_t*out,size_t count)const{
 if(!width_||!out||count<pixel_count()||!std::isfinite(time)||std::abs(time)>1000000||!std::isfinite(sample.alpha)||sample.alpha<0||sample.alpha>1||!color_ok(sample.modulate))return false;
 if(!sample.active||sample.alpha==0){std::fill(out,out+pixel_count(),0);return true;}
 const auto&s=settings_;const float osc_time=s.translation_ping_pong_y!=0?std::cos(s.translation_ping_pong_y*time):time;
 const float phase=osc_time*s.speed_y,amplitude_factor=std::cos(s.amplitude_ping_pong_y*time);
 const float move_x=time*s.move_x/s.move_divisor,move_y=time*s.move_y/s.move_divisor;
 if(!std::isfinite(phase)||!std::isfinite(move_x)||!std::isfinite(move_y))return false;
 for(size_t i=0;i<palette_.size();++i){const auto c=palette_[i];const auto&m=sample.modulate;
  const auto channel=[](float f){return uint32_t(std::clamp(f,0.0f,255.0f)+.5f);};
  frame_palette_[i]=channel(float(c&255)*m.r)|(channel(float((c>>8)&255)*m.g)<<8)|(channel(float((c>>16)&255)*m.b)<<16)|(channel(float(c>>24)*m.a*sample.alpha*s.opacity)<<24);
 }
 const uint32_t texture_width=s.texture_width,texture_height=s.texture_height;
 const float float_width=float(texture_width),float_height=float(texture_height);
 for(uint32_t x=0;x<width_;++x){
  column_index_[x]=coordinate(u_[x]+move_x,texture_width,float_width);
  column_displacement_[x]=(s.frequency_y!=0&&s.amplitude_y!=0)?s.amplitude_y*std::cos(s.frequency_y*u_[x]+phase)*amplitude_factor:0;
 }
 // Same source arithmetic order; one cosine per column, with no trig identity,
 // phase/time quantization, approximation, mesh or row-shift shortcut.
 for(uint32_t y=0;y<height_;++y)for(uint32_t x=0;x<width_;++x){float v=v_[y];v+=column_displacement_[x];v+=move_y;const auto row=coordinate(v,texture_height,float_height)*texture_width;out[size_t(y)*width_+x]=frame_palette_[texels_[row+column_index_[x]]];}
 return true;
}
}

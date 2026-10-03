#pragma once
#include "encore/world_effect_data.hpp"
namespace encore::upstream {
struct WorldEffectSample {bool active=false;float alpha=0;WorldEffectColor modulate{};double animation_time=0;};
class WorldEffect {
public:
 bool initialize(WorldEffectView);
 bool appear();bool disappear();bool advance(double delta);void reset();
 bool active()const{return active_;}bool fading_out()const{return fade_out_;}
 WorldEffectSample sample()const;
private:
 WorldEffectView view_;float clip_time_=0,fade_time_=0,alpha_=0,fade_start_=0;
 bool active_=false,fading_=false,fade_out_=false;
};
// Bounded nearest-repeat kernel for the reviewed vertical distortion mechanism.
// Expanded canvas is 1:1 and centered around the original source rectangle.
class WorldEffectKernel {
public:
 bool prepare(WorldEffectView,uint32_t width,uint32_t height,std::string&error);
 bool compose(float shader_time,const WorldEffectSample&,uint32_t*rgba,size_t word_count)const;
 size_t pixel_count()const{return size_t(width_)*height_;}
private:
 WorldEffectSettings settings_{};uint32_t width_=0,height_=0;
 std::vector<uint8_t>texels_;std::vector<uint32_t>palette_;
 std::vector<float>u_,v_;
 mutable std::vector<float>column_displacement_;
 mutable std::vector<uint32_t>column_index_,frame_palette_;
};
}

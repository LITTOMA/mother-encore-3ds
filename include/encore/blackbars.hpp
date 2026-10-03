#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace encore::upstream {
// Bounded two-key, non-looping cubic CanvasLayer tracks. All source geometry,
// timing, easing and color are checked external data; this has no actor rules.
class Blackbars {
public:
 struct Rect {float x=0,y=0,w=0,h=0;};
 struct Pose {std::array<Rect,2> bars{};uint32_t color=0;};
 bool load(const uint8_t*p,size_t n,std::string&error){
  auto fail=[&](const char*s){error=s;return false;};
  if(!p||n!=124||std::memcmp(p,"ENCBAR01",8)||integer(p+8)!=1||integer(p+12)!=n||integer(p+20)!=1)return fail("Blackbar schema/size rejected");
  uint32_t crc=~0u;for(size_t i=24;i<n;++i){crc^=p[i];for(int b=0;b<8;++b)crc=(crc>>1)^(0xedb88320u&uint32_t(-int32_t(crc&1)));}
  if(~crc!=integer(p+16))return fail("Blackbar checksum rejected");
  Blackbars candidate;candidate.width_=real(p+24);candidate.height_=real(p+28);candidate.sizes_={real(p+32),real(p+36)};candidate.color_=integer(p+40);
  if(!positive(candidate.width_,8192)||!positive(candidate.height_,8192)||(candidate.color_>>24)!=255)return fail("Blackbar canvas/color rejected");
  for(float size:candidate.sizes_)if(!positive(size,candidate.height_))return fail("Blackbar rectangle rejected");
  for(size_t i=0;i<4;++i){auto&t=candidate.tracks_[i];const auto*q=p+44+i*20;t={real(q),real(q+4),real(q+8),real(q+12),real(q+16)};
   if(!positive(t.duration,120)||!positive(t.end,t.duration)||!std::isfinite(t.from)||!std::isfinite(t.to)||!positive(t.ease,100)||std::abs(t.from)>8192||std::abs(t.to)>8192)return fail("Blackbar track rejected");
  }
  for(size_t i=0;i<2;++i)if(candidate.tracks_[i].from!=candidate.tracks_[i+2].to||candidate.tracks_[i].to!=candidate.tracks_[i+2].from)return fail("Blackbar open/close endpoints rejected");
  candidate.ready_=true;candidate.reset();*this=candidate;return true;
 }
 bool load_file(const char*path,std::string&error){
  FILE*f=std::fopen(path,"rb");if(!f){error="Blackbar pack unavailable";return false;}std::array<uint8_t,125>b{};const auto n=std::fread(b.data(),1,b.size(),f);const bool ok=!std::ferror(f);std::fclose(f);if(!ok){error="Blackbar pack read failed";return false;}return load(b.data(),n,error);
 }
 void reset(){open_=false;time_=ready_?std::max(tracks_[2].duration,tracks_[3].duration):0;}
 bool update(bool open,double delta){
  if(!ready_||!std::isfinite(delta)||delta<0||delta>1)return false;
  // Source play() restarts the selected clip from its authored first key,
  // including interrupted transitions; repeated toggle(target) does nothing.
  if(open!=open_){open_=open;time_=0;}
  time_=std::min(time_+delta,double(std::max(tracks_[open_?0:2].duration,tracks_[open_?1:3].duration)));return true;
 }
 bool target_open()const{return open_;}
 Pose pose(float width,float height)const{
  Pose p;p.color=color_;if(!ready_||!positive(width,8192)||!positive(height,8192))return p;
  for(size_t i=0;i<2;++i){const float y=sample(tracks_[(open_?0:2)+i],time_)+(i?height-height_:0);const float top=std::clamp(y,0.0f,height),bottom=std::clamp(y+sizes_[i],0.0f,height);p.bars[i]={0,top,width,std::max(0.0f,bottom-top)};}return p;
 }
private:
 struct Track{float duration=0,end=0,from=0,to=0,ease=0;};
 static uint32_t integer(const uint8_t*p){return uint32_t(p[0])|uint32_t(p[1])<<8|uint32_t(p[2])<<16|uint32_t(p[3])<<24;}
 static float real(const uint8_t*p){const auto bits=integer(p);float value;std::memcpy(&value,&bits,4);return value;}
 static bool positive(float x,float limit){return std::isfinite(x)&&x>0&&x<=limit;}
 static float sample(const Track&t,double time){
  if(time>=t.end)return t.to;
  float f=std::clamp(float(time)/t.end,0.0f,1.0f);
  f=t.ease<1?1-std::pow(1-f,1/t.ease):std::pow(f,t.ease);
  return .5f*((2*t.from)+(-t.from+t.to)*f+(2*t.from-5*t.from+4*t.to-t.to)*f*f+(-t.from+3*t.from-3*t.to+t.to)*f*f*f);
 }
 std::array<Track,4>tracks_{};std::array<float,2>sizes_{};float width_=0,height_=0;uint32_t color_=0;double time_=0;bool ready_=false,open_=false;
};
}

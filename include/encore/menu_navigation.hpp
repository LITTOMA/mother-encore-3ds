#pragma once
#include <cmath>
#include <cstdint>
namespace encore::upstream {
struct MenuDirection { int x=0,y=0; };
// Called once per physical input sample, outside fixed-step catch-up. Context
// changes suppress an already-held direction until neutral; no queued repeats.
class MenuNavigationRepeat {
public:
 void reset(){context_=0;direction_={};remaining_=0;blocked_=false;}
 MenuDirection sample(uint32_t context,int x,int y,double elapsed,double delay,double interval,bool grid=false){
  x=(x>0)-(x<0);y=grid?((y>0)-(y<0)):0;
  if(y)x=0; // Source inventory resolves a diagonal vertically.
  if(context!=context_){context_=context;direction_={};remaining_=0;blocked_=x||y;return {};}
  if(!context||!std::isfinite(delay)||!std::isfinite(interval)||delay<=0||interval<=0){reset();return {};}
  if(!x&&!y){direction_={};remaining_=0;blocked_=false;return {};}
  if(blocked_)return {};
  if(x!=direction_.x||y!=direction_.y){direction_={x,y};remaining_=delay;return direction_;}
  if(std::isfinite(elapsed)&&elapsed>0)remaining_-=elapsed;
  if(remaining_>1e-7)return {}; // Ignore float resource roundoff at exact sample boundaries.
  remaining_=interval; // Discard repeat debt: a stalled frame cannot skip options.
  return direction_;
 }
private:
 uint32_t context_=0;MenuDirection direction_{};double remaining_=0;bool blocked_=false;
};
}

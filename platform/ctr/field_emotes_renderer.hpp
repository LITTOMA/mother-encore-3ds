#pragma once
#include "loading_texture.hpp"
#include "encore/field_emotes.hpp"
#include <algorithm>
#include <cmath>
#include <string>
// Borrows an existing admitted actor atlas; no duplicate conversion or loading
// inside GPU frames. Source scale/center/flip remain source pixels 1:1.
class FieldEmoteRenderer {
 const encore::upstream::FieldEmoteData*data_=nullptr;C2D_Image atlas_{};
public:
 bool initialize(const encore::upstream::FieldEmoteData&data,C2D_Image atlas,std::string&error){if(!data.valid()||!atlas.tex||!atlas.subtex||Tex3DS_SubTextureRotated(atlas.subtex)||atlas.subtex->width!=data.width()||atlas.subtex->height!=data.height()){error="Emotes borrowed source atlas rejected";return false;}data_=&data;atlas_=atlas;C3D_TexSetFilter(atlas.tex,GPU_NEAREST,GPU_NEAREST);error.clear();return true;}
 bool draw(const encore::upstream::FieldEmoteRuntime&runtime,uint32_t id,encore::upstream::Vec2 parent_world,encore::upstream::Vec2 parent_scale,float dx,float dy,bool ancestor_visible,bool source_pixel_snap,uint32_t color=0xffffffffu)const{
  if(!data_||!atlas_.tex||!std::isfinite(dx)||!std::isfinite(dy)||!std::isfinite(parent_world.x)||!std::isfinite(parent_world.y)||!std::isfinite(parent_scale.x)||!std::isfinite(parent_scale.y))return false;const auto*d=data_->record(id);const auto*s=runtime.instance(id);if(!d||!s||!s->ready)return false;if(!ancestor_visible||!s->visible||s->scale.x==0||s->scale.y==0||parent_scale.x==0||parent_scale.y==0)return true;if(s->frame>=d->columns*d->rows)return false;
  const uint32_t w=data_->width()/d->columns,h=data_->height()/d->rows,u=(s->frame%d->columns)*w,v=(s->frame/d->columns)*h;auto sub=*atlas_.subtex;const float step_u=(sub.right-sub.left)/data_->width(),step_v=(sub.bottom-sub.top)/data_->height(),origin_u=sub.left,origin_v=sub.top;sub.left=origin_u+u*step_u;sub.right=origin_u+(u+w)*step_u;sub.top=origin_v+v*step_v;sub.bottom=origin_v+(v+h)*step_v;sub.width=uint16_t(w);sub.height=uint16_t(h);
  const float left=s->offset.x-((d->flags&2)?w/2.f:0),top=s->offset.y-((d->flags&2)?h/2.f:0);float x1=dx+parent_world.x+(s->position.x+left*s->scale.x)*parent_scale.x,x2=dx+parent_world.x+(s->position.x+(left+w)*s->scale.x)*parent_scale.x,y1=dy+parent_world.y+(s->position.y+top*s->scale.y)*parent_scale.y,y2=dy+parent_world.y+(s->position.y+(top+h)*s->scale.y)*parent_scale.y;if(x2<x1)std::swap(sub.left,sub.right);if(y2<y1)std::swap(sub.top,sub.bottom);float x=std::min(x1,x2),y=std::min(y1,y2);if(source_pixel_snap){x=std::floor(x+.5f);y=std::floor(y+.5f);}C2D_ImageTint tint;C2D_PlainImageTint(&tint,color,0);return C2D_DrawImageAt({atlas_.tex,&sub},x,y,0,&tint,std::abs(x2-x1)/w,std::abs(y2-y1)/h);
 }
};

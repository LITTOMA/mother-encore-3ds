#pragma once
#include "encore/field_menus.hpp"
#include "battle_renderer.hpp"
#include <memory>
#include <cstring>

// Original source pixels are carried in the checked menu pack. The platform
// performs only GPU upload, swizzling and clipping. No game bindings live here.
class FieldMapRenderer {
 struct Image {C3D_Tex texture{};uint32_t width=0,height=0,tw=0,th=0;bool ready=false;~Image(){if(ready)C3D_TexDelete(&texture);}};
 std::vector<std::unique_ptr<Image>>images_;
 static unsigned morton(unsigned x,unsigned y){return(x&1)|((y&1)<<1)|((x&2)<<1)|((y&2)<<2)|((x&4)<<2)|((y&4)<<3);}
 bool image(uint32_t id,float x,float y,float dx,float dy,bool flip=false)const{
  if(id>=images_.size())return false;const auto&a=*images_[id];float x0=std::max(0.f,x),y0=std::max(0.f,y),x1=std::min(320.f,x+a.width),y1=std::min(180.f,y+a.height);if(x1<=x0||y1<=y0)return true;
  const float v0=(y0-y)/a.th,v1=(y1-y)/a.th;Tex3DS_SubTexture sub{u16(x1-x0),u16(y1-y0),(x0-x)/a.tw,flip?1-(float(a.height)/a.th-v0):1-v0,(x1-x)/a.tw,flip?1-(float(a.height)/a.th-v1):1-v1};
  return C2D_DrawImageAt({const_cast<C3D_Tex*>(&a.texture),&sub},dx+x0,dy+y0,0,nullptr,1,1);
 }
public:
 ~FieldMapRenderer(){free();}
 void free(){if(!images_.empty())C3D_FrameSync();images_.clear();}
 bool ready()const{return !images_.empty();}
 bool load(const encore::upstream::FieldMenusData&data,std::string&e){
  if(!data.valid()){e="Field map checked data missing";return false;}std::vector<std::unique_ptr<Image>>next;
  for(const auto&s:data.images){auto a=std::make_unique<Image>();a->width=s.width;a->height=s.height;a->tw=a->th=8;while(a->tw<s.width)a->tw*=2;while(a->th<s.height)a->th*=2;
   if(!C3D_TexInit(&a->texture,a->tw,a->th,GPU_RGBA8)){e="Field map texture allocation failed";return false;}a->ready=true;std::memset(a->texture.data,0,a->texture.size);auto*out=static_cast<uint32_t*>(a->texture.data);
   for(uint32_t y=0;y<s.height;++y)for(uint32_t x=0;x<s.width;++x){const auto at=(size_t(y)*s.width+x)*4;const auto*rgba=s.rgba.data()+at;out[((y/8)*(a->tw/8)+x/8)*64+morton(x,y)]=uint32_t(rgba[0])<<24|uint32_t(rgba[1])<<16|uint32_t(rgba[2])<<8|rgba[3];}
   C3D_TexSetFilter(&a->texture,GPU_NEAREST,GPU_NEAREST);C3D_TexSetWrap(&a->texture,GPU_CLAMP_TO_EDGE,GPU_CLAMP_TO_EDGE);C3D_TexFlush(&a->texture);next.push_back(std::move(a));
  }
  free();images_.swap(next);e.clear();return true;
 }
 bool draw(const encore::upstream::FieldMenus&menu,const BattleRenderer&font,float width,float height,float left=0,float top=0)const{
  using Text=encore::upstream::FieldMenuText;const auto*d=menu.content();if(!d||!ready())return false;const float dx=left+(width-320)/2,dy=top+(height-180)/2;const auto&state=menu.state();const bool zh=state.chinese;
  BattleRenderer::draw_rect(dx,dy,320,180,C2D_Color32(0,0,0,255));if(!image(menu.map_missing()?1:0,-menu.map_x(),-menu.map_y(),dx,dy))return false;
  if(!menu.map_missing()&&menu.markers_visible())for(const auto&m:d->markers){const float x=m.x-menu.map_x(),y=m.y-menu.map_y();if(m.image!=UINT32_MAX){if(!image(m.image,x,y,dx,dy,m.flip))return false;}else if(!font.draw_text_clipped(m.label.get(zh).c_str(),dx+x,dy+y,dx,dy,dx+320,dy+180,C2D_Color32(255,255,255,255)))return false;}
  if(!menu.map_missing()){const auto&s=state.session;const auto&locator=d->images[2];const float x=(state.in_field?d->map_offset_x+s.position_x/d->map_divisor:d->map_home_x)-locator.width/2.f-menu.map_x(),y=(state.in_field?d->map_offset_y+s.position_y/d->map_divisor:d->map_home_y)-locator.height/2.f-menu.map_y();if(!image(2,x,y,dx,dy))return false;}
  else if(!font.draw_text_clipped(d->text(Text::MapMissing,zh).c_str(),dx+10,dy+75,dx,dy,dx+320,dy+150,C2D_Color32(255,255,255,255)))return false;
  return font.draw_text_clipped(menu.page().footer.c_str(),dx+4,dy+165,dx,dy,dx+320,dy+180,C2D_Color32(255,255,255,255));
 }
};

#pragma once
#include "save_menu_renderer.hpp"
#include "encore/continue_menu.hpp"
#include "encore/title_locale_data.hpp"
// Caller provides an active upper-screen target and translates the 1:1 game
// viewport origin. Reuses the already loaded save renderer; no second flavor set.
class ContinueRenderer {
 using Data=encore::upstream::ContinueMenuData;using Layout=encore::upstream::ContinueLayout;
 BattleRenderer art_;const Data*data_=nullptr;
 const encore::upstream::TitleLocaleData*title_locale_=nullptr;
 const encore::upstream::LocaleSelection*locale_=nullptr;
 std::string loaded_locale_;std::vector<encore::upstream::SaveMenuRect>option_rects_;
 bool viewport(float width,float height)const{if(!data_||!std::isfinite(width)||!std::isfinite(height))return false;const auto r=data_->layout(Layout::Reference);return(width==r.x&&height==r.y)||(width==r.w&&height==r.h);}
public:
 bool ready()const{return data_!=nullptr;}
 // Attach before load. Both owners must outlive this renderer. free() releases
 // all GPU resources but retains these bindings for the naming/title lifecycle.
 bool attach_localized_title(const encore::upstream::TitleLocaleData&d,const encore::upstream::LocaleSelection&locale,std::string&e){
  if(ready()||!d.valid()||!locale.catalog()||!locale.catalog()->valid()||locale.code().empty()){e="Attach checked title localization before renderer load";return false;}
  title_locale_=&d;locale_=&locale;e.clear();return true;
 }
 bool load(const Data&d,const char*prefix,std::string&e){
  // Caller invokes only between frames; no texture loading from draw/text paths.
  free();if(!d.valid()||!prefix){e="Continue renderer requires checked data";return false;}
  auto resolved=d.resources();std::vector<encore::upstream::SaveMenuRect>positions;
  if(title_locale_){if(!locale_||!title_locale_->resolve(d,*locale_,resolved,positions,e))return false;}
  else for(const auto&o:d.title_options())positions.push_back(o.rect);
  std::vector<std::string>paths;std::vector<BattleRenderer::Resource>resources;paths.reserve(resolved.size());for(const auto&r:resolved)paths.emplace_back(std::string(prefix)+r.path);
  for(size_t i=0;i<paths.size();++i){const auto&r=resolved[i];resources.push_back({paths[i].c_str(),1,r.width,r.height,r.columns,r.rows});}if(!art_.load(resources,1,1,e))return false;data_=&d;option_rects_=std::move(positions);if(locale_)loaded_locale_=locale_->code();e.clear();return true;
 }
 void free(){art_.free();data_=nullptr;loaded_locale_.clear();option_rects_.clear();}
 // Does not draw transition overlay: call draw_overlay after restored world/UI.
 bool draw(const encore::upstream::ContinueMenu&menu,const SaveMenuRenderer&save,const BattleRenderer&eb,float width,float height)const{
  using namespace encore::upstream;if(!menu.is_open())return true;if(!viewport(width,height)||menu.data()!=data_||!save.data_||menu.slot_model().data()!=save.data_||(locale_&&loaded_locale_!=locale_->code()))return false;const int viewport=data_->viewport_index(width);if(viewport<0)return false;auto p=menu.pose();if(p.world_visible)return true;
  BattleRenderer::draw_rect(0,0,width,height,p.phase==ContinuePhase::Title?data_->title_background_color():data_->background_color());
  if(p.phase==ContinuePhase::Title){for(const auto&layer:data_->title_layers()){const auto&offset=layer.viewport_offsets[size_t(viewport)];if(!art_.draw_sprite(layer.resource,0,layer.rect.x+offset.x,layer.rect.y+offset.y,layer.rect.w,layer.rect.h,0xffffffff))return false;}for(size_t i=0;i<data_->title_options().size();++i){const auto&o=data_->title_options()[i];const auto&r=option_rects_[i];const auto&offset=o.viewport_offsets[size_t(viewport)];if(!art_.draw_sprite(i==p.title_option?o.selected_resource:o.resource,0,r.x+offset.x,r.y+offset.y,r.w,r.h,0xffffffff))return false;}return true;}
  const auto&sd=*save.data_;auto body=data_->layout(Layout::Body);const float w=width-body.x+body.w,h=sd.layout(SaveMenuLayout::Card).h;auto actions=data_->layout(Layout::Actions);const auto&rects=data_->viewports()[size_t(viewport)].actions;
  for(size_t i=0;i<menu.slot_model().slots().size();++i){const float y=p.slots.cards_y+i*sd.slot_spacing();if(y+h<0||y>=height)continue;const auto&s=menu.slot_model().slots()[i];if(!save.card(s,uint32_t(i+1),body.x,y,w,eb))return false;if(s.occupied)for(uint32_t a=0;a<rects.size();++a){const auto&r=rects[a];if(!save.bottle(data_->text(a),body.x+actions.x+r.x,y+actions.y+r.y,sd.text_color()))return false;}}
  if(p.slots.cursor_visible){const auto&r=p.slots.cursor_margins;if(!save.box(sd.cursor_resource(),{body.x+r.x,p.slots.cursor_y+r.y,w+r.w-r.x,r.h-r.y},SaveMenuLayout::CursorPatch))return false;}
  if(p.arrow_visible){auto offset=data_->layout(Layout::ArrowOffset);const float y=p.slots.cards_y+(p.slots.selected_slot-1)*sd.slot_spacing();if(!save.sprite(sd.arrow_resource(),p.arrow_frame,body.x+actions.x+p.action_x[size_t(viewport)]+offset.x,y+actions.y+rects[p.action].y+offset.y))return false;}
  return true;
 }
 bool draw_overlay(const encore::upstream::ContinueMenu&menu,float width,float height,float focus_x,float focus_y)const{
  using namespace encore::upstream;if(!menu.is_open())return true;if(!viewport(width,height)||!std::isfinite(focus_x)||!std::isfinite(focus_y)||menu.data()!=data_)return false;const auto p=menu.pose().fade;if(!p.visible)return true;
  if(!p.circle){if(p.alpha>0)BattleRenderer::draw_rect(0,0,width,height,C2D_Color32(0,0,0,uint8_t(std::round(p.alpha*255))));return true;}
  const auto ref=data_->layout(Layout::Reference),g=data_->layout(Layout::FadeGeometry),offset=data_->layout(Layout::FadeOffset),title=data_->layout(Layout::TitleViewportOffsets);const int viewport=data_->viewport_index(width);if(viewport<0)return false;const float title_x=viewport?title.w:title.x,title_y=viewport?title.h:title.y;const float cx=(p.focused?focus_x:ref.x/2+title_x)+offset.x,cy=(p.focused?focus_y:ref.y/2+title_y)+offset.y,ratio=g.w/g.h;
  // Evaluate the original hard shader step at pixel centers, then coalesce only
  // equal opaque coverage. This does not approximate the circle with triangles.
  for(int y=0;y<int(height);++y){int start=-1;for(int x=0;x<=int(width);++x){bool opaque=false;if(x<int(width)){float dx=((x+.5f-cx)/g.x)*ratio,dy=(y+.5f-cy)/g.y;opaque=std::sqrt(dx*dx+dy*dy)>=p.cut;}if(opaque&&start<0)start=x;if(!opaque&&start>=0){BattleRenderer::draw_rect(float(start),float(y),float(x-start),1,data_->background_color());start=-1;}}}
  return true;
 }
};

#pragma once
#include "battle_renderer.hpp"
#include "encore/save_menu.hpp"
#include <iomanip>
#include <sstream>
// Draw only into the caller's active upper game target. No input, filesystem
// save operations, lower-screen widgets or management actions live here.
class SaveMenuRenderer {
 friend class ContinueRenderer; // Reuses checked card art without duplicating texture allocations.
 using Data=encore::upstream::SaveMenuData;using Layout=encore::upstream::SaveMenuLayout;using Text=encore::upstream::SaveMenuText;using Rect=encore::upstream::SaveMenuRect;
 BattleRenderer art_;const Data*data_=nullptr;
 static float pixel(float x){return std::floor(x+.5f);}
 float width(const std::string&s)const{return art_.text_width(s.c_str())-(s.empty()||s.back()==' '?0:data_->bottle_spacing());}
 bool bottle(const std::string&s,float x,float y,uint32_t color)const{
  float dx=x;for(unsigned char c:s){const auto it=std::lower_bound(data_->glyphs().begin(),data_->glyphs().end(),c,[](const encore::upstream::SaveMenuGlyph&g,unsigned char cp){return g.codepoint<cp;});if(it==data_->glyphs().end()||it->codepoint!=c||!it->advance)return false;const auto&g=*it;
   if(g.width&&g.height&&!art_.draw_region(data_->outline_resource(),g.u,g.v,g.width,g.height,pixel(dx+g.offset_x),pixel(y+g.offset_y),g.width,g.height,data_->outline_color(),1))return false;
   dx+=g.advance;}
  return art_.draw_text(s.c_str(),pixel(x),pixel(y),1,1,color);
 }
 bool box(uint32_t resource,Rect r,Layout patch)const{auto m=data_->layout(patch);uint32_t p[]={uint32_t(m.x),uint32_t(m.y),uint32_t(m.w),uint32_t(m.h)};return art_.draw_ninepatch(resource,pixel(r.x),pixel(r.y),r.w,r.h,p,data_->text_color());}
 bool sprite(uint32_t resource,uint32_t frame,float x,float y)const{const auto&a=data_->resources()[resource];float w=float(a.width/a.columns),h=float(a.height/a.rows);return art_.draw_sprite(resource,frame,pixel(x-w/2),pixel(y-h/2),w,h,data_->text_color());}
 static std::string digits(uint64_t v,uint32_t minimum){auto s=std::to_string(v);return s.size()<minimum?std::string(minimum-s.size(),'0')+s:s;}
 bool card(const encore::upstream::SaveSlotMetadata&s,uint32_t number,float x,float y,float w,const BattleRenderer&eb)const{
  const auto&d=*data_;const auto white=d.text_color();const auto h=d.layout(Layout::Card).h;
  const auto&flavor=d.flavors()[s.occupied?size_t(d.flavor_index(s.menu_flavor)):0];
  if(!box(flavor.resource,{x,y,w,h},Layout::CardPatch))return false;
  auto at=[&](Layout key,bool right=false){auto r=d.layout(key);r.x+=x+(right?w:0);r.y+=y;return r;};
  if(!s.occupied){auto r=at(Layout::NoData);r.w+=w-d.layout(Layout::Card).w;BattleRenderer::draw_rect(pixel(r.x),pixel(r.y),r.w,r.h,flavor.background_color);
   auto label=d.layout(Layout::NoDataLabel);label.w+=w-d.layout(Layout::Card).w;const auto&text=d.text(Text::NoData);if(!bottle(text,r.x+label.x+(label.w-width(text))/2,r.y+label.y+(label.h-d.bottle_height())/2,white))return false;
  }else{
   auto r=at(Layout::Name);if(!eb.draw_text(s.lead_name.c_str(),pixel(r.x),pixel(r.y),1,1,white))return false;
   r=at(Layout::Level,true);auto level=d.text(Text::Level)+std::to_string(s.highest_level);if(!bottle(level,r.x+r.w-width(level),r.y,white))return false;
   r=at(Layout::Title,true);if(!eb.draw_text(s.scene_label.c_str(),pixel(r.x),pixel(r.y),1,1,white))return false;
   r=at(Layout::Time,true);const auto&time=d.text(Text::Time);if(!bottle(time,r.x,r.y,d.time_color()))return false;
   const auto tf=d.layout(Layout::TimeFormat);const uint64_t hours=uint64_t(s.playtime_seconds/tf.x);std::string tail;if(std::to_string(hours).size()>tf.h)tail=d.text(Text::TooMuchTime);else tail=d.text(Text::TimeSpace)+digits(hours,uint32_t(tf.w))+d.text(Text::TimeSeparator)+digits(uint64_t(s.playtime_seconds/tf.y)%uint64_t(tf.y),uint32_t(tf.w));
   if(!bottle(tail,r.x+art_.text_width(time.c_str()),r.y,white))return false;
   for(auto layout:{Layout::DividerVertical,Layout::DividerHorizontal}){r=at(layout,true);BattleRenderer::draw_rect(pixel(r.x),pixel(r.y),r.w,r.h,flavor.divider_color);}
   for(size_t i=0;i<s.party.size();++i){r=d.layout(i?Layout::IconRest:Layout::IconFirst);const auto&icon=d.icons()[size_t(d.icon_index(s.party[i]))];if(!sprite(icon.resource,0,x+r.x+(i?i*r.w:0),y+r.y))return false;}
  }
  auto r=at(Layout::FileNumber);return bottle(std::to_string(number),r.x,r.y,white);
 }
public:
 bool load(const Data&d,const char*prefix,std::string&e){
  free();if(!d.valid()||!prefix){e="Save renderer requires checked menu data";return false;}
  std::vector<std::string>paths;std::vector<BattleRenderer::Resource>resources;paths.reserve(d.resources().size());for(const auto&r:d.resources())paths.emplace_back(std::string(prefix)+r.path);
  for(size_t i=0;i<paths.size();++i){const auto&r=d.resources()[i];resources.push_back({paths[i].c_str(),1,r.width,r.height,r.columns,r.rows});}
  // Texture-only adapter needs no full-screen software surface.
  if(!art_.load(resources,1,1,e))return false;
  std::vector<BattleRenderer::Glyph>glyphs;for(const auto&g:d.glyphs())glyphs.push_back({g.codepoint,d.font_resource(),g.u,g.v,g.width,g.height,g.advance,g.offset_x,g.offset_y});
  if(!art_.set_glyphs(std::move(glyphs),e)){free();return false;}data_=&d;return true;
 }
 void free(){art_.free();data_=nullptr;}
 bool draw(const encore::upstream::SaveMenu&menu,const BattleRenderer&eb,uint32_t current_flavor=0,float viewport_width=0,float viewport_height=0)const{
  using namespace encore::upstream;if(!menu.is_open())return true;if(!data_||menu.data()!=data_||current_flavor>=data_->flavors().size())return false;
  const auto&d=*data_;auto p=menu.pose();const auto reference=d.layout(Layout::Reference);if(viewport_width<=0)viewport_width=reference.w;if(viewport_height<=0)viewport_height=reference.h;
  auto body=d.layout(Layout::Body);const float w=viewport_width-body.x+body.w,h=d.layout(Layout::Card).h;
  for(size_t i=0;i<menu.slots().size();++i){float y=p.cards_y+i*d.slot_spacing();if(y+h>=0&&y<viewport_height&&!card(menu.slots()[i],uint32_t(i+1),body.x,y,w,eb))return false;}
  if(p.cursor_visible){auto r=p.cursor_margins;if(!box(d.cursor_resource(),{body.x+r.x,p.cursor_y+r.y,w+r.w-r.x,r.h-r.y},Layout::CursorPatch))return false;}
  if(p.phase!=SaveMenuPhase::Overwrite)return true;
  auto r=d.layout(Layout::Confirm);const auto&flavor=d.flavors()[current_flavor];if(!box(flavor.confirm_resource,r,Layout::ConfirmPatch))return false;
  auto text=d.layout(Layout::ConfirmText);const auto&prompt=d.text(Text::Overwrite);if(!eb.draw_text(prompt.c_str(),pixel(r.x+text.x+(text.w-eb.text_width(prompt.c_str()))/2),pixel(r.y+text.y),1,1,d.text_color()))return false;
  const auto choices=d.layout(Layout::Choices),yes=d.layout(Layout::ChoiceYes),no=d.layout(Layout::ChoiceNo),offset=d.layout(Layout::ArrowOffset);
  for(auto pair:{std::make_pair(Text::Yes,yes),std::make_pair(Text::No,no)}){const auto&t=d.text(pair.first);const auto b=pair.second;if(!eb.draw_text(t.c_str(),pixel(r.x+choices.x+b.x+(b.w-eb.text_width(t.c_str()))/2),pixel(r.y+choices.y+b.y),1,1,d.text_color()))return false;}
  const float ax=yes.x+(no.x-yes.x)*p.arrow_choice,ay=yes.y+(no.y-yes.y)*p.arrow_choice;
  return sprite(d.arrow_resource(),p.arrow_frame,r.x+choices.x+ax+offset.x,r.y+choices.y+ay+offset.y);
 }
};

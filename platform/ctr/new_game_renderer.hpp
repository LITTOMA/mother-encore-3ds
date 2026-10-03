#pragma once
#include "battle_renderer.hpp"
#include "encore/new_game_setup.hpp"
#include "encore/utf8.hpp"
#include "house_button_prompts_renderer.hpp"
// Original authored 320x180 UI is centered at 1:1 in the expanded viewport.
// All positions, art, characters, animation keys and menu text come from ENCNAMES.
class NewGameRenderer {
 BattleRenderer art_,settings_art_;const HouseButtonPromptsRenderer*prompts_=nullptr;const encore::upstream::StartupSettingsData*settings_=nullptr;const encore::upstream::NewGameSetupData*data_=nullptr;
 static float pixel(float f){return std::floor(f+.5f);}
 bool sprite(uint32_t id,uint32_t frame,float x,float y)const{const auto&r=data_->resources[id];const float w=float(r.width/r.columns),h=float(r.height/r.rows);return art_.draw_sprite(id,frame,pixel(x-w/2),pixel(y-h/2),w,h,data_->colors[1]);}
 bool draw_settings(const encore::upstream::NewGameSetup&menu,const BattleRenderer&font)const{
  using namespace encore::upstream;const auto&d=*settings_;const bool confirmation=menu.phase()==NamingPhase::Confirmation;
  auto box=[&](uint32_t resource,const SettingsRect&r){return settings_art_.draw_ninepatch(resource,r.x,r.y,r.w,r.h,d.patch.data(),d.text_color);};
  auto label=[&](const std::string&text,SettingsRect r,bool right=false){const float x=r.x+(right?r.w-font.text_width(text.c_str()):0);return font.draw_text(text.c_str(),pixel(x),pixel(r.y),1,1,d.text_color);};
  auto offset=[](const SettingsRect&a,const SettingsRect&b){return SettingsRect{a.x+b.x,a.y+b.y,b.w,b.h};};
  auto arrow=[&](const SettingsRect&r){return sprite(1,data_->arrow.frame(menu.elapsed()),r.x+data_->arrow_offset[0],r.y+data_->arrow_offset[1]);};
  const auto frame=confirmation?d.confirmation_settings_box:d.settings_box;if(!box(d.box_resource,frame))return false;
  const auto&chosen=menu.settings();const std::string values[]={menu.localized("settings.speed/"+std::to_string(d.speed_index(chosen.text_speed)),d.speed_labels[size_t(d.speed_index(chosen.text_speed))]),menu.localized("settings.flavor/"+std::to_string(d.flavor_index(chosen.menu_flavor)),d.flavor_labels[size_t(d.flavor_index(chosen.menu_flavor))]),menu.localized("settings.prompt/"+std::to_string(d.prompt_index(chosen.button_prompts)),d.prompt_labels[size_t(d.prompt_index(chosen.button_prompts))])};
  for(size_t i=0;i<d.rows.size()-(confirmation?1:0);++i){auto l=offset(frame,d.rows[i].label),v=offset(frame,d.rows[i].value);if(confirmation){l.x+=d.confirmation_row_offset[0];l.y+=d.confirmation_row_offset[1];v.x+=d.confirmation_row_offset[0];v.y+=d.confirmation_row_offset[1];}if(!label(menu.localized("settings.row/"+std::to_string(i),d.rows[i].text),l)||(i<3&&!label(values[i],v,true)))return false;if(!confirmation&&menu.phase()==NamingPhase::Settings&&i==menu.settings_row()&&!arrow(l))return false;}
  if(confirmation){
   if(menu.values().size()!=d.confirmation_fields.size())return false;
   for(size_t i=0;i<d.confirmation_fields.size();++i){const auto&f=d.confirmation_fields[i];if(!box(d.card_resource,f.box)||!box(d.inside_resource,offset(f.box,f.inside))||!label(menu.values()[i],offset(f.box,f.label)))return false;const auto icon=offset(f.box,f.icon);const auto&r=d.resources[f.resource];if(!settings_art_.draw_sprite(f.resource,0,icon.x+(icon.w-r.width)/2,icon.y+(icon.h-r.height)/2,float(r.width),float(r.height),d.text_color))return false;}
   if(!box(d.box_resource,d.confirmation_box)||!label(menu.localized("settings.certainty",d.certainty.text),offset(d.confirmation_box,d.certainty.rect)))return false;
   for(size_t i=0;i<d.confirmation_choices.size();++i){const auto&q=d.confirmation_choices[i];const auto r=offset(d.confirmation_box,q.rect);if(!label(menu.localized("settings.confirm/"+std::to_string(i),q.text),r)||(i==menu.confirmation()&&!arrow(r)))return false;}
  }else if(menu.settings_row()<d.panels.size()){
   const auto&panel=d.panels[menu.settings_row()];if(!box(d.box_resource,panel.box))return false;
   if(menu.settings_row()==2&&prompts_&&!prompts_->draw_preview(menu.phase()==NamingPhase::SettingOption?menu.option():uint32_t(d.prompt_index(chosen.button_prompts)),panel.box.x,panel.box.y))return false;
   for(size_t i=0;i<panel.labels.size();++i){const auto&q=panel.labels[i];auto text=menu.localized("settings.panel/"+std::to_string(menu.settings_row())+"/"+std::to_string(i),q.text);if(menu.phase()==NamingPhase::SettingOption&&menu.settings_row()==0&&i==menu.option())encore::utf8_prefix(text,menu.preview_characters(),text);const auto r=offset(panel.box,q.rect);if(!label(text,r)||(menu.phase()==NamingPhase::SettingOption&&i==menu.option()&&!arrow(r)))return false;}
  }
  return true;
 }
public:
 void bind_prompts(const HouseButtonPromptsRenderer&value){prompts_=&value;}
 bool ready()const{return data_!=nullptr;}
 bool load(const encore::upstream::NewGameSetupData&data,const encore::upstream::StartupSettingsData&settings,std::string&e){
  free();if(!data.valid()||!settings.valid()){e="Naming renderer requires checked data";return false;}std::vector<std::string>paths;std::vector<BattleRenderer::Resource>resources;paths.reserve(data.resources.size());for(const auto&r:data.resources)paths.emplace_back("romfs:/"+r.path);for(size_t i=0;i<paths.size();++i){const auto&r=data.resources[i];resources.push_back({paths[i].c_str(),1,r.width,r.height,r.columns,r.rows});}if(!art_.load(resources,1,1,e))return false;paths.clear();resources.clear();paths.reserve(settings.resources.size());for(const auto&r:settings.resources)paths.emplace_back("romfs:/"+r.path);for(size_t i=0;i<paths.size();++i){const auto&r=settings.resources[i];resources.push_back({paths[i].c_str(),1,r.width,r.height,r.columns,r.rows});}if(!settings_art_.load(resources,1,1,e)){free();return false;}data_=&data;settings_=&settings;return true;
 }
 void free(){art_.free();settings_art_.free();data_=nullptr;settings_=nullptr;}
 bool draw(const encore::upstream::NewGameSetup&menu,const BattleRenderer&font,float width,float height)const{
  if(!menu.active())return true;
  if(!data_||menu.data()!=data_||!((width==320&&height==180)||(width==400&&height==240)))return false;
  const auto&d=*data_;
  BattleRenderer::draw_rect(0,0,width,height,d.colors[0]);C3D_Mtx saved;C2D_ViewSave(&saved);C2D_ViewTranslate((width-320)/2,(height-180)/2);
  const auto draw=[&](){
   if(menu.phase()!=encore::upstream::NamingPhase::Editing)return draw_settings(menu,font);
   for(auto index:{0u,4u}){const auto&r=d.layouts[index];if(!settings_art_.draw_ninepatch(settings_->box_resource,r.x,r.y,r.w,r.h,d.patch.data(),d.colors[1]))return false;}
   const auto&f=menu.field();if((f.shadow&&!sprite(3,0,f.shadow_position[0],f.shadow_position[1]))||!sprite(f.resource,f.actor.frame(menu.elapsed()),f.actor_position[0],f.actor_position[1]))return false;
   const auto&box=d.layouts[0];const auto&prompt=d.layouts[1];const auto&field=d.layouts[2];const auto&label=d.layouts[3];
   if(!font.draw_text(menu.prompt().c_str(),pixel(box.x+prompt.x+(prompt.w-font.text_width(menu.prompt().c_str()))/2),pixel(box.y+prompt.y),1,1,d.colors[1]))return false;
   BattleRenderer::draw_rect(box.x+field.x,box.y+field.y,field.w,field.h,encore::ctr::loading_menu_flavor_color(d.colors[2]));
   BattleRenderer::draw_rect(box.x+field.x-1,box.y+field.y+1,field.w+2,field.h-2,encore::ctr::loading_menu_flavor_color(d.colors[2]));
   const auto name=menu.dotted_name();if(!font.draw_text(name.c_str(),pixel(box.x+field.x+(field.w-font.text_width(name.c_str()))/2),pixel(box.y+field.y+label.y+(field.h-label.y-font.text_height(d.font_height))/2),1,1,d.colors[3]))return false;
   if(!art_.draw_sprite(4+menu.panel(),0,0,0,320,180,d.colors[1]))return false;
   for(const auto&k:d.panels[menu.panel()])if(k.kind){const auto value=menu.localized("naming.command/"+std::to_string(k.kind),k.value);const float x=k.rect.x+(k.kind==3?k.rect.w-font.text_width(value.c_str()):0);if(!font.draw_text(value.c_str(),pixel(x),pixel(k.rect.y),1,1,d.colors[1]))return false;}
   const auto cursor=menu.cursor();return sprite(1,d.arrow.frame(menu.elapsed()),cursor[0]+d.arrow_offset[0],cursor[1]+d.arrow_offset[1]);
  };const bool ok=draw();C2D_ViewRestore(&saved);return ok&&menu.locale_error().empty();
 }
};

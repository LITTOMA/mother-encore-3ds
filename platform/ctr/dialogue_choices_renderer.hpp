#pragma once
#include "battle_renderer.hpp"
#include "encore/dialogue_choices.hpp"
#include "encore/localization.hpp"

// Source option Labels/Arrow are children of the already animated House
// dialogue box. The caller passes that box's rendered origin, including its
// existing expanded-viewport anchor. No independent menu frame is invented.
class DialogueChoicesRenderer {
 const encore::upstream::LocaleSelection*locale_=nullptr;BattleRenderer arrow_;const encore::upstream::DialogueChoicesData*data_=nullptr;
public:
 void set_locale(const encore::upstream::LocaleSelection*value){locale_=value;}
 bool load(const encore::upstream::DialogueChoicesData&data,const char*prefix,std::string&error){
  free();if(!data.valid()||!prefix){error="Dialogue choice renderer requires checked data";return false;}
  const auto&r=data.arrow_resource();const std::string path=std::string(prefix)+r.path;
  const std::vector<BattleRenderer::Resource>resources={{path.c_str(),1,r.width,r.height,r.columns,r.rows}};
  if(!arrow_.load(resources,1,1,error))return false;
  data_=&data;return true;
 }
 void free(){arrow_.free();data_=nullptr;}
 bool draw(const encore::upstream::DialogueChoices&choices,const BattleRenderer&font,float box_x,float box_y)const{
  const auto pose=choices.pose();if(!pose.visible)return true;
  if(!data_||choices.data()!=data_||!choices.group())return false;
  const auto grid=data_->grid();const auto color=data_->text_color();
  for(const auto&option:choices.group()->options){const auto r=option.rect;const auto text=locale_?std::string(locale_->text(option.translation_key).text):option.text;
   if(!font.draw_text(text.c_str(),std::floor(box_x+grid.x+r.x+.5f),std::floor(box_y+grid.y+r.y+(r.h-data_->font_height())/2+.5f),1,1,color))return false;
  }
  const auto geometry=data_->arrow_geometry();return arrow_.draw_sprite(0,pose.arrow_frame,std::floor(box_x+pose.arrow_x-geometry.w/2+.5f),std::floor(box_y+pose.arrow_y-geometry.h/2+.5f),geometry.w,geometry.h,color);
 }
 // The reviewed two-option Dialoguebox/Options cells. Every checked pair uses the same rects.
 bool draw_pair(std::string_view left,std::string_view right,float rect_x,float rect_y,uint32_t frame,const BattleRenderer& font,float box_x,float box_y)const{
  if(!data_||data_->groups().empty()||data_->groups().front().options.size()<2)return false;
  const auto grid=data_->grid();const auto color=data_->text_color();const auto& options=data_->groups().front().options;
  const std::string labels[2]={std::string(left),std::string(right)};
  for(int i=0;i<2;++i){const auto r=options[size_t(i)].rect;
   if(!font.draw_text(labels[i].c_str(),std::floor(box_x+grid.x+r.x+.5f),std::floor(box_y+grid.y+r.y+(r.h-data_->font_height())/2+.5f),1,1,color))return false;}
  const auto geometry=data_->arrow_geometry();const float ax=grid.x+rect_x+geometry.x,ay=grid.y+rect_y+geometry.y;
  return arrow_.draw_sprite(0,frame,std::floor(box_x+ax-geometry.w/2+.5f),std::floor(box_y+ay-geometry.h/2+.5f),geometry.w,geometry.h,color);
 }
};

#pragma once
#include "battle_renderer.hpp"
#include "encore/house_button_prompts.hpp"
// Original BottleRocket outlined A and rotated select_arrow are baked losslessly
// into one checked asset. A is the explicit CTR ui_accept mapping. Static source
// visible pose only; this does not emulate Show/Float/Hide/Press choreography.
class HouseButtonPromptsRenderer {
 BattleRenderer art_;const encore::upstream::HouseButtonPromptData*data_=nullptr;
 bool prompt(float x,float y)const{const auto&r=data_->prompt_rect;return art_.draw_sprite(0,0,std::floor(x+r.x+.5f),std::floor(y+r.y+.5f),r.z,r.w,data_->color);}
public:
 bool load(const encore::upstream::HouseButtonPromptData&d,std::string&e){free();if(!d.valid()){e="Prompt renderer requires checked data";return false;}std::vector<std::string>paths;std::vector<BattleRenderer::Resource>resources;paths.reserve(d.resources.size());for(const auto&r:d.resources)paths.emplace_back("romfs:/"+r.path);for(size_t i=0;i<paths.size();++i){const auto&r=d.resources[i];resources.push_back({paths[i].c_str(),1,r.width,r.height,1,1});}if(!art_.load(resources,1,1,e))return false;data_=&d;return true;}
 void free(){art_.free();data_=nullptr;}
 bool ready()const{return data_!=nullptr;}
 bool draw(const encore::upstream::HousePromptPose&p,float camera_x,float camera_y)const{return !p.visible||(data_&&prompt(p.position.x-camera_x,p.position.y-camera_y));}
 // x/y are the source ButtonPrompts panel's top-left, already viewport-adjusted.
 bool draw_preview(uint32_t choice,float x,float y)const{if(!data_||choice>=data_->choice_masks.size())return false;for(const auto&p:data_->previews){const auto&r=data_->resources[p.resource];if(!art_.draw_sprite(p.resource,0,std::floor(x+p.position.x-r.width/2.f+.5f),std::floor(y+p.position.y-r.height/2.f+.5f),r.width,r.height,data_->color))return false;if((data_->choice_masks[choice]&p.category)&&!prompt(x+p.position.x+p.offset.x,y+p.position.y+p.offset.y))return false;}return true;}
};

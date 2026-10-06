#pragma once
#include "field_equipment_renderer.hpp"
#include "encore/item_use.hpp"
#include <cmath>

// Shares admitted panel/cursor textures and font faces with the Pause frontend.
// The caller owns these objects for the full lifetime of the active submenu.
class FieldItemUseRenderer {
 using Menu=encore::upstream::FieldItemUseMenu;
 using Phase=encore::upstream::FieldItemUsePhase;
 using Role=encore::upstream::ItemUseLayoutRole;
 using Parameter=encore::upstream::ItemUseParameter;
 using ArtRole=encore::upstream::FieldLayoutRole;
 using FieldParameter=encore::upstream::FieldParameter;
 using Rect=encore::upstream::BattleValue;
 encore::upstream::ItemUseView data_=nullptr;
 encore::upstream::ItemView items_;
 const FieldEquipmentRenderer* art_=nullptr;
 const encore::upstream::LocaleSelection* locale_=nullptr;
 const ItemDetailsRenderer* details_=nullptr;
 mutable std::string error_;
 bool fail(const char* message)const{error_=message;return false;}
 static float pixel(float x){return std::floor(x+.5f);}
 static uint32_t tint(Rect c){return encore::ctr::loading_menu_flavor_color(C2D_Color32f(c.x,c.y,c.z,c.w));}
 Rect rect(Role role,float dx,float dy)const{auto r=data_->layout(role);r.x+=dx;r.y+=dy;return r;}
 bool text(const BattleRenderer& font,std::string_view label,Rect r,uint32_t color)const{
  if(r.z<=0||r.w<=0)return true;
  const std::string value(label);return font.draw_text_clipped(value.c_str(),pixel(r.x),pixel(r.y),r.x,r.y,r.x+r.z,r.y+r.w,color)||fail("Field Items source glyph unavailable");
 }
 bool panel(Rect r,ArtRole role,float left,float top,float width,float height)const{
  return art_->draw_borrowed_art(role,r,0,left,top,width,height)||fail(art_->error().c_str());
 }
 bool cursor(const Menu& menu,Rect label,bool target,float left,float top,float width,float height)const{
  Rect shape;if(!art_->borrowed_rect(ArtRole::PauseCursor,shape))return fail(art_->error().c_str());
  const auto offset=data_->parameter(Parameter::CursorOffsets),center=data_->parameter(Parameter::CursorCenter);shape.x=label.x+(target?offset.z:offset.x)+center.x-shape.z/2;shape.y=label.y+(target?offset.w:offset.y)+center.y-shape.w/2;
  const auto field=art_->content();const auto step=uint32_t(std::fmod(menu.cursor_time()*field.parameter(FieldParameter::CursorFps),4.0));
  const auto frame=uint32_t(field.parameter(FieldParameter(uint32_t(FieldParameter::CursorFrame0)+step)));
  return art_->draw_borrowed_art(ArtRole::PauseCursor,shape,frame,left,top,width,height)||fail(art_->error().c_str());
 }
public:
 bool initialize(encore::upstream::ItemUseView data,encore::upstream::ItemView items,const FieldEquipmentRenderer* art,const encore::upstream::LocaleSelection* locale,const ItemDetailsRenderer* details,std::string& error){
  if(!data||!data->valid()||!items.valid()||!art||!art->ready()||!data->bind_items(items,error)){if(error.empty())error="Field Items requires checked presentation owners";return false;}
  Rect unused;if(!art->borrowed_rect(ArtRole::DescriptionPanel,unused)||!art->borrowed_rect(ArtRole::ItemsTargetPanel,unused)||!art->borrowed_rect(ArtRole::ItemsActionPanel,unused)||!art->borrowed_rect(ArtRole::PauseCursor,unused)){error=art->error();return false;}
  if(!details||!details->ready()){error="Field Items checked descriptions unavailable";return false;}
  data_=data;items_=items;art_=art;locale_=locale;details_=details;error_.clear();return true;
 }
 const std::string& error()const{return error_;}
 bool draw(const Menu& menu,const BattleRenderer& font,float width,float height,float left=0,float top=0)const{
  using namespace encore::upstream;
  if(!menu.visible())return true;
  if(!data_||data_!=menu.content()||!art_->ready()||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0)return fail("Field Items presentation state rejected");
  const auto platform=data_->parameter(Parameter::PlatformViewport);const float dx=left+(width-platform.x)/2,dy=top+(height-platform.y)/2;
  const auto inv=rect(Role::Inventory,dx,dy),grid=data_->parameter(Parameter::Grid),origin=data_->parameter(Parameter::GridOrigin),size=data_->parameter(Parameter::LabelSize);
  const auto normal=tint(items_.parameter(ItemParameter::NormalColor)),disabled=tint(items_.parameter(ItemParameter::DisabledColor));
  if(!panel(inv,ArtRole::DescriptionPanel,left,top,width,height))return false;
  const auto divider=rect(Role::Divider,dx,dy);BattleRenderer::draw_rect(divider.x,divider.y,divider.z,divider.w,tint(data_->layout_color(Role::Divider)));
  Rect selected{};const auto& inventory=menu.snapshot().inventory;
  for(uint32_t i=0;i<inventory.size();++i){
   const auto item=inventory.instance(i);const auto definition=items_.definition(item.definition);std::string label(items_.string(definition.name));
   if(locale_&&(!locale_->catalog()||!locale_->catalog()->bound("item.name/"+std::to_string(item.definition),label,locale_->code(),label,error_)))return false;
   Rect row{inv.x+origin.x+float(i%uint32_t(grid.x))*grid.z,inv.y+origin.y+float(i/uint32_t(grid.x))*grid.w,size.x,size.y};
   if(!text(font,label,row,data_->rule(item.definition)?normal:disabled))return false;
   if(i==menu.selection())selected=row;
  }
  if(menu.phase()==Phase::Items&&inventory.size()&&!cursor(menu,selected,false,left,top,width,height))return false;
  if(menu.description_visible()){
   const auto* item=menu.selected_item();if(item){const auto description=rect(Role::Description,dx,dy),inset=data_->parameter(Parameter::TextInset);if(!panel(description,ArtRole::DescriptionPanel,left,top,width,height)||!details_->draw(*item,font,description.x+inset.x,description.y+inset.y,description.z-inset.x-inset.z,description.w-inset.y-inset.w))return fail(details_->error().c_str());}
  }
  // The upstream script passes the cursor's global Position2D into an action
  // control's local position. Preserve that source parent offset explicitly.
  const auto source=data_->parameter(Parameter::SourceViewport),placement=data_->parameter(Parameter::SubmenuPlacement),point=data_->parameter(Parameter::SubmenuPoint),offset=data_->parameter(Parameter::CursorOffsets),center=data_->parameter(Parameter::CursorCenter);
  const float adapter_x=dx+(platform.x-source.x)/2,adapter_y=dy+(platform.y-source.y)/2;
  auto action=rect(Role::Action,dx,dy);const auto base_action=action;
  if(inventory.size()){action.x=selected.x-adapter_x+offset.x+center.x+point.x+placement.x*(menu.selection()%uint32_t(grid.x)?placement.z:placement.y)+point.z+adapter_x;action.y=std::min(placement.w,selected.y-adapter_y+offset.y+center.y+point.y)+point.w+adapter_y;}
  const float submenu_dx=action.x-base_action.x,submenu_dy=action.y-base_action.y;
  if(menu.phase()==Phase::Action||menu.phase()==Phase::Targets){
   const auto inset=data_->parameter(Parameter::ActionOrigin);Rect row{action.x+inset.x,action.y+inset.y,action.z-inset.x,size.y};
   if(!panel(action,ArtRole::ItemsActionPanel,left,top,width,height)||!text(font,menu.locale().action,row,normal))return false;
   if(menu.phase()==Phase::Action&&!cursor(menu,row,false,left,top,width,height))return false;
  }
  if(menu.phase()==Phase::Targets){
   const auto target=rect(Role::Targets,dx+submenu_dx,dy+submenu_dy),origin=data_->parameter(Parameter::TargetOrigin),title=rect(Role::TargetTitle,dx+submenu_dx,dy+submenu_dy);
   const auto c=data_->layout_color(Role::TargetTitle);if(!panel(target,ArtRole::ItemsTargetPanel,left,top,width,height)||!text(font,menu.locale().title,title,C2D_Color32f(c.x,c.y,c.z,c.w)))return false;
   for(uint32_t i=0;i<menu.snapshot().party.size();++i){Rect row{target.x+origin.x,target.y+origin.y+i*origin.z,target.z-origin.x,size.y};if(!text(font,menu.snapshot().party[i].nickname,row,normal))return false;if(i==menu.target_selection()&&!cursor(menu,row,true,left,top,width,height))return false;}
  }
  if(menu.phase()==Phase::Message){
   const auto message=rect(Role::Message,dx,dy),inset=data_->parameter(Parameter::TextInset),alignment=data_->parameter(Parameter::MessageAlignment),patch=data_->parameter(Parameter::MessagePatch);
   if(!art_->draw_borrowed_art(ArtRole::DescriptionPanel,message,0,left,top,width,height,&patch))return fail(art_->error().c_str());
   Rect body{message.x+inset.x,message.y+inset.y,message.z-inset.x-inset.z,message.w-inset.y-inset.w};
   const auto& label=menu.message();const float measured=font.text_width(label.c_str()),line=art_->content().parameter(FieldParameter::MainLineHeight);
   const float x=body.x+(alignment.x==1?(body.z-measured)/2:alignment.x==2?body.z-measured:0),y=body.y+(alignment.y==1?(body.w-line)/2:alignment.y==2?body.w-line:0);
   if(!font.draw_text_clipped(label.c_str(),pixel(x),pixel(y),body.x,body.y,body.x+body.z,body.y+body.w,normal))return fail("Field Items message glyph unavailable");
  }
  return true;
 }
};
